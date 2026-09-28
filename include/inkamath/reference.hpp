#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "inkamath/expression.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/expression_visitor.hpp"

#include "inkamath/convergence.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <stdexcept>
#include <vector>

// Nesting too deep, told apart from other failures because a recurrence can
// recover from it by filling its terms from the base up.
struct DepthExceeded : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Which call a memoised answer is for. The definition is named by its address,
// which holds for as long as the memo does: redefining a global replaces its
// object and clears the memo in the same breath. An index needs no string; the
// arguments are encoded as each value encodes itself.
struct MemoKey {
    const void* definition = nullptr;
    bool        indexed    = false;
    int         index      = 0;
    std::string arguments;

    bool operator==(const MemoKey&) const = default;
};

struct MemoHash {
    std::size_t operator()(const MemoKey& key) const {
        std::size_t hash = std::hash<const void*>()(key.definition);
        hash             = hash * 31 + std::hash<int>()(key.index) * 2 + (key.indexed ? 1 : 0);
        return key.arguments.empty() ? hash : hash * 31 + std::hash<std::string>()(key.arguments);
    }
};

// One clause of a definition: 'f_0 = 1' or 'f(x)_n = ...'.
template <typename T>
struct Clause {
    ParametersDefinition<T> parameters;
    PExpression<T>          expression;
    // What the user typed. '?' prints it back rather than rendering the
    // expression, so the answer is the definition, not a normalisation of it.
    std::string             written;

    explicit operator bool() const {return bool(expression);}
};

template <typename T>
class ReferenceStack;

template <typename T>
class EvaluationVisitor;

template <typename T>
class Reference {
public:

    void add_expression(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression, const std::string& written = std::string()) {
        if(reference_name_.empty()) {
            reference_name_ = ai_reference_name;
        }
        else if(reference_name_ != ai_reference_name) {
            throw std::runtime_error(std::string("Interpreter internal error : invalid reference names ") + ai_reference_name + " and " + reference_name_);
        }

        // One definition per name, kept as the clauses that make it up in the
        // order they were written. A plain definition replaces all of them
        // (C11), which is how a definition is started over; anything else
        // replaces the clause that names the same thing and is appended when
        // there is none.
        const Clause<T> clause{ai_parameters, ai_expression, written};
        // One call binds the parameters once, for whichever clause answers, so
        // the clauses have to agree on their names. One that disagrees could
        // only ever read a global under its own name (MODERNIZATION.md, C51).
        const bool starts_over = IsPlain(clause);
        if(!clauses_.empty() && !starts_over
           && ai_parameters.parameters_names() != CallParameters().parameters_names()) {
            throw std::runtime_error(reference_name_ + " takes ("
                                     + Joined(CallParameters().parameters_names())
                                     + "), so a clause cannot take ("
                                     + Joined(ai_parameters.parameters_names()) + ")");
        }
        // A base clause answers for one index rather than for every call, so
        // it is not a default and keeps its place: a guard added after one
        // could never apply, and saying so beats doing nothing.
        if(ai_parameters.guarded() && ai_parameters.indexed() && !ai_parameters.general()
           && Base(ai_parameters.index())) {
            throw std::runtime_error(reference_name_ + "_" + std::to_string(ai_parameters.index())
                                     + " is already defined without a guard,"
                                       " so this clause can never apply");
        }
        // A size is written, not guessed from the cells a clause happens to give.
        if (ai_parameters.cells() && !ai_parameters.row_name().empty() &&
            (!ai_parameters.rows() || !ai_parameters.cols())) {
            throw std::runtime_error(reference_name_ + " has no size; write it as " +
                                     reference_name_ + "[" + ai_parameters.row_name() + "<=rows, " +
                                     ai_parameters.col_name() + "<=cols]");
        }
        if (!starts_over &&
            ((ai_parameters.cells() && Sequence()) || (ai_parameters.indexed() && Cells()))) {
            throw std::runtime_error(reference_name_ +
                                     (Cells() ? " is defined by its cells, so it has no index"
                                              : " is a sequence, so it has no cells of its own"));
        }
        if (starts_over) {
            clauses_.clear();
        } else if (!ai_parameters.guarded() || ai_parameters.cells()) {
            // An index turns a value into a sequence, and a cell into a matrix
            // defined by its cells, so the plain clause goes.
            std::erase_if(clauses_, IsPlain);
        }
        // Writing a clause again replaces it where it stands. Position is what
        // dispatch follows, so a clause that moved would answer differently
        // (MODERNIZATION.md, C45); a guarded clause is named by its left-hand
        // side, which is how it can be corrected at all (C46).
        for(Clause<T>& existing : clauses_) {
            const bool same =
                ai_parameters.guarded()
                    ? existing.parameters.guarded() &&
                          existing.parameters.signature() == ai_parameters.signature()
                    : (IsGeneral(existing) && IsGeneral(clause)) ||
                          (IsBase(existing) && IsBase(clause) &&
                           existing.parameters.index() == clause.parameters.index()) ||
                          (IsAllCells(existing) && IsAllCells(clause)) ||
                          (IsOneCell(existing) && IsOneCell(clause) &&
                           existing.parameters.row() == clause.parameters.row() &&
                           existing.parameters.col() == clause.parameters.col());
            if(same) {
                existing = clause;
                return;
            }
        }
        clauses_.push_back(clause);
    }

    [[nodiscard]] const std::vector<Clause<T>>& Clauses() const { return clauses_; }

    // '?name', or '?name_0' for one clause of a sequence.
    std::string Describe(const ParametersCall<T>& call, ReferenceStack<T>& stack) const {
        int index = 0;
        if(call.TryEvalIndex(stack, index)) {
            if(Plain()) {
                throw std::runtime_error(reference_name_ + " is not a sequence");
            }
            const Clause<T>* clause = Base(index);
            if(!clause) {
                throw std::runtime_error(reference_name_ + " has no clause for index "
                                         + std::to_string(index));
            }
            return Written(*clause);
        }

        std::string description;
        for(const Clause<T>& clause : clauses_) {
            if(!description.empty()) description += '\n';
            description += Written(clause);
        }
        return description;
    }

    T Eval(const ParametersCall<T>& call, ReferenceStack<T>& stack, bool global = true) const {
        const ParametersDefinition<T>& parameters = CallParameters();
        parameters.CheckArity(reference_name_, call);

        // The index and the arguments belong to the caller, so they are
        // evaluated before the callee's scope exists.
        EvaluationVisitor<T> caller(stack);
        int index = 0;
        const bool indexed = call.TryEvalIndex(stack, index);
        typename ParametersDefinition<T>::Arguments arguments =
                parameters.EvaluateArguments(call, caller);

        // Only a global's answer is a function of the key and the globals
        // alone (MODERNIZATION.md, phase 9); a local shares its name with the
        // global it shadows, so the stack says which this is. A limit is not
        // keyed -- the terms it walks are, through this same path.
        const bool memoisable = global && !call.limit() && (indexed || !arguments.empty());
        MemoKey    key;
        if(memoisable) {
            key = Key(indexed, index, arguments);
            if(const T* memoised = stack.Memoised(key)) {
                return *memoised;
            }
        }

        typename ReferenceStack<T>::Frame frame(stack);
        ParametersDefinition<T>::Bind(arguments, stack);
        EvaluationVisitor<T> evaluator(stack);
        parameters.BindDefaults(call, evaluator);
        if(call.limit()) {
            // A guarded general clause is a general clause: it is the index
            // that makes it one (MODERNIZATION.md, C54).
            if(!FirstThat([](const Clause<T>& c) {return c.parameters.general();})) {
                throw std::runtime_error(reference_name_ + " has no general clause, so it has no limit");
            }
            return Converge(arguments, call, stack, global);
        }
        // Storing after the call returns, so that an evaluation which ran out
        // of budget is retried rather than remembered.
        // Caught only where a fill can follow, and filled only after the
        // handler: MSVC runs a handler on top of the stack that threw, which
        // is 256 references deep here.
        if (!memoisable || !indexed || !stack.CanFill()) {
            const T evaluation = EvalImp(indexed, index, evaluator);
            if (memoisable) stack.Memoise(key, evaluation);
            return evaluation;
        }
        std::exception_ptr depth;
        T                  evaluation;
        try {
            evaluation = EvalImp(indexed, index, evaluator);
        } catch (const DepthExceeded&) {
            depth = std::current_exception();
        }
        if (depth) evaluation = Filled(index, arguments, call, stack, evaluator, depth);
        if(memoisable) {
            stack.Memoise(key, evaluation);
        }
        return evaluation;
    }

private:
    // A recurrence nests one reference per term it reaches back, so a term far
    // from its base runs out of depth. Filled from the base up instead, each
    // term finds the one before it remembered. Only what failed comes here, so
    // what answered before answers as it did; and a fill that fails -- a term
    // no evaluation of this one would have asked for -- reports the depth.
    T Filled(int index, const typename ParametersDefinition<T>::Arguments& arguments,
             const ParametersCall<T>& call, ReferenceStack<T>& stack,
             EvaluationVisitor<T>& evaluator, const std::exception_ptr& depth) const {
        const Clause<T>* lowest = EndBase(true);
        if (!lowest) {
            stack.FillFailed();
            std::rethrow_exception(depth);
        }
        const typename ReferenceStack<T>::Filling filling(stack);
        try {
            for (int k = lowest->parameters.index() + 1; k < index; ++k)
                (void)Term(k, arguments, call, stack, true);
        } catch (const std::runtime_error&) {
            stack.FillFailed();
            std::rethrow_exception(depth);
        }
        return EvalImp(true, index, evaluator);
    }

    // One term, evaluated as indexing would evaluate it: in a frame of its own,
    // and remembered where the definition's answers can be.
    T Term(int k, const typename ParametersDefinition<T>::Arguments& arguments,
           const ParametersCall<T>& call, ReferenceStack<T>& stack, bool memoisable) const {
        const MemoKey key = memoisable ? Key(true, k, arguments) : MemoKey();
        if (memoisable)
            if (const T* memoised = stack.Memoised(key)) return *memoised;
        typename ReferenceStack<T>::Frame frame(stack);
        ParametersDefinition<T>::Bind(arguments, stack);
        EvaluationVisitor<T> evaluator(stack);
        CallParameters().BindDefaults(call, evaluator);
        const T evaluation = EvalImp(true, k, evaluator);
        if (memoisable) stack.Memoise(key, evaluation);
        return evaluation;
    }

    // The values as each type encodes them, not their printed form, which
    // rounds and would make two different arguments one key.
    MemoKey Key(bool indexed, int index,
                const typename ParametersDefinition<T>::Arguments& arguments) const {
        MemoKey key{this, indexed, indexed ? index : 0, std::string()};
        for (const auto& argument : arguments) {
            key.arguments += '\0';
            key.arguments += argument.first;
            key.arguments += '=';
            numeric_interface<T>::key(argument.second, key.arguments);
        }
        return key;
    }

    // The clauses of one name share their parameter list; any of them answers
    // for the whole definition.
    const ParametersDefinition<T>& CallParameters() const {
        static const ParametersDefinition<T> none;
        return clauses_.empty() ? none : clauses_.front().parameters;
    }

    // The three unguarded shapes. A guarded clause has no shape: it is never
    // replaced and never stands in for the definition.
    static bool IsPlain(const Clause<T>& c) {
        return !c.parameters.guarded() && !c.parameters.indexed() && !c.parameters.cells();
    }
    static bool IsBase(const Clause<T>& c)
        {return !c.parameters.guarded() && c.parameters.indexed() && !c.parameters.general();}
    static bool IsGeneral(const Clause<T>& c)
        {return !c.parameters.guarded() && c.parameters.general();}
    static bool IsAllCells(const Clause<T>& c) {
        return !c.parameters.guarded() && c.parameters.cells() && !c.parameters.row_name().empty();
    }
    static bool IsOneCell(const Clause<T>& c) {
        return c.parameters.cells() && c.parameters.row_name().empty();
    }

    template <typename Predicate>
    const Clause<T>* FirstThat(Predicate fits) const {
        auto found = std::find_if(clauses_.begin(), clauses_.end(), fits);
        return found == clauses_.end() ? nullptr : &*found;
    }

    const Clause<T>* Plain() const {return FirstThat(IsPlain);}
    const Clause<T>* General() const {return FirstThat(IsGeneral);}
    const Clause<T>* Base(int index) const {
        return FirstThat([&](const Clause<T>& c) {return IsBase(c) && c.parameters.index() == index;});
    }
    bool Guarded() const {
        return FirstThat([](const Clause<T>& c) {return c.parameters.guarded();}) != nullptr;
    }
    bool Sequence() const {
        return FirstThat([](const Clause<T>& c) { return c.parameters.indexed(); }) != nullptr;
    }
    bool Cells() const {
        return FirstThat([](const Clause<T>& c) { return c.parameters.cells(); }) != nullptr;
    }

    // How far down the general clause reaches, and which term a limit starts
    // from: the lowest and the highest index a base clause stands at.
    const Clause<T>* EndBase(bool lowest) const {
        const Clause<T>* found = nullptr;
        for(const Clause<T>& clause : clauses_) {
            if(IsBase(clause)
               && (!found || (clause.parameters.index() < found->parameters.index()) == lowest)) {
                found = &clause;
            }
        }
        return found;
    }

    static std::string Joined(const std::vector<std::string>& names) {
        std::string joined;
        for(const std::string& name : names) {
            if(!joined.empty()) joined += ", ";
            joined += name;
        }
        return joined;
    }

    // A clause bound from inside an expression has no written form to quote.
    std::string Written(const Clause<T>& clause) const {
        return clause.written.empty() ? reference_name_ : clause.written;
    }

    // Does this clause answer this call? The shape is checked first and the
    // guard last, because a guard may read the index it is being asked about
    // -- and the index is bound on trial, so that a clause which does not
    // answer leaves the scope as it found it.
    bool Selects(const Clause<T>& clause, bool indexed, int index,
                 EvaluationVisitor<T>& evaluator) const {
        const ParametersDefinition<T>& p = clause.parameters;
        if(p.indexed() != indexed) return false;
        if(!p.general() && indexed && p.index() != index) {
            return false;
        }
        if(!p.general()) {
            return !p.guarded() || numeric_interface<T>::truth(p.guard()->accept(evaluator));
        }
        typename ReferenceStack<T>::Trial trial(evaluator.stack(), p.index_name());
        SetIndex(p.index_name(), index, evaluator.stack());
        if(p.guarded() && !numeric_interface<T>::truth(p.guard()->accept(evaluator))) {
            return false;
        }
        trial.keep();
        return true;
    }

    T EvalImp(bool indexed, int index, EvaluationVisitor<T>& evaluator) const {
        if (Cells()) {
            if (indexed) {
                throw std::runtime_error(reference_name_ + " is not a sequence");
            }
            return EvaluateCells(evaluator);
        }
        // Clauses are tried in the order they were written, and the clause not
        // chosen is not evaluated -- which is what index dispatch has always
        // done. Order is the writer's to choose because neither precedence
        // serves both cases: a guard reading the previous term must not be
        // reached at the base index, while a guard ruling an index out must be
        // (MODERNIZATION.md, phase 10). An unguarded clause that would answer
        // every call -- a plain definition, or the general clause -- is the
        // definition's default and is tried last wherever it stands, so that
        // a base case beats the general one however the two were written
        // (README.md section 4) and so that a definition can be patched up
        // afterwards with the cases it turned out to need.
        for(const Clause<T>& clause : clauses_) {
            if(IsGeneral(clause) || IsPlain(clause)) continue;
            if(Selects(clause, indexed, index, evaluator)) {
                return clause.expression->accept(evaluator);
            }
        }
        if(indexed) {
            // An index on something that is not a sequence used to be dropped
            // without a word, which is the last of C13's silent answers.
            if(Plain()) {
                throw std::runtime_error(reference_name_ + " is not a sequence");
            }
            // A general clause reaches down only as far as the lowest base
            // clause; below that the sequence is simply not defined. With no
            // base clause at all it applies everywhere, which is right for a
            // closed form and divergent for a recurrence -- the budget says so.
            const Clause<T>* lowest = EndBase(true);
            if(const Clause<T>* general = General()) {
                if(!lowest || index >= lowest->parameters.index()) {
                    return EvaluateGeneralClause(*general, index, evaluator);
                }
            }
            if(Guarded()) {
                throw std::runtime_error("no clause of " + reference_name_ + " applies");
            }
            throw std::runtime_error(reference_name_ + " has no clause for index "
                                     + std::to_string(index));
        }
        if(const Clause<T>* plain = Plain()) {
            return plain->expression->accept(evaluator);
        }
        if(Guarded()) {
            throw std::runtime_error("no clause of " + reference_name_ + " applies");
        }
        // Nothing sensible to invent: a sequence has no value under its bare
        // name. A limit is something the user asks for, not something a
        // lookup does on its way past.
        const Clause<T>* lowest = EndBase(true);
        const std::string example = lowest ? std::to_string(lowest->parameters.index()) : "0";
        throw std::runtime_error(reference_name_ + " is a sequence; index it (" + reference_name_
                                 + "_" + example + ")"
                                 + (General()
                                    ? " or take its limit (lim " + reference_name_ + ")"
                                    : ""));
    }

    // A matrix defined by its cells (README.md section 2). Its size is what its
    // clauses for all cells bound it to, and they agree on it.
    T EvaluateCells(EvaluationVisitor<T>& evaluator) const {
        std::optional<Extent> extent;
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!p.cells() || p.row_name().empty()) continue;
            const Extent size{Size(p.rows()->accept(evaluator)), Size(p.cols()->accept(evaluator))};
            if (extent && *extent != size) {
                throw std::runtime_error("the clauses of " + reference_name_ +
                                         " give it different sizes");
            }
            extent = size;
        }
        if (!extent) {
            throw std::runtime_error(reference_name_ + " has no size; write it as " +
                                     reference_name_ + "[j<=rows, k<=cols]");
        }
        T matrix(*extent);
        for (size_t row = 1; row <= extent->rows; ++row) {
            for (size_t col = 1; col <= extent->cols; ++col) {
                const std::optional<T> cell =
                    Cell(static_cast<int>(row), static_cast<int>(col), evaluator);
                if (!cell) continue;
                if (cell->Size() != Extent{1, 1}) {
                    throw std::runtime_error("a cell of " + reference_name_ +
                                             " must be a single value, not a " +
                                             cell->Size().toString() + " matrix");
                }
                matrix(row, col) = (*cell)(1, 1);
            }
        }
        return matrix;
    }

    static size_t Size(const T& value) {
        const int size = AsIndex<T>(value);
        if (size < 1) {
            throw std::runtime_error("a size must be at least 1, not " + std::to_string(size));
        }
        return static_cast<size_t>(size);
    }

    // A cell's own clause if it has one, as a base clause beats a sequence's
    // general clause; else the first clause for all cells that holds, guarded
    // ones in the order written and the unguarded one last; else none, and the
    // cell is 0, as a short row of a literal is padded. The names are bound on
    // trial, so that one clause's names cannot shadow a global in the next.
    std::optional<T> Cell(int row, int col, EvaluationVisitor<T>& evaluator) const {
        ReferenceStack<T>& stack = evaluator.stack();
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!IsOneCell(clause) || p.row() != row || p.col() != col) continue;
            if (p.guarded() && !numeric_interface<T>::truth(p.guard()->accept(evaluator))) continue;
            return clause.expression->accept(evaluator);
        }
        for (const bool guarded : {true, false}) {
            for (const Clause<T>& clause : clauses_) {
                const ParametersDefinition<T>& p = clause.parameters;
                if (!p.cells() || p.row_name().empty() || p.guarded() != guarded) continue;
                typename ReferenceStack<T>::Trial row_name(stack, p.row_name());
                typename ReferenceStack<T>::Trial col_name(stack, p.col_name());
                SetIndex(p.row_name(), row, stack);
                SetIndex(p.col_name(), col, stack);
                if (p.guarded() && !numeric_interface<T>::truth(p.guard()->accept(evaluator)))
                    continue;
                return clause.expression->accept(evaluator);
            }
        }
        return std::nullopt;
    }

    T EvaluateGeneralClause(const Clause<T>& general, int index,
                            EvaluationVisitor<T>& evaluator) const {
        SetIndex(general.parameters.index_name(), index, evaluator.stack());
        return general.expression->accept(evaluator);
    }

    // Every term goes through EvalImp, so the terms a limit walks are the
    // terms an index gives. Evaluating the general clause directly made them
    // two different sequences as soon as a guard existed (C54), and so did
    // one frame for every term, where a local outlived its term (C66).
    T Converge(const typename ParametersDefinition<T>::Arguments& arguments,
               const ParametersCall<T>& call, ReferenceStack<T>& stack, bool global) const {
        const auto evaluate = [&](long long k) {
            return Term(static_cast<int>(k), arguments, call, stack, global);
        };
        long long index = 0;
        Convergence<T> convergence(reference_name_);
        // With no base clause there is no term to compare the first one
        // against. Comparing it to a default-constructed T said that any
        // sequence starting near zero had converged to it.
        if (const Clause<T>* highest = EndBase(false)) {
            index = highest->parameters.index();
            (void)convergence.Next(evaluate(index));
        }

        T evaluation;
        for (size_t term = 0; term < Convergence<T>::max_terms; ++term) {
            evaluation = evaluate(++index);
            if (convergence.Next(evaluation)) {
                return Convergence<T>::Limit(evaluation);
            }
        }
        throw std::runtime_error(reference_name_ + " did not converge within " +
                                 std::to_string(Convergence<T>::max_terms) + " terms (last term " +
                                 numeric_interface<T>::toString(evaluation) + ")");
    }

    void SetIndex(const std::string& name, int index, ReferenceStack<T>& stack) const {
        stack.BindValue(name, T(index));
    }

    std::string reference_name_;

    // The whole definition: its clauses, in the order they were written.
    std::vector<Clause<T>> clauses_;
};

#endif // HPP_INKREFERENCE

#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "inkamath/expression.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/expression_visitor.hpp"

#include "inkamath/convergence.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
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
struct Scope;

template <typename T>
struct Model;

template <typename T>
class Reference {
public:
    Reference() = default;
    explicit Reference(std::string name) : reference_name_(std::move(name)) {}

    [[nodiscard]] const std::string& Name() const { return reference_name_; }

    // Where the definition was written, and so where the names it reads are
    // sought; null for a local, which reads them where it is evaluated.
    const Scope<T>* home = nullptr;

    // What makes a name something other than a value: a model, a file used,
    // or an input nothing supplies (DESIGN.md, phase 15).
    std::shared_ptr<const Model<T>> model;
    std::shared_ptr<const Scope<T>> file;
    bool                            input = false;

    // What an unnamed instance's argument read of the frame it was written in,
    // which is gone by the time the argument is evaluated.
    std::vector<std::pair<std::string, T>> captured;

    [[nodiscard]] bool Value() const { return !model && !file && !input; }

    // Whether it is written as a name applied to arguments and nothing else,
    // 'gain(x_n = n)', which is an instance where the name is a model's. Asked
    // at every evaluation, so answered once.
    [[nodiscard]] bool Applied() const {
        if (!applied_) {
            const Clause<T>*     only = clauses_.size() == 1 ? &clauses_.front() : nullptr;
            const Expression<T>* callee =
                only && IsPlain(*only) && only->parameters.parameters_names().empty()
                    ? only->expression.get()
                    : nullptr;
            if (const auto* member = dynamic_cast<const MemberExpression<T>*>(callee))
                callee = member->Member().get();
            const auto* call = dynamic_cast<const FuncExpression<T>*>(callee);
            applied_         = dynamic_cast<const RefExpression<T>*>(callee) ||
                       (call && !call->m_e2() && !call->limit());
        }
        return *applied_;
    }

    void add_expression(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression, const std::string& written = std::string()) {
        applied_.reset();
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
        // only ever read a global under its own name (DESIGN.md, C51).
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
        if (ai_parameters.guarded() && ai_parameters.indexed() && !ai_parameters.general() &&
            !ai_parameters.cells() && Base(ai_parameters.index())) {
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
        // A matrix's cells and a sequence's terms do not mix; a term's cells are
        // a sequence's.
        const bool matrix_cells = FirstThat(
            [](const Clause<T>& c) { return c.parameters.cells() && !c.parameters.indexed(); });
        if (!starts_over && ((ai_parameters.cells() && !ai_parameters.indexed() && Sequence()) ||
                             (ai_parameters.indexed() && matrix_cells))) {
            throw std::runtime_error(
                reference_name_ + (matrix_cells ? " is defined by its cells, so it has no index"
                                                : " is a sequence, so it has no cells of its own"));
        }
        if (starts_over) {
            clauses_.clear();
        } else if (ai_parameters.indexed() ||
                   (!ai_parameters.guarded() && !ai_parameters.cells())) {
            // An index turns a value into a sequence, so the plain clause goes,
            // guarded or not: kept, the name was both (DESIGN.md, C70).
            // A cell clause of a matrix keeps it: a matrix written whole has
            // cells, and the clause overrides one, as a base clause does a
            // general one.
            std::erase_if(clauses_, IsPlain);
        }
        // Writing a clause again replaces it where it stands. Position is what
        // dispatch follows, so a clause that moved would answer differently
        // (DESIGN.md, C45); a guarded clause is named by its left-hand
        // side, which is how it can be corrected at all (C46).
        for(Clause<T>& existing : clauses_) {
            const bool same =
                ai_parameters.guarded()
                    ? existing.parameters.guarded() &&
                          existing.parameters.signature() == ai_parameters.signature()
                    : !existing.parameters.guarded() && Shape(existing) == Shape(clause);
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
        // alone (DESIGN.md, phase 9); a local shares its name with the
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

        typename ReferenceStack<T>::Within within(stack, home);
        typename ReferenceStack<T>::Frame  frame(stack);
        ParametersDefinition<T>::Bind(captured, stack);
        ParametersDefinition<T>::Bind(arguments, stack);
        EvaluationVisitor<T> evaluator(stack);
        parameters.BindDefaults(call, evaluator);
        if(call.limit()) {
            // A guarded general clause is a general clause: it is the index
            // that makes it one (DESIGN.md, C54).
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
    // term finds the one before it remembered, stepping as the recurrence
    // does: one that reads two back fills every other term, those its index
    // reaches. Only what failed comes here, so what answered before answers as
    // it did; and a fill that fails asked for a term this one reaches, so its
    // reason is this one's.
    T Filled(int index, const typename ParametersDefinition<T>::Arguments& arguments,
             const ParametersCall<T>& call, ReferenceStack<T>& stack,
             EvaluationVisitor<T>& evaluator, const std::exception_ptr& depth) const {
        const Clause<T>* lowest = EndBase(true);
        if (!lowest) {
            stack.FillFailed();
            std::rethrow_exception(depth);
        }
        const int       base     = lowest->parameters.index();
        const long long distance = static_cast<long long>(index) - base;
        // Thrown as the depth it stands for, so that each call out to the one
        // asked for says it again with its own index.
        if (distance > ReferenceStack<T>::max_filled) {
            stack.FillFailed();
            throw DepthExceeded(reference_name_ + "_" + std::to_string(index) + " is " +
                                std::to_string(distance) +
                                " terms from its base, and a fill stops at " +
                                std::to_string(ReferenceStack<T>::max_filled));
        }
        typename ReferenceStack<T>::Filling filling(stack);
        const int                           stride = Stride();
        try {
            for (long long k = index - (distance - 1) / stride * stride; k < index; k += stride) {
                filling.Next();
                (void)Term(static_cast<int>(k), arguments, call, stack, true);
            }
        } catch (const DepthExceeded&) {
            stack.FillFailed();
            std::rethrow_exception(depth);
        } catch (const std::runtime_error&) {
            stack.FillFailed();
            throw;
        }
        filling.Next();
        return EvalImp(true, index, evaluator);
    }

    // How far apart the terms a fill computes: the greatest common divisor of
    // how far back the general clauses read terms, this sequence's or another
    // that reads it back, as a term reaches only those below it by such
    // steps; 1 where a read is not the index less a constant, and where there
    // is none. A read at a constant index reaches no further.
    int Stride() const {
        int stride = 0;
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!p.general()) continue;
            if (!Lags(clause.expression, p.index_name(), stride) ||
                !Lags(p.guard(), p.index_name(), stride))
                return 1;
        }
        return stride == 0 ? 1 : stride;
    }

    bool Lags(const PExpression<T>& expression, const std::string& index, int& stride) const {
        if (!expression) return true;
        if (const auto* term = dynamic_cast<const FuncExpression<T>*>(expression.get());
            term && term->m_e2() && !dynamic_cast<const ValExpression<T>*>(term->m_e2().get())) {
            const std::optional<int> lag = Lag(term->m_e2(), index);
            if (!lag) return false;
            stride = std::gcd(stride, *lag < 0 ? -*lag : *lag);
        }
        for (const PExpression<T>& child : expression->Children())
            if (!Lags(child, index, stride)) return false;
        return true;
    }

    // 'n', or 'n' plus or less a whole constant, as how far back it reads.
    static std::optional<int> Lag(const PExpression<T>& at, const std::string& index) {
        if (const auto* ref = dynamic_cast<const RefExpression<T>*>(at.get()))
            return ref->Name() == index ? std::optional<int>(0) : std::nullopt;
        const auto* sum = dynamic_cast<const AddExpression<T>*>(at.get());
        if (!sum) return std::nullopt;
        for (const auto& [side, other] :
             {std::pair(sum->m_e1(), sum->m_e2()), std::pair(sum->m_e2(), sum->m_e1())}) {
            const auto*              constant = dynamic_cast<const ValExpression<T>*>(other.get());
            const std::optional<int> lag      = constant ? Lag(side, index) : std::nullopt;
            if (!lag) continue;
            try {
                return *lag - AsIndex<T>(constant->value);
            } catch (const std::runtime_error&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    // One term, evaluated as indexing would evaluate it: in a frame of its own,
    // and remembered where the definition's answers can be.
    T Term(int k, const typename ParametersDefinition<T>::Arguments& arguments,
           const ParametersCall<T>& call, ReferenceStack<T>& stack, bool memoisable) const {
        const MemoKey key = memoisable ? Key(true, k, arguments) : MemoKey();
        if (memoisable)
            if (const T* memoised = stack.Memoised(key)) return *memoised;
        typename ReferenceStack<T>::Within within(stack, home);
        typename ReferenceStack<T>::Frame  frame(stack);
        ParametersDefinition<T>::Bind(captured, stack);
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
    // What an unguarded clause answers for, so that writing one again
    // replaces it: which index, if one, and which cells, if one.
    static std::tuple<bool, bool, int, bool, bool, int, int> Shape(const Clause<T>& c) {
        const ParametersDefinition<T>& p    = c.parameters;
        const bool                     base = p.indexed() && !p.general();
        return {p.indexed(),
                p.general(),
                base ? p.index() : 0,
                p.cells(),
                IsOneCell(c),
                IsOneCell(c) ? p.row() : 0,
                IsOneCell(c) ? p.col() : 0};
    }
    // A clause that gives a whole term at one index: a base clause, written
    // whole or by its cells, which name every cell.
    static bool IsBaseTerm(const Clause<T>& c) {
        const ParametersDefinition<T>& p = c.parameters;
        return p.indexed() && !p.general() && (p.cells() ? !IsOneCell(c) : !p.guarded());
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
            if (IsBaseTerm(clause) &&
                (!found || (clause.parameters.index() < found->parameters.index()) == lowest)) {
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
        if (Cells() && !Sequence()) {
            if (indexed) {
                throw std::runtime_error(reference_name_ + " is not a sequence");
            }
            return EvaluateCells(evaluator);
        }
        if (Cells() && indexed) return EvaluateTerm(index, evaluator);
        // Clauses are tried in the order they were written, and the clause not
        // chosen is not evaluated -- which is what index dispatch has always
        // done. Order is the writer's to choose because neither precedence
        // serves both cases: a guard reading the previous term must not be
        // reached at the base index, while a guard ruling an index out must be
        // (DESIGN.md, phase 10). An unguarded clause that would answer
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
        if (Guarded() && !Cells()) {
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
    // clauses for all cells bound it to, or the matrix written whole, and they
    // agree on it; the matrix written whole gives every cell no clause does.
    T EvaluateCells(EvaluationVisitor<T>& evaluator) const {
        std::optional<Extent> extent;
        std::optional<T>      whole;
        if (const Clause<T>* plain = Plain()) {
            whole  = plain->expression->accept(evaluator);
            extent = whole->Size();
        }
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
        T matrix = whole ? *whole : T(*extent);
        // A clause for one cell can name a cell outside the size, which says so
        // as reading it would.
        for (const Clause<T>& clause : clauses_) {
            if (IsOneCell(clause)) (void)matrix(clause.parameters.row(), clause.parameters.col());
        }
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
    // cell is the matrix written whole's, or 0 without one, as a short row of a
    // literal is padded.
    std::optional<T> Cell(int row, int col, EvaluationVisitor<T>& evaluator) const {
        const auto matrix = [](const Clause<T>& c) { return !c.parameters.indexed(); };
        if (auto one = OneCell(row, col, 0, matrix, evaluator)) return one->first;
        return AllCells(row, col, 0, matrix, evaluator);
    }

    // The clause for this one cell among those that fit, if one holds, and
    // what it gives. A general clause sees the index; the names are bound on
    // trial, so that one clause's names cannot shadow a global in the next.
    template <typename Fits>
    std::optional<std::pair<T, const Clause<T>*>> OneCell(int row, int col, int index, Fits fits,
                                                          EvaluationVisitor<T>& evaluator) const {
        ReferenceStack<T>& stack = evaluator.stack();
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!IsOneCell(clause) || !fits(clause) || p.row() != row || p.col() != col) continue;
            typename ReferenceStack<T>::Trial term(stack, p.index_name());
            if (p.general()) SetIndex(p.index_name(), index, stack);
            if (p.guarded() && !numeric_interface<T>::truth(p.guard()->accept(evaluator))) continue;
            return std::pair<T, const Clause<T>*>(clause.expression->accept(evaluator), &clause);
        }
        return std::nullopt;
    }

    template <typename Fits>
    std::optional<T> AllCells(int row, int col, int index, Fits fits,
                              EvaluationVisitor<T>& evaluator) const {
        ReferenceStack<T>& stack = evaluator.stack();
        for (const bool guarded : {true, false}) {
            for (const Clause<T>& clause : clauses_) {
                const ParametersDefinition<T>& p = clause.parameters;
                if (!p.cells() || p.row_name().empty() || p.guarded() != guarded || !fits(clause))
                    continue;
                typename ReferenceStack<T>::Trial term(stack, p.index_name());
                typename ReferenceStack<T>::Trial row_name(stack, p.row_name());
                typename ReferenceStack<T>::Trial col_name(stack, p.col_name());
                if (p.general()) SetIndex(p.index_name(), index, stack);
                SetIndex(p.row_name(), row, stack);
                SetIndex(p.col_name(), col, stack);
                if (p.guarded() && !numeric_interface<T>::truth(p.guard()->accept(evaluator)))
                    continue;
                return clause.expression->accept(evaluator);
            }
        }
        return std::nullopt;
    }

    // A term of a sequence defined by its cells (README.md section 4). From
    // the most specific clause to the least: one for this index and this
    // cell; the base term at this index, written whole or by its cells; one
    // for this cell of every term; the general clauses, by their cells and
    // then whole. A base term and a cell of every term are each the more
    // specific in one and the less in the other, so where both give a cell
    // the definition is asked which it means (DESIGN.md, next in line).
    T EvaluateTerm(int index, EvaluationVisitor<T>& evaluator) const {
        const auto base = [index](const Clause<T>& c) {
            return c.parameters.indexed() && !c.parameters.general() &&
                   c.parameters.index() == index;
        };
        const auto general = [](const Clause<T>& c) { return c.parameters.general(); };
        const bool based = FirstThat([&](const Clause<T>& c) { return base(c) && IsBaseTerm(c); });
        const Clause<T>* lowest = EndBase(true);
        if (!based && lowest && index < lowest->parameters.index()) {
            throw std::runtime_error(reference_name_ + " has no clause for index " +
                                     std::to_string(index));
        }
        // The term written whole, which is the size and every cell no clause
        // gives: the base's, or the general clauses' as they are dispatched.
        std::optional<T> whole;
        if (based) {
            if (const Clause<T>* b =
                    FirstThat([&](const Clause<T>& c) { return base(c) && !c.parameters.cells(); }))
                whole = b->expression->accept(evaluator);
        } else {
            for (const Clause<T>& clause : clauses_) {
                const ParametersDefinition<T>& p = clause.parameters;
                if (!p.general() || p.cells() || !p.guarded()) continue;
                if (Selects(clause, true, index, evaluator)) {
                    whole = clause.expression->accept(evaluator);
                    break;
                }
            }
            if (!whole) {
                if (const Clause<T>* g = FirstThat(
                        [](const Clause<T>& c) { return IsGeneral(c) && !c.parameters.cells(); }))
                    whole = EvaluateGeneralClause(*g, index, evaluator);
            }
        }
        const auto level = [&](const Clause<T>& c) { return based ? base(c) : general(c); };
        std::optional<Extent> extent;
        if (whole) extent = whole->Size();
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!p.cells() || p.row_name().empty() || !level(clause)) continue;
            typename ReferenceStack<T>::Trial term(evaluator.stack(), p.index_name());
            if (p.general()) SetIndex(p.index_name(), index, evaluator.stack());
            const Extent size{Size(p.rows()->accept(evaluator)), Size(p.cols()->accept(evaluator))};
            if (extent && *extent != size) {
                throw std::runtime_error("the clauses of " + reference_name_ +
                                         " give it different sizes");
            }
            extent = size;
        }
        if (!extent) {
            throw std::runtime_error(reference_name_ + " has no size; write it as " +
                                     reference_name_ + "_n[j<=rows, k<=cols]");
        }
        T term = whole ? *whole : T(*extent);
        for (const Clause<T>& clause : clauses_) {
            if (IsOneCell(clause) && (base(clause) || general(clause)))
                (void)term(clause.parameters.row(), clause.parameters.col());
        }
        const std::string name = reference_name_ + "_" + std::to_string(index);
        for (size_t row = 1; row <= extent->rows; ++row) {
            for (size_t col = 1; col <= extent->cols; ++col) {
                const int        r = static_cast<int>(row), c = static_cast<int>(col);
                std::optional<T> cell;
                if (auto own = OneCell(r, c, index, base, evaluator)) {
                    cell = own->first;
                } else if (based) {
                    if (auto every = OneCell(r, c, index, general, evaluator)) {
                        throw std::runtime_error(
                            name + " and " + reference_name_ + "_" +
                            every->second->parameters.index_name() + "[" + std::to_string(r) + "," +
                            std::to_string(c) + "] both give row " + std::to_string(r) +
                            ", column " + std::to_string(c) + " of " + name + "; write " + name +
                            "[" + std::to_string(r) + "," + std::to_string(c) + "] to say which");
                    }
                    cell = AllCells(r, c, index, base, evaluator);
                } else if (auto every = OneCell(r, c, index, general, evaluator)) {
                    cell = every->first;
                } else {
                    cell = AllCells(r, c, index, general, evaluator);
                }
                if (!cell) continue;
                if (cell->Size() != Extent{1, 1}) {
                    throw std::runtime_error("a cell of " + reference_name_ +
                                             " must be a single value, not a " +
                                             cell->Size().toString() + " matrix");
                }
                term(row, col) = (*cell)(1, 1);
            }
        }
        return term;
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

    mutable std::optional<bool> applied_;
};

// The names one file, one instance or the session defines, and where a name
// none of them defines is sought next.
template <typename T>
struct Scope {
    std::unordered_map<std::string, std::shared_ptr<const Reference<T>>> names;
    const Scope*                                                         parent = nullptr;
    // How the session names it, 'g' or 'filters'; empty for the session's own.
    std::string label;
    // The file it was read from, for a file's.
    std::string file;
    // What defines a named instance, 'gain(x_n = n)'.
    std::string defined;
    // An instance's model.
    const Model<T>* model = nullptr;

    [[nodiscard]] std::string Qualified(const std::string& name) const {
        return label.empty() ? name : label + "." + name;
    }
};

// A function whose value is a group of definitions (DESIGN.md, phase
// 15). Its signature is its interface -- a parameter has a default, an input
// has none -- and its body is installed afresh in each instance.
template <typename T>
struct Model {
    struct Parameter {
        std::string    name;
        std::string    index;     // 'n', for an input written 'x_n'
        PExpression<T> fallback;  // null for an input
    };
    struct Statement {
        std::string                     name;
        PExpression<T>                  definition;  // an EqualExpression, or null
        std::shared_ptr<const Model<T>> model;       // or a model defined inside
        std::string                     written;
    };
    struct Argument {
        PExpression<T> expression;
        std::string    index;
    };

    std::string            header;  // 'gain(k = 2, b = 1, x_n)'
    std::vector<Parameter> parameters;
    std::vector<Statement> body;
    const Scope<T>*        scope = nullptr;  // where it was written

    [[nodiscard]] std::string Describe() const {
        std::string text = header + " = {\n";
        for (const Statement& statement : body) {
            const std::string written =
                statement.model ? statement.model->Describe() : statement.written;
            for (size_t start = 0; start < written.size();) {
                const size_t end = std::min(written.find('\n', start), written.size());
                text += "    " + written.substr(start, end - start) + "\n";
                start = end + 1;
            }
        }
        return text + "}";
    }

    // One name an instance has, to read in a message: the first the body
    // defines, at its lowest index if it is a sequence.
    [[nodiscard]] std::string Example() const {
        const auto head = std::find_if(body.begin(), body.end(),
                                       [](const Statement& s) { return bool(s.definition); });
        if (head == body.end()) return parameters.empty() ? "" : parameters.front().name;
        std::optional<int> lowest;
        bool               indexed = false;
        for (const Statement& statement : body) {
            if (statement.name != head->name || !statement.definition) continue;
            const std::vector<PExpression<T>>& left =
                statement.definition->Children()[0]->Children();
            if (left.empty() || !left[1]) continue;
            indexed = true;
            if (const auto* at = dynamic_cast<const ValExpression<T>*>(left[1].get()))
                lowest = std::min(lowest.value_or(AsIndex<T>(at->value)), AsIndex<T>(at->value));
        }
        return indexed ? head->name + "_" + std::to_string(lowest.value_or(0)) : head->name;
    }

    // One argument per parameter, empty where the call gives none.
    std::vector<std::optional<Argument>> Bind(const std::string&       name,
                                              const ParametersCall<T>& call) const {
        std::vector<PExpression<T>> given;
        if (const auto* list = dynamic_cast<const MatExpression<T>*>(call.arguments().get()))
            given = list->Children();
        if (given.size() > parameters.size()) {
            throw std::runtime_error(
                name + " expects " + std::to_string(parameters.size()) +
                (parameters.size() == 1 ? " argument, got " : " arguments, got ") +
                std::to_string(given.size()));
        }
        std::vector<std::optional<Argument>> bound(parameters.size());
        size_t                               positional = 0;
        for (const PExpression<T>& argument : given) {
            const auto* named = dynamic_cast<const EqualExpression<T>*>(argument.get());
            if (!named) {
                bound[positional] = Argument{argument, parameters[positional].index};
                ++positional;
                continue;
            }
            std::string index;
            if (const auto* term = dynamic_cast<const FuncExpression<T>*>(named->m_e1().get())) {
                const auto* variable = dynamic_cast<const RefExpression<T>*>(term->m_e2().get());
                if (!variable || term->m_e1())
                    throw std::runtime_error("an argument is named as 'k = 2', or 'x_n = n'");
                index = variable->Name();
            }
            const std::string& key   = named->m_e1()->Name();
            const auto         found = std::find_if(parameters.begin(), parameters.end(),
                                                    [&](const Parameter& p) { return p.name == key; });
            if (found == parameters.end())
                throw std::runtime_error(name + " has no parameter " + key);
            std::optional<Argument>& slot = bound[static_cast<size_t>(found - parameters.begin())];
            if (slot) throw std::runtime_error(name + " got two values for " + key);
            slot = Argument{named->m_e2(), index};
        }
        return bound;
    }
};

#endif // HPP_INKREFERENCE

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
// object and clears the memo in the same breath. Arguments are the same when
// their bits are, not their values: dbl(1/2) and dbl(~0.5) are two calls.
template <typename T>
struct MemoKey {
    const void*                            definition = nullptr;
    bool                                   indexed    = false;
    int                                    index      = 0;
    std::vector<std::pair<std::string, T>> arguments;

    bool operator==(const MemoKey& other) const {
        return definition == other.definition && indexed == other.indexed && index == other.index &&
               std::equal(arguments.begin(), arguments.end(), other.arguments.begin(),
                          other.arguments.end(), [](const auto& a, const auto& b) {
                              return a.first == b.first &&
                                     numeric_interface<T>::same(a.second, b.second);
                          });
    }
};

template <typename T>
struct MemoHash {
    std::size_t operator()(const MemoKey<T>& key) const {
        std::size_t hash = std::hash<const void*>()(key.definition);
        hash             = hash * 31 + std::hash<int>()(key.index) * 2 + (key.indexed ? 1 : 0);
        for (const auto& argument : key.arguments)
            hash = hash * 31 + numeric_interface<T>::hash(argument.second);
        return hash;
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
            (!ai_parameters.rows() || !ai_parameters.cols() ||
             (ai_parameters.tensor() && !ai_parameters.slices()))) {
            throw std::runtime_error(
                reference_name_ + " has no size; write it as " + reference_name_ + "[" +
                (ai_parameters.tensor() ? ai_parameters.slice_name() + "<=slices, " : "") +
                ai_parameters.row_name() + "<=rows, " + ai_parameters.col_name() + "<=cols]");
        }
        // One index names a row, so a definition by rows and columns has no
        // clause for one, and a column's clauses name no column; a tensor's
        // name all three.
        if (ai_parameters.cells()) {
            if (const Clause<T>* other = FirstThat([&](const Clause<T>& c) {
                    return c.parameters.cells() && c.parameters.rank() != ai_parameters.rank();
                })) {
                static const char* const names[] = {
                    " is defined by one index, so a clause names one",
                    " is defined by a row and a column, so a clause names both",
                    " is defined by a slice, a row and a column, so a clause names all three"};
                throw std::runtime_error(reference_name_ + names[other->parameters.rank() - 1]);
            }
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
        // The walk over the cells never asks a guarded clause written whole
        // (C82), and a guard on each cell says the same.
        const auto whole_guarded = [](const Clause<T>& c) {
            return c.parameters.guarded() && !c.parameters.cells();
        };
        if (!starts_over && ai_parameters.guarded() && !ai_parameters.cells() &&
            FirstThat([](const Clause<T>& c) { return c.parameters.cells(); })) {
            throw std::runtime_error(reference_name_ +
                                     " is defined by its cells, so a clause for all of it cannot "
                                     "be guarded; guard its cells");
        }
        if (ai_parameters.cells() && FirstThat(whole_guarded)) {
            throw std::runtime_error(reference_name_ +
                                     " has a guarded clause for all of it, so it cannot be "
                                     "defined by its cells; guard its cells");
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

    // The value of a plain definition by a literal of numbers, once built.
    [[nodiscard]] const T* Kept() const {
        const Clause<T>* only = clauses_.size() == 1 ? &clauses_.front() : nullptr;
        if (!Value() || !only || !IsPlain(*only) || !only->parameters.parameters_names().empty())
            return nullptr;
        const auto* literal = dynamic_cast<const MatExpression<T>*>(only->expression.get());
        return literal && literal->built ? &*literal->built : nullptr;
    }

    // A definition by cells at a call, or nothing for one that is not: walked
    // for the value and for grad's parts alike, as Walk evaluates a clause,
    // asks a guard and stores a cell (DESIGN.md, grad of a definition by cells).
    template <typename Walk>
    std::optional<typename Walk::Result> ByCells(bool indexed, int index, Walk& walk) const {
        if (!Cells() || (Sequence() && !indexed)) return std::nullopt;
        if (Sequence()) return EvaluateTerm(index, walk);
        if (indexed) throw std::runtime_error(reference_name_ + " is not a sequence");
        return EvaluateCells(walk);
    }

    // How far down the general clause reaches, and which term a limit starts
    // from: the lowest and the highest index a base clause stands at.
    const Clause<T>* EndBase(bool lowest) const {
        const Clause<T>* found = nullptr;
        for (const Clause<T>& clause : clauses_) {
            if (IsBaseTerm(clause) &&
                (!found || (clause.parameters.index() < found->parameters.index()) == lowest)) {
                found = &clause;
            }
        }
        return found;
    }

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
        // The key holds the arguments, so that a call copies none of them.
        MemoKey<T> key{this, indexed, indexed ? index : 0,
                       parameters.EvaluateArguments(call, caller)};

        const auto& arguments = key.arguments;

        // Only a global's answer is a function of the key and the globals
        // alone (DESIGN.md, phase 9); a local shares its name with the
        // global it shadows, so the stack says which this is. A limit is not
        // keyed -- the terms it walks are, through this same path.
        const bool memoisable = global && !call.limit() && (indexed || !arguments.empty());
        if (memoisable) {
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
            if (memoisable) stack.Memoise(std::move(key), evaluation);
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
            stack.Memoise(std::move(key), evaluation);
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
        MemoKey<T> key = memoisable ? MemoKey<T>{this, true, k, arguments} : MemoKey<T>();
        if (memoisable)
            if (const T* memoised = stack.Memoised(key)) return *memoised;
        typename ReferenceStack<T>::Within within(stack, home);
        typename ReferenceStack<T>::Frame  frame(stack);
        ParametersDefinition<T>::Bind(captured, stack);
        ParametersDefinition<T>::Bind(arguments, stack);
        EvaluationVisitor<T> evaluator(stack);
        CallParameters().BindDefaults(call, evaluator);
        const T evaluation = EvalImp(true, k, evaluator);
        if (memoisable) stack.Memoise(std::move(key), evaluation);
        return evaluation;
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
    static std::tuple<bool, bool, int, bool, bool, int, int, int> Shape(const Clause<T>& c) {
        const ParametersDefinition<T>& p    = c.parameters;
        const bool                     base = p.indexed() && !p.general();
        return {p.indexed(),
                p.general(),
                base ? p.index() : 0,
                p.cells(),
                IsOneCell(c),
                IsOneCell(c) ? p.row() : 0,
                IsOneCell(c) ? p.col() : 0,
                IsOneCell(c) ? p.slice() : 0};
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
    template <typename Walk>
    bool Selects(const Clause<T>& clause, bool indexed, int index, Walk& walk) const {
        const ParametersDefinition<T>& p = clause.parameters;
        if(p.indexed() != indexed) return false;
        if(!p.general() && indexed && p.index() != index) {
            return false;
        }
        if(!p.general()) {
            return !p.guarded() || walk.Holds(*this, clause, index, 0, 0, 0);
        }
        typename ReferenceStack<T>::Trial trial(walk.stack(), p.index_name());
        SetIndex(p.index_name(), index, walk.stack());
        if (p.guarded() && !walk.Holds(*this, clause, index, 0, 0, 0)) return false;
        trial.keep();
        return true;
    }

    // The walk over the cells for the value.
    struct Values {
        using Result = T;
        EvaluationVisitor<T>& evaluator;

        T Eval(const PExpression<T>& e) { return e->accept(evaluator); }
        T Bound(const PExpression<T>& e) { return e->accept(evaluator); }
        // A guard, told to --check where the clause is every term's.
        bool Holds(const Reference& definition, const Clause<T>& clause, int index, int, int row,
                   int col) {
            const bool held =
                numeric_interface<T>::truth(clause.parameters.guard()->accept(evaluator));
            if (clause.parameters.general() && evaluator.stack().guards)
                evaluator.stack().guards(definition, clause, index, row, col, held, evaluator);
            return held;
        }
        static T&   Value(T& value) { return value; }
        static T    Blank(Extent extent) { return T(extent); }
        static void Store(T& into, int slice, int row, int col, typename T::value_type single,
                          const T&) {
            into(slice, row, col) = single;
        }
        ReferenceStack<T>& stack() { return evaluator.stack(); }
    };

    T EvalImp(bool indexed, int index, EvaluationVisitor<T>& evaluator) const {
        Values values{evaluator};
        if (std::optional<T> cells = ByCells(indexed, index, values)) return std::move(*cells);
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
            if (Selects(clause, indexed, index, values)) {
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
                    return EvaluateGeneralClause(*general, index, values);
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
    template <typename Walk>
    typename Walk::Result EvaluateCells(Walk& walk) const {
        std::optional<Extent>                extent;
        std::optional<typename Walk::Result> whole;
        if (const Clause<T>* plain = Plain()) {
            whole  = walk.Eval(plain->expression);
            extent = walk.Value(*whole).Size();
        }
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!p.cells() || p.row_name().empty()) continue;
            const Extent size = Bounds(p, walk);
            if (extent && *extent != size) {
                throw std::runtime_error("the clauses of " + reference_name_ +
                                         " give it different sizes");
            }
            extent = size;
        }
        if (!extent) {
            throw std::runtime_error(reference_name_ + " has no size; write it as " +
                                     reference_name_ + Bounded());
        }
        typename Walk::Result matrix = whole ? std::move(*whole) : walk.Blank(*extent);
        // A clause for one cell can name a cell outside the size, which says so
        // as reading it would.
        for (const Clause<T>& clause : clauses_) {
            if (IsOneCell(clause)) Named(clause.parameters, walk.Value(matrix), reference_name_);
        }
        for (int slice = 1; slice <= Slices(*extent); ++slice) {
            for (int row = 1; row <= static_cast<int>(extent->rows); ++row) {
                for (int col = 1; col <= static_cast<int>(extent->cols); ++col) {
                    auto cell = Cell(slice, row, col, walk);
                    if (cell) walk.Store(matrix, slice, row, col, Single(*cell, walk), *cell);
                }
            }
        }
        return matrix;
    }

    // A cell is named by as many indices as reading it takes: a value written
    // whole, unlike clauses for all cells, says its rank only when evaluated.
    static void Named(const ParametersDefinition<T>& p, T& value, const std::string& name) {
        if (p.tensor() != bool(value.Size().slices)) {
            throw std::runtime_error("a clause for one cell of " + name + ", a " +
                                     value.Size().Described() + ", names " +
                                     (p.tensor() ? "no slice" : "its slice, row and column"));
        }
        (void)value(p.slice(), p.row(), p.col());
    }

    template <typename Walk>
    typename T::value_type Single(typename Walk::Result& result, Walk& walk) const {
        const T& cell = walk.Value(result);
        if (!cell.IsScalar()) {
            throw std::runtime_error("a cell of " + reference_name_ +
                                     " must be a single value, not a " + cell.Size().Described());
        }
        return cell(1, 1);
    }

    // How a clause for all cells bounds them, at the rank of the clauses given.
    std::string Bounded() const {
        const bool tensor = FirstThat([](const Clause<T>& c) { return c.parameters.tensor(); });
        return std::string("[") + (tensor ? "b<=slices, " : "") + "j<=rows, k<=cols]";
    }

    static int Slices(const Extent& extent) {
        return extent.slices ? static_cast<int>(extent.slices) : 1;
    }

    // The size a clause for all cells bounds its names to.
    template <typename Walk>
    static Extent Bounds(const ParametersDefinition<T>& p, Walk& walk) {
        return Extent{Size(walk.Bound(p.rows())), Size(walk.Bound(p.cols())),
                      p.tensor() ? Size(walk.Bound(p.slices())) : 0};
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
    template <typename Walk>
    std::optional<typename Walk::Result> Cell(int slice, int row, int col, Walk& walk) const {
        const auto matrix = [](const Clause<T>& c) { return !c.parameters.indexed(); };
        if (auto one = OneCell(slice, row, col, 0, matrix, walk)) return std::move(one->first);
        return AllCells(slice, row, col, 0, matrix, walk);
    }

    // The clause for this one cell among those that fit, if one holds, and
    // what it gives. A general clause sees the index; the names are bound on
    // trial, so that one clause's names cannot shadow a global in the next.
    template <typename Fits, typename Walk>
    std::optional<std::pair<typename Walk::Result, const Clause<T>*>> OneCell(int slice, int row,
                                                                              int col, int index,
                                                                              Fits  fits,
                                                                              Walk& walk) const {
        ReferenceStack<T>& stack = walk.stack();
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!IsOneCell(clause) || !fits(clause) || p.slice() != slice || p.row() != row ||
                p.col() != col)
                continue;
            typename ReferenceStack<T>::Trial term(stack, p.index_name());
            if (p.general()) SetIndex(p.index_name(), index, stack);
            if (p.guarded() && !walk.Holds(*this, clause, index, slice, row, col)) continue;
            return std::pair(walk.Eval(clause.expression), &clause);
        }
        return std::nullopt;
    }

    template <typename Fits, typename Walk>
    std::optional<typename Walk::Result> AllCells(int slice, int row, int col, int index, Fits fits,
                                                  Walk& walk) const {
        ReferenceStack<T>& stack = walk.stack();
        for (const bool guarded : {true, false}) {
            for (const Clause<T>& clause : clauses_) {
                const ParametersDefinition<T>& p = clause.parameters;
                if (!p.cells() || p.row_name().empty() || p.guarded() != guarded || !fits(clause))
                    continue;
                typename ReferenceStack<T>::Trial term(stack, p.index_name());
                typename ReferenceStack<T>::Trial row_name(stack, p.row_name());
                typename ReferenceStack<T>::Trial col_name(stack, p.col_name());
                typename ReferenceStack<T>::Trial slice_name(stack, p.slice_name());
                if (p.general()) SetIndex(p.index_name(), index, stack);
                if (p.tensor()) SetIndex(p.slice_name(), slice, stack);
                SetIndex(p.row_name(), row, stack);
                SetIndex(p.col_name(), col, stack);
                if (p.guarded() && !walk.Holds(*this, clause, index, slice, row, col)) continue;
                return walk.Eval(clause.expression);
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
    template <typename Walk>
    typename Walk::Result EvaluateTerm(int index, Walk& walk) const {
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
        std::optional<typename Walk::Result> whole;
        if (based) {
            if (const Clause<T>* b =
                    FirstThat([&](const Clause<T>& c) { return base(c) && !c.parameters.cells(); }))
                whole = walk.Eval(b->expression);
        } else {
            for (const Clause<T>& clause : clauses_) {
                const ParametersDefinition<T>& p = clause.parameters;
                if (!p.general() || p.cells() || !p.guarded()) continue;
                if (Selects(clause, true, index, walk)) {
                    whole = walk.Eval(clause.expression);
                    break;
                }
            }
            if (!whole) {
                if (const Clause<T>* g = FirstThat(
                        [](const Clause<T>& c) { return IsGeneral(c) && !c.parameters.cells(); }))
                    whole = EvaluateGeneralClause(*g, index, walk);
            }
        }
        const auto level = [&](const Clause<T>& c) { return based ? base(c) : general(c); };
        std::optional<Extent> extent;
        if (whole) extent = walk.Value(*whole).Size();
        for (const Clause<T>& clause : clauses_) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!p.cells() || p.row_name().empty() || !level(clause)) continue;
            typename ReferenceStack<T>::Trial term(walk.stack(), p.index_name());
            if (p.general()) SetIndex(p.index_name(), index, walk.stack());
            const Extent size = Bounds(p, walk);
            if (extent && *extent != size) {
                throw std::runtime_error("the clauses of " + reference_name_ +
                                         " give it different sizes");
            }
            extent = size;
        }
        if (!extent) {
            throw std::runtime_error(reference_name_ + " has no size; write it as " +
                                     reference_name_ + "_n" + Bounded());
        }
        typename Walk::Result term = whole ? std::move(*whole) : walk.Blank(*extent);
        const std::string     name = reference_name_ + "_" + std::to_string(index);
        for (const Clause<T>& clause : clauses_) {
            if (IsOneCell(clause) && (base(clause) || general(clause)))
                Named(clause.parameters, walk.Value(term), name);
        }
        for (int s = 1; s <= Slices(*extent); ++s) {
            for (int r = 1; r <= static_cast<int>(extent->rows); ++r) {
                for (int c = 1; c <= static_cast<int>(extent->cols); ++c) {
                    std::optional<typename Walk::Result> cell;
                    if (auto own = OneCell(s, r, c, index, base, walk)) {
                        cell = std::move(own->first);
                    } else if (based) {
                        if (auto every = OneCell(s, r, c, index, general, walk)) {
                            const std::string at = (extent->slices ? std::to_string(s) + "," : "") +
                                                   std::to_string(r) + "," + std::to_string(c);
                            throw std::runtime_error(
                                name + " and " + reference_name_ + "_" +
                                every->second->parameters.index_name() + "[" + at + "] both give " +
                                (extent->slices ? "slice " + std::to_string(s) + ", " : "") +
                                "row " + std::to_string(r) + ", column " + std::to_string(c) +
                                " of " + name + "; write " + name + "[" + at + "] to say which");
                        }
                        cell = AllCells(s, r, c, index, base, walk);
                    } else if (auto every = OneCell(s, r, c, index, general, walk)) {
                        cell = std::move(every->first);
                    } else {
                        cell = AllCells(s, r, c, index, general, walk);
                    }
                    if (cell) walk.Store(term, s, r, c, Single(*cell, walk), *cell);
                }
            }
        }
        return term;
    }

    template <typename Walk>
    typename Walk::Result EvaluateGeneralClause(const Clause<T>& general, int index,
                                                Walk& walk) const {
        SetIndex(general.parameters.index_name(), index, walk.stack());
        return walk.Eval(general.expression);
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

#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "inkamath/expression.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/expression_visitor.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

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
        const bool starts_over = !ai_parameters.guarded() && !ai_parameters.indexed();
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
        if(!ai_parameters.guarded() && !ai_parameters.indexed()) {
            clauses_.clear();
        }
        else if(!ai_parameters.guarded()) {
            // An index turns a value into a sequence, so the plain clause goes.
            std::erase_if(clauses_, IsPlain);
        }
        // Writing a clause again replaces it where it stands. Position is what
        // dispatch follows, so a clause that moved would answer differently
        // (MODERNIZATION.md, C45); a guarded clause is named by its left-hand
        // side, which is how it can be corrected at all (C46).
        for(Clause<T>& existing : clauses_) {
            const bool same = ai_parameters.guarded()
                ? existing.parameters.guarded()
                      && existing.parameters.signature() == ai_parameters.signature()
                : (IsGeneral(existing) && IsGeneral(clause))
                      || (IsBase(existing) && IsBase(clause)
                          && existing.parameters.index() == clause.parameters.index());
            if(same) {
                existing = clause;
                return;
            }
        }
        clauses_.push_back(clause);
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
        typename ParametersDefinition<T>::Arguments arguments =
                parameters.EvaluateArguments(call, caller);

        // Only a global's answer is a function of the key and the globals
        // alone (MODERNIZATION.md, phase 9); a local shares its name with the
        // global it shadows, so the stack says which this is. A limit is not
        // keyed -- the terms it walks are, through this same path.
        const bool memoisable = global && !call.limit() && (indexed || !arguments.empty());
        std::string key;
        if(memoisable) {
            key = MemoKey(indexed, index, arguments);
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
            return Converge(evaluator);
        }
        // Storing after the call returns, so that an evaluation which ran out
        // of budget is retried rather than remembered.
        const T evaluation = EvalImp(indexed, index, evaluator);
        if(memoisable) {
            stack.Memoise(key, evaluation);
        }
        return evaluation;
    }

private:
    // The bytes of the values, not their printed form, which rounds to nine
    // digits and would make two different arguments one key.
    std::string MemoKey(bool indexed, int index,
                        const typename ParametersDefinition<T>::Arguments& arguments) const {
        std::string key = reference_name_;
        if(indexed) {
            key += '_';
            key += std::to_string(index);
        }
        for(const auto& argument : arguments) {
            // The extent, not only the cells: two arguments with the same
            // values in different shapes are two arguments, and the extent is
            // also what says how many bytes of value follow (C50).
            const T& value = argument.second;
            key += '\0';
            key += argument.first;
            key += '=';
            key += value.Size().toString();
            key += ':';
            key.append(reinterpret_cast<const char*>(value.data()),
                       value.Size().count() * sizeof(*value.data()));
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
    static bool IsPlain(const Clause<T>& c)
        {return !c.parameters.guarded() && !c.parameters.indexed();}
    static bool IsBase(const Clause<T>& c)
        {return !c.parameters.guarded() && c.parameters.indexed() && !c.parameters.general();}
    static bool IsGeneral(const Clause<T>& c)
        {return !c.parameters.guarded() && c.parameters.general();}

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

    T EvaluateGeneralClause(const Clause<T>& general, int index,
                            EvaluationVisitor<T>& evaluator) const {
        SetIndex(general.parameters.index_name(), index, evaluator.stack());
        return general.expression->accept(evaluator);
    }

    // Terms until the series is within the tolerance of its limit. Since
    // phase 9 this cap is a judgement about how long to keep trying and not a
    // technical limit: 'lim' walks the terms upward, so each one finds its
    // predecessor memoised, and a hundred thousand of them cost 250ms and no
    // depth. Raising it was measured and rejected -- MODERNIZATION.md, C36.
    static constexpr size_t max_terms = 100;
    static constexpr double tolerance = 1E-10;

    // Every term goes through EvalImp, so the terms a limit walks are the
    // terms an index gives. Evaluating the general clause directly made them
    // two different sequences as soon as a guard existed (C54).
    T Converge(EvaluationVisitor<T>& evaluator) const {
        long long index = 0;
        T previous;
        // With no base clause there is no term to compare the first one
        // against. Comparing it to a default-constructed T said that any
        // sequence starting near zero had converged to it.
        const Clause<T>* highest = EndBase(false);
        bool comparable = highest != nullptr;
        if(comparable) {
            index = highest->parameters.index();
            previous = EvalImp(true, static_cast<int>(index), evaluator);
        }

        T evaluation = previous;
        decltype(numeric_interface<T>::abs(evaluation)) previous_step{};
        bool stepped = false;
        for(size_t term = 0; term < max_terms; ++term) {
            evaluation = EvalImp(true, static_cast<int>(++index), evaluator);
            if(comparable) {
                // Naming the sequence, because the reason a term cannot be
                // compared -- a matrix has no absolute value, two terms have
                // different sizes -- reads as an internal error on its own.
                const auto step = [&]() {
                    try {
                        return numeric_interface<T>::abs(evaluation-previous);
                    }
                    catch(const std::exception& reason) {
                        throw std::runtime_error(reference_name_ + " has no limit: "
                                                 + reason.what());
                    }
                }();
                // '<=' and not '!(> tolerance)': a difference that is NaN
                // answers false to both, and must count as not converged.
                if(step <= tolerance && stepped && TailUnder(step, previous_step)) {
                    return evaluation;
                }
                previous_step = step;
                stepped = true;
            }
            previous = evaluation;
            comparable = true;
        }
        throw std::runtime_error(reference_name_ + " did not converge within "
                                 + std::to_string(max_terms) + " terms (last term "
                                 + numeric_interface<T>::toString(evaluation) + ")");
    }

    // A small step is not a small remainder. If the steps shrink by a factor
    // r each term, what is left of the series is about step*r/(1-r); for
    // 1/n^2, where r approaches 1, that is 1/n -- five orders of magnitude
    // above the step that would otherwise have been called convergence.
    // One step is no evidence at all, which is why 'stepped' is required.
    template <typename S>
    static bool TailUnder(S step, S previous_step) {
        if(!(previous_step > 0)) return true;
        const S ratio = step / previous_step;
        if(!(ratio < 1)) return false;
        return step * ratio / (1 - ratio) <= tolerance;
    }

    void SetIndex(const std::string& name, int index, ReferenceStack<T>& stack) const {
        stack.BindValue(name, T(index));
    }

    std::string reference_name_;

    // The whole definition: its clauses, in the order they were written.
    std::vector<Clause<T>> clauses_;
};

#endif // HPP_INKREFERENCE

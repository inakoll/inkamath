#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "inkamath/expression.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/expression_visitor.hpp"

#include <algorithm>
#include <map>
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
    // Clauses print in the order they were written.
    size_t                  order = 0;

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

        // One definition per name. An indexed clause extends a sequence,
        // creating one if the name held a plain definition; a plain
        // definition replaces whatever was there.
        const Clause<T> clause{ai_parameters, ai_expression, written, next_order_++};
        if(ai_parameters.general()) {
            plain_ = Clause<T>();
            general_ = clause;
        }
        else if(ai_parameters.indexed()) {
            plain_ = Clause<T>();
            base_[ai_parameters.index()] = clause;
        }
        else {
            base_.clear();
            general_ = Clause<T>();
            plain_ = clause;
        }
    }

    // '?name', or '?name_0' for one clause of a sequence.
    std::string Describe(const ParametersCall<T>& call, ReferenceStack<T>& stack) const {
        int index = 0;
        if(call.TryEvalIndex(stack, index)) {
            if(plain_) {
                throw std::runtime_error(reference_name_ + " is not a sequence");
            }
            auto clause = base_.find(index);
            if(clause == base_.end()) {
                throw std::runtime_error(reference_name_ + " has no clause for index "
                                         + std::to_string(index));
            }
            return Written(clause->second);
        }

        std::vector<const Clause<T>*> clauses;
        if(plain_) clauses.push_back(&plain_);
        for(const auto& clause : base_) clauses.push_back(&clause.second);
        if(general_) clauses.push_back(&general_);
        std::sort(clauses.begin(), clauses.end(),
                  [](const Clause<T>* a, const Clause<T>* b) {return a->order < b->order;});

        std::string description;
        for(const Clause<T>* clause : clauses) {
            if(!description.empty()) description += '\n';
            description += Written(*clause);
        }
        return description;
    }

    T Eval(const ParametersCall<T>& call, ReferenceStack<T>& stack) const {
        const ParametersDefinition<T>& parameters = CallParameters();
        parameters.CheckArity(reference_name_, call);

        // The index and the arguments belong to the caller, so they are
        // evaluated before the callee's scope exists.
        EvaluationVisitor<T> caller(stack);
        int index = 0;
        const bool indexed = call.TryEvalIndex(stack, index);
        typename ParametersDefinition<T>::Arguments arguments =
                parameters.EvaluateArguments(call, caller);

        typename ReferenceStack<T>::Frame frame(stack);
        ParametersDefinition<T>::Bind(arguments, stack);
        EvaluationVisitor<T> evaluator(stack);
        if(call.limit()) {
            if(!general_) {
                throw std::runtime_error(reference_name_ + " has no general clause, so it has no limit");
            }
            return Converge(evaluator);
        }
        return EvalImp(indexed, index, evaluator);
    }

private:
    // The clauses of one name share their parameter list; any of them answers
    // for the whole definition.
    const ParametersDefinition<T>& CallParameters() const {
        if(general_) return general_.parameters;
        if(!base_.empty()) return base_.begin()->second.parameters;
        return plain_.parameters;
    }

    // A clause bound from inside an expression has no written form to quote.
    std::string Written(const Clause<T>& clause) const {
        return clause.written.empty() ? reference_name_ : clause.written;
    }

    T EvalImp(bool indexed, int index, EvaluationVisitor<T>& evaluator) const {
        if(plain_) {
            // An index on something that is not a sequence used to be dropped
            // without a word, which is the last of C13's silent answers.
            if(indexed) {
                throw std::runtime_error(reference_name_ + " is not a sequence");
            }
            return plain_.expression->accept(evaluator);
        }
        if(indexed) {
            auto clause = base_.find(index);
            if(clause != base_.end()) {
                return clause->second.expression->accept(evaluator);
            }
            // A general clause reaches down only as far as the lowest base
            // clause; below that the sequence is simply not defined. With no
            // base clause at all it applies everywhere, which is right for a
            // closed form and divergent for a recurrence -- the budget says so.
            if(general_ && (base_.empty() || index >= base_.begin()->first)) {
                return EvaluateGeneralClause(index, evaluator);
            }
            throw std::runtime_error(reference_name_ + " has no clause for index "
                                     + std::to_string(index));
        }
        // Nothing sensible to invent: a sequence has no value under its bare
        // name. A limit is something the user asks for, not something a
        // lookup does on its way past.
        const std::string example = base_.empty() ? "0" : std::to_string(base_.begin()->first);
        throw std::runtime_error(reference_name_ + " is a sequence; index it (" + reference_name_
                                 + "_" + example + ")"
                                 + (general_
                                    ? " or take its limit (lim " + reference_name_ + ")"
                                    : ""));
    }

    T EvaluateGeneralClause(long long index, EvaluationVisitor<T>& evaluator) const {
        SetIndex(index, evaluator.stack());
        return general_.expression->accept(evaluator);
    }

    // Terms until two in a row agree to within the tolerance. The budget
    // cannot go much higher: each term of a recurrence nests one more
    // reference, and ReferenceStack::max_depth is 256.
    static constexpr size_t max_terms = 100;
    static constexpr double tolerance = 1E-10;

    T Converge(EvaluationVisitor<T>& evaluator) const {
        long long index = 0;
        T previous;
        if(!base_.empty()) {
            index = base_.rbegin()->first;
            previous = base_.rbegin()->second.expression->accept(evaluator);
        }

        T evaluation = previous;
        using difference_type = decltype(numeric_interface<T>::abs(std::declval<T>()));
        difference_type diff = numeric_interface<difference_type>::one();
        for(size_t term = 0; term < max_terms; ++term) {
            evaluation = EvaluateGeneralClause(++index, evaluator);
            diff = numeric_interface<T>::abs(evaluation-previous);
            previous = evaluation;
            if(!(diff > tolerance)) {
                return evaluation;
            }
        }
        throw std::runtime_error(reference_name_ + " did not converge within "
                                 + std::to_string(max_terms) + " terms (last term "
                                 + numeric_interface<T>::toString(evaluation) + ")");
    }

    void SetIndex(long long index, ReferenceStack<T>& stack) const {
        stack.Set(general_.parameters.index_name(), ParametersDefinition<T>(),
                  PExpression<T>(new ValExpression<T>(T(index))));
    }

    // Signed: an index may be negative, and this map is read in order
    // (rbegin) to pick the highest known term.
    typedef std::map<long long, Clause<T>> BaseClauses;

    std::string reference_name_;
    size_t      next_order_ = 0;

    // Exactly one of these is populated: a plain definition, or a sequence
    // made of base clauses and at most one general clause.
    Clause<T>                   plain_;

    BaseClauses                 base_;
    Clause<T>                   general_;
};

#endif // HPP_INKREFERENCE

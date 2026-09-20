#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "inkamath/expression.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/expression_visitor.hpp"

#include <map>
#include <tuple>
#include <stdexcept>

template <typename T>
using ExpressionDefinition =
        std::tuple<
            ParametersDefinition<T>,
            PExpression<T>
        >;

template <typename T>
class ReferenceStack;

template <typename T>
class EvaluationVisitor;

template <typename T>
class Reference {
public:

    void add_expression(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression) {
        if(reference_name_.empty()) {
            reference_name_ = ai_reference_name;
        }
        else if(reference_name_ != ai_reference_name) {
            throw std::runtime_error(std::string("Interpreter internal error : invalid reference names ") + ai_reference_name + " and " + reference_name_);
        }

        // One definition per name. An indexed clause extends a sequence,
        // creating one if the name held a plain definition; a plain
        // definition replaces whatever was there.
        if(ai_parameters.general()) {
            plain_ = ExpressionDefinition<T>();
            general_ = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
        else if(ai_parameters.indexed()) {
            plain_ = ExpressionDefinition<T>();
            base_[ai_parameters.index()] = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
        else {
            base_.clear();
            general_ = ExpressionDefinition<T>();
            plain_ = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
    }

    T Eval(const ParametersCall<T>& call, ReferenceStack<T>& stack) {
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
            if(!std::get<1>(general_)) {
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
        if(std::get<1>(general_)) return std::get<0>(general_);
        if(!base_.empty()) return std::get<0>(base_.begin()->second);
        return std::get<0>(plain_);
    }

    T EvalImp(bool indexed, int index, EvaluationVisitor<T>& evaluator) {
        if(std::get<1>(plain_)) {
            return std::get<1>(plain_)->accept(evaluator);
        }
        if(indexed) {
            auto clause = base_.find(index);
            if(clause != base_.end()) {
                return std::get<1>(clause->second)->accept(evaluator);
            }
            // A general clause reaches down only as far as the lowest base
            // clause; below that the sequence is simply not defined. With no
            // base clause at all it applies everywhere, which is right for a
            // closed form and divergent for a recurrence -- the budget says so.
            if(std::get<1>(general_) && (base_.empty() || index >= base_.begin()->first)) {
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
                                 + (std::get<1>(general_)
                                    ? " or take its limit (lim " + reference_name_ + ")"
                                    : ""));
    }

    T EvaluateGeneralClause(long long index, EvaluationVisitor<T>& evaluator) {
        SetIndex(index, evaluator.stack());
        return std::get<1>(general_)->accept(evaluator);
    }

    // Terms until two in a row agree to within the tolerance. The budget
    // cannot go much higher: each term of a recurrence nests one more
    // reference, and ReferenceStack::max_depth is 256.
    static constexpr size_t max_terms = 100;
    static constexpr double tolerance = 1E-10;

    T Converge(EvaluationVisitor<T>& evaluator) {
        long long index = 0;
        T previous;
        if(!base_.empty()) {
            index = base_.rbegin()->first;
            previous = std::get<1>(base_.rbegin()->second)->accept(evaluator);
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

    void SetIndex(long long index, ReferenceStack<T>& stack) {
        stack.Set(std::get<0>(general_).index_name(), ParametersDefinition<T>(),
                  PExpression<T>(new ValExpression<T>(T(index))));
    }

    // Signed: an index may be negative, and this map is read in order
    // (rbegin) to pick the highest known term.
    typedef std::map<long long, ExpressionDefinition<T>> BaseClauses;

    std::string reference_name_;
	
    // Exactly one of these is populated: a plain definition, or a sequence
    // made of base clauses and at most one general clause.
    ExpressionDefinition<T>     plain_;

    BaseClauses                 base_;
    ExpressionDefinition<T>     general_;
	
};

#endif // HPP_INKREFERENCE

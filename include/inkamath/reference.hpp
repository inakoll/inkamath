#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "inkamath/expression.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/mapstack.hpp"
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

    T Eval( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
        CallParameters().CheckArity(reference_name_, ai_parameters);
        return this->EvalImp(ai_parameters, stack);
    }

private:
    // The clauses of one name share their parameter list; any of them answers
    // for the whole definition.
    const ParametersDefinition<T>& CallParameters() const {
        if(std::get<1>(general_)) return std::get<0>(general_);
        if(!base_.empty()) return std::get<0>(base_.begin()->second);
        return std::get<0>(plain_);
    }

    T EvalImp( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
        EvaluationVisitor<T> evaluator(stack);
        T result;

        if(std::get<1>(plain_)) {
            TryEvaluatePlainExpression(ai_parameters, evaluator, result);
        }
        else if(!TryEvaluateBaseClause(ai_parameters, evaluator, result)
                && !TryEvaluateGeneralClause(ai_parameters, evaluator, result)) {
            // Nothing sensible to invent: a sequence has no value under its
            // bare name, and no value at an index no clause covers.
            int index = 0;
            if(ai_parameters.TryEvalIndex(stack, index)) {
                throw std::runtime_error(reference_name_ + " has no clause for index "
                                         + std::to_string(index));
            }
            throw std::runtime_error(reference_name_ + " is a sequence; index it, as in "
                                     + reference_name_ + "_"
                                     + std::to_string(base_.begin()->first));
        }

        return result;
    }

    bool TryEvaluateBaseClause(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ReferenceStack<T>& stack = evaluator.stack();
        int index_value;
        if(ai_parameters.TryEvalIndex(stack, index_value)) {
            // Evaluation to an index is requested
            auto ind_definition = base_.find(index_value);
            if(ind_definition != base_.end()) {
                ParametersDefinition<T> ind_params_def;
                PExpression<T> ind_expr_def;
                std::tie(ind_params_def, ind_expr_def) = ind_definition->second;

                ind_params_def.SetCallParameters(ai_parameters, evaluator);
                if(ind_expr_def) {
                    evaluation = ind_expr_def->accept(evaluator);
                    succeed = true;
                }
            }
       }
       return succeed;
    }

    bool TryEvaluateGeneralClause(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ReferenceStack<T>& stack = evaluator.stack();
        PExpression<T> gen_expr_def;
        ParametersDefinition<T> gen_params_def;
        std::tie(gen_params_def, gen_expr_def) = general_;
        if(gen_expr_def) {
            int requested = 0;
            // A general clause reaches down only as far as the lowest base
            // clause; below that the sequence is simply not defined. With no
            // base clause at all it applies everywhere, which is right for a
            // closed form and divergent for a recurrence -- the budget says so.
            if(ai_parameters.TryEvalIndex(stack, requested)
               && (base_.empty() || requested >= base_.begin()->first)) {
                // The index the caller asked for, not the offset in its
                // written form: 's_(n-1)' is index n-1, not index -1.
                const long long index = requested;
                gen_params_def.SetCallParameters(ai_parameters, evaluator);
                typename ReferenceStack<T>::Guard guard(stack);
                stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(index))));
                evaluation = gen_expr_def->accept(evaluator);
                succeed = true;
            }
            else {
                // Bind the arguments once, in the caller's scope. Binding them
                // again after the first clause has run would evaluate 'h(i*x)'
                // against the 'x' that binding had just introduced.
                typename ReferenceStack<T>::Guard guard(stack);
                gen_params_def.SetCallParameters(ai_parameters, evaluator);

                long long start_index = 0;
                T start_evaluation;
                if(!base_.empty()) {
                    start_index = base_.rbegin()->first;
                    start_evaluation = std::get<1>(base_.rbegin()->second)->accept(evaluator);
                }

                evaluation = start_evaluation;
                using difference_type = decltype(numeric_interface<T>::abs(std::declval<T>()));
                difference_type diff = numeric_interface<difference_type>::one();
                size_t iter_count = 0;
                while(diff > 1E-10 && iter_count < 30) {
                    ++start_index;
                    stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(start_index))));
                    evaluation = gen_expr_def->accept(evaluator);
                    diff = numeric_interface<T>::abs(evaluation-start_evaluation);
                    start_evaluation = evaluation;
                    ++iter_count;
                }
                succeed = true;
            }
        }
        return succeed;
    }

    bool TryEvaluatePlainExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ParametersDefinition<T> single_params_def;
        PExpression<T> plain_def;
        std::tie(single_params_def, plain_def) = plain_;
        if(plain_def) {

            single_params_def.SetCallParameters(ai_parameters, evaluator);
            evaluation = plain_def->accept(evaluator);
            succeed = true;
        }
        return succeed;
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

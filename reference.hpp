#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "expression.hpp"
#include "parameters.hpp"

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;


#include <map>
#include <tuple>
#include <stdexcept>

#include "expression_visitor.hpp"

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

        if(ai_parameters.a() != 0) {
            general_expr_ = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
        else if(ai_parameters.indexed()) {
            indexed_expr_[ai_parameters.b()] = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
        else {
            single_expr_ = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }

        ParametersDefinition<T> gen_params_def;
        gen_params_def = std::get<0>(general_expr_);
        if( gen_params_def.parameters_dict() != ai_parameters.parameters_dict()
                || gen_params_def.parameters_names() != ai_parameters.parameters_names()) {
            // memoized index is invalidated when adding a new expression
            // with different parameters
            memoized_index_.clear();
        }
    }
	
    T Eval( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
        EvaluationVisitor<T> evaluator(stack);
        T result;

        if(TryEvaluateIndexedExpression(ai_parameters, evaluator, result)) {
            return result;
        }
        else if(TryEvaluateGeneralExpression(ai_parameters, evaluator, result)) {
            return result;
        }
        else if(TryEvaluateSimpleExpression(ai_parameters, evaluator, result)) {
            return result;
        }

        return result;
    }

    T SafeRecursiveEval( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
        // if functionnal parameters are identical to the general expr
        // return the memoized value at the evaluated index of this potentially recursive function
        // or return {}
        T evaluation = {};
        EvaluationVisitor<T> evaluator(stack);

        // Important note: Indexed expression shall not be recursive! ==> stack overflow
        if(!TryEvaluateIndexedExpression(ai_parameters, evaluator, evaluation)) {
            PExpression<T> gen_expr_def;
            ParametersDefinition<T> gen_params_def;
            std::tie(gen_params_def, gen_expr_def) = general_expr_;
            if(     gen_params_def.a() == ai_parameters.a()
                    &&  gen_params_def.parameters_dict().empty()
                    &&  ai_parameters.parameters_dict().empty()
                    &&  gen_params_def.parameters_names() == ai_parameters.parameters_names()) {
                int index_value;
                if(ai_parameters.TryEvalIndex(stack, index_value)) {
                    auto it = memoized_index_.find(index_value);
                    if(it != memoized_index_.end()) {
                        evaluation = it->second;
                    }
                    else if(index_value < 0) {
                        evaluation = {};
                    }
                    else if(!TryEvaluateIndexedExpression(ai_parameters, evaluator, evaluation)) {
                        ParametersCall<T> fwd_parameter(0,index_value,true);
                        TryEvaluateGeneralExpression(fwd_parameter, evaluator, evaluation);
                    }
                }
            }
        }
        return evaluation;
    }
	
private:
    bool TryEvaluateIndexedExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ReferenceStack<T>& stack = evaluator.stack();
        int index_value;
        if(ai_parameters.TryEvalIndex(stack, index_value)) {
            // Evaluation to an index is requested
            auto ind_definition = indexed_expr_.find(index_value);
            if(ind_definition != indexed_expr_.end()) {
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

    bool TryEvaluateGeneralExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ReferenceStack<T>& stack = evaluator.stack();
        PExpression<T> gen_expr_def;
        ParametersDefinition<T> gen_params_def;
        std::tie(gen_params_def, gen_expr_def) = general_expr_;
        if(gen_expr_def) {
            if(ai_parameters.indexed()) {
                size_t index = ai_parameters.b() - gen_params_def.b();
                if(ai_parameters.a() != 0) {
                    index *= ai_parameters.a();
                }
                if(gen_params_def.a() != 0) {
                    index /= gen_params_def.a();
                }
                gen_params_def.SetCallParameters(ai_parameters, evaluator);
                typename ReferenceStack<T>::Guard guard(stack);
                stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(index))));
                evaluation = gen_expr_def->accept(evaluator);
                succeed = true;
            }
            else {
                size_t start_index = 0;
                T start_evaluation;
                if(!memoized_index_.empty() || !indexed_expr_.empty()) {
                    if(!memoized_index_.empty()) {
                        start_index = memoized_index_.rbegin()->first;
                    }
                    if(!indexed_expr_.empty()) {
                        start_index = std::max(start_index, indexed_expr_.rbegin()->first);
                    }
                    if(start_index == memoized_index_.rbegin()->first) {
                        start_evaluation = memoized_index_.rbegin()->second;
                    }
                    else {

                        ParametersDefinition<T> ind_params_def;
                        PExpression<T> ind_expr_def;
                        std::tie(ind_params_def, ind_expr_def) = indexed_expr_.rbegin()->second;

                        ind_params_def.SetCallParameters(ai_parameters, evaluator);
                        start_evaluation = ind_expr_def->accept(evaluator);
                    }
                }
                gen_params_def.SetCallParameters(ai_parameters, evaluator);
                stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(start_index))));
                evaluation = start_evaluation;
                using difference_type = decltype(numeric_interface<T>::abs(std::declval<T>()));
                difference_type diff = numeric_interface<difference_type>::one();
                while(diff > 1E-10) {
                    start_index += gen_params_def.a();
                    stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(start_index))));
                    evaluation = gen_expr_def->accept(evaluator);
                    diff = numeric_interface<T>::abs(evaluation-start_evaluation);
                    start_evaluation = evaluation;
                }
                memoized_index_[start_index] = evaluation;
                succeed = true;
            }
        }
        return succeed;
    }

    bool TryEvaluateSimpleExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ParametersDefinition<T> single_params_def;
        PExpression<T> single_expr_def;
        std::tie(single_params_def, single_expr_def) = single_expr_;
        if(single_expr_def) {

            single_params_def.SetCallParameters(ai_parameters, evaluator);
            evaluation = single_expr_def->accept(evaluator);
            succeed = true;
        }
        return succeed;
    }

    typedef std::map<size_t, ExpressionDefinition<T>> Indexed_expr;
    typedef std::map<size_t, T> Indexed_values;

    std::string reference_name_;
	
	/* Associated expression in simple assignation */
    ExpressionDefinition<T> 	single_expr_;
	
	/* Associated expressions for indexed expression */
    Indexed_expr                indexed_expr_;
    Indexed_values              memoized_index_;
    ExpressionDefinition<T> 	general_expr_;
	
};

#endif // HPP_INKREFERENCE

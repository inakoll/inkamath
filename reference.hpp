#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "expression.hpp"

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
        else if(ai_parameters.b() != 0) {
            indexed_expr_[ai_parameters.b()] = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
        else {
            single_expr_ = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
    }
	
    T Eval( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
        EvaluationVisitor<T> evaluator(stack);
        T result;

        if(TryEvaluateIndexedExpression(ai_parameters, evaluator, stack, result)) {
            return result;
        }
        else if(TryEvaluateGeneralExpression(ai_parameters, evaluator, stack, result)) {
            return result;
        }
        else if(TryEvaluateSimpleExpression(ai_parameters, evaluator, stack, result)) {
            return result;
        }


        throw std::runtime_error(reference_name_ + " not defined (internal interpreter error");

    }
	
private:
    bool TryEvaluateIndexedExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, ReferenceStack<T>& stack, T& evaluation) {
        bool succeed = false;

        int index_value;
        if(ai_parameters.TryEvalIndex(stack, index_value)) {
            // Evaluation to an index is requested
            auto ind_definition = indexed_expr_.find(index_value);
            if(ind_definition != indexed_expr_.end()) {
                ParametersDefinition<T> ind_params_def;
                PExpression<T> ind_expr_def;
                std::tie(ind_params_def, ind_expr_def) = ind_definition->second;

                ind_params_def.SetCallParameters(ai_parameters, stack);
                if(ind_expr_def) {
                    evaluation = ind_expr_def->accept(evaluator);
                    succeed = true;
                }
            }
       }
       return succeed;
    }

    bool TryEvaluateGeneralExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, ReferenceStack<T>& stack, T& evaluation) {
        bool succeed = false;
        PExpression<T> gen_expr_def;
        ParametersDefinition<T> gen_params_def;
        std::tie(gen_params_def, gen_expr_def) = general_expr_;
        if(gen_expr_def && (ai_parameters.a() != 0 || ai_parameters.b() != 0)) {
            size_t index = ai_parameters.b() - gen_params_def.b();
            if(ai_parameters.a() != 0) {
                index *= ai_parameters.a();
            }
            if(gen_params_def.a() != 0) {
                index /= gen_params_def.a();
            }
            gen_params_def.SetCallParameters(ai_parameters, stack);
            stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(index))));
            evaluation = gen_expr_def->accept(evaluator);
            succeed = true;
        }
        return succeed;
    }

    bool TryEvaluateSimpleExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, ReferenceStack<T>& stack, T& evaluation) {
        bool succeed = false;
        ParametersDefinition<T> single_params_def;
        PExpression<T> single_expr_def;
        std::tie(single_params_def, single_expr_def) = single_expr_;
        if(single_expr_def) {

            single_params_def.SetCallParameters(ai_parameters, stack);
            evaluation = single_expr_def->accept(evaluator);
            succeed = true;
        }
        return succeed;
    }

    typedef std::map<size_t, ExpressionDefinition<T>> Indexed_expr;

    std::string reference_name_;
	
	/* Associated expression in simple assignation */
    ExpressionDefinition<T> 	single_expr_;
	
	/* Associated expressions for indexed expression */
    Indexed_expr                indexed_expr_;
    ExpressionDefinition<T> 	general_expr_;
	
};

#endif // HPP_INKREFERENCE

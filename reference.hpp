#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "expression.hpp"

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;


#include <map>
#include <tuple>
#include <stdexcept>


#include "expression_visitor.hpp"
//#include "reference_stack.hpp"

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

        ParametersDefinition<T> gen_params_def;
        PExpression<T> gen_expr_def;
        std::tie(gen_params_def, gen_expr_def) = general_expr_;

        if(ai_parameters.a() == 0 && ai_parameters.b() != 0) {
            // Evaluation to an index is requested
            auto ind_definition = indexed_expr_.find(ai_parameters.b());
            if(ind_definition != indexed_expr_.end()) {
                ParametersDefinition<T> ind_params_def;
                PExpression<T> ind_expr_def;
                std::tie(ind_params_def, ind_expr_def) = ind_definition->second;
                size_t index_value = (ai_parameters.b() - ind_params_def.b()) / ind_params_def.a();
                if((ai_parameters.b() - ind_params_def.b()) % ind_params_def.a() != 0) {
                    throw std::runtime_error(std::string("Invalid index value for '") + reference_name_ + "'.");
                }
                else {
                    index_value;
                    // TODO : Eval index_expr_[b]
                    // 1) parameters definition dictionnary
                    // 2) parameters call expression in parameters definition names
                    // 3) parameters call dictionnary
                    // 4) associated expression
                }
            }

        }
        else {
            // Evaluation not at an index
            return EvaluationVisitor<T>(gen_expr_def, stack).value();
        }
        // TODO
        return T();

    }
	
	
	
private:
    /* Return the simple assigned expression if it makes sense */
    ExpressionDefinition<T> GetExpr() {
        if(std::get<1>(single_expr_).get() == nullptr)
            throw std::runtime_error("This reference cannot be evaluated in this context.");
        return single_expr_;
    }

    /* Return the indexed expression associated to i if it makes sense */
    ExpressionDefinition<T> GetExpr(size_t i) {
        if(indexed_expr_.count(i) != 0) {
            // If the reference is singulary defined for i then return this expression
            return indexed_expr_[i];
        }
        else if(std::get<1>(general_expr_).get() != nullptr) {
            // Or try to get the expression from the general definition
            return GetExprFromGeneralExpr(i);
        }
        // Else try to return the simple assigned expression
        return this->GetExpr();
    }




    typedef std::map<size_t, ExpressionDefinition<T>> Indexed_expr;

    std::string reference_name_;
	
	/* Associated expression in simple assignation */
    ExpressionDefinition<T> 	single_expr_;
	
	/* Associated expressions for indexed expression */
    Indexed_expr                indexed_expr_;
    ExpressionDefinition<T> 	general_expr_;
	
	/* The weak indexed expression are value expression created to resolve the 
	   potential recursion of general_expr_ */
	Indexed_expr 	weak_indexed_expr_;

	/* Evaluate the general expression and manage potential recursion */
	PExpression<T> GetExprFromGeneralExpr(size_t i) {
		// TODO: implement this.
		return PExpression<T>();
	}	
};

#endif // HPP_INKREFERENCE

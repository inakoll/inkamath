#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "expression.hpp"
#define PExpression std::shared_ptr<Expression<T> >

#include <map>

template <typename T>
class Reference {
	
	/* Return the simple assigned expression if it makes sense */
	PExpression GetExpr() {
		if(single_expr_.get() == nullptr)
			throw std::runtime_exception("This reference cannot be evaluated in this context.");
		return single_expr_;			
	}
	
	/* Return the indexed expression associated to i if it makes sense */
	PExpression GetExpr(size_t i) {
		if(indexed_expr_.count(i) != 0) {
			// If the reference is singulary defined for i then return this expression
			return indexed_expr_[i];
		}
		else if(general_expr_.get() != nullptr) {
			// Or try to get the expression from the general definition
			return GetExprFromGeneralExpr(i);
		}
		// Else try to return the simple assigned expression
		return this->GetExpr();
	}
	
	
	
private:
	typedef std::map<size_t, PExpression> Indexed_expr;
	
	/* Associated expression in simple assignation */
	PExpression 	single_expr_;
	
	/* Associated expressions for indexed expression */
	Indexed_expr 	indexed_expr_;
	PExpression 	general_expr_;
	
	/* The weak indexed expression are value expression created to resolve the 
	   potential recursion of general_expr_ */
	Indexed_expr 	weak_indexed_expr_;

	/* Evaluate the general expression and manage potential recursion */
	PExpression GetExprFromGeneralExpr(size_t i) {
		// TODO: implement this.
		return PExpression();
	}	
};

#endif // HPP_INKREFERENCE
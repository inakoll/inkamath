#ifndef H_EXPR_VISITOR
#define H_EXPR_VISITOR

#include <memory>

template <typename T>
class Expression;

#define PExpression std::shared_ptr<Expression<T> >

template <typename T>
class EqualExpression;

template <typename T>
class AddExpression;

template <typename T>
class NegExpression;

template <typename T>
class MultExpression;

template <typename T>
class DivExpression;

template <typename T>
class PowExpression;

template <typename T>
class FactExpression;

template <typename T>
class ValExpression;

template <typename T>
class MatExpression;

template <typename T>
class ParametersDefExpression;

template <typename T>
class RefExpression;

template <typename T>
class FuncExpression;

template <typename T>
class ExprVisitor {
public:
	virtual PExpression visit(EqualExpression<T>* expr) = 0;
	virtual PExpression visit(AddExpression<T>* expr) = 0;
	virtual PExpression visit(NegExpression<T>* expr) = 0;
	virtual PExpression visit(MultExpression<T>* expr) = 0;
	virtual PExpression visit(DivExpression<T>* expr) = 0;
	virtual PExpression visit(PowExpression<T>* expr) = 0;
	virtual PExpression visit(FactExpression<T>* expr) = 0;
	virtual PExpression visit(ValExpression<T>* expr) = 0;
	virtual PExpression visit(MatExpression<T>* expr) = 0;
	virtual PExpression visit(ParametersDefExpression<T>* expr) = 0;
	virtual PExpression visit(RefExpression<T>* expr) = 0;
	virtual PExpression visit(FuncExpression<T>* expr) = 0;
};

#include <vector>
#include <string>
#include <stdexcept>
#include "expression_dict.hpp"
#include "numeric_interface.hpp"

template <typename T>
class ParametersVisitor : public ExprVisitor<T> {
public:

	virtual PExpression visit(MatExpression<T>* expr) {
		if(visitor_depth == 0) {
			for(size_t i = 0; i < expr->VirtualSize(); ++i) {
				expr->GetExpr(i)->accept(this);
			}
		}
		++visitor_depth;
		return PExpression();
	}
	
	virtual PExpression visit(EqualExpression<T>* expr) {
		kewword_params_begin = true;
		this->parameters_dict[expr->m_e1->Name()] = expr->m_e2;
		++visitor_depth;
		return PExpression();
	}
	
	virtual PExpression visit(RefExpression<T>* expr) {
		if(!kewword_params_begin) {
			this->parameters_names.push_back(expr->Name());
		}
		else {
			throw(std::runtime_error("Invalid parameters. Non-keyword argument found after a keyword argument."));
		}
		++visitor_depth;
		return PExpression();
	}
	
	PExpression visit_others_expr_imp(Expression<T>* expr) {
		if(!kewword_params_begin) {
			this->parameters_expr.push_back(expr);
		}
		else {
			throw(std::runtime_error("Invalid parameters. Non-keyword argument found after a keyword argument."));
		}
		++visitor_depth;
		return PExpression();
	}
	
	virtual PExpression visit(FuncExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}
	
	virtual PExpression visit(AddExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}
	
	virtual PExpression visit(NegExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression visit(MultExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression visit(DivExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression visit(PowExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression visit(FactExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression visit(ValExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression visit(ParametersDefExpression<T>* expr) {
		return visit_others_expr_imp(expr);
		// ParametersDefExpression inside another one ?
		// This shouldn't happen and would be a bug.
		// Since we have to handle this kind of expression here in this visitor :
		// TODO throw an interpreter internal error here.
	}


	size_t visitor_depth = 0;
	bool kewword_params_begin = false;
	std::vector<std::string> parameters_names;
	std::vector<PExpression> parameters_expr;
	ExprDict parameters_dict;
	
};

template <typename T>
class SubVisitor : public ExprVisitor<T> {
public:
	virtual PExpression visit(RefExpression<T>* expr) {
		this->index_name = expr->Name();
		this->a = numeric_interface<T>::one();
		return PExpression();
	}
	
	virtual PExpression visit(ValExpression<T>* expr) {
		b = expr->Eval();
		return PExpression();
	}

	virtual PExpression visit(AddExpression<T>* expr) {
		SubVisitor l,r;
		expr->m_e1->accept(&l);
		expr->m_e2->accept(&r);
		if(l.index_name != "" && r.index_name != "" && l.index_name != r.index_name) {
			a = 0;
			b = 0;
			index_name = "";
			throw std::runtime_error("Multiple index name found inside a sub-expression.");
		}
		a = l.a + r.a;
		b = l.b + r.b;
		this->index_name = l.index_name;
		return PExpression();
	}
	
	virtual PExpression visit(NegExpression<T>* expr) {
		SubVisitor l;
		expr->m_e->accept(&l);
		a = -l.a;
		b = -l.b;
		this->index_name = l.index_name;
		return PExpression();
	}

	virtual PExpression visit(MultExpression<T>* expr) {
		SubVisitor l,r;
		expr->m_e1->accept(&l);
		expr->m_e2->accept(&r);
		if(l.a == 0) {
			this->index_name = r.index_name;
			a = l.b * r.a;
		}
		else if(r.a == 0) {
			this->index_name = l.index_name;
			a = l.a * r.b;
		}
		else {
			a = 0;
			b = 0;
			index_name = "";
			throw std::runtime_error("Unexpected second degree polynom inside a sub expression.");
		}
		b = l.b * r.b;
		return PExpression();
	}


	virtual PExpression visit(ParametersDefExpression<T>* expr) {
		// ParametersDefExpression inside a SubExpr ?
		// This shouldn't happen and would be a bug.
		// Since we have to handle this kind of expression here in this visitor :
		// TODO throw an interpreter internal error here.
	}
	
	PExpression visit_unexpected_expression() {
		a = 0;
		b = 0;
		index_name = "";
		throw std::runtime_error("Unexpected expression inside an sub expression.");
		return PExpression();
	}
	
	virtual PExpression visit(MatExpression<T>* expr) {
		return visit_unexpected_expression();
	}
	
	virtual PExpression visit(EqualExpression<T>* expr) {
		return visit_unexpected_expression();
	}
	
	virtual PExpression visit(FuncExpression<T>* expr) {
		return visit_unexpected_expression();
	}
	
	virtual PExpression visit(DivExpression<T>* expr) {
		return visit_unexpected_expression();
	}

	virtual PExpression visit(PowExpression<T>* expr) {
		return visit_unexpected_expression();
	}

	virtual PExpression visit(FactExpression<T>* expr) {
		return visit_unexpected_expression();
	}

private:
	std::string index_name;
	T a;
	T b;
	
};

#undef PExpression

#endif // H_EXPR_VISITOR

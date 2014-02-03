#ifndef H_EXPR_VISITOR
#define H_EXPR_VISITOR

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include "expression_dict.hpp"
#include "numeric_interface.hpp"

template <typename T>
class Expression;

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;

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
class RefExpression;

template <typename T>
class FuncExpression;

template <typename T>
class EmptyExpression;

template <typename T>
class RecursivePlaceholderExpression;

template <typename T>
class ParametersCall;

template <typename T>
class ExprVisitor {
public:
    virtual PExpression<T> visit(EmptyExpression<T>* expr) = 0;
    virtual PExpression<T> visit(EqualExpression<T>* expr) = 0;
    virtual PExpression<T> visit(AddExpression<T>* expr) = 0;
    virtual PExpression<T> visit(NegExpression<T>* expr) = 0;
    virtual PExpression<T> visit(MultExpression<T>* expr) = 0;
    virtual PExpression<T> visit(DivExpression<T>* expr) = 0;
    virtual PExpression<T> visit(PowExpression<T>* expr) = 0;
    virtual PExpression<T> visit(FactExpression<T>* expr) = 0;
    virtual PExpression<T> visit(ValExpression<T>* expr) = 0;
    virtual PExpression<T> visit(MatExpression<T>* expr) = 0;
    virtual PExpression<T> visit(RefExpression<T>* expr) = 0;
    virtual PExpression<T> visit(FuncExpression<T>* expr) = 0;
};



template <typename T>
class ParametersVisitor : public ExprVisitor<T> {
public:

	virtual PExpression<T> visit(MatExpression<T>* expr) {
		if(visitor_depth == 0) {
			for(size_t i = 0; i < expr->VirtualSize(); ++i) {
                expr->GetExpr(i)->accept(*this);
			}
		}
		++visitor_depth;
		return PExpression<T>();
	}
	
	virtual PExpression<T> visit(EqualExpression<T>* expr) {
		kewword_params_begin = true;
		this->parameters_dict[expr->m_e1->Name()] = expr->m_e2;
		++visitor_depth;
		return PExpression<T>();
	}
	
	virtual PExpression<T> visit(RefExpression<T>* expr) {
		if(!kewword_params_begin) {
			this->parameters_names.push_back(expr->Name());
		}
		else {
			throw(std::runtime_error("Invalid parameters. Non-keyword argument found after a keyword argument."));
		}
		++visitor_depth;
		return PExpression<T>();
	}
	
	PExpression<T> visit_others_expr_imp(Expression<T>* expr) {
		if(!kewword_params_begin) {
            this->parameters_expr.push_back(expr->self());
		}
		else {
			throw(std::runtime_error("Invalid parameters. Non-keyword argument found after a keyword argument."));
		}
		++visitor_depth;
		return PExpression<T>();
	}

    virtual PExpression<T> visit(EmptyExpression<T>* expr) {
        return visit_others_expr_imp(expr);
    }
	
	virtual PExpression<T> visit(FuncExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}
	
	virtual PExpression<T> visit(AddExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}
	
	virtual PExpression<T> visit(NegExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression<T> visit(MultExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression<T> visit(DivExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression<T> visit(PowExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression<T> visit(FactExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

	virtual PExpression<T> visit(ValExpression<T>* expr) {
		return visit_others_expr_imp(expr);
	}

    std::vector<std::string> get_parameters_names() {
        return parameters_names;
    }

    std::vector<PExpression<T>> get_parameters_expr() {
        return parameters_expr;
    }

    ExprDict<T> get_parameters_dict() {
        return parameters_dict;
    }

private:
	size_t visitor_depth = 0;
	bool kewword_params_begin = false;
	std::vector<std::string> parameters_names;
	std::vector<PExpression<T>> parameters_expr;
    ExprDict<T> parameters_dict;
	
};

template <typename T>
class SubVisitor : public ExprVisitor<T> {
public:
	virtual PExpression<T> visit(RefExpression<T>* expr) {
		this->index_name = expr->Name();
        this->a = 1;
		return PExpression<T>();
	}
	
	virtual PExpression<T> visit(ValExpression<T>* expr) {
        b = numeric_interface<T>::toInt(expr->Eval());
		return PExpression<T>();
	}

	virtual PExpression<T> visit(AddExpression<T>* expr) {
		SubVisitor l,r;
        expr->m_e1->accept(l);
        expr->m_e2->accept(r);
		if(l.index_name != "" && r.index_name != "" && l.index_name != r.index_name) {
			a = 0;
			b = 0;
			index_name = "";
			throw std::runtime_error("Multiple index name found inside a sub-expression.");
		}
		a = l.a + r.a;
		b = l.b + r.b;
		this->index_name = l.index_name;
		return PExpression<T>();
	}
	
	virtual PExpression<T> visit(NegExpression<T>* expr) {
		SubVisitor l;
        expr->m_e->accept(l);
		a = -l.a;
		b = -l.b;
		this->index_name = l.index_name;
		return PExpression<T>();
	}

	virtual PExpression<T> visit(MultExpression<T>* expr) {
		SubVisitor l,r;
        expr->m_e1->accept(l);
        expr->m_e2->accept(r);
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
		return PExpression<T>();
	}
	
	PExpression<T> visit_unexpected_expression() {
		a = 0;
		b = 0;
		index_name = "";
		throw std::runtime_error("Unexpected expression inside an sub expression.");
		return PExpression<T>();
	}

    virtual PExpression<T> visit(EmptyExpression<T>*) {
        return visit_unexpected_expression();
    }
	
    virtual PExpression<T> visit(MatExpression<T>* ) {
		return visit_unexpected_expression();
	}
	
    virtual PExpression<T> visit(EqualExpression<T>* ) {
		return visit_unexpected_expression();
	}
	
    virtual PExpression<T> visit(FuncExpression<T>* ) {
		return visit_unexpected_expression();
	}
	
    virtual PExpression<T> visit(DivExpression<T>* ) {
		return visit_unexpected_expression();
	}

    virtual PExpression<T> visit(PowExpression<T>* ) {
		return visit_unexpected_expression();
	}

    virtual PExpression<T> visit(FactExpression<T>* ) {
		return visit_unexpected_expression();
	}

    std::string get_index_name() {
        return index_name;
    }

    int get_a() {
        return a;
    }

    int get_b() {
        return b;
    }

private:
	std::string index_name;
    int a;
    int b;
	
};

template <typename T>
class RecursiveExprVisitor : public ExprVisitor<T> {
public:
    RecursiveExprVisitor(const std::string& name, PExpression<T> recexp)
        : name_(name)
    {
        recexp->accept(*this);
    }

    virtual PExpression<T> visit(EmptyExpression<T>* expr) {
        return PExpression<T>();
    }

    template <typename U>
    PExpression<T> binary_visit(U* expr) {
        auto e1 = expr->m_e1->accept(*this);
        auto e2 = expr->m_e2->accept(*this);
        if(e1) {
            expr->m_e1 = e1;
        }
        if(e2) {
            expr->m_e2 = e2;
        }
        return PExpression<T>();
    }

    template <typename U>
    PExpression<T> unary_visit(U* expr) {
        auto e = expr->m_e->accept(*this);
        if(e) {
            expr->m_e = e;
        }
        return PExpression<T>();
    }

    virtual PExpression<T> visit(EqualExpression<T>* expr) {
        return binary_visit(expr);
    }

    virtual PExpression<T> visit(AddExpression<T>* expr) {
        return binary_visit(expr);
    }

    virtual PExpression<T> visit(NegExpression<T>* expr) {
        return unary_visit(expr);
    }

    virtual PExpression<T> visit(MultExpression<T>* expr) {
        return binary_visit(expr);
    }

    virtual PExpression<T> visit(DivExpression<T>* expr) {
        return binary_visit(expr);
    }

    virtual PExpression<T> visit(PowExpression<T>* expr) {
        return binary_visit(expr);
    }

    virtual PExpression<T> visit(FactExpression<T>* expr) {
        return unary_visit(expr);
    }

    virtual PExpression<T> visit(ValExpression<T>* expr) {
        return PExpression<T>();
    }

    virtual PExpression<T> visit(MatExpression<T>* expr) {
        for(size_t i = 0; i < expr->VirtualSize(); ++i) {
            auto e = expr->GetExpr(i)->accept(*this);
            if(e) {
                expr->GetExpr(i) = e;
            }
        }
        return PExpression<T>();
    }

    virtual PExpression<T> visit(RefExpression<T>* expr) {
        PExpression<T> e;
        if(expr->Name() == name_) {
            e.reset(new RecursivePlaceholderExpression<T>(name_, ParametersCall<T>()));
            wrapped_.push_back(e);
        }
        return e;
    }

    virtual PExpression<T> visit(FuncExpression<T>* expr) {
        PExpression<T> e;
        if(expr->Name() == name_) {
            e.reset(new RecursivePlaceholderExpression<T>(name_, expr->parameters_call()));
            wrapped_.push_back(e);
        }
        return e;
    }

    std::vector<PExpression<T>> wrapped() {
        return wrapped_;
    }

private:
    std::string name_;
    std::vector<PExpression<T>> wrapped_;
};

#include "reference_stack.hpp"

template <typename T>
class EvaluationVisitor : public ExprVisitor<T> {
public:
    EvaluationVisitor(PExpression<T> evalexpr, ReferenceStack<T>& stack) : stack_(stack), value_() {
        evalexpr->accept(*this);
    }

    virtual PExpression<T> visit(EmptyExpression<T>*) {
        return PExpression<T>();
    }

    virtual PExpression<T> visit(EqualExpression<T>* expr) {
        this->stack_.Set(expr->Name(), expr->parameters_definition(), expr->m_e2);
        value_ = EvaluationVisitor<T>(expr->m_e1, stack_).value();
        return PExpression<T>();
    }

    virtual PExpression<T> visit(AddExpression<T>* expr) {
        value_ = EvaluationVisitor<T>(expr->m_e1, stack_).value()
               + EvaluationVisitor<T>(expr->m_e2, stack_).value();
        return PExpression<T>();
    }

    virtual PExpression<T> visit(NegExpression<T>* expr) {
        value_ = - EvaluationVisitor<T>(expr->m_e, stack_).value();
        return PExpression<T>();
    }

    virtual PExpression<T> visit(MultExpression<T>* expr) {
        value_ = EvaluationVisitor<T>(expr->m_e1, stack_).value()
               * EvaluationVisitor<T>(expr->m_e2, stack_).value();
        return PExpression<T>();
    }

    virtual PExpression<T> visit(DivExpression<T>* expr) {
        value_ = EvaluationVisitor<T>(expr->m_e1, stack_).value()
               / EvaluationVisitor<T>(expr->m_e2, stack_).value();
        return PExpression<T>();
    }

    virtual PExpression<T> visit(PowExpression<T>* expr) {
        value_ =  numeric_interface<T>::pow(
                    EvaluationVisitor<T>(expr->m_e1, stack_).value(),
                    EvaluationVisitor<T>(expr->m_e2, stack_).value());
        return PExpression<T>();
    }

    virtual PExpression<T> visit(FactExpression<T>* expr) {
        value_ =  T(numeric_interface<T>::fact(
                    EvaluationVisitor<T>(expr->m_e, stack_).value()));
        return PExpression<T>();
    }

    virtual PExpression<T> visit(ValExpression<T>* expr) {
        value_ = expr->m_value;
        return PExpression<T>();
    }

    virtual PExpression<T> visit(MatExpression<T>* expr) {
        // TODO Implement real evaluation for MatExpression
        // For the time being, one dimension only...
        value_ = EvaluationVisitor<T>(expr->GetExpr(0),stack_).value();
        return PExpression<T>();
    }

    virtual PExpression<T> visit(RefExpression<T>* expr) {
        value_ = stack_.Eval(expr->Name(), ParametersCall<T>());
        return PExpression<T>();
    }

    virtual PExpression<T> visit(FuncExpression<T>* expr) {
        // TODO Implement real evaluation for FuncExpression
        // For the time being, no paramters...
        value_ = stack_.Eval(expr->Name(), ParametersCall<T>());
        return PExpression<T>();
    }

    T value() {
        return value_;
    }

private:
    ReferenceStack<T>& stack_;
    T value_;
};


#endif // H_EXPR_VISITOR

#ifndef H_EXPR_VISITOR
#define H_EXPR_VISITOR

#include <memory>
#include "inkamath/pexpression.hpp"
#include <vector>
#include <map>
#include <tuple>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include "inkamath/expression_dict.hpp"
#include "inkamath/numeric_interface.hpp"

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
class ParametersCall;

template <typename T>
class ParametersDefinition;

template <typename T, typename ReturnType>
class ExpressionVisitor {
public:
    virtual ReturnType visit(EqualExpression<T>* expr) = 0;
    virtual ReturnType visit(AddExpression<T>* expr) = 0;
    virtual ReturnType visit(NegExpression<T>* expr) = 0;
    virtual ReturnType visit(MultExpression<T>* expr) = 0;
    virtual ReturnType visit(DivExpression<T>* expr) = 0;
    virtual ReturnType visit(PowExpression<T>* expr) = 0;
    virtual ReturnType visit(FactExpression<T>* expr) = 0;
    virtual ReturnType visit(ValExpression<T>* expr) = 0;
    virtual ReturnType visit(MatExpression<T>* expr) = 0;
    virtual ReturnType visit(RefExpression<T>* expr) = 0;
    virtual ReturnType visit(FuncExpression<T>* expr) = 0;
};

// Design choice: limit the number of visitor base classes.
// => Only two base classes that any AST node has to accept explicitly:
// - TransformationVisitor returning a PExpression<T>
// - FoldingVisitor returning a T

// class TransformationVisitor
// This is the base class of visitors that modify the parser AST to:
// - add semantic nodes
// - simplify the tree
// - change the representation of the expression (example: pretty printing
//    that would use the special string node to accumulate the representation)
template <typename T>
class TransformationVisitor : public ExpressionVisitor<T, PExpression<T>> {
};

// class FoldingVisitor
// This is the base class of visitors that "fold" the tree to:
// - evaluate the expression (interpreter)
// - generate code (compiler)
template <typename T>
class FoldingVisitor : public ExpressionVisitor<T, T> {
};

template <typename T>
class ParametersVisitor : public TransformationVisitor<T> {
public:

	PExpression<T> visit(MatExpression<T>* expr) override {
        if(visitor_depth++ == 0) {
            for(const auto& e : expr->Children()) {
                e->accept(*this);
			}
		}
        else {
            visit_others_expr_imp(expr);
        }

		return PExpression<T>();
	}
	
	PExpression<T> visit(EqualExpression<T>* expr) override {
		keyword_params_begin = true;
        this->parameters_dict[expr->m_e1()->Name()] = expr->m_e2();
        this->parameters_names.push_back(expr->m_e1()->Name());
		++visitor_depth;
		return PExpression<T>();
	}
	
	PExpression<T> visit(RefExpression<T>* expr) override {
		if(!keyword_params_begin) {
			this->parameters_names.push_back(expr->Name());
            this->parameters_expr.push_back(expr->self());
		}
		else {
			throw std::runtime_error("a positional argument cannot follow a keyword argument");
		}
		++visitor_depth;
		return PExpression<T>();
	}
	
	PExpression<T> visit_others_expr_imp(Expression<T>* expr) {
		if(!keyword_params_begin) {
            this->parameters_expr.push_back(expr->self());
		}
		else {
			throw std::runtime_error("a positional argument cannot follow a keyword argument");
		}
		++visitor_depth;
		return PExpression<T>();
	}
	
	PExpression<T> visit(FuncExpression<T>* expr) override {
		return visit_others_expr_imp(expr);
	}
	
	PExpression<T> visit(AddExpression<T>* expr) override {
		return visit_others_expr_imp(expr);
	}
	
	PExpression<T> visit(NegExpression<T>* expr) override {
		return visit_others_expr_imp(expr);
	}

	PExpression<T> visit(MultExpression<T>* expr) override {
		return visit_others_expr_imp(expr);
	}

	PExpression<T> visit(DivExpression<T>* expr) override {
		return visit_others_expr_imp(expr);
	}

	PExpression<T> visit(PowExpression<T>* expr) override {
		return visit_others_expr_imp(expr);
	}

	PExpression<T> visit(FactExpression<T>* expr) override {
		return visit_others_expr_imp(expr);
	}

	PExpression<T> visit(ValExpression<T>* expr) override {
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
	bool keyword_params_begin = false;
	std::vector<std::string> parameters_names;
	std::vector<PExpression<T>> parameters_expr;
    ExprDict<T> parameters_dict;
	
};

template <typename T>
class ReferenceStack;

template <typename T>
class EvaluationVisitor : public FoldingVisitor<T> {
public:

    explicit EvaluationVisitor(ReferenceStack<T>& stack) : stack_(stack) {}

    ReferenceStack<T>& stack() {return stack_;}

    // Installing the definition, without evaluating anything. A definition at
    // the top level is a statement and never gets as far as visit().
    void Bind(EqualExpression<T>* expr, const std::string& written = std::string()) {
        // The left-hand side is a bare name, or a call carrying the
        // parameter list and the index: 'f(x)_n = ...'.
        const std::vector<PExpression<T>>& signature = expr->m_e1()->Children();
        if(signature.empty()) {
            this->stack_.Set(expr->Name(), ParametersDefinition<T>(), expr->m_e2(), written);
        }
        else {
            this->stack_.Set(expr->Name(),
                             ParametersDefinition<T>(signature[0], signature[1], *this),
                             expr->m_e2(), written);
        }
    }

    T visit(EqualExpression<T>* expr) override {
        Bind(expr);
        return expr->m_e1()->accept(*this);
    }

    // The two operands are sequenced: C++ leaves the order of `f(a) + f(b)`
    // unspecified, and an operand can bind a name or raise a diagnostic, so
    // without this the answer depends on the compiler.
    T visit(AddExpression<T>* expr) override {
        const T left = expr->m_e1()->accept(*this);
        return left + expr->m_e2()->accept(*this);
    }

    T visit(NegExpression<T>* expr) override {
        return -expr->m_e()->accept(*this);
    }

    T visit(MultExpression<T>* expr) override {
        const T left = expr->m_e1()->accept(*this);
        return left * expr->m_e2()->accept(*this);
    }

    T visit(DivExpression<T>* expr) override {
        const T left = expr->m_e1()->accept(*this);
        return left / expr->m_e2()->accept(*this);
    }

    T visit(PowExpression<T>* expr) override {
        const T base = expr->m_e1()->accept(*this);
        return numeric_interface<T>::pow(base, expr->m_e2()->accept(*this));
    }

    T visit(FactExpression<T>* expr) override {
        return  T(numeric_interface<T>::fact(expr->m_e()->accept(*this)));
    }

    T visit(ValExpression<T>* expr) override {
        return expr->value;
    }

    T visit(MatExpression<T>* expr) override {

        const size_t n = expr->Size().rows;
        const size_t m = expr->Size().cols;
        std::vector<T> evaluation(n*m);
        std::vector<Extent> sizes(n*m);

        // Evaluating the matrix expression
        for(size_t i = 0; i < n; ++i) {
            for(size_t j = 0; j < m; ++j) {
                evaluation[i*m+j] = expr->Children()[i*m+j]->accept(*this);
                sizes[i*m+j] = evaluation[i*m+j].Size();
            }
        }

        // Compute the result size of each row and col in the matrix expression
        std::vector<size_t> i_rows(n, 1);
        std::vector<size_t> j_cols(m, 1);
        for(size_t i = 0; i < n; ++i) {
            for(size_t j = 0; j < m; ++j) {
                i_rows[i] = std::max(i_rows[i], sizes[i*m+j].rows);
                j_cols[j] = std::max(j_cols[j], sizes[i*m+j].cols);
            }
        }

        // Where each block row and column starts in the result: an exclusive
        // prefix sum over the block sizes, whose totals are the result extent.
        std::vector<size_t> ri_rows(n);
        std::vector<size_t> rj_cols(m);
        size_t rn = 0;
        for(size_t i = 0; i < n; ++i) {
            ri_rows[i] = rn;
            rn += i_rows[i];
        }
        size_t rm = 0;
        for(size_t j = 0; j < m; ++j) {
            rj_cols[j] = rm;
            rm += j_cols[j];
        }

        // Populate the final matrix with the right size
        T retval(Extent{rn, rm});
        for(size_t i = 0; i < n; ++i) {
            for(size_t j = 0; j < m; ++j) {
                for(size_t ri = 0; ri < i_rows[i]; ++ri) {
                    for(size_t rj = 0; rj < j_cols[j]; ++rj) {
                        const Extent s = sizes[i*m+j];
                        if(ri < s.rows && rj < s.cols) {
                            // get the evaluated cell result
                            retval((ri_rows[i]+ri+1), (rj_cols[j]+rj+1)) = evaluation[i*m+j](ri+1, rj+1);
                        }
                        else {
                            // extend the previous (up and left) evaluated cell result
                            retval((ri_rows[i]+ri+1), (rj_cols[j]+rj+1)) = evaluation[i*m+j](s.rows, s.cols);
                        }
                    }
                }
            }
        }

        return retval;
    }

    T visit(RefExpression<T>* expr) override {
        return stack_.Eval(expr->Name(), ParametersCall<T>());
    }

    T visit(FuncExpression<T>* expr) override {
        return stack_.Eval(expr->Name(), ParametersCall<T>(expr->m_e1(), expr->m_e2(), expr->limit()));
    }

private:
    ReferenceStack<T>& stack_;
};

#endif // H_EXPR_VISITOR

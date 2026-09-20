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
#include "inkamath/dynarraylike.hpp"
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

// Design choice: limit the number of visitors base class.
// => Only two visitors base class that any AST node should accept explicitly:
// - TransformationVisitor returning a PExpression<T>
// - FoldingVisitor returning a T

// class TransformationVisitor
// This is the base class of visitors that modify the parser-AST to :
// - add semantic nodes
// - simplify the tree
// - change the representation of the expression (example: pretty printing
//    that would use the special string node to accumulate the representation)
template <typename T>
class TransformationVisitor : public ExpressionVisitor<T, PExpression<T>> {
};

template <typename T>
inline void transform_visitation(TransformationVisitor<T>& v, PExpression<T>& to_transform) {
    PExpression<T> e = to_transform->accept(v);
    if(e) {
        to_transform = e;
    }
}

// class FoldingVisitor
// This is the base class of visitors that "folds" the tree to :
// - evaluate the expression (interpreter)
// - generate code (compiler)
template <typename T>
class FoldingVisitor : public ExpressionVisitor<T, T> {
};

// class StatefullVisitor
// This base class is provided for convenience (and sanity reasons).
// This is simply a TransformationVisitor that is not supposed to modify
// the AST but rather gather some information about an AST in its internal state.
// Use TransformationVisitor instead if you mean to modify the AST.
// Visitor derived from TransformationVisitor can have an internal state too...
// If you choose StatefullVisitor, just remember that nothing prevents you
// from modifying the AST in secret... (don't do that).
template <typename T>
class StatefulVisitor : public TransformationVisitor<T> {
};

template <typename T>
class ParametersVisitor : public StatefulVisitor<T> {
public:

	PExpression<T> visit(MatExpression<T>* expr) override {
        if(visitor_depth++ == 0) {
            for(auto e : expr->children) {
                e->accept(*this);
			}
		}
        else {
            visit_others_expr_imp(expr);
        }

		return PExpression<T>();
	}
	
	PExpression<T> visit(EqualExpression<T>* expr) override {
		kewword_params_begin = true;
        this->parameters_dict[expr->m_e1()->Name()] = expr->m_e2();
        this->parameters_names.push_back(expr->m_e1()->Name());
		++visitor_depth;
		return PExpression<T>();
	}
	
	PExpression<T> visit(RefExpression<T>* expr) override {
		if(!kewword_params_begin) {
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
		if(!kewword_params_begin) {
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
	bool kewword_params_begin = false;
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
    void Bind(EqualExpression<T>* expr) {
        if(expr->children[0]->children.size() > 0) {
            this->stack_.Set(expr->Name(), ParametersDefinition<T>(expr->children[0]->children[0], expr->children[0]->children[1], *this), expr->children[1]);
        }
        else {
            this->stack_.Set(expr->Name(), ParametersDefinition<T>(), expr->children[1]);
        }
    }

    T visit(EqualExpression<T>* expr) override {
        Bind(expr);
        return expr->m_e1()->accept(*this);
    }

    T visit(AddExpression<T>* expr) override {
        return expr->m_e1()->accept(*this)
             + expr->m_e2()->accept(*this);
    }

    T visit(NegExpression<T>* expr) override {
        return -expr->m_e()->accept(*this);
    }

    T visit(MultExpression<T>* expr) override {
        return expr->m_e1()->accept(*this)
             * expr->m_e2()->accept(*this);
    }

    T visit(DivExpression<T>* expr) override {
        return expr->m_e1()->accept(*this)
             / expr->m_e2()->accept(*this);
    }

    T visit(PowExpression<T>* expr) override {
        return  numeric_interface<T>::pow(
                    expr->m_e1()->accept(*this),
                    expr->m_e2()->accept(*this));
    }

    T visit(FactExpression<T>* expr) override {
        return  T(numeric_interface<T>::fact(expr->m_e()->accept(*this)));
    }

    T visit(ValExpression<T>* expr) override {
        return expr->value;
    }

    T visit(MatExpression<T>* expr) override {

        size_t n, m;
        std::tie(n, m) = expr->Size();
        dynarray<T> evaluation(n*m);
        dynarray<std::pair<size_t, size_t>> sizes(n*m);

        // Evaluating the matrix expression
        for(size_t i = 0; i < n; ++i) {
            for(size_t j = 0; j < m; ++j) {
                evaluation[i*m+j] = expr->children[i*m+j]->accept(*this);
                sizes[i*m+j] = evaluation[i*m+j].Size();
            }
        }

        // Compute the result size of each row and col in the matrix expression
        dynarray<size_t> i_rows(n);
        dynarray<size_t> j_cols(m);
        i_rows.fill(1);
        j_cols.fill(1);
        for(size_t i = 0; i < n; ++i) {
            for(size_t j = 0; j < m; ++j) {
                i_rows[i] = std::max(i_rows[i], sizes[i*m+j].first);
                j_cols[j] = std::max(j_cols[j], sizes[i*m+j].second);
            }
        }

        // Compute the size of each previous (up and left) result matrix blocks
        dynarray<size_t> ri_rows = i_rows;
        dynarray<size_t> rj_cols = j_cols;

        for(size_t i = 1; i < n; ++i) {
            ri_rows[i] += i_rows[i-1];
        }
        size_t rn = ri_rows.back();
        ri_rows.back() = 0;
        std::rotate(ri_rows.begin(), ri_rows.end()-1, ri_rows.end());

        for(size_t j = 1; j < m; ++j) {
            rj_cols[j] += j_cols[j-1];
        }
        size_t rm = rj_cols.back();
        rj_cols.back() = 0;
        std::rotate(rj_cols.begin(), rj_cols.end()-1, rj_cols.end());

        // Populate the final matrix with the right size
        T retval(rn, rm);
        for(size_t i = 0; i < n; ++i) {
            for(size_t j = 0; j < m; ++j) {
                for(size_t ri = 0; ri < i_rows[i]; ++ri) {
                    for(size_t rj = 0; rj < j_cols[j]; ++rj) {
                        auto s = sizes[i*m+j];
                        if(ri < s.first && rj < s.second) {
                            // get the evaluated cell result
                            retval((ri_rows[i]+ri+1), (rj_cols[j]+rj+1)) = evaluation[i*m+j](ri+1, rj+1);
                        }
                        else {
                            // extend the previous (up and left) evaluated cell result
                            retval((ri_rows[i]+ri+1), (rj_cols[j]+rj+1)) = evaluation[i*m+j](s.first, s.second);
                        }
                    }
                }
            }
        }

        // Surprisingly it just works (at least for now).
        // Note to self : refactor later
        return retval; // finally
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

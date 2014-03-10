#ifndef H_EXPR
#define H_EXPR

#include <iostream>
#include <string>
#include <algorithm> // max
#include <cmath>
#include <list>
#include <utility> // pair
#include <iterator> // back_inserter
#include <unordered_map>

#include <memory>

#include "pmath.hpp"
#include "matrix.hpp"
#include "numeric_interface.hpp"
#include "expression_dict.hpp"
#include "dynarraylike.hpp"


#define _EXPRESION_EPSILON 1E-10


template <typename T>
class Expression;

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;

template <typename T>
class FoldingVisitor;

template <typename T>
class TransformationVisitor;







template <typename T>
class Expression : public std::enable_shared_from_this<Expression<T>>
{
public:
    Expression() {}

    explicit Expression(std::initializer_list<PExpression<T>> expressions) : children(std::move(expressions)) {}
    explicit Expression(dynarray<PExpression<T>>&& exprs) : children(std::move(exprs)) {}
    explicit Expression(const dynarray<PExpression<T>>& exprs) : children(exprs) {}

    virtual ~Expression() {}
    virtual PExpression<T> Clone() const = 0;
	
    PExpression<T> self() {
        return this->shared_from_this();
    }
	
    virtual T accept(FoldingVisitor<T> &v) = 0;
    virtual PExpression<T> accept(TransformationVisitor<T> &v) = 0;

    virtual std::string Name()
    {
        return std::string();
    }
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(1,1);
    }

    dynarray<PExpression<T>> children;
protected:
private:
    // interdiction de la copie
    Expression(const Expression<T>& e) = delete;
    Expression& operator=(const Expression<T>& e) = delete;
	
};

template <typename T>
class UnaryExpression : public Expression<T>
{
public:
    explicit UnaryExpression(PExpression<T> e) : Expression<T>{e}
    {}

    inline PExpression<T> m_e() const {return this->children[0];}
    inline PExpression<T>& m_e() {return this->children[0];}

};

template <typename T>
class BinaryExpression : public Expression<T>
{
public:
    explicit BinaryExpression(PExpression<T> e1, PExpression<T> e2) : Expression<T>({e1, e2})
    {}

    inline PExpression<T> m_e1() const {return this->children[0];}
    inline PExpression<T> m_e2() const {return this->children[1];}
    inline PExpression<T>& m_e1() {return this->children[0];}
    inline PExpression<T>& m_e2() {return this->children[1];}
};

template <typename T>
class EqualExpression : public BinaryExpression<T>
{
public:
    explicit EqualExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<EqualExpression<T>>(
                                 this->m_e1()->Clone(),
                                 this->m_e2()->Clone());
    }

    virtual std::string Name() const {
        return BinaryExpression<T>::m_e1()->Name();
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};


template <typename T>
class AddExpression : public BinaryExpression<T>
{
public:
    explicit AddExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<AddExpression<T>>(this->m_e1()->Clone(), this->m_e2()->Clone());
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class NegExpression : public UnaryExpression<T>
{
public:
    explicit NegExpression(PExpression<T> e) : UnaryExpression<T>(e) {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<NegExpression<T>>(this->m_e()->Clone());
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class MultExpression : public BinaryExpression<T>
{
public:
    explicit MultExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<MultExpression<T>>(this->m_e1()->Clone(), this->m_e2()->Clone());
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class DivExpression : public BinaryExpression<T>
{
public:
    explicit DivExpression(PExpression<T> e1, PExpression<T> e2)
        : BinaryExpression<T>(e1,e2)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<DivExpression<T>>(this->m_e1()->Clone(), this->m_e2()->Clone());
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class PowExpression : public BinaryExpression<T>
{
public:
    explicit PowExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<PowExpression<T>>(this->m_e1()->Clone(), this->m_e2()->Clone());
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class FactExpression : public UnaryExpression<T>
{
public:
    explicit FactExpression(PExpression<T> e)
        : UnaryExpression<T>(e)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<FactExpression<T>>(this->m_e()->Clone());
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class ValExpression : public Expression<T>
{
public:
    explicit ValExpression(const T& v) : Expression<T>(), value(v) {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<ValExpression<T>>(value);
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }

    const T value;
};

template <typename T>
class ParametersCall;

template <typename T>
class RecursivePlaceholderExpression : public Expression<T>
{
public:
    explicit RecursivePlaceholderExpression(const std::string& name, const ParametersCall<T>& params)
        : Expression<T>(), value_(), name_(name), params_(params)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<RecursivePlaceholderExpression<T>>(name_, params_);
    }

    virtual std::string Name()
    {
        return name_;
    }

    const ParametersCall<T>& params() {return params_;}

    void Set(const T& value)
    {
        value_ = value;
    }

    T get() const {
        return value_;
    }

    virtual T accept(FoldingVisitor<T>& v) {return v.visit(this);}
    virtual PExpression<T> accept(TransformationVisitor<T>& v)  {return v.visit(this);}

protected:
    T value_;
    std::string name_;
    ParametersCall<T> params_;
};

template <typename T>
class RecursiveExpression : public Expression<T>
{
public:
    explicit RecursiveExpression(PExpression<T> expr, dynarray<PExpression<T>> recursive_placeholders)
        : Expression<T>(std::move(recursive_placeholders)), expr_(expr)
    {}

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<RecursiveExpression<T>>(expr_, this->children);
    }

    PExpression<T> recursive_expr() const {return expr_;}

    virtual T accept(FoldingVisitor<T>& v) {return v.visit(this);}
    virtual PExpression<T> accept(TransformationVisitor<T>& v)  {return v.visit(this);}


protected:
    PExpression<T> expr_;
};

template <typename T>
class MatExpression : public Expression<T>
{
public:

    explicit MatExpression(PExpression<T> e)
        :  Expression<T>({e}), n_(1), m_(1)
    {}

    MatExpression(size_t n, size_t m, dynarray<PExpression<T>> expr)
        : Expression<T>(expr), n_(n), m_(m)
    {}

    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(n_,m_);
    }

    virtual PExpression<T> Clone() const
    {
        dynarray<PExpression<T>> expr(this->children.size());
        for(size_t i =0; i < expr.size(); ++i) {
            expr[i] = this->children[i]->Clone();
        }

        return std::make_shared<MatExpression<T>>(n_, m_, std::move(expr));
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }

protected:
    size_t n_;
    size_t m_;
};

template <typename T>
class RefExpression : public Expression<T>
{
public:
    explicit RefExpression(const std::string& name)
        : Expression<T>(), m_name(name)
    { }

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<RefExpression<T>>(m_name);
    }

    virtual std::string Name()
    {
        return m_name;
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
    std::string m_name;
};

template <typename T>
class FuncExpression : public BinaryExpression<T>
{
public:
    explicit FuncExpression(PExpression<T> ref_expression, PExpression<T> e1, PExpression<T> e2)
        : BinaryExpression<T>(e1,e2), m_name(ref_expression->Name()), ref_expression_(ref_expression)
    { }

    virtual PExpression<T> Clone() const
    {
        return std::make_shared<FuncExpression<T>>(
                ref_expression_->Clone(),
                this->m_e1() ? this->m_e1()->Clone() : nullptr,
                this->m_e2() ? this->m_e2()->Clone() : nullptr);
    }

    virtual std::string Name()
    {
        return m_name;
    }

    virtual PExpression<T> accept(TransformationVisitor<T> &v) {
        return v.visit(this);
    }

    virtual T accept(FoldingVisitor<T> &v) {
        return v.visit(this);
    }
protected:
    std::string m_name;
    PExpression<T> ref_expression_;
};

#endif

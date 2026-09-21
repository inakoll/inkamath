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
#include "inkamath/pexpression.hpp"

#include "inkamath/matrix.hpp"
#include "inkamath/numeric_interface.hpp"
#include "inkamath/expression_dict.hpp"
#include "inkamath/extent.hpp"

#include <vector>

template <typename T>
class Expression;

template <typename T>
class FoldingVisitor;

template <typename T>
class TransformationVisitor;

template <typename T>
class Expression : public std::enable_shared_from_this<Expression<T>>
{
public:
    Expression() {}

    explicit Expression(std::initializer_list<PExpression<T>> expressions) : children_(std::move(expressions)) {}
    explicit Expression(std::vector<PExpression<T>> exprs) : children_(std::move(exprs)) {}

    virtual ~Expression() = default;
    PExpression<T> self() {
        return this->shared_from_this();
    }
	
    virtual T accept(FoldingVisitor<T> &v) = 0;
    virtual PExpression<T> accept(TransformationVisitor<T> &v) = 0;

    virtual std::string Name()
    {
        return std::string();
    }
    virtual Extent Size() const
    {
        return Extent();
    }

    // The generic view, for a visitor that does not care which node it is on.
    // Named views (m_e1, m_e) sit over the same storage; nothing writes to
    // either after parsing.
    const std::vector<PExpression<T>>& Children() const {return children_;}

    Expression(const Expression<T>& e) = delete;
    Expression& operator=(const Expression<T>& e) = delete;

private:
    std::vector<PExpression<T>> children_;
};

template <typename T>
class UnaryExpression : public Expression<T>
{
public:
    explicit UnaryExpression(PExpression<T> e) : Expression<T>{e}
    {}

    PExpression<T> m_e() const {return this->Children()[0];}

};

template <typename T>
class BinaryExpression : public Expression<T>
{
public:
    explicit BinaryExpression(PExpression<T> e1, PExpression<T> e2) : Expression<T>({e1, e2})
    {}

    PExpression<T> m_e1() const {return this->Children()[0];}
    PExpression<T> m_e2() const {return this->Children()[1];}
};

template <typename T>
class EqualExpression : public BinaryExpression<T>
{
public:
    explicit EqualExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}

    std::string Name() override {
        return BinaryExpression<T>::m_e1()->Name();
    }

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
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

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }
protected:
};

template <typename T>
class NegExpression : public UnaryExpression<T>
{
public:
    explicit NegExpression(PExpression<T> e) : UnaryExpression<T>(e) {}

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
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

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
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

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
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

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
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

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }
protected:
};

template <typename T>
class ValExpression : public Expression<T>
{
public:
    explicit ValExpression(const T& v) : Expression<T>(), value(v) {}

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }

    const T value;
};

template <typename T>
class ParametersCall;

// One cell of a matrix: 'm[i,j]'.
template <typename T>
class CellExpression : public Expression<T>
{
public:
    CellExpression(PExpression<T> matrix, PExpression<T> row, PExpression<T> col)
        : Expression<T>({matrix, row, col})
    {}

    PExpression<T> Matrix() const {return this->Children()[0];}
    PExpression<T> Row() const {return this->Children()[1];}
    PExpression<T> Col() const {return this->Children()[2];}

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }
};

template <typename T>
class MatExpression : public Expression<T>
{
public:

    explicit MatExpression(PExpression<T> e)
        :  Expression<T>({e}), n_(1), m_(1)
    {}

    MatExpression(size_t n, size_t m, std::vector<PExpression<T>> expr)
        : Expression<T>(std::move(expr)), n_(n), m_(m)
    {}

    Extent Size() const override
    {
        return Extent{n_, m_};
    }

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
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

    std::string Name() override
    {
        return m_name;
    }

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }
protected:
    std::string m_name;
};

template <typename T>
class FuncExpression : public BinaryExpression<T>
{
public:
    explicit FuncExpression(PExpression<T> ref_expression, PExpression<T> e1, PExpression<T> e2, bool limit = false)
        : BinaryExpression<T>(e1,e2), m_name(ref_expression->Name()), ref_expression_(ref_expression), limit_(limit)
    { }

    // 'lim f' asks the reference for the limit of its general clause rather
    // than for one term.
    bool limit() const {return limit_;}

    std::string Name() override
    {
        return m_name;
    }

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }
protected:
    std::string m_name;
    PExpression<T> ref_expression_;
    bool limit_;
};

#endif

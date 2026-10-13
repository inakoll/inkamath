#ifndef H_EXPR
#define H_EXPR

#include <algorithm>  // max
#include <cmath>
#include <iostream>
#include <iterator>  // back_inserter
#include <list>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>  // pair

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

    explicit Expression(std::initializer_list<PExpression<T>> expressions)
        : children_(std::move(expressions)) {
        Measure();
    }
    explicit Expression(std::vector<PExpression<T>> exprs) : children_(std::move(exprs)) {
        Measure();
    }

    // Every walk of a tree recurses once per level -- the evaluator's, the
    // destructor's, the printers' -- so a tree deeper than this is refused
    // as it is built, while what was built can still be destroyed. Measured
    // against the stack in DESIGN.md, C20 and C63.
    static constexpr std::size_t max_depth = 1000;
    [[nodiscard]] std::size_t    Depth() const { return depth_; }

    // How many grads by several names the tree holds, so that a definition
    // that holds none, nearly every one, is not walked for them.
    [[nodiscard]] std::size_t Groups() const { return groups_; }

    virtual ~Expression() = default;
    PExpression<T> self() {
        return this->shared_from_this();
    }
	
    virtual T accept(FoldingVisitor<T> &v) = 0;
    virtual PExpression<T> accept(TransformationVisitor<T> &v) = 0;

    virtual const std::string& Name() const
    {
        static const std::string none;
        return none;
    }

    // The left-hand side of a definition; empty for everything else.
    virtual const std::string& Signature() const
    {
        static const std::string none;
        return none;
    }
    // The generic view, for a visitor that does not care which node it is on.
    // Named views (m_e1, m_e) sit over the same storage; nothing writes to
    // either after parsing.
    const std::vector<PExpression<T>>& Children() const {return children_;}

    Expression(const Expression<T>& e) = delete;
    Expression& operator=(const Expression<T>& e) = delete;

private:
    void Measure() {
        for (const PExpression<T>& child : children_)
            if (child) {
                depth_ = std::max(depth_, child->Depth() + 1);
                groups_ += child->Groups();
            }
        if (depth_ > max_depth)
            throw std::runtime_error("expression nests more than " + std::to_string(max_depth) +
                                     " deep");
    }

    std::vector<PExpression<T>> children_;
    std::size_t                 depth_ = 1;

protected:
    std::size_t groups_ = 0;
};

template <typename T>
class UnaryExpression : public Expression<T>
{
public:
    explicit UnaryExpression(PExpression<T> e) : Expression<T>{e}
    {}

    const PExpression<T>& m_e() const {return this->Children()[0];}

};

template <typename T>
class BinaryExpression : public Expression<T>
{
public:
    explicit BinaryExpression(PExpression<T> e1, PExpression<T> e2) : Expression<T>({e1, e2})
    {}

    const PExpression<T>& m_e1() const {return this->Children()[0];}
    const PExpression<T>& m_e2() const {return this->Children()[1];}
};

template <typename T>
class EqualExpression final : public BinaryExpression<T> {
public:
    explicit EqualExpression(PExpression<T> e1, PExpression<T> e2, bool listed = false)
        : BinaryExpression<T>(e1, e2), listed_(listed) {}

    const std::string& Name() const override {
        return BinaryExpression<T>::m_e1()->Name();
    }

    // 'g() = 1', a list that adds nothing, refused (C314, C321).
    [[nodiscard]] bool Listed() const { return listed_; }

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }

private:
    bool listed_;
};

template <typename T>
class AddExpression final : public BinaryExpression<T> {
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
class NegExpression final : public UnaryExpression<T> {
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

// '~', which makes its operand inexact.
template <typename T>
class InexactExpression final : public UnaryExpression<T> {
public:
    explicit InexactExpression(PExpression<T> e) : UnaryExpression<T>(e) {}

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }

    T accept(FoldingVisitor<T>& v) override { return v.visit(this); }
};

// 'm'': the rows as columns.
template <typename T>
class TransposeExpression final : public UnaryExpression<T> {
public:
    explicit TransposeExpression(PExpression<T> e) : UnaryExpression<T>(e) {}

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }

    T accept(FoldingVisitor<T>& v) override { return v.visit(this); }
};

// A function built in, applied to each cell, as 'floor(x)'; ReferenceStack
// defines them.
template <typename T>
class BuiltinExpression final : public UnaryExpression<T> {
public:
    using Cell = typename T::value_type;
    BuiltinExpression(PExpression<T> e, std::string named, Cell (*f)(const Cell&))
        : UnaryExpression<T>(e), name(std::move(named)), function(f) {}

    const std::string name;
    Cell (*const function)(const Cell&);

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }

    T accept(FoldingVisitor<T>& v) override { return v.visit(this); }
};

template <typename T>
class MultExpression final : public BinaryExpression<T> {
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
class DivExpression final : public BinaryExpression<T> {
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
class PowExpression final : public BinaryExpression<T> {
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
class FactExpression final : public UnaryExpression<T> {
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
class ValExpression final : public Expression<T> {
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

// A comparison, answering one or zero. One class with an operator rather than
// six classes: six would be eighteen visit methods for one idea.
template <typename T>
class CompareExpression final : public BinaryExpression<T> {
public:
    CompareExpression(Comparison op, PExpression<T> e1, PExpression<T> e2)
        : BinaryExpression<T>(e1, e2), op_(op)
    {}

    Comparison Op() const {return op_;}

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }

private:
    Comparison op_;
};

// 'a and b', 'a or b': 1 or 0, the right side read only when the left has
// not decided.
template <typename T>
class LogicExpression final : public BinaryExpression<T> {
public:
    LogicExpression(bool conjunction, PExpression<T> e1, PExpression<T> e2)
        : BinaryExpression<T>(e1, e2), conjunction_(conjunction) {}

    [[nodiscard]] bool        Conjunction() const { return conjunction_; }
    [[nodiscard]] const char* Word() const { return conjunction_ ? "and" : "or"; }

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }

    T accept(FoldingVisitor<T>& v) override { return v.visit(this); }

private:
    bool conjunction_;
};

// One cell of a matrix, 'm[i,j]', or with no column one row, 'm[i]'; of a
// tensor, 't[b,i,j]', its slice written first.
template <typename T>
class CellExpression final : public Expression<T> {
public:
    CellExpression(PExpression<T> matrix, PExpression<T> row, PExpression<T> col,
                   PExpression<T> slice = PExpression<T>())
        : Expression<T>({matrix, row, col, slice}) {}

    const PExpression<T>& Matrix() const {return this->Children()[0];}
    const PExpression<T>& Row() const {return this->Children()[1];}
    const PExpression<T>& Col() const {return this->Children()[2];}
    const PExpression<T>& Slice() const { return this->Children()[3]; }

    PExpression<T> accept(TransformationVisitor<T> &v) override {
        return v.visit(this);
    }

    T accept(FoldingVisitor<T> &v) override {
        return v.visit(this);
    }
};

// 'sum_(k=a)^b body' and 'prod_(k=a)^b body'. Without an upper bound, the
// series itself: its partial sums or products taken to their limit. The
// reductions 'max_', 'min_', 'argmax_' and 'argmin_' always have one.
enum class Fold { sum, prod, max, min, argmax, argmin };

template <typename T>
class SeriesExpression final : public Expression<T> {
public:
    SeriesExpression(Fold fold, std::string index, PExpression<T> lower, PExpression<T> upper,
                     PExpression<T> body)
        : Expression<T>({lower, upper, body}), fold_(fold), index_(std::move(index)) {}

    [[nodiscard]] Fold                  Op() const { return fold_; }
    [[nodiscard]] bool                  Product() const { return fold_ == Fold::prod; }
    [[nodiscard]] bool                  Reduction() const { return fold_ > Fold::prod; }
    [[nodiscard]] const std::string&    Index() const { return index_; }
    [[nodiscard]] const char*           Role() const { return role_.c_str(); }
    [[nodiscard]] const PExpression<T>& Lower() const { return this->Children()[0]; }
    // Null for a series without an upper bound.
    [[nodiscard]] const PExpression<T>& Upper() const { return this->Children()[1]; }
    [[nodiscard]] const PExpression<T>& Body() const { return this->Children()[2]; }

    [[nodiscard]] const char* Noun() const {
        static const char* const nouns[] = {"sum",     "product", "maximum",
                                            "minimum", "argmax",  "argmin"};
        return nouns[static_cast<int>(fold_)];
    }
    // A reduction refuses what max(a, b) refuses, in its own words, and
    // keeps a later term only where it ranks strictly higher, so the first
    // of equals, as max(a, b) keeps a (DESIGN.md, reductions over an index).
    [[nodiscard]] std::string Empty(int first, int last) const {
        return std::string("the ") + Noun() + " has no terms: " + index_ + " runs from " +
               std::to_string(first) + " to " + std::to_string(last);
    }
    const T& Ranked(const T& term) const {
        const std::string noun = std::string("the ") + Noun();
        if (!term.IsScalar())
            throw std::runtime_error(noun + " needs single values, not a " +
                                     term.Size().Described() + "; write it by its cells");
        if (!(term == term))
            throw std::runtime_error(noun + " needs a number, not " +
                                     numeric_interface<T>::toString(term));
        using V = typename T::value_type;
        if (!(numeric_interface<V>::imaginary(*term.data()) == numeric_interface<V>::zero()))
            throw std::runtime_error(noun + " needs real numbers, not " +
                                     numeric_interface<T>::toString(term));
        return term;
    }
    [[nodiscard]] bool High() const { return fold_ == Fold::max || fold_ == Fold::argmax; }
    [[nodiscard]] bool Indexes() const { return fold_ == Fold::argmax || fold_ == Fold::argmin; }
    // Whether the later is kept, as a truth: inexact where it read an inexact
    // term, which makes what it chose so, as a guard's clause (C241).
    T Later(const T& kept, const T& next) const {
        return numeric_interface<T>::compare(High() ? kept : next, High() ? next : kept,
                                             Comparison::Less);
    }
    static T Chosen(const T& value, const T& truth) {
        if (numeric_interface<T>::approximated(truth)) return numeric_interface<T>::marked(value);
        return numeric_interface<T>::exact(truth) ? value : numeric_interface<T>::inexact(value);
    }
    // The term kept, or its index, of terms ranked, the first at 'first'.
    T Reduced(const std::vector<T>& terms, int first) const {
        T kept = terms[0], at = T(first);
        for (std::size_t k = 1; k < terms.size(); ++k) {
            const T truth = Later(kept, terms[k]);
            if (numeric_interface<T>::truth(truth))
                kept = terms[k], at = T(first + static_cast<int>(k));
            kept = Chosen(kept, truth);
            at   = Chosen(at, truth);
        }
        return Indexes() ? at : kept;
    }

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }
    T              accept(FoldingVisitor<T>& v) override { return v.visit(this); }

private:
    Fold        fold_;
    std::string index_;
    // Kept by the bindings this node makes, none of which outlives it.
    std::string role_ = std::string("the ") + Noun() + "'s index";
};

// 'grad_(x = a) body': the derivative of the body with respect to x at a. The
// point is read where it is written, the name only in the body, as a sum's.
// 'grad_(x = a, y = b) body' has a part for each name.
template <typename T>
class GradExpression final : public Expression<T> {
public:
    GradExpression(std::vector<std::string> variables, std::vector<PExpression<T>> points,
                   PExpression<T> body)
        : Expression<T>(Joined(std::move(points), std::move(body))),
          variables_(std::move(variables)) {
        this->groups_ += Several();
    }

    [[nodiscard]] const std::vector<std::string>& Variables() const { return variables_; }
    [[nodiscard]] const std::string&              Variable() const { return variables_[0]; }
    [[nodiscard]] const PExpression<T>&           Point(std::size_t k = 0) const {
        return this->Children()[k];
    }
    [[nodiscard]] const PExpression<T>& Body() const { return this->Children().back(); }
    [[nodiscard]] bool                  Several() const { return variables_.size() > 1; }

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }
    T              accept(FoldingVisitor<T>& v) override { return v.visit(this); }

private:
    static std::vector<PExpression<T>> Joined(std::vector<PExpression<T>> points,
                                              PExpression<T>              body) {
        points.push_back(std::move(body));
        return points;
    }
    std::vector<std::string> variables_;
};

template <typename T>
class MatExpression final : public Expression<T> {
public:

    explicit MatExpression(PExpression<T> e)
        :  Expression<T>({e}), n_(1), m_(1)
    {}

    MatExpression(size_t n, size_t m, std::vector<PExpression<T>> expr)
        : Expression<T>(std::move(expr)), n_(n), m_(m)
    {}

    // Only numbers, a short row's padding included: it reads no name and
    // takes no step, so it is built once, into `built`.
    const bool numbers = std::all_of(
        this->Children().begin(), this->Children().end(),
        [](const PExpression<T>& cell) { return dynamic_cast<ValExpression<T>*>(cell.get()); });
    std::optional<T> built;

    Extent Size() const
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

// '[a;; b]': matrix literals stacked as the slices of a tensor.
template <typename T>
class TensorExpression final : public Expression<T> {
public:
    explicit TensorExpression(std::vector<PExpression<T>> slices)
        : Expression<T>(std::move(slices)) {}

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }
    T              accept(FoldingVisitor<T>& v) override { return v.visit(this); }
};

template <typename T>
class RefExpression final : public Expression<T> {
public:
    explicit RefExpression(const std::string& name)
        : Expression<T>(), m_name(name)
    { }

    const std::string& Name() const override
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

// 'g.y_3': a name read in an instance or a file, which is the object on the
// left (DESIGN.md, phase 15).
template <typename T>
class MemberExpression final : public Expression<T> {
public:
    MemberExpression(PExpression<T> object, PExpression<T> member)
        : Expression<T>({std::move(object), std::move(member)}) {}

    [[nodiscard]] const PExpression<T>& Object() const { return this->Children()[0]; }
    [[nodiscard]] const PExpression<T>& Member() const { return this->Children()[1]; }

    PExpression<T> accept(TransformationVisitor<T>& v) override { return v.visit(this); }
    T              accept(FoldingVisitor<T>& v) override { return v.visit(this); }
};

template <typename T>
class ParametersCall;

template <typename T>
class FuncExpression final : public Expression<T> {
public:
    // The third child is the guard of a definition's left-hand side, and the
    // fourth to sixth the row, column and slice of a clause for cells, 'M[j<=2,
    // k<=2]'; all are null everywhere else. Keeping them here is what lets
    // ParametersDefinition read the whole left-hand side from one place.
    explicit FuncExpression(PExpression<T> ref_expression, PExpression<T> e1, PExpression<T> e2,
                            bool limit = false, PExpression<T> guard = PExpression<T>(),
                            std::string    signature = std::string(),
                            PExpression<T> row       = PExpression<T>(),
                            PExpression<T> col       = PExpression<T>(),
                            PExpression<T> slice     = PExpression<T>())
        : Expression<T>({e1, e2, guard, row, col, slice}),
          m_name(ref_expression->Name()),
          limit_(limit),
          signature_(std::move(signature)) {}

    // The left-hand side of a definition, as the tokens spell it.
    const std::string& Signature() const override {return signature_;}

    const PExpression<T>& m_e1() const {return this->Children()[0];}
    const PExpression<T>& m_e2() const {return this->Children()[1];}

    // 'lim f' asks the reference for the limit of its general clause rather
    // than for one term.
    bool limit() const {return limit_;}

    // Read from the call's syntax, which never changes: once per call site
    // rather than once per call. Defined with ParametersCall.
    const ParametersCall<T>& Call() const;

    const std::string& Name() const override
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
    bool limit_;
    std::string signature_;
    mutable std::unique_ptr<const ParametersCall<T>> call_;
};

// Whether an expression reads a name, or calls it.
template <typename T>
bool Mentions(const Expression<T>& e, const std::string& name) {
    if ((dynamic_cast<const RefExpression<T>*>(&e) || dynamic_cast<const FuncExpression<T>*>(&e)) &&
        e.Name() == name)
        return true;
    for (const PExpression<T>& child : e.Children())
        if (child && Mentions(*child, name)) return true;
    return false;
}

#endif

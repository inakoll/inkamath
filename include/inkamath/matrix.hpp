#ifndef H_MATRIX
#define H_MATRIX

#include "inkamath/extent.hpp"
#include "inkamath/numeric_interface.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// A value refused where a single value is needed, and its shape, which a
// call given a value of that shape says in its own name (DESIGN.md, sizes
// inferred in a definition by cells).
struct NotSingle : std::runtime_error {
    // A message as runtime_error takes it, so that a literal is not made a
    // string where the check is inlined.
    template <typename Message>
    NotSingle(const Message& message, Extent extent) : std::runtime_error(message), shape(extent) {}
    Extent shape;
};

template <typename T>
class Matrix
{
public:
    typedef T value_type;

    Matrix() = default;
    explicit Matrix(const T& value) : scalar_(value) {}
    explicit Matrix(Extent extent, const T& value = T()) : extent_(extent), scalar_(value)
    {
        if(!IsScalar()) cells_.assign(extent.count(), value);
    }

    Extent Size() const {return extent_;}

    // Subscripts are 1-based, as they are written; two read a tensor's first slice.
    T&       operator()(long long i, long long j) { return data()[Offset(1, i, j)]; }
    const T& operator()(long long i, long long j) const { return data()[Offset(1, i, j)]; }
    T&       operator()(long long b, long long i, long long j) { return data()[Offset(b, i, j)]; }

    // A matrix prints as the literal that would produce it, with its columns
    // aligned: what is printed can be typed back. A 1x1 is just its value --
    // it is what every scalar answer and every diagnostic quoting one is. A
    // tensor's columns are aligned across its slices, and ';;' ends each
    // slice but the last, or the only one, which would otherwise read back
    // as a matrix.
    static std::string toString(const Matrix<T>& a) {
        return toString(a, [](const T& value) { return numeric_interface<T>::toString(value); });
    }

    // `show` prints a cell, so a matrix is laid out alike however it is shown.
    template <typename Show>
    static std::string toString(const Matrix<T>& a, Show show) {
        if(a.IsScalar()) {
            return show(a(1, 1));
        }

        const size_t             cols = a.extent_.cols, rows = a.extent_.count() / cols;
        std::vector<std::string> cells(a.extent_.count());
        std::vector<size_t>      width(cols, 0);
        for (size_t k = 0; k < cells.size(); ++k) {
            cells[k]        = show(a.data()[k]);
            width[k % cols] = std::max(width[k % cols], cells[k].size());
        }

        std::string text = "[";
        for (size_t i = 1; i <= rows; ++i) {
            // One space, so that a continued row starts under the bracket.
            if(i > 1) text += "\n ";
            for (size_t j = 1; j <= cols; ++j) {
                if(j > 1) text += ", ";
                const std::string& cell = cells[(i - 1) * cols + (j - 1)];
                text.append(width[j-1] - cell.size(), ' ');
                text += cell;
            }
            if (a.IsTensor() && i % a.extent_.rows == 0 && (i < rows || rows == a.extent_.rows))
                text += ";;";
            else if (i < rows)
                text += ";";
        }
        return text + "]";
    }

    static int toInt(const Matrix<T>& a) {
        return numeric_interface<T>::toInt(a.Scalar(
            a.IsTensor() ? "a tensor is not a single value" : "a matrix is not a single value"));
    }

    // The extent, not only the cells: two values with the same cells in
    // different shapes are two values (C50).
    static void key(const Matrix<T>& a, std::string& out) {
        out += a.extent_.toString();
        out += ':';
        std::for_each(a.data(), a.data() + a.extent_.count(),
                      [&out](const T& value) { numeric_interface<T>::key(value, out); });
    }

    static bool same(const Matrix<T>& a, const Matrix<T>& b) {
        return a.extent_ == b.extent_ &&
               std::equal(a.data(), a.data() + a.extent_.count(), b.data(),
                          [](const T& x, const T& y) { return numeric_interface<T>::same(x, y); });
    }
    static std::size_t hash(const Matrix<T>& a) {
        std::size_t mixed = a.extent_.count();
        std::for_each(a.data(), a.data() + a.extent_.count(), [&mixed](const T& value) {
            mixed = mixed * 31 + numeric_interface<T>::hash(value);
        });
        return mixed;
    }

    static bool exact(const Matrix<T>& a) {
        return std::all_of(a.data(), a.data() + a.extent_.count(),
                           [](const T& value) { return numeric_interface<T>::exact(value); });
    }

    static bool approximated(const Matrix<T>& a) {
        return std::any_of(a.data(), a.data() + a.extent_.count(), [](const T& value) {
            return numeric_interface<T>::approximated(value);
        });
    }

    static Matrix<T> inexact(const Matrix<T>& a, double remainder = 0) {
        Matrix<T> c(a);
        std::transform(c.data(), c.data() + c.extent_.count(), c.data(), [&](const T& value) {
            return numeric_interface<T>::inexact(value, remainder);
        });
        return c;
    }

    static Matrix<T> floor(const Matrix<T>& a) {
        Matrix<T> c(a);
        std::transform(c.data(), c.data() + c.extent_.count(), c.data(),
                       [](const T& value) { return numeric_interface<T>::floor(value); });
        return c;
    }

    // The rows as columns, and no cell conjugated: MATLAB's and Julia's quote
    // is the conjugate transpose, which is the same only for real matrices.
    static Matrix<T> transpose(const Matrix<T>& a) {
        if (a.IsTensor())
            return Sliced(a, Matrix<T>(),
                          [](const Matrix<T>& s, const Matrix<T>&) { return transpose(s); });
        Matrix<T> t(Extent{a.extent_.cols, a.extent_.rows});
        for (size_t i = 1; i <= a.extent_.rows; ++i)
            for (size_t j = 1; j <= a.extent_.cols; ++j) t(j, i) = a(i, j);
        return t;
    }

    // One cell, as a 1x1: everything in this language is a matrix.
    static Matrix<T> cell(const Matrix<T>& a, int i, int j) {
        if (a.IsTensor())
            throw std::runtime_error("a " + a.extent_.Described() +
                                     " takes one index or three, not two");
        return Matrix<T>(a(i, j));
    }

    static Matrix<T> cell(const Matrix<T>& a, int b, int i, int j) {
        if (!a.IsTensor())
            throw std::runtime_error("a " + a.extent_.Described() +
                                     " takes one index or two, not three");
        return Matrix<T>(a.data()[a.Offset(b, i, j)]);
    }

    // One index is a row, a 1xn matrix that keeps its orientation; a
    // tensor's is a slice, as a batch's is a sample.
    static Matrix<T> row(const Matrix<T>& a, int i) {
        if (a.IsTensor()) {
            if (i < 1 || static_cast<size_t>(i) > a.extent_.slices)
                throw std::runtime_error("slice " + std::to_string(i) + " is outside a " +
                                         a.extent_.Described());
            return a.Slice(static_cast<size_t>(i));
        }
        if (i < 1 || static_cast<size_t>(i) > a.extent_.rows)
            throw std::runtime_error("row " + std::to_string(i) + " is outside a " +
                                     a.extent_.toString() + " matrix");
        Matrix<T> row(Extent{1, a.extent_.cols});
        for (size_t j = 1; j <= a.extent_.cols; ++j) row(1, j) = a(i, j);
        return row;
    }

    // A comparison answers one or zero -- there is no truth type, because
    // every value here is a number. Cell by cell was considered and left out:
    // nothing in the language reduces a matrix of ones and zeros to a single
    // truth, so it would invite an idiom it cannot finish.
    friend bool operator==(const Matrix<T>& a, const Matrix<T>& b) {
        return a.extent_ == b.extent_ &&
               std::equal(a.data(), a.data() + a.extent_.count(), b.data());
    }

    static Matrix<T> compare(const Matrix<T>& a, const Matrix<T>& b, Comparison op)
    {
        // Two whole matrices are equal or not, which is one truth; an order
        // cell by cell would be a matrix of them.
        if (op == Comparison::Equal || op == Comparison::NotEqual) {
            for (const Matrix<T>* m : {&a, &b})
                std::for_each(m->data(), m->data() + m->extent_.count(),
                              [](const T& x) { Numeric(x); });
            return Matrix<T>((op == Comparison::Equal) == (a == b) ? numeric_interface<T>::one()
                                                                   : numeric_interface<T>::zero());
        }
        return Matrix<T>(Ordered(Numeric(a.Comparable()), op, Numeric(b.Comparable()))
                             ? numeric_interface<T>::one()
                             : numeric_interface<T>::zero());
    }

    // A guard holds when it is not zero.
    static bool truth(const Matrix<T>& a, const std::string& subject = "a guard") {
        if(!a.IsScalar()) {
            throw NotSingle(subject + " needs single values, not a " + a.extent_.Described(),
                            a.extent_);
        }
        return !(Numeric(a.scalar_, subject) == numeric_interface<T>::zero());
    }

    // A 1x1 matrix -- which every literal and every intermediate scalar is --
    // keeps its cell inline rather than on the heap. A tensor of one cell is
    // not a single value.
    bool     IsScalar() const { return extent_.count() == 1 && !IsTensor(); }
    bool     IsTensor() const { return extent_.slices != 0; }
    T* data() {return IsScalar() ? &scalar_ : cells_.data();}
    const T* data() const {return IsScalar() ? &scalar_ : cells_.data();}

    /* Implementation of the numeric interface */
    static Matrix<T> pow(const Matrix<T>& a, const Matrix<T>& b)
    {
        const T exponent = b.Scalar("a matrix cannot be an exponent");
        if (a.IsTensor())
            throw std::runtime_error("only a matrix has a power, not a " + a.extent_.Described());
        if(a.IsScalar()) {
            return Matrix<T>(numeric_interface<T>::pow(a(1,1), exponent));
        }

        // A matrix power is repeated multiplication, of the inverse when it is
        // negative. There is no root, so the exponent has to be a whole
        // number, and the matrix has to be square to multiply by itself.
        const int whole = numeric_interface<T>::toInt(exponent);
        if (!(exponent == T(whole))) {
            // A value int cannot hold is not therefore a fraction (as C56 was).
            if (numeric_interface<T>::abs(exponent) > 2147483647.0) {
                throw std::runtime_error(
                    "a matrix power must be between -2147483648 and 2147483647, not " +
                    numeric_interface<T>::toString(exponent));
            }
            throw std::runtime_error("a matrix power must be a whole number, not "
                                     + numeric_interface<T>::toString(exponent));
        }
        if(a.extent_.rows != a.extent_.cols) {
            throw std::runtime_error("only a square matrix has a power");
        }

        // By squaring the base -- C23 squared the accumulator -- so that a
        // power costs products by the bit, not by the unit.
        Matrix<T> base = whole < 0 ? Inverse(a) : a;
        unsigned  n = whole < 0 ? 0u - static_cast<unsigned>(whole) : static_cast<unsigned>(whole);
        Matrix<T> r = Identity(a.extent_);
        for (; n != 0; n >>= 1) {
            if (n & 1) r = r * base;
            if (n > 1) base = base * base;
        }
        // Whole as it is, an inexact exponent is still one.
        return numeric_interface<T>::exact(exponent) ? r : inexact(r);
    }

    static Matrix<T> Identity(Extent extent) {
        Matrix<T> r(extent);
        for (size_t i = 1; i <= extent.rows; ++i) r(i, i) = T(1);
        return r;
    }

    // Gauss-Jordan. The largest pivot keeps an inexact inverse accurate, and
    // an exact one is exact whichever pivot it takes.
    static Matrix<T> Inverse(Matrix<T> a) {
        const size_t n = a.extent_.rows;
        Matrix<T>    r = Identity(a.extent_);
        for (size_t col = 1; col <= n; ++col) {
            size_t pivot = col;
            for (size_t row = col + 1; row <= n; ++row) {
                // The largest, for accuracy; any, rather than an exact zero,
                // though its size as a double may be zero too.
                if (numeric_interface<T>::abs(a(row, col)) >
                        numeric_interface<T>::abs(a(pivot, col)) ||
                    (a(pivot, col) == T(0) && !(a(row, col) == T(0))))
                    pivot = row;
            }
            if (a(pivot, col) == T(0)) throw std::runtime_error("a singular matrix has no inverse");
            for (size_t j = 1; j <= n; ++j) {
                std::swap(a(pivot, j), a(col, j));
                std::swap(r(pivot, j), r(col, j));
            }
            const T scale = a(col, col);
            for (size_t j = 1; j <= n; ++j) {
                a(col, j) = a(col, j) / scale;
                r(col, j) = r(col, j) / scale;
            }
            for (size_t row = 1; row <= n; ++row) {
                const T factor = a(row, col);
                if (row == col || factor == T(0)) continue;
                for (size_t j = 1; j <= n; ++j) {
                    a(row, j) = a(row, j) - factor * a(col, j);
                    r(row, j) = r(row, j) - factor * r(col, j);
                }
            }
        }
        return r;
    }

    // 'a^-1*b', without the inverse where both are exact and b is columns a
    // can solve for: a is factored, as Inverse eliminates it and before b is
    // read, as the inverse was, then b's columns are solved for -- a fraction
    // of the work, and the same answer, exactly. Inexact, the two round
    // differently, so anything else is the inverse times b, as written and in
    // that order (DESIGN.md, next in line).
    template <typename Right>
    static Matrix<T> solve(const Matrix<T>& a, Right b) {
        const Matrix<T> minus_one(T(-1));
        if (a.IsScalar() || a.IsTensor() || a.extent_.rows != a.extent_.cols || !exact(a)) {
            const Matrix<T> left = pow(a, minus_one);
            return left * b();
        }
        const Factors   factors = Factor(a);
        const Matrix<T> right   = b();
        if (right.IsScalar() || right.IsTensor() || right.extent_.rows != a.extent_.rows ||
            !exact(right))
            return pow(a, minus_one) * right;
        return Solve(factors, right);
    }

    // The rows in the order the pivots took them, and below the diagonal the
    // multipliers that eliminated each column, above it what remains.
    struct Factors {
        Matrix<T>           lu;
        std::vector<size_t> rows;
    };

    static Factors Factor(Matrix<T> a) {
        const size_t        n = a.extent_.rows;
        std::vector<size_t> rows(n);
        for (size_t i = 0; i < n; ++i) rows[i] = i + 1;
        for (size_t col = 1; col <= n; ++col) {
            size_t pivot = col;
            for (size_t row = col + 1; row <= n; ++row) {
                if (numeric_interface<T>::abs(a(row, col)) >
                        numeric_interface<T>::abs(a(pivot, col)) ||
                    (a(pivot, col) == T(0) && !(a(row, col) == T(0))))
                    pivot = row;
            }
            if (a(pivot, col) == T(0)) throw std::runtime_error("a singular matrix has no inverse");
            for (size_t j = 1; j <= n; ++j) std::swap(a(pivot, j), a(col, j));
            std::swap(rows[pivot - 1], rows[col - 1]);
            for (size_t row = col + 1; row <= n; ++row) {
                const T factor = a(row, col) / a(col, col);
                a(row, col)    = factor;
                if (factor == T(0)) continue;
                for (size_t j = col + 1; j <= n; ++j) a(row, j) = a(row, j) - factor * a(col, j);
            }
        }
        return {std::move(a), std::move(rows)};
    }

    static Matrix<T> Solve(const Factors& factors, const Matrix<T>& b) {
        const Matrix<T>& lu = factors.lu;
        const size_t     n  = lu.extent_.rows;
        Matrix<T>        x(b.extent_);
        for (size_t c = 1; c <= b.extent_.cols; ++c) {
            for (size_t i = 1; i <= n; ++i) {
                T sum = b(factors.rows[i - 1], c);
                for (size_t j = 1; j < i; ++j) sum = sum - lu(i, j) * x(j, c);
                x(i, c) = sum;
            }
            for (size_t i = n; i >= 1; --i) {
                T sum = x(i, c);
                for (size_t j = i + 1; j <= n; ++j) sum = sum - lu(i, j) * x(j, c);
                x(i, c) = sum / lu(i, i);
            }
        }
        return x;
    }

    static auto fact(const Matrix<T>& a)
    {
        return numeric_interface<T>::fact(a.Scalar("a matrix has no factorial"));
    }

    // How far apart two terms of a limit are: their largest difference, cell
    // by cell. Of two shapes there is no distance, where a difference would
    // stretch a single value over the other.
    static auto distance(const Matrix<T>& a, const Matrix<T>& b) {
        if (a.extent_ != b.extent_)
            throw std::runtime_error("its terms are " + b.extent_.toString() + ", then " +
                                     a.extent_.toString());
        decltype(numeric_interface<T>::abs(a.data()[0])) largest = 0;
        for (size_t k = 0; k < a.extent_.count(); ++k) {
            const auto d = numeric_interface<T>::abs(a.data()[k] - b.data()[k]);
            if (std::isnan(d) || d > largest) largest = d;  // NaN, once, stays
        }
        return largest;
    }

    static auto abs(const Matrix<T>& a)
    {
        return numeric_interface<T>::abs(a.Scalar("a matrix has no absolute value"));
    }

    // Matrices of one size as the slices of a tensor, '[a;; b]'.
    static Matrix<T> Stack(const std::vector<Matrix<T>>& slices) {
        const Extent size = slices.front().extent_;
        Matrix<T>    t(Extent{size.rows, size.cols, slices.size()});
        T*           cell = t.data();
        for (const Matrix<T>& slice : slices) {
            if (slice.extent_ != size)
                throw std::runtime_error("the slices of a tensor have one size, not " +
                                         size.toString() + " and " + slice.extent_.toString());
            cell = std::copy_n(slice.data(), size.count(), cell);
        }
        return t;
    }

    Matrix<T> Slice(size_t b) const {
        Matrix<T> slice(Extent{extent_.rows, extent_.cols});
        std::copy_n(data() + (b - 1) * slice.extent_.count(), slice.extent_.count(), slice.data());
        return slice;
    }

    /* Symmetric operators */
    friend Matrix<T> operator*(const Matrix<T>& a, const Matrix<T>& b) {
        if (a.IsTensor() || b.IsTensor())
            return Sliced(a, b, [](const Matrix<T>& x, const Matrix<T>& y) { return x * y; });
        return a.mul(b);
    }

    friend Matrix<T> operator+(const Matrix<T>& a, const Matrix<T>& b)
    {
        return a.BinaryOp(b, std::plus<T>());
    }

    friend Matrix<T> operator-(const Matrix<T>& a, const Matrix<T>& b)
    {
        return a.BinaryOp(b, std::minus<T>());
    }

    friend Matrix<T> operator/(const Matrix<T>& a, const Matrix<T>& b)
    {
        return a.BinaryOp(b, std::divides<T>());
    }

    // Subtracting from zero rather than negating: std::negate on a complex
    // flips the sign of a zero imaginary part, and a -0 there puts the value
    // on the far side of the branch cut, so '(-4)^0.5' answered '-i*2'.
    Matrix<T> operator-() const
    {
        Matrix<T> c(*this);
        const T zero = numeric_interface<T>::zero();
        std::transform(c.data(), c.data() + c.extent_.count(), c.data(),
                       [zero](const T& value) {return zero - value;});
        return c;
    }

    // Whatever meets a tensor meets it slice by slice, a matrix or a single
    // value every slice, so that (T op M)[b] = T[b] op M (DESIGN.md, tensors
    // of rank 3).
    template <typename Func>
    static Matrix<T> Sliced(const Matrix<T>& a, const Matrix<T>& b, Func f) {
        if (a.IsTensor() && b.IsTensor() && a.extent_.slices != b.extent_.slices)
            throw std::runtime_error("a " + a.extent_.Described() + " and a " +
                                     b.extent_.Described() + " have different numbers of slices");
        std::vector<Matrix<T>> slices;
        for (size_t k = 1; k <= std::max(a.extent_.slices, b.extent_.slices); ++k)
            slices.push_back(f(a.IsTensor() ? a.Slice(k) : a, b.IsTensor() ? b.Slice(k) : b));
        return Stack(slices);
    }

private:
    // Signed, so that 'm[0-1,1]' names the row it asked for rather than a
    // number that wrapped.
    size_t Offset(long long b, long long i, long long j) const {
        const size_t slices = IsTensor() ? extent_.slices : 1;
        if (b < 1 || static_cast<unsigned long long>(b) > slices || i < 1 ||
            static_cast<unsigned long long>(i) > extent_.rows || j < 1 ||
            static_cast<unsigned long long>(j) > extent_.cols) {
            throw std::runtime_error((IsTensor() ? "slice " + std::to_string(b) + ", " : "") +
                                     "row " + std::to_string(i) + ", column " + std::to_string(j) +
                                     " is outside a " + extent_.Described());
        }
        return (static_cast<size_t>(b - 1) * extent_.rows + static_cast<size_t>(i - 1)) *
                   extent_.cols +
               static_cast<size_t>(j - 1);
    }

    // NaN is not a number, so a comparison or a truth of it has no answer and
    // is refused, where C would guess (DESIGN.md, a NaN reaches every term
    // that reads it).
    static const T& Numeric(const T& x, const std::string& subject = "a comparison") {
        if (!(x == x))
            throw std::runtime_error(subject + " needs a number, not " +
                                     numeric_interface<T>::toString(x));
        return x;
    }

    // Ordering needs real numbers, as the factorial does.
    static bool Ordered(const T& x, Comparison op, const T& y)
    {
        const auto left  = Real(x);
        const auto right = Real(y);
        switch(op) {
        case Comparison::Less:         return left <  right;
        case Comparison::Greater:      return left >  right;
        case Comparison::LessEqual:    return left <= right;
        case Comparison::GreaterEqual: return left >= right;
        default:                       return false;
        }
    }

    static auto Real(const T& value)
    {
        if(!(numeric_interface<T>::imaginary(value) == 0)) {
            throw std::runtime_error("a comparison needs real numbers, not "
                                     + numeric_interface<T>::toString(value));
        }
        return numeric_interface<T>::real(value);
    }

    const T& Comparable() const
    {
        if(!IsScalar()) {
            throw NotSingle("a comparison needs single values, not a " + extent_.Described(),
                            extent_);
        }
        return scalar_;
    }

    // The single cell of a 1x1 matrix. Most of the numeric interface is only
    // defined there.
    const T& Scalar(const char* message) const {
        if(!IsScalar()) {
            throw NotSingle(message, extent_);
        }
        return scalar_;
    }

    template <typename Func>
    Matrix<T> BinaryOp(const Matrix<T>& other, Func f) const
    {
        // Nearly every value is a single number: say so before anything general.
        if (IsScalar() && other.IsScalar()) return Matrix<T>(f(scalar_, other.scalar_));
        if (IsTensor() || other.IsTensor())
            return Sliced(*this, other, [&f](const Matrix<T>& x, const Matrix<T>& y) {
                return x.BinaryOp(y, f);
            });
        if(extent_ != other.extent_) {
            // A single value stretches to the other side's size, as it does
            // for '*' and inside a literal. The operand order is kept: '1-a'
            // subtracts each cell from one.
            if(IsScalar()) {
                const T value = *data();
                Matrix<T> c(other.extent_);
                std::transform(other.data(), other.data() + other.extent_.count(), c.data(),
                               [&value, &f](const T& cell) {return f(value, cell);});
                return c;
            }
            if(other.IsScalar()) {
                const T value = *other.data();
                Matrix<T> c(extent_);
                std::transform(data(), data() + extent_.count(), c.data(),
                               [&value, &f](const T& cell) {return f(cell, value);});
                return c;
            }
            throw std::runtime_error("these matrices have different sizes");
        }
        Matrix<T> c(extent_);
        std::transform(data(), data() + extent_.count(), other.data(), c.data(), f);
        return c;
    }

    Matrix<T> mul(const Matrix<T>& other) const
    {
        if (IsScalar() && other.IsScalar()) return Matrix<T>(scalar_ * other.scalar_);
        if(IsScalar() || other.IsScalar()) {
            const bool     this_is_scalar = IsScalar();
            const T        scalar = this_is_scalar ? scalar_ : other.scalar_;
            Matrix<T>      c(this_is_scalar ? other : *this);
            std::transform(c.data(), c.data() + c.extent_.count(), c.data(),
                           [&scalar](const T& v) {return v * scalar;});
            return c;
        }
        if(extent_.cols != other.extent_.rows) {
            throw std::runtime_error("a matrix product needs as many columns on the left as rows on the right");
        }
        Matrix<T> c(Extent{extent_.rows, other.extent_.cols});
        for(size_t i = 1; i <= c.extent_.rows; ++i) {
            for(size_t j = 1; j <= c.extent_.cols; ++j) {
                for(size_t k = 1; k <= extent_.cols; ++k) {
                    c(i,j) += (*this)(i,k) * other(k,j);
                }
            }
        }
        return c;
    }

    Extent         extent_;
    T              scalar_ = T();   // the cell of a 1x1 matrix
    std::vector<T> cells_;          // the cells of any other
};

template <typename T>
std::string toString(const Matrix<T>& a) {return Matrix<T>::toString(a);}

template <typename T>
std::ostream& operator<<(std::ostream& stream, const Matrix<T>& matrix)
{
    return stream << Matrix<T>::toString(matrix);
}

#endif // H_MATRIX

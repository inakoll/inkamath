#ifndef H_MATRIX
#define H_MATRIX

#include "inkamath/extent.hpp"
#include "inkamath/numeric_interface.hpp"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

    // Subscripts are 1-based, as they are written.
    T& operator()(long long i, long long j) {return data()[Offset(i, j)];}
    const T& operator()(long long i, long long j) const {return data()[Offset(i, j)];}

    // A matrix prints as the literal that would produce it, with its columns
    // aligned: what is printed can be typed back. A 1x1 is just its value --
    // it is what every scalar answer and every diagnostic quoting one is.
    static std::string toString(const Matrix<T>& a) {
        return toString(a, [](const T& value) { return numeric_interface<T>::toString(value); });
    }

    // `show` prints a cell, so a matrix is laid out alike however it is shown.
    template <typename Show>
    static std::string toString(const Matrix<T>& a, Show show) {
        if(a.IsScalar()) {
            return show(a(1, 1));
        }

        std::vector<std::string> cells(a.extent_.count());
        std::vector<size_t> width(a.extent_.cols, 0);
        for(size_t i = 1; i <= a.extent_.rows; ++i) {
            for(size_t j = 1; j <= a.extent_.cols; ++j) {
                std::string& cell = cells[(i-1)*a.extent_.cols + (j-1)];
                cell              = show(a(i, j));
                width[j-1] = std::max(width[j-1], cell.size());
            }
        }

        std::string text = "[";
        for(size_t i = 1; i <= a.extent_.rows; ++i) {
            // One space, so that a continued row starts under the bracket.
            if(i > 1) text += "\n ";
            for(size_t j = 1; j <= a.extent_.cols; ++j) {
                if(j > 1) text += ", ";
                const std::string& cell = cells[(i-1)*a.extent_.cols + (j-1)];
                text.append(width[j-1] - cell.size(), ' ');
                text += cell;
            }
            if(i < a.extent_.rows) text += ";";
        }
        return text + "]";
    }

    static int toInt(const Matrix<T>& a) {return numeric_interface<T>::toInt(a.Scalar());}

    // The extent, not only the cells: two values with the same cells in
    // different shapes are two values (C50).
    static void key(const Matrix<T>& a, std::string& out) {
        out += a.extent_.toString();
        out += ':';
        std::for_each(a.data(), a.data() + a.extent_.count(),
                      [&out](const T& value) { numeric_interface<T>::key(value, out); });
    }

    static bool exact(const Matrix<T>& a) {
        return std::all_of(a.data(), a.data() + a.extent_.count(),
                           [](const T& value) { return numeric_interface<T>::exact(value); });
    }

    static Matrix<T> inexact(const Matrix<T>& a) {
        Matrix<T> c(a);
        std::transform(c.data(), c.data() + c.extent_.count(), c.data(),
                       [](const T& value) { return numeric_interface<T>::inexact(value); });
        return c;
    }

    // One cell, as a 1x1: everything in this language is a matrix.
    static Matrix<T> cell(const Matrix<T>& a, int i, int j) {return Matrix<T>(a(i, j));}

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
            return Matrix<T>((op == Comparison::Equal) == (a == b) ? numeric_interface<T>::one()
                                                                   : numeric_interface<T>::zero());
        }
        return Matrix<T>(Ordered(a.Comparable(), op, b.Comparable())
                             ? numeric_interface<T>::one()
                             : numeric_interface<T>::zero());
    }

    // A guard holds when it is not zero. NaN is not zero and so holds, while
    // every comparison with it is false -- the one place the convention bites.
    static bool truth(const Matrix<T>& a)
    {
        if(!a.IsScalar()) {
            throw std::runtime_error("a guard needs a single value, not a "
                                     + a.extent_.toString() + " matrix");
        }
        return !(a.scalar_ == numeric_interface<T>::zero());
    }

    // A 1x1 matrix -- which every literal and every intermediate scalar is --
    // keeps its cell inline rather than on the heap.
    bool IsScalar() const {return extent_.count() == 1;}
    T* data() {return IsScalar() ? &scalar_ : cells_.data();}
    const T* data() const {return IsScalar() ? &scalar_ : cells_.data();}

    /* Implementation of the numeric interface */
    static Matrix<T> pow(const Matrix<T>& a, const Matrix<T>& b)
    {
        const T exponent = b.Scalar("a matrix cannot be an exponent");
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

    static auto fact(const Matrix<T>& a)
    {
        return numeric_interface<T>::fact(a.Scalar("a matrix has no factorial"));
    }

    static auto abs(const Matrix<T>& a)
    {
        return numeric_interface<T>::abs(a.Scalar("a matrix has no absolute value"));
    }

    /* Symmetric operators */
    friend Matrix<T> operator*(const Matrix<T>& a, const Matrix<T>& b) {return a.mul(b);}

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

private:
    // Signed, so that 'm[0-1,1]' names the row it asked for rather than a
    // number that wrapped.
    size_t Offset(long long i, long long j) const
    {
        if(i < 1 || static_cast<unsigned long long>(i) > extent_.rows
           || j < 1 || static_cast<unsigned long long>(j) > extent_.cols) {
            throw std::runtime_error("row " + std::to_string(i) + ", column " + std::to_string(j)
                                     + " is outside a " + extent_.toString() + " matrix");
        }
        return static_cast<size_t>(i-1)*extent_.cols + static_cast<size_t>(j-1);
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
            throw std::runtime_error("a comparison needs single values, not a "
                                     + extent_.toString() + " matrix");
        }
        return scalar_;
    }

    // The single cell of a 1x1 matrix. Most of the numeric interface is only
    // defined there.
    const T& Scalar(const char* message =
                    "a matrix is not a single value") const
    {
        if(!IsScalar()) {
            throw std::runtime_error(message);
        }
        return scalar_;
    }

    template <typename Func>
    Matrix<T> BinaryOp(const Matrix<T>& other, Func f) const
    {
        // Nearly every value is a single number: say so before anything general.
        if (IsScalar() && other.IsScalar()) return Matrix<T>(f(scalar_, other.scalar_));
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

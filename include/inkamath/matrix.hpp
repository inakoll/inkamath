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
    static std::string toString(const Matrix<T>& a)
    {
        if(a.IsScalar()) {
            return numeric_interface<T>::toString(a(1,1));
        }

        std::vector<std::string> cells(a.extent_.count());
        std::vector<size_t> width(a.extent_.cols, 0);
        for(size_t i = 1; i <= a.extent_.rows; ++i) {
            for(size_t j = 1; j <= a.extent_.cols; ++j) {
                std::string& cell = cells[(i-1)*a.extent_.cols + (j-1)];
                cell = numeric_interface<T>::toString(a(i,j));
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
    static Matrix<T> compare(const Matrix<T>& a, const Matrix<T>& b, Comparison op)
    {
        const T& x = a.Comparable();
        const T& y = b.Comparable();
        const bool answer = (op == Comparison::Equal)    ? x == y
                          : (op == Comparison::NotEqual) ? !(x == y)
                          : Ordered(x, op, y);
        return Matrix<T>(answer ? numeric_interface<T>::one() : numeric_interface<T>::zero());
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

        // A matrix power is repeated multiplication. There is no inverse and
        // no root here, so the exponent has to be a whole number that is not
        // negative, and the matrix has to be square to multiply by itself.
        const int whole = numeric_interface<T>::toInt(exponent);
        if(numeric_interface<T>::abs(exponent - T(whole)) != 0) {
            throw std::runtime_error("a matrix power must be a whole number, not "
                                     + numeric_interface<T>::toString(exponent));
        }
        if(whole < 0) {
            throw std::runtime_error("a matrix power cannot be negative");
        }
        if(a.extent_.rows != a.extent_.cols) {
            throw std::runtime_error("only a square matrix has a power");
        }

        Matrix<T> r(a.extent_);
        for(size_t i = 1; i <= a.extent_.rows; ++i) {
            r(i,i) = T(1);
        }
        for(int i = 0; i < whole; ++i) {
            r = r*a;
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

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
class Matrix;

template <typename T>
struct numeric_interface_imp_types<Matrix<T>>
{
    typedef typename numeric_interface_imp_types<T>::fact fact;
    typedef typename numeric_interface_imp_types<T>::abs abs;
    typedef typename numeric_interface_imp_types<T>::sqrt sqrt;
};

template <typename T>
class Matrix
{
public:
    typedef T value_type;

    Matrix() : cells_(1) {}
    explicit Matrix(const T& value) : cells_(1, value) {}
    explicit Matrix(Extent extent, const T& value = T())
        : extent_(extent), cells_(extent.count(), value) {}

    Extent Size() const {return extent_;}

    // Subscripts are 1-based, as they are written.
    T& operator()(size_t i, size_t j) {return cells_[Offset(i, j)];}
    const T& operator()(size_t i, size_t j) const {return cells_[Offset(i, j)];}

    static std::string toString(const Matrix<T>& a)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(10);
        for(size_t i = 1; i <= a.extent_.rows; ++i) {
            // Rows are separated, not terminated: a 1x1 matrix is just its
            // value, which is what a diagnostic quoting one needs.
            if(i > 1) oss << "\n";
            for(size_t j = 1; j <= a.extent_.cols; ++j) {
                if(j > 1) oss << " ";
                oss << numeric_interface<T>::toString(a(i,j));
            }
        }
        return oss.str();
    }

    static int toInt(const Matrix<T>& a) {return numeric_interface<T>::toInt(a.Scalar());}

    /* Implementation de Numerical interface */
    static Matrix<T> pow(const Matrix<T>& a, const Matrix<T>& b)
    {
        const T exponent = b.Scalar("Pow is not implemented for Matrix type.");
        if(a.extent_ == Extent{1,1}) {
            return Matrix<T>(numeric_interface<T>::pow(a(1,1), exponent));
        }
        Matrix<T> r = a;
        for(int i = 1; i < numeric_interface<T>::toInt(exponent); ++i) {
            r = r*r;
        }
        return r;
    }

    static typename numeric_interface_imp_types<Matrix<T>>::fact fact(const Matrix<T>& a)
    {
        return numeric_interface<T>::fact(a.Scalar("Fact is not implemented for Matrix type."));
    }

    static typename numeric_interface_imp_types<Matrix<T>>::abs abs(const Matrix<T>& a)
    {
        return numeric_interface<T>::abs(a.Scalar("Abs is not implemented for Matrix type."));
    }

    static typename numeric_interface_imp_types<Matrix<T>>::sqrt sqrt(const Matrix<T>&)
    {
        throw std::runtime_error("Sqrt is not implemented for Matrix type.");
    }

    /* Symetric operators */
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

    Matrix<T> operator-() const
    {
        Matrix<T> c(*this);
        std::transform(c.cells_.begin(), c.cells_.end(), c.cells_.begin(), std::negate<T>());
        return c;
    }

private:
    size_t Offset(size_t i, size_t j) const
    {
        if(i == 0 || i > extent_.rows || j == 0 || j > extent_.cols) {
            throw std::runtime_error("Out of matrix range.\n");
        }
        return (i-1)*extent_.cols + (j-1);
    }

    // The single cell of a 1x1 matrix. Most of the numeric interface is only
    // defined there.
    const T& Scalar(const char* message =
                    "Incompatible dimension in matrix assigmentation. Conversion\n") const
    {
        if(extent_ != Extent{1,1}) {
            throw std::runtime_error(message);
        }
        return cells_.front();
    }

    template <typename Func>
    Matrix<T> BinaryOp(const Matrix<T>& other, Func f) const
    {
        if(extent_ != other.extent_) {
            throw std::runtime_error("Incompatible dimensions in matrix operation.\n");
        }
        Matrix<T> c(extent_);
        std::transform(cells_.begin(), cells_.end(), other.cells_.begin(), c.cells_.begin(), f);
        return c;
    }

    Matrix<T> mul(const Matrix<T>& other) const
    {
        if(extent_ == Extent{1,1} || other.extent_ == Extent{1,1}) {
            const bool     this_is_scalar = extent_ == Extent{1,1};
            const T        scalar = this_is_scalar ? cells_.front() : other.cells_.front();
            Matrix<T>      c(this_is_scalar ? other : *this);
            std::transform(c.cells_.begin(), c.cells_.end(), c.cells_.begin(),
                           [&scalar](const T& v) {return v * scalar;});
            return c;
        }
        if(extent_.cols != other.extent_.rows) {
            throw std::runtime_error("Incompatible dimensions in matrix product.\n");
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
    std::vector<T> cells_;
};

template <typename T>
std::string toString(const Matrix<T>& a) {return Matrix<T>::toString(a);}

template <typename T>
std::ostream& operator<<(std::ostream& stream, const Matrix<T>& matrix)
{
    return stream << Matrix<T>::toString(matrix);
}

#endif // H_MATRIX

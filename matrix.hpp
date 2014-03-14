#ifndef H_MATRIX
#define H_MATRIX

#include <iostream>
#include <vector>
#include <utility> // pair
#include <functional>
#include <algorithm> // max, transform


#include <stdexcept>
#include <cstring> // memcpy


#include "numeric_interface.hpp"

template <typename T>
class Matrix
{
public:
    Matrix(const T& val = 0);
    Matrix(const size_t&, const size_t&, const T& val = 0);
    Matrix(std::vector<std::vector<T> >&);
    Matrix(const Matrix<T>& other)
    {
        m_rows = other.m_rows;
        m_cols = other.m_cols;
        m_mat = new T[m_rows*m_cols];
        memcpy(m_mat,other.m_mat,m_rows*m_cols*(sizeof(T)));
    }

    Matrix<T>& operator=(const Matrix<T>& other)
    {
        Matrix<T> tmp(other);
        std::swap(tmp.m_mat,m_mat);
        m_rows = other.m_rows;
        m_cols = other.m_cols;
        return *this;
    }

    ~Matrix();

    std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(m_rows,m_cols);
    }
    const T& get(size_t i) const
    {
        return m_mat[i];
    }
    T& operator()(const size_t&, const size_t&);
    T  operator()(const size_t&, const size_t&) const;

    static std::string toString(const Matrix<T>& a);
    static int toInt(const Matrix<T>& a);
    static T toT(const Matrix<T>& a);

    template <typename Func>
    Matrix<T> UnaryOp(const Matrix<T>&) const ;

    template <typename Func>
    Matrix<T> BinaryOp(const Matrix<T>&) const ;


    Matrix<T> mul(const Matrix<T>& other) const;

    /* Implementation de Numerical interface */
    static Matrix<T>  pow(const Matrix<T> & a, const Matrix<T> & b)
    {
        if (b.m_rows == 1 && b.m_cols == 1 )
        {
            if(a.m_rows == 1 && a.m_cols == 1)
            {
                return numeric_interface<T>::pow(a(1,1), b(1,1));
            }
            else
            {
                Matrix<T> r = a;
                for(int i = 1; i < numeric_interface<T>::toInt(b(1,1)); ++i)
                {
                    r = r*r;
                }
                return r;
            }
        }
        else
        {
            throw(std::runtime_error("Pow is not implemented for Matrix type."));
        }
    }

    static typename numeric_interface_imp_types<Matrix<T> >::fact fact(const Matrix<T>& a)
    {
        if (a.m_rows == 1 && a.m_cols == 1 )
        {
            return numeric_interface<T>::fact(a(1,1));
        }
        else
        {
            throw(std::runtime_error("Fact is not implemented for Matrix type."));
        }
    }

	static typename numeric_interface_imp_types<Matrix<T> >::abs abs(const Matrix<T>& a)
    {
        if (a.m_rows == 1 && a.m_cols == 1 )
        {
            return numeric_interface<T>::abs(a(1,1));
        }
        else
        {
            throw(std::runtime_error("Abs is not implemented for Matrix type."));
        }
    }

	static typename numeric_interface_imp_types<Matrix<T> >::sqrt sqrt(const Matrix<T>& a)
	{
		throw(std::runtime_error("Sqrt is not implemented for Matrix type."));
	}

    /* Symetric operators */
    friend Matrix<T> operator*(const Matrix<T>& a, const Matrix<T>& b)
    {
        return a.mul(b);
    }

    friend Matrix<T> operator+(const Matrix<T>& a, const Matrix<T>& b)
    {
        return a.BinaryOp<std::plus<value_type> >(b);
    }

    friend Matrix<T> operator-(const Matrix<T>& a, const Matrix<T>& b)
    {
        return a.BinaryOp<std::minus<value_type> >(b);
    }

    Matrix<T> operator-(void) const
    {
        return UnaryOp<std::negate<value_type> >(*this);
    }

    friend Matrix<T> operator/(const Matrix<T>& a, const Matrix<T>& b)
    {
        return a.BinaryOp<std::divides<value_type> >(b);
    }

    typedef T value_type;

protected:
    size_t m_rows;
    size_t m_cols;

    T* m_mat;
};

template <typename T>
struct numeric_interface_imp_types<Matrix<T> >
{
	typedef typename numeric_interface_imp_types<T>::fact fact;
	typedef typename numeric_interface_imp_types<T>::abs abs;
	typedef typename numeric_interface_imp_types<T>::sqrt sqrt;
};

template <typename T>
Matrix<T>::Matrix(const T& val)
{
    m_rows=m_cols=1;
    m_mat = new T[1];
    *m_mat = val;
}

template <typename T>
Matrix<T>::Matrix(const size_t& rows, const size_t& cols, const T& val) : m_rows(rows), m_cols(cols)
{
    m_mat = new T[m_rows*m_cols];
    for (size_t i = 0; i < m_rows*cols; ++i) m_mat[i] = val;
}

template <typename T>
Matrix<T>::Matrix(std::vector<std::vector<T > >& mat) : m_rows(0), m_cols(0), m_mat(0)
{
    m_rows=mat.size();

    for (size_t i = 0; i < m_rows; ++i)
    {
        m_cols = std::max(mat.at(i).size(),m_cols);
    }

	// todo : reimplement this matrix class...
    m_mat = new T[m_rows*m_cols];
	// m_mat migth be leaked within this constructor

    for (size_t i = 0; i < m_rows; ++i)
    {
        for (size_t j = 0; j < m_cols; ++j)
        {
            if (j < mat.at(i).size())
            {
                //if(mat.at(i).at(j) == 0) throw(std::runtime_error("Missing coefficient after '[' or ';'\n")); // rien a faire la
                operator()(i+1,j+1) = mat.at(i).at(j);
            }
            else
            {
                operator()(i+1,j+1) = value_type();
            }
        }
    }
}

template <typename T>
Matrix<T>::~Matrix()
{
    delete[] m_mat;
}

template <typename T>
T & Matrix<T>::operator()(const size_t& i, const size_t& j)
{
    if (i > 0 && i<=m_rows && j > 0 && j<=m_cols)
    {
        return m_mat[(i-1)*m_cols+(j-1)];
    }
    else // Erreur !
    {
        throw(std::runtime_error("Out of matrix range.\n"));
    }
}

template <typename T>
T Matrix<T>::operator()(const size_t& i, const size_t& j) const
{
    if (i > 0 && i<=m_rows && j > 0 && j<=m_cols)
    {
        return m_mat[(i-1)*m_cols+(j-1)];
    }
    else // Erreur !
    {
        throw(std::runtime_error("Out of matrix range.\n"));
    }
}

template <typename T>
Matrix<T> Matrix<T>::mul(const Matrix<T>& other) const
{
    if (m_cols == 1 && m_rows == 1)
    {
        Matrix<T> c(other);
        std::transform(c.m_mat,c.m_mat+c.m_rows*c.m_cols,c.m_mat, std::bind2nd(std::multiplies<value_type>(),operator()(1,1)));
        return c;
    }
    else if (other.m_cols == 1 && other.m_rows ==1)
    {
        Matrix<T> c(*this);
        std::transform(c.m_mat,c.m_mat+c.m_rows*c.m_cols,c.m_mat, std::bind2nd(std::multiplies<value_type>(),other(1,1)));
        return c;
    }
    else if (m_cols != other.m_rows)
    {
        /* exception : dimensions des matrices non compatibles */
        throw(std::runtime_error("Incompatible dimensions in matrix product.\n"));
    }
    else
    {
        Matrix<T> c(m_rows, other.m_cols);
        for (size_t i=1 ; i<=m_rows ; ++i)
        {
            for (size_t j=1 ; j<=other.m_cols ; ++j)
            {
                c(i,j) = 0;
                for (size_t k = 1; k <= m_cols; ++k)
                {
                    c(i,j) += (operator()(i,k))*other(k,j);
                }
            }
        }
        return c;
    }
}

template <typename T>
std::string Matrix<T>::toString(const Matrix<T>& a)
{
	const int MyPrec = 10;
    size_t i,j;
    std::ostringstream oss;
	oss << std::fixed;
	oss << std::setprecision(MyPrec);
    for (i=1 ; i<=a.m_rows ; ++i)
    {
        for (j=1 ; j<a.m_cols ; ++j)
        {
            oss << numeric_interface<T>::toString(a(i,j)) << " ";
        }
        oss << numeric_interface<T>::toString(a(i,j)) << "\n";
    }
    return oss.str();
}

template <typename T>
std::string toString(const Matrix<T>& a) {
    return Matrix<T>::toString(a);
}

template <typename T>
int Matrix<T>::toInt(const Matrix<T>& a)
{
    if (a.m_rows == 1 && a.m_cols == 1)
    {
        return numeric_interface<T>::toInt(*a.m_mat);
    }
    else
    {
        throw(std::runtime_error("Incompatible dimension in matrix assigmentation. Conversion\n"));
    }
}

template <typename T>
T Matrix<T>::toT(const Matrix<T>& a)
{
    if (a.m_rows == 1 && a.m_cols == 1)
    {
        return *a.m_mat;
    }
    else
    {
        throw(std::runtime_error("Incompatible dimension in matrix assigmentation. Conversion\n"));
        return 0;
    }
}

template <typename T> template <typename Func>
Matrix<T> Matrix<T>::UnaryOp(const Matrix<T>& other) const
{
    Func f;
    Matrix<T> c(other.m_rows,other.m_cols);
    for (size_t i = 1; i <= c.m_rows; ++i)
    {
        for (size_t j = 1; j<= c.m_cols; ++j)
        {
            c(i,j) = f(other(i,j));
        }
    }
    return c;
}

template <typename T> template <typename Func>
Matrix<T> Matrix<T>::BinaryOp(const Matrix<T>& other) const
{
    Func f;
    if (m_rows == other.m_rows && m_cols == other.m_cols)
    {
        Matrix<T> c(m_rows,m_cols);
        for (size_t i = 1; i <= c.m_rows; ++i)
        {
            for (size_t j = 1; j<= c.m_cols; ++j)
            {
                c(i,j) = f(operator()(i,j),other(i,j));
            }
        }
        return c;
    }
    else
    {
        throw(std::runtime_error("Incompatible dimensions in matrix operation.\n"));
    }
}


template <typename T>
std::ostream& operator <<(std::ostream& Stream, const Matrix<T>& Obj)
{
	static const int PREC = 10;
	Stream.setf(std::ios::fixed,std::ios::floatfield);
	Stream.precision(PREC);
    return Stream << Matrix<T>::toString(Obj);
}

#endif

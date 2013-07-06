#ifndef H_NTRAITS
#define H_NTRAITS

#include "best_promotion.hpp"

#include <type_traits> // std::is_arithmetic, std::is_same
#include <limits> // std::numeric_limits
#include <cmath> // std::pow
#include <complex> // std::complex
#include <cstdlib> // std::strtod, std::strtol, std::strtoul
#include <string> // std::string
#include <sstream> // std::ostringstream

#define _NUMERIC_INTERFACE_PRECISION 9

template <typename T, bool>
struct numeric_interface_imp;

template <typename T>
struct numeric_interface
: public numeric_interface_imp<T,std::is_arithmetic<T>::value>
{
    static char  complex_char_;
    static char& complex_char() {return complex_char_;}
};

template <typename T>
char numeric_interface<T>::complex_char_ = 'i';

template <typename T>
struct numeric_interface_imp_types
{
	typedef T fact;
	typedef T abs;
	typedef T sqrt;
};



template <typename T, bool>
struct numeric_interface_imp
{
	 static const int precision = _NUMERIC_INTERFACE_PRECISION;
     static T zero() {return T::zero();}
     static T one() {return T::one();}
     static int toInt(const T& a) {return T::toInt(a);}
     static std::string toString(const T& a) {return T::toString(a);}
     static T pow(const T& a, const T& b) {return T::pow(a,b);}

     static typename numeric_interface_imp_types<T>::fact fact(const T& a) {return T::fact(a);}
	 
	 static typename numeric_interface_imp_types<T>::abs abs(const T& a) {return T::abs(a);}
	 
	 static typename numeric_interface_imp_types<T>::sqrt sqrt(const T& a) {return T::sqrt(a);}

     static bool parse(T& num, const char* begin, char* &end)
     {
         return T::parse(num,begin,end);
     }
};

template <typename T>
struct numeric_interface_imp<std::complex<T>,false>
{
	static const int precision = _NUMERIC_INTERFACE_PRECISION;
    static std::complex<T> zero()
    {
        return std::complex<T>(numeric_interface<T>::zero(),
                               numeric_interface<T>::zero());
    }

    static std::complex<T> one()
    {
        return std::complex<T>(numeric_interface<T>::one(),
                               numeric_interface<T>::one());
    }

    static int toInt(const std::complex<T>& a)
    {
        return static_cast<size_t>(numeric_interface<T>::toInt(a.real()));
    }

    static std::string toString(const std::complex<T>& a)
    {
        std::string s;
        T real = a.real();
        T imag = a.imag();
        if(a.real() != 0)
        {
            s = numeric_interface<T>::toString(real);

            if(imag > 0)
            {
                s += "+i";
            }
            else if(imag < 0)
            {
                s += "-i";
                imag = -imag;
            }
            if(imag != 1 && imag != -1 && imag != 0)
            {
                s+= "*" + numeric_interface<T>::toString(imag);
            }
        }
        else if(a.imag() != 0)
        {
            if(imag > 0)
            {
                s += "i";
            }
            else if(imag < 0)
            {
                s += "-i";
                imag = -imag;
            }
            if(imag != 1 && imag != -1 && imag != 0)
            {
                s+= "*" + numeric_interface<T>::toString(imag);
            }
        }
        else
        {
            s = "0";
        }

        return s;
    }

    static std::complex<T> pow(const std::complex<T>& a,
                               const std::complex<T>& b)
    {
        return std::pow(a,b);
    }

    static std::complex<T> pow(const std::complex<T>& a, const T& b)
    {
        return std::pow(a,b);
    }

    static std::complex<T> pow(const T& a, const std::complex<T>& b)
    {
        return std::pow(a,b);
    }

    static std::complex<T> pow(const std::complex<T>& a, int b)
    {
        return std::pow(a,b);
    }

    static typename numeric_interface_imp_types<T>::fact fact(const std::complex<T>& a)
    {
        return numeric_interface<T>::fact(a.real());
    }

	static typename numeric_interface_imp_types<T>::abs abs(const std::complex<T>& a)
	{
		return numeric_interface<T>::sqrt(a.real()*a.real()+a.imag()*a.imag());
	}

    static bool parse(std::complex<T>& num, const char* begin, char* &end)
    {
        T zero = numeric_interface<T>::zero();
        T a = zero;
        bool ret = numeric_interface<T>::parse(a,begin,end);

        if(*end == numeric_interface<std::complex<T> >::complex_char())
        {
            ++end;
            if(!ret)
            {
                a = numeric_interface<T>::one();
                ret = true;
            }
            num = std::complex<T>(zero,a);
        }
        else
        {
            num = std::complex<T>(a,zero);
        }
        return ret;
    }
};

template <typename T>
struct numeric_interface_imp_types<std::complex<T> >
{
	typedef typename numeric_interface_imp_types<T>::fact fact;
	typedef typename numeric_interface_imp_types<T>::abs abs;
	typedef typename numeric_interface_imp_types<T>::sqrt sqrt;
};


template <typename T>
struct numeric_interface_imp<T,true>
{
	static const int precision = _NUMERIC_INTERFACE_PRECISION;
    typedef typename best_promotion<T>::type best_type;

    static T zero() {return 0;}
    static T one() {return 1;}
    static int toInt(const T& a) {return static_cast<int>(a);}
    static std::string toString(const T& a) 
	{
		std::ostringstream oss;
		oss << std::setprecision(precision);
		oss << a;
		return oss.str();
	}
    static T pow(const T& a,const T& b) {return std::pow(a,b);}
    
	static T fact(const T& n)
    {
        T i = 1;
        T n1 = 1;
        while (n >= i)
        {
            n1 *= i;
            ++i;
        }
        return n1;
    }

	static T abs(const T& a) {return std::abs(a);}
	static T sqrt(const T& a) {return std::sqrt(a);}

    /* dummy template parameter */
    /*
    * gcc conforms to standard;
    * standard (14.7.3.2) would not allow to
    * explicitly specialize parse_imp here
    */
    template <bool,typename U>
    struct parse_imp
    {
        static bool parse(T&, const char*, char*)
        {
            return false;
        }
    };

    template <typename U>
    struct parse_imp<true,U>
    {
        static bool parse(T& num, const char* begin, char* &end)
        {
            best_type tmp;
            bool ret = numeric_interface<best_type>::parse(tmp,begin,end);

            num = static_cast<T>(tmp);
            ret = ret &&
                    (tmp >= std::numeric_limits<T>::min()) &&
                    (tmp <= std::numeric_limits<T>::max());
            return ret;
        }
    };

    static bool parse(T& num, const char* begin, char* &end)
    {
        return parse_imp<!std::is_same<T, best_type>::value,int>::
                    parse(num,begin,end);
    }
};

template <>
bool numeric_interface_imp<double,true>::
parse(double& num, const char* begin, char* &end)
{
    num = (std::strtod(begin,&end));
    return (end!=begin);
}

template <>
bool numeric_interface_imp<long,true>::
parse(long& num, const char* begin, char* &end)
{
    num = (std::strtol(begin,&end,10));
    return (end!=begin);
}

template <>
bool numeric_interface_imp<unsigned long,true>::
parse(unsigned long& num, const char* begin, char* &end)
{
    num = (std::strtoul(begin,&end,10));
    return (end!=begin);
}

#undef _NUMERIC_INTERFACE_PRECISION

#endif

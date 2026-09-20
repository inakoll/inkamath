#ifndef H_NTRAITS
#define H_NTRAITS

#include <concepts> // std::convertible_to
#include <type_traits> // std::is_arithmetic
#include <cmath> // std::pow
#include <limits> // std::numeric_limits
#include <complex> // std::complex
#include <cstdlib> // std::strtod
#include <string> // std::string
#include <sstream> // std::ostringstream
#include <iomanip> // std::setprecision

inline constexpr int numeric_interface_precision = 9;

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

template <typename T, bool>
struct numeric_interface_imp
{
	 static const int precision = numeric_interface_precision;
     static T zero() {return T::zero();}
     static T one() {return T::one();}
     static int toInt(const T& a) {return T::toInt(a);}
     static std::string toString(const T& a) {return T::toString(a);}
     static T pow(const T& a, const T& b) {return T::pow(a,b);}

     // Deduced: for a complex or a matrix these narrow to the scalar type.
     static auto fact(const T& a) {return T::fact(a);}
     static auto abs(const T& a) {return T::abs(a);}

     static bool parse(T& num, const char* begin, char* &end)
     {
         return T::parse(num,begin,end);
     }
};

template <typename T>
struct numeric_interface_imp<std::complex<T>,false>
{
	static const int precision = numeric_interface_precision;
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
        return numeric_interface<T>::toInt(a.real());
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
        // exp(b*log(a)) is NaN at a == 0, where IEEE 754 gives 0^0 == 1.
        // The integer overload below computes it by repeated multiplication.
        // The range check is not pedantry: converting a double outside int's
        // range is undefined, and `2^2147483648` answered 0.
        if(b.imag() == 0 && b.real() == std::floor(b.real())
           && b.real() >= static_cast<T>(std::numeric_limits<int>::min())
           && b.real() <= static_cast<T>(std::numeric_limits<int>::max())) {
            return std::pow(a, static_cast<int>(b.real()));
        }
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

    static auto fact(const std::complex<T>& a)
    {
        return numeric_interface<T>::fact(a.real());
    }

    static auto abs(const std::complex<T>& a)
    {
        return std::sqrt(a.real()*a.real()+a.imag()*a.imag());
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
struct numeric_interface_imp<T,true>
{
	static const int precision = numeric_interface_precision;

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

    // Declared, not defined: only the types the interpreter actually parses
    // have an implementation, and a missing one is a link error naming the
    // type rather than a silent 'false'.
    static bool parse(T& num, const char* begin, char* &end);
};

template <>
inline bool numeric_interface_imp<double,true>::
parse(double& num, const char* begin, char* &end)
{
    num = (std::strtod(begin,&end));
    return (end!=begin);
}

// What the interpreter needs of the type it evaluates to. Stating it is the
// point of C9: sqrt was missing for complex and threw for Matrix, and nothing
// said so because nothing asked. Notably absent are zero() and one(), which
// Matrix has never had.
template <typename T>
concept Numeric = requires(const T& a, const T& b) {
    { a + b } -> std::convertible_to<T>;
    { a - b } -> std::convertible_to<T>;
    { a * b } -> std::convertible_to<T>;
    { a / b } -> std::convertible_to<T>;
    { -a } -> std::convertible_to<T>;
    { numeric_interface<T>::pow(a, b) } -> std::convertible_to<T>;
    { T(numeric_interface<T>::fact(a)) } -> std::same_as<T>;
    { numeric_interface<T>::abs(a) > 1.0 } -> std::convertible_to<bool>;
    { numeric_interface<T>::toInt(a) } -> std::convertible_to<int>;
    { numeric_interface<T>::toString(a) } -> std::convertible_to<std::string>;
};

// What the lexer needs of the type it reads numbers into.
template <typename T>
concept Parsable = requires(T& num, const char* begin, char*& end) {
    { numeric_interface<T>::parse(num, begin, end) } -> std::convertible_to<bool>;
};

#endif

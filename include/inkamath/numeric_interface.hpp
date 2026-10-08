#ifndef H_NTRAITS
#define H_NTRAITS

#include <type_traits> // std::is_arithmetic
#include <cmath> // std::pow
#include <limits> // std::numeric_limits
#include <complex> // std::complex
#include <cstdlib> // std::strtod
#include <stdexcept> // std::runtime_error
#include <string> // std::string
#include <sstream> // std::ostringstream
#include <iomanip> // std::setprecision

// The comparisons, here rather than beside the expression node because the
// number types have to answer them.
enum class Comparison {Less, Greater, LessEqual, GreaterEqual, Equal, NotEqual};

inline constexpr int numeric_interface_precision = 9;

// Declaring `parse` is not the same as defining it, and a `requires` clause
// cannot tell them apart. This says which types the lexer can actually read.
template <typename T>
inline constexpr bool numeric_interface_parses = false;

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
     static std::string   toString(const T& a, int digits) { return T::toString(a, digits); }
     static std::string   fraction(const T& a, int digits) { return T::fraction(a, digits); }
     static T pow(const T& a, const T& b) {return T::pow(a,b);}
     static T cell(const T& a, int i, int j) {return T::cell(a,i,j);}
     static T             cell(const T& a, int b, int i, int j) { return T::cell(a, b, i, j); }
     static T             row(const T& a, int i) { return T::row(a, i); }
     static T compare(const T& a, const T& b, Comparison op) {return T::compare(a,b,op);}
     static bool truth(const T& a) {return T::truth(a);}
     static bool truth(const T& a, const std::string& subject) { return T::truth(a, subject); }
     // A comparison asks for these (phase 10), and a class-type number goes
     // through this path, so without them none compiled (DESIGN.md, C65).
     static T real(const T& a) { return T::real(a); }
     static T imaginary(const T& a) { return T::imaginary(a); }
     static T    inexact(const T& a, double remainder = 0) { return T::inexact(a, remainder); }
     static T    transpose(const T& a) { return T::transpose(a); }
     static T    floor(const T& a) { return T::floor(a); }
     template <typename Right>
     static T solve(const T& a, Right b) {
         return T::solve(a, b);
     }
     static bool exact(const T& a) { return T::exact(a); }
     static bool approximated(const T& a) { return T::approximated(a); }
     static T           marked(const T& a) { return T::marked(a); }
     static void key(const T& a, std::string& out) { T::key(a, out); }
     static bool        same(const T& a, const T& b) { return T::same(a, b); }
     static std::size_t hash(const T& a) { return T::hash(a); }

     // Deduced: for a complex or a matrix these narrow to the scalar type.
     static auto fact(const T& a) {return T::fact(a);}
     static auto abs(const T& a) {return T::abs(a);}
     static auto distance(const T& a, const T& b) { return T::distance(a, b); }

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
                               numeric_interface<T>::zero());
    }

    static int toInt(const std::complex<T>& a)
    {
        return numeric_interface<T>::toInt(a.real());
    }

    static T real(const std::complex<T>& a) {return a.real();}
    static T imaginary(const std::complex<T>& a) {return a.imag();}
    static std::complex<T> inexact(const std::complex<T>& a, double = 0) { return a; }
    static bool            exact(const std::complex<T>&) { return false; }
    static bool            approximated(const std::complex<T>&) { return false; }

    static std::string toString(const std::complex<T>& a) {
        return toString(a, [](const T& part) { return numeric_interface<T>::toString(part); });
    }

    // `part` prints a real number; the layout around it is the same whichever.
    template <typename Part>
    static std::string toString(const std::complex<T>& a, Part part) {
        const T real = a.real();
        T       imag = a.imag();

        // A NaN part is present but has no sign, and answers false to every
        // comparison. Asking whether it is zero, rather than how it compares
        // to zero, is what keeps the 'i' from being dropped while its
        // magnitude is still printed (DESIGN.md, C31).
        const bool has_real = !(real == 0);
        const bool has_imag = !(imag == 0);
        if(!has_real && !has_imag)
        {
            return "0";
        }

        std::string s;
        if(has_real)
        {
            s = part(real);
        }
        if(has_imag)
        {
            if(imag < 0)
            {
                s += "-i";
                imag = -imag;
            }
            else
            {
                s += has_real ? "+i" : "i";
            }
            if(!(imag == 1))
            {
                s += "*" + part(imag);
            }
        }
        // Never exact, as i is a double: one mark before the parts, but for
        // two NaNs, no value at all (DESIGN.md, C240).
        return has_imag && !(real != real && imag != imag) ? "~(" + s + ")" : s;
    }

    static std::complex<T> pow(const std::complex<T>& a,
                               const std::complex<T>& b)
    {
        // NaN has no power and is none, where C's pow(1, NaN) and pow(NaN, 0) are 1.
        if (a != a || b != b) return a + b;
        // A real power of a real number is the real one: as a complex power,
        // 2^0.5 came out a bit off, and 2^1024 squared an infinity into a NaN
        // imaginary part.
        if (a.imag() == 0 && b.imag() == 0 && (a.real() >= 0 || b.real() == std::floor(b.real()))) {
            // Rounded correctly, where libm's pow need not be (C153).
            if (b.real() == 0.5 && a.real() > 0) return std::sqrt(a.real());
            return std::pow(a.real(), b.real());
        }
        // The principal root, exactly i of -1 where exp(log(a)/2) leaves
        // cos(pi/2) in the real part.
        if (b == T(0.5)) return std::sqrt(a);
        // A whole power of a complex number is repeated multiplication:
        // exp(b*log(a)) leaves i^2 a rounding error away from -1.
        // The range check is not pedantry: converting a double outside int's
        // range is undefined, and `2^2147483648` answered 0.
        if(b.imag() == 0 && b.real() == std::floor(b.real())
           && b.real() >= static_cast<T>(std::numeric_limits<int>::min())
           && b.real() <= static_cast<T>(std::numeric_limits<int>::max())) {
            return pow(a, static_cast<int>(b.real()));
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

    // Spelled out because std::pow(complex, int) is not standard: libstdc++
    // keeps it as an extension, and elsewhere the int becomes a double and
    // the power goes through exp and log, which gave (0-1)^2 an imaginary
    // part of 1e-16 (DESIGN.md, C61). The multiplications are the ones
    // libstdc++ does, in its order, so no answer on Linux moves.
    static std::complex<T> pow(const std::complex<T>& a, int b)
    {
        const unsigned  n = b < 0 ? 0u - static_cast<unsigned>(b) : static_cast<unsigned>(b);
        std::complex<T> y = Power(a, n);
        if (b >= 0) return y;
        // One over a power that overflowed is 0, where the answer may still be
        // a double: 2^-1074 is the smallest one.
        if (std::isfinite(y.real()) && std::isfinite(y.imag())) return std::complex<T>(1) / y;
        return Power(std::complex<T>(1) / a, n);
    }

    static std::complex<T> Power(std::complex<T> x, unsigned n) {
        std::complex<T> y = n % 2 ? x : std::complex<T>(1);
        while (n >>= 1) {
            x = x * x;
            if (n % 2) y = y * x;
        }
        return y;
    }

    static auto fact(const std::complex<T>& a)
    {
        // The loop below multiplies while 'i <= n', which answers something
        // plausible for every argument it has no business accepting: 5.5
        // truncated to 120, -3 gave the empty product, and the imaginary part
        // never reached it at all.
        if(!(a.imag() == 0)) {
            throw std::runtime_error("a factorial needs a real number, not "
                                     + toString(a));
        }
        const T value = a.real();
        if(value < 0) {
            throw std::runtime_error("a factorial cannot be negative");
        }
        if(value != std::floor(value)) {
            throw std::runtime_error("a factorial needs a whole number, not "
                                     + numeric_interface<T>::toString(value));
        }
        return numeric_interface<T>::fact(value);
    }

    // A real one by its own: squared, one past 2^512 was infinite (C99).
    static auto abs(const std::complex<T>& a)
    {
        if (a.imag() == 0) return std::abs(a.real());
        return std::sqrt(a.real()*a.real()+a.imag()*a.imag());
    }

    static bool parse(std::complex<T>& num, const char* begin, char* &end)
    {
        T zero = numeric_interface<T>::zero();
        T a = zero;
        bool ret = numeric_interface<T>::parse(a,begin,end);

        // The unit alone, as Number::parse reads it.
        if (!ret && *end == numeric_interface<std::complex<T>>::complex_char()) {
            ++end;
            num = std::complex<T>(zero, numeric_interface<T>::one());
            return true;
        } else {
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
    static T real(const T& a) {return a;}
    static T imaginary(const T&) {return 0;}
    static T    inexact(const T& a, double = 0) { return a; }
    static bool exact(const T&) { return false; }
    static bool approximated(const T&) { return false; }
    // Converting a double outside int's range is undefined, and NaN is
    // undefined too; both clamp here, and the caller compares the answer with
    // what it was given to see that it did (DESIGN.md, C56).
    static int toInt(const T& a)
    {
        if(!(a >= static_cast<T>(std::numeric_limits<int>::min()))) return std::numeric_limits<int>::min();
        if(!(a <= static_cast<T>(std::numeric_limits<int>::max()))) return std::numeric_limits<int>::max();
        return static_cast<int>(a);
    }
    static std::string toString(const T& a) {
        // Spelled out because the library decides it: MSVC prints the NaN
        // that 0/0 gives as '-nan(ind)' (DESIGN.md, C62).
        if (std::isnan(a)) return std::signbit(a) ? "-nan" : "nan";
        std::ostringstream oss;
        oss << std::setprecision(precision);
        oss << a;
        return oss.str();
    }
    static T pow(const T& a,const T& b) {return std::pow(a,b);}
    
	static T fact(const T& n)
    {
        // Stopping at the first infinite product, because past it every
        // further term is infinite too -- and because the counter is a T,
        // which at 2^53 stops advancing under '++i' and leaves the loop
        // running for ever (DESIGN.md, C47).
        const T infinite = std::numeric_limits<T>::infinity();
        T i = 1;
        T n1 = 1;
        while (n >= i && n1 < infinite)
        {
            n1 *= i;
            ++i;
        }
        return n >= i ? infinite : n1;
    }

	static T abs(const T& a) {return std::abs(a);}

    // Declared, not defined: only the types the interpreter actually parses
    // have an implementation, and a missing one is a link error naming the
    // type rather than a silent 'false'.
    static bool parse(T& num, const char* begin, char* &end);
};

template <>
inline constexpr bool numeric_interface_parses<double> = true;

template <typename T>
inline constexpr bool numeric_interface_parses<std::complex<T>> = numeric_interface_parses<T>;

template <>
inline bool numeric_interface_imp<double,true>::
parse(double& num, const char* begin, char* &end)
{
    num = (std::strtod(begin,&end));
    return (end!=begin);
}

#endif

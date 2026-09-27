#ifndef INKAMATH_NUMBER_HPP
#define INKAMATH_NUMBER_HPP

#include "inkamath/numeric_interface.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <complex>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>

// A number that knows whether it is exact (MODERNIZATION.md, phase 13): a
// fraction over 64 bits for as long as its reduced parts fit, and a complex
// double once anything inexact has touched it. Every field is always set and
// nothing pads them, because a memo key is a value's bytes and has to carry
// its kind: dbl(1/2) and dbl(~0.5) are different calls.
class Number {
public:
    using inexact_type = std::complex<double>;

    Number() = default;
    Number(int n) : num_(n) {}
    Number(long long n) : num_(n) {
        // Exact values stay within [-top, top], so that negating one is safe.
        if (n < -top) *this = Number(static_cast<double>(n));
    }
    Number(double x) : den_(0), inexact_(x) {}
    Number(inexact_type z) : den_(0), inexact_(z) {}

    bool exact() const { return den_ != 0; }

    inexact_type Inexact() const {
        return exact() ? inexact_type(static_cast<double>(num_) / static_cast<double>(den_))
                       : inexact_;
    }

    friend Number operator+(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) {
            if (const auto sum = Sum(a.num_, a.den_, b.num_, b.den_)) return *sum;
        }
        return Number(a.Inexact() + b.Inexact());
    }

    friend Number operator-(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) {
            if (const auto sum = Sum(a.num_, a.den_, -b.num_, b.den_)) return *sum;
        }
        return Number(a.Inexact() - b.Inexact());
    }

    friend Number operator*(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) {
            if (const auto product = Product(a.num_, a.den_, b.num_, b.den_)) return *product;
        }
        return Number(a.Inexact() * b.Inexact());
    }

    friend Number operator/(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) {
            if (b.num_ == 0) throw std::runtime_error("division by zero");
            const long long num = b.num_ < 0 ? -b.den_ : b.den_;
            const long long den = b.num_ < 0 ? -b.num_ : b.num_;
            if (const auto product = Product(a.num_, a.den_, num, den)) return *product;
        }
        return Number(a.Inexact() / b.Inexact());
    }

    Number& operator+=(const Number& b) { return *this = *this + b; }

    friend bool operator==(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) return a.num_ == b.num_ && a.den_ == b.den_;
        return a.Inexact() == b.Inexact();
    }

    // Each spelled out rather than derived from '<', which would turn a NaN's
    // "false to every comparison" into "true to half of them".
    friend bool operator<(const Number& a, const Number& b) { return Order(a, b, std::less<>()); }
    friend bool operator>(const Number& a, const Number& b) {
        return Order(a, b, std::greater<>());
    }
    friend bool operator<=(const Number& a, const Number& b) {
        return Order(a, b, std::less_equal<>());
    }
    friend bool operator>=(const Number& a, const Number& b) {
        return Order(a, b, std::greater_equal<>());
    }

    /* The numeric interface */
    static Number zero() { return 0; }
    static Number one() { return 1; }
    static Number real(const Number& a) { return a.exact() ? a : Number(a.inexact_.real()); }
    static Number imaginary(const Number& a) {
        return a.exact() ? Number(0) : Number(a.inexact_.imag());
    }
    static Number inexact(const Number& a) { return Number(a.Inexact()); }

    static int toInt(const Number& a) {
        if (!a.exact()) return numeric_interface<inexact_type>::toInt(a.inexact_);
        const long long whole = a.num_ / a.den_;
        if (whole < std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
        if (whole > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
        return static_cast<int>(whole);
    }

    static double abs(const Number& a) {
        if (!a.exact()) return numeric_interface<inexact_type>::abs(a.inexact_);
        return std::abs(static_cast<double>(a.num_)) / static_cast<double>(a.den_);
    }

    // Every number prints in decimal: an exact whole number in full, anything
    // else to `digits` significant digits -- 17 at most for a double, which
    // holds no more -- and with '~' in front unless what is printed is all of
    // the value. A complex number is marked part by part.
    static std::string toString(const Number& a, int digits = numeric_interface_precision) {
        if (!a.exact()) {
            const int shown = std::min(digits, 17);
            return numeric_interface<inexact_type>::toString(
                a.inexact_, [shown](double part) { return Decimal(part, shown); });
        }
        if (a.den_ == 1) return std::to_string(a.num_);
        return Shown(
            Leading(Magnitude(a.num_), static_cast<unsigned long long>(a.den_), digits + 1),
            a.num_ < 0, digits);
    }

    static std::string fraction(const Number& a, int digits) {
        if (!a.exact()) {
            throw std::runtime_error(toString(a, digits) +
                                     " is approximate, so it has no exact fraction");
        }
        return a.den_ == 1 ? std::to_string(a.num_)
                           : std::to_string(a.num_) + "/" + std::to_string(a.den_);
    }

    // A whole power of an exact number is exact; anything else is approached.
    static Number pow(const Number& a, const Number& b) {
        if (a.exact() && b.exact() && b.den_ == 1) {
            if (const auto power = Power(a, b.num_)) return *power;
        }
        return Number(numeric_interface<inexact_type>::pow(a.Inexact(), b.Inexact()));
    }

    static Number fact(const Number& a) {
        if (!a.exact()) return Number(numeric_interface<inexact_type>::fact(a.inexact_));
        if (a.num_ < 0) throw std::runtime_error("a factorial cannot be negative");
        if (a.den_ != 1)
            throw std::runtime_error("a factorial needs a whole number, not " + toString(a));
        long long product = 1;
        for (long long k = 2; k <= a.num_; ++k) {
            if (!Multiply(product, k, product)) {
                return Number(numeric_interface<double>::fact(static_cast<double>(a.num_)));
            }
        }
        return Number(product);
    }

    // strtod says where the literal ends, and the literal is exact as written,
    // point and exponent included: 0.1 is 1/10. One too long for 64 bits is
    // approximated, as is '0x10', which reads only because strtod does.
    static bool parse(Number& num, const char* begin, char*& end) {
        double     value = 0;
        const bool read  = numeric_interface<double>::parse(value, begin, end);
        if (*end == numeric_interface<Number>::complex_char()) {
            ++end;
            num = Number(inexact_type(0, read ? value : 1));
            return true;
        }
        if (!read) return false;
        num = Literal(begin, end).value_or(Number(value));
        return true;
    }

private:
    static constexpr long long top = std::numeric_limits<long long>::max();

    // Already reduced, with the sign on the numerator.
    Number(long long num, long long den, std::nullptr_t) : num_(num), den_(den) {}

    static unsigned long long Magnitude(long long n) {
        return static_cast<unsigned long long>(n < 0 ? -n : n);
    }

    // Portable, because MSVC has neither __int128 nor the GCC overflow
    // builtins. Both keep their result within [-top, top].
    static bool Add(long long a, long long b, long long& sum) {
        if (b > 0 ? a > top - b : a < -top - b) return false;
        sum = a + b;
        return true;
    }

    static bool Multiply(long long a, long long b, long long& product) {
        if (a != 0 && Magnitude(b) > static_cast<unsigned long long>(top) / Magnitude(a))
            return false;
        product = a * b;
        return true;
    }

    // Knuth's addition and multiplication of reduced fractions (TAOCP 4.5.1):
    // the common factors come out before anything is multiplied, so the
    // product checked for overflow is the reduced result itself, and the
    // sum's numerator is larger than the result's by the last gcd at most.
    static std::optional<Number> Sum(long long a, long long b, long long c, long long d) {
        const long long g    = std::gcd(b, d);
        long long       left = 0, right = 0, t = 0, den = 0;
        if (!Multiply(a, d / g, left) || !Multiply(c, b / g, right) || !Add(left, right, t)) {
            return std::nullopt;
        }
        if (t == 0) return Number(0);
        const long long h = std::gcd(t, g);
        if (!Multiply(b / g, d / h, den)) return std::nullopt;
        return Number(t / h, den, nullptr);
    }

    static std::optional<Number> Product(long long a, long long b, long long c, long long d) {
        if (a == 0 || c == 0) return Number(0);
        const long long g   = std::gcd(a, d);
        const long long h   = std::gcd(c, b);
        long long       num = 0, den = 0;
        if (!Multiply(a / g, c / h, num) || !Multiply(b / h, d / g, den)) return std::nullopt;
        return Number(num, den, nullptr);
    }

    // By squaring. A numerator and a denominator with no common factor keep
    // none, and a square is taken only when a later bit needs it, so nothing
    // overflows that the answer would not.
    static std::optional<Number> Power(const Number& base, long long exponent) {
        long long xn = base.num_, xd = base.den_;
        if (exponent < 0) {
            if (xn == 0) throw std::runtime_error("division by zero");
            const long long flip = xn < 0 ? -1 : 1;
            xn                   = flip * base.den_;
            xd                   = flip * base.num_;
        }
        unsigned long long bits = Magnitude(exponent);
        long long          rn = 1, rd = 1;
        while (bits != 0) {
            if (bits & 1) {
                if (!Multiply(rn, xn, rn) || !Multiply(rd, xd, rd)) return std::nullopt;
            }
            bits >>= 1;
            if (bits != 0 && (!Multiply(xn, xn, xn) || !Multiply(xd, xd, xd))) return std::nullopt;
        }
        return Number(rn, rd, nullptr);
    }

    static std::optional<Number> Literal(const char* begin, const char* end) {
        long long   digits = 0, exponent = 0;
        bool        point = false;
        const char* c     = begin;
        for (; c != end && *c != 'e' && *c != 'E'; ++c) {
            if (*c == '.') {
                point = true;
                continue;
            }
            if (*c < '0' || *c > '9') return std::nullopt;
            if (!Multiply(digits, 10, digits) || !Add(digits, *c - '0', digits))
                return std::nullopt;
            if (point) --exponent;
        }
        if (c != end) {
            const bool negative = *++c == '-';
            if (*c == '-' || *c == '+') ++c;
            long long written = 0;
            for (; c != end; ++c) {
                if (!Multiply(written, 10, written) || !Add(written, *c - '0', written))
                    return std::nullopt;
            }
            if (!Add(exponent, negative ? -written : written, exponent)) return std::nullopt;
        }
        if (digits == 0) return Number(0);
        const auto scale = Power(Number(10), exponent);
        if (!scale) return std::nullopt;
        return Product(digits, 1, scale->num_, scale->den_);
    }

    // The leading significant digits of a positive number, the power of ten
    // of the first, and whether anything but zeros follows them.
    struct Digits {
        std::string digits;
        int         exponent = 0;
        bool        rest     = false;
    };

    // At least `count` of them for p/q, by long division.
    static Digits Leading(unsigned long long p, unsigned long long q, int count) {
        Digits lead;
        lead.exponent = -1;
        if (p / q != 0) {
            lead.digits   = std::to_string(p / q);
            lead.exponent = static_cast<int>(lead.digits.size()) - 1;
        }
        unsigned long long r = p % q;
        while (r != 0 && lead.digits.size() < static_cast<std::size_t>(count)) {
            // 10r, a digit at a time: r and q are below 2^63, so no sum overflows.
            unsigned long long next  = 0;
            char               digit = '0';
            for (int k = 0; k < 10; ++k) {
                next += r;
                if (next >= q) {
                    next -= q;
                    ++digit;
                }
            }
            r = next;
            if (lead.digits.empty() && digit == '0') {
                --lead.exponent;
            } else {
                lead.digits += digit;
            }
        }
        lead.rest = r != 0;
        return lead;
    }

    // All of them for a double, which is a fraction over a power of two and
    // has 767 significant digits at most.
    static std::string Decimal(double x, int count) {
        if (!std::isfinite(x)) return numeric_interface<double>::toString(x);
        if (x == 0) return "0";
        char       text[800];
        const auto end =
            std::to_chars(text, text + sizeof text, std::abs(x), std::chars_format::scientific, 766)
                .ptr;
        const std::string written(text, end);
        const std::size_t e = written.find('e');
        Digits            lead;
        lead.digits   = written.substr(0, 1) + written.substr(2, e - 2);
        lead.exponent = std::stoi(written.substr(e + 1));
        return Shown(lead, x < 0, count);
    }

    // Rounded half to even, with an exponent below 1e-4 and from 10^count up.
    static std::string Shown(Digits lead, bool negative, int count) {
        std::string&      d    = lead.digits;
        const std::size_t kept = static_cast<std::size_t>(count);
        d.resize(std::max(d.size(), kept + 1), '0');
        const char next = d[kept];
        const bool rest = lead.rest || d.find_first_not_of('0', kept + 1) != std::string::npos;
        d.resize(kept);
        int e = lead.exponent;
        if (next > '5' || (next == '5' && (rest || (d.back() - '0') % 2 == 1))) {
            std::size_t k = kept;
            while (k > 0 && d[k - 1] == '9') d[--k] = '0';
            if (k == 0) {
                d.insert(0, "1");
                d.pop_back();
                ++e;
            } else {
                ++d[k - 1];
            }
        }
        d.erase(d.find_last_not_of('0') + 1);
        std::string text;
        if (e < -4 || e >= count) {
            const int power = e < 0 ? -e : e;
            text            = d.substr(0, 1) + (d.size() > 1 ? "." + d.substr(1) : "") +
                   (e < 0 ? "e-" : "e+") + (power < 10 ? "0" : "") + std::to_string(power);
        } else if (e < 0) {
            text = "0." + std::string(static_cast<std::size_t>(-e - 1), '0') + d;
        } else {
            const std::size_t whole = static_cast<std::size_t>(e) + 1;
            d.resize(std::max(d.size(), whole), '0');
            text = d.substr(0, whole) + (d.size() > whole ? "." + d.substr(whole) : "");
        }
        const bool exact = next == '0' && !rest;
        return (exact ? "" : "~") + std::string(negative ? "-" : "") + text;
    }

    template <typename Op>
    static bool Order(const Number& a, const Number& b, Op op) {
        if (a.exact() && b.exact()) return op(Compare(a, b), 0);
        return op(a.Inexact().real(), b.Inexact().real());
    }

    // The sign of a/b - c/d, read from a*d against c*b in 128 bits: two
    // exact numbers compare exactly however large their parts.
    static int Compare(const Number& x, const Number& y) {
        const int sx = (x.num_ > 0) - (x.num_ < 0);
        const int sy = (y.num_ > 0) - (y.num_ < 0);
        if (sx != sy) return sx < sy ? -1 : 1;
        if (sx == 0) return 0;
        unsigned long long lh = 0, ll = 0, rh = 0, rl = 0;
        Wide(Magnitude(x.num_), static_cast<unsigned long long>(y.den_), lh, ll);
        Wide(Magnitude(y.num_), static_cast<unsigned long long>(x.den_), rh, rl);
        const int magnitude = lh != rh ? (lh < rh ? -1 : 1) : ll != rl ? (ll < rl ? -1 : 1) : 0;
        return sx * magnitude;
    }

    static void Wide(unsigned long long x, unsigned long long y, unsigned long long& high,
                     unsigned long long& low) {
        const unsigned long long half   = 0xffffffffULL;
        const unsigned long long p00    = (x & half) * (y & half);
        const unsigned long long p01    = (x & half) * (y >> 32);
        const unsigned long long p10    = (x >> 32) * (y & half);
        const unsigned long long p11    = (x >> 32) * (y >> 32);
        const unsigned long long middle = (p00 >> 32) + (p01 & half) + (p10 & half);
        low                             = (middle << 32) | (p00 & half);
        high                            = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
    }

    long long    num_     = 0;
    long long    den_     = 1;  // 0 marks an inexact number
    inexact_type inexact_ = 0;  // the value when inexact, and zero otherwise
};

static_assert(sizeof(Number) == 2 * sizeof(long long) + sizeof(Number::inexact_type),
              "a memo key is a value's bytes, so a Number may have no padding");

template <>
inline constexpr bool numeric_interface_parses<Number> = true;

#endif  // INKAMATH_NUMBER_HPP

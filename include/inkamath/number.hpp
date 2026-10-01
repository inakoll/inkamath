#ifndef INKAMATH_NUMBER_HPP
#define INKAMATH_NUMBER_HPP

#include "inkamath/bignum.hpp"
#include "inkamath/numeric_interface.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <complex>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>

// Keeps a rare path out of a common one, so the common one stays small enough
// to be inlined itself. Spelled per compiler, as each warns at the other's.
#if defined(_MSC_VER) && !defined(__clang__)
#define INKAMATH_NOINLINE __declspec(noinline)
#else
#define INKAMATH_NOINLINE [[gnu::noinline]]
#endif

// A number that knows whether it is exact (DESIGN.md, phase 13): a
// fraction, over 64 bits while its reduced parts fit and over naturals of any
// size until they pass a thousand digits, and a complex double once anything
// inexact has touched it.
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
        if (!exact()) return inexact_;
        const double magnitude =
            big_ ? Nearest(big_->num, big_->den)
                 : Nearest(Magnitude(num_), static_cast<unsigned long long>(den_));
        return Negative() ? -magnitude : magnitude;
    }

    // 64 bits first, and everything else out of line, so that the first stays
    // small enough to inline into a matrix product.
    friend Number operator+(const Number& a, const Number& b) {
        long long num = 0, den = 1;
        if (a.small() && b.small() && Sum(a.num_, a.den_, b.num_, b.den_, num, den))
            return Number(num, den, nullptr);
        return Plus(a, b);
    }

    friend Number operator-(const Number& a, const Number& b) {
        long long num = 0, den = 1;
        if (a.small() && b.small() && Sum(a.num_, a.den_, -b.num_, b.den_, num, den))
            return Number(num, den, nullptr);
        return Minus(a, b);
    }

    friend Number operator*(const Number& a, const Number& b) {
        long long num = 0, den = 1;
        if (a.small() && b.small() && Product(a.num_, a.den_, b.num_, b.den_, num, den))
            return Number(num, den, nullptr);
        return Times(a, b);
    }

    friend Number operator/(const Number& a, const Number& b) {
        long long num = 0, den = 1;
        if (a.small() && b.small() && b.num_ != 0) {
            const long long flip = b.num_ < 0 ? -1 : 1;
            if (Product(a.num_, a.den_, flip * b.den_, flip * b.num_, num, den))
                return Number(num, den, nullptr);
        }
        return Over(a, b);
    }

    Number& operator+=(const Number& b) { return *this = *this + b; }

    // A number is big only when it does not fit in 64 bits, so a small one
    // and a big one are never equal.
    friend bool operator==(const Number& a, const Number& b) {
        if (a.small() && b.small()) return a.num_ == b.num_ && a.den_ == b.den_;
        if (a.exact() && b.exact()) return a.big_ && b.big_ && *a.big_ == *b.big_;
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
    static Number real(const Number& a) {
        return a.exact() ? a : Approximate(a.inexact_.real(), approximated(a));
    }
    static Number imaginary(const Number& a) {
        return a.exact() ? Number(0) : Approximate(a.inexact_.imag(), approximated(a));
    }
    static Number inexact(const Number& a) { return a.exact() ? Number(a.Inexact()) : a; }
    static bool   exact(const Number& a) { return a.exact(); }
    // Inexact because an exact value passed the thousand digits, here or in
    // what it was computed from, which '~' alone cannot tell from 1/3.
    static bool approximated(const Number& a) { return !a.exact() && a.num_ != 0; }

    // A memo key carries the kind: dbl(1/2) and dbl(~0.5) are different calls.
    // The kind also says how many bytes follow, so keys never run together.
    static void key(const Number& a, std::string& out) {
        out += a.big_ ? 'b' : a.exact() ? 'e' : approximated(a) ? 'a' : 'i';
        if (a.big_) {
            Append(a.big_->negative, out);
            for (const Natural* part : {&a.big_->num, &a.big_->den}) {
                Append(part->size(), out);
                for (const Natural::limb l : part->limbs()) Append(l, out);
            }
        } else if (a.exact()) {
            Append(a.num_, out);
            Append(a.den_, out);
        } else {
            Append(a.inexact_.real(), out);
            Append(a.inexact_.imag(), out);
        }
    }

    static int toInt(const Number& a) {
        if (!a.exact()) return numeric_interface<inexact_type>::toInt(a.inexact_);
        if (a.big_) {
            const Natural whole = a.big_->num / a.big_->den;
            const bool    fits  = whole.fits() && whole.low() <= std::numeric_limits<int>::max();
            const int n = fits ? static_cast<int>(whole.low()) : std::numeric_limits<int>::max();
            return a.big_->negative ? (fits ? -n : std::numeric_limits<int>::min()) : n;
        }
        const long long whole = a.num_ / a.den_;
        if (whole < std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
        if (whole > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
        return static_cast<int>(whole);
    }

    static double abs(const Number& a) {
        if (!a.exact()) return numeric_interface<inexact_type>::abs(a.inexact_);
        if (a.big_) return Nearest(a.big_->num, a.big_->den);
        return Nearest(Magnitude(a.num_), static_cast<unsigned long long>(a.den_));
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
        if (a.big_) {
            const Big& b = *a.big_;
            if (b.den == Natural(1)) return (b.negative ? "-" : "") + b.num.Decimal();
            return Shown(Leading(b.num, b.den, digits + 1), b.negative, digits);
        }
        if (a.den_ == 1) return std::to_string(a.num_);
        return Shown(
            Leading(Magnitude(a.num_), static_cast<unsigned long long>(a.den_), digits + 1),
            a.num_ < 0, digits);
    }

    static std::string fraction(const Number& a, int digits) {
        if (!a.exact()) {
            throw std::runtime_error(toString(a, digits) + " was approximated" +
                                     (approximated(a) ? " past a thousand digits" : "") +
                                     ", so it has no exact fraction");
        }
        if (a.big_) {
            const Big&        b    = *a.big_;
            const std::string sign = b.negative ? "-" : "";
            return b.den == Natural(1) ? sign + b.num.Decimal()
                                       : sign + b.num.Decimal() + "/" + b.den.Decimal();
        }
        return a.den_ == 1 ? std::to_string(a.num_)
                           : std::to_string(a.num_) + "/" + std::to_string(a.den_);
    }

    // A whole power of an exact number is exact; anything else is approached.
    static Number pow(const Number& a, const Number& b) {
        const bool whole = b.small() ? b.den_ == 1 : b.big_ && b.big_->den == Natural(1);
        const bool odd =
            b.small() ? (b.num_ & 1) != 0 : b.big_ && (b.big_->num.limbs()[0] & 1) != 0;
        if (a.exact() && whole) {
            if (b.small()) {
                if (a.small()) {
                    if (const auto power = Power(a, b.num_)) return *power;
                }
                if (const auto power = BigPower(a.Ratio(), b.num_)) return *power;
            } else if (a.small() && (a.num_ == 0 || a.num_ == 1 || a.num_ == -1) && a.den_ == 1) {
                // Past 64 bits only these stay within the thousand digits.
                if (a.num_ == 0 && b.big_->negative) throw std::runtime_error("division by zero");
                return a.num_ == -1 && !odd ? Number(1) : a;
            }
        }
        // An exact number to a whole power gets here only past the bound.
        const bool past = (a.exact() && whole) || approximated(a) || approximated(b);
        // A whole exponent past 2^53 has no odd double, so a negative base
        // takes its sign from the exact exponent.
        const inexact_type base = a.Inexact();
        if (whole && base.imag() == 0 && base.real() < 0) {
            const double magnitude = std::pow(-base.real(), b.Inexact().real());
            return Approximate(odd ? -magnitude : magnitude, past);
        }
        return Approximate(numeric_interface<inexact_type>::pow(base, b.Inexact()), past);
    }

    // The largest whole number not above a: exact of an exact number, as
    // division rounds toward zero and a negative quotient needs one less.
    static Number floor(const Number& a) {
        if (!a.exact()) {
            if (!(a.inexact_.imag() == 0))
                throw std::runtime_error("floor needs a real number, not " + toString(a));
            return Approximate(std::floor(a.inexact_.real()), approximated(a));
        }
        if (a.big_) {
            auto [whole, rest] = DivMod(a.big_->num, a.big_->den);
            if (a.big_->negative && !rest.zero()) whole = whole + Natural(1);
            return Normalized(Big{a.big_->negative, whole, Natural(1)});
        }
        const long long whole = a.num_ / a.den_;
        return Number(a.num_ % a.den_ != 0 && a.num_ < 0 ? whole - 1 : whole);
    }

    static Number fact(const Number& a) {
        if (!a.exact())
            return Approximate(numeric_interface<inexact_type>::fact(a.inexact_), approximated(a));
        if (a.Negative()) throw std::runtime_error("a factorial cannot be negative");
        if (a.big_ ? !(a.big_->den == Natural(1)) : a.den_ != 1)
            throw std::runtime_error("a factorial needs a whole number, not " + toString(a));
        const double inexact = numeric_interface<double>::fact(a.Inexact().real());
        if (a.big_) return Approximate(inexact, true);
        long long product = 1, k = 2;
        for (; k <= a.num_; ++k) {
            if (!Multiply(product, k, product)) break;
        }
        if (k > a.num_) return Number(product);
        Natural big(static_cast<Natural::wide>(product));
        for (; k <= a.num_; ++k) {
            big = big * Natural(static_cast<Natural::wide>(k));
            if (Past(big)) return Approximate(inexact, true);
        }
        return Number(Big{false, big, Natural(1)});
    }

    // strtod says where the literal ends, and the literal is exact as written,
    // point and exponent included: 0.1 is 1/10. One past the thousand digits
    // is approximated, as is '0x10', which reads only because strtod does.
    static bool parse(Number& num, const char* begin, char*& end) {
        double     value = 0;
        const bool read  = numeric_interface<double>::parse(value, begin, end);
        if (*end == numeric_interface<Number>::complex_char()) {
            ++end;
            num = Number(inexact_type(0, read ? value : 1));
            return true;
        }
        if (!read) return false;
        num = Literal(begin, end, value).value_or(Number(value));
        return true;
    }

private:
    static constexpr long long top = std::numeric_limits<long long>::max();

    // Reduced, with a denominator that is not zero, and past 64 bits in one
    // part at least: a number that fits is always small.
    struct Big {
        bool    negative = false;
        Natural num, den;

        friend bool operator==(const Big&, const Big&) = default;
    };

    // Shared, because a big number is never changed once made and a matrix
    // copies its cells: every copy is the one number.
    explicit Number(Big b) : big_(std::make_shared<const Big>(std::move(b))) {}

    bool small() const { return exact() && !big_; }
    bool Negative() const { return big_ ? big_->negative : num_ < 0; }

    Big Ratio() const {
        if (big_) return *big_;
        return Big{num_ < 0, Natural(Magnitude(num_)),
                   Natural(static_cast<unsigned long long>(den_))};
    }

    INKAMATH_NOINLINE static Number Plus(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) return BigSum(a.Ratio(), b.Ratio());
        return Approximate(a.Inexact() + b.Inexact(), approximated(a) || approximated(b));
    }

    INKAMATH_NOINLINE static Number Minus(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) {
            Big negated      = b.Ratio();
            negated.negative = !negated.negative;
            return BigSum(a.Ratio(), negated);
        }
        return Approximate(a.Inexact() - b.Inexact(), approximated(a) || approximated(b));
    }

    INKAMATH_NOINLINE static Number Times(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) return BigProduct(a.Ratio(), b.Ratio());
        return Approximate(a.Inexact() * b.Inexact(), approximated(a) || approximated(b));
    }

    INKAMATH_NOINLINE static Number Over(const Number& a, const Number& b) {
        if (a.exact() && b.exact()) {
            if (b.small() && b.num_ == 0) throw std::runtime_error("division by zero");
            Big reciprocal = b.Ratio();
            std::swap(reciprocal.num, reciprocal.den);
            return BigProduct(a.Ratio(), reciprocal);
        }
        return Approximate(a.Inexact() / b.Inexact(), approximated(a) || approximated(b));
    }

    // Exactness ends at a thousand digits in either part.
    static const Natural& Limit() {
        static const Natural limit = Pow10(1000);
        return limit;
    }

    static bool Past(const Natural& n) { return Natural::Compare(n, Limit()) >= 0; }

    // An inexact value, and whether it was approximated past the bound.
    static Number Approximate(inexact_type z, bool past) {
        Number n(z);
        n.num_ = past;
        return n;
    }
    static bool Past(const Big& b) { return Past(b.num) || Past(b.den); }

    static Natural Pow10(std::size_t n) {
        Natural power(1), base(10);
        for (; n != 0; n >>= 1) {
            if (n & 1) power = power * base;
            if (n > 1) base = base * base;
        }
        return power;
    }

    // A reduced fraction as the smallest kind that holds it.
    static Number Normalized(Big b) {
        if (b.num.zero()) return Number(0);
        const auto fits = [](const Natural& n) { return n.fits() && n.low() <= top; };
        if (fits(b.num) && fits(b.den)) {
            const long long num = static_cast<long long>(b.num.low());
            return Number(b.negative ? -num : num, static_cast<long long>(b.den.low()), nullptr);
        }
        if (Past(b)) {
            const double magnitude = Nearest(b.num, b.den);
            return Approximate(b.negative ? -magnitude : magnitude, true);
        }
        return Number(std::move(b));
    }

    // Knuth's methods again, over the naturals.
    static Number BigSum(const Big& x, const Big& y) {
        const Natural g    = Gcd(x.den, y.den);
        const Natural left = x.num * (y.den / g), right = y.num * (x.den / g);
        Big           sum;
        if (x.negative == y.negative) {
            sum = Big{x.negative, left + right, Natural()};
        } else if (Natural::Compare(left, right) >= 0) {
            sum = Big{x.negative, left - right, Natural()};
        } else {
            sum = Big{y.negative, right - left, Natural()};
        }
        if (sum.num.zero()) return Number(0);
        const Natural h = Gcd(sum.num, g);
        sum.num         = sum.num / h;
        sum.den         = (x.den / g) * (y.den / h);
        return Normalized(std::move(sum));
    }

    static Number BigProduct(const Big& x, const Big& y) {
        if (x.num.zero() || y.num.zero()) return Number(0);
        const Natural g = Gcd(x.num, y.den), h = Gcd(y.num, x.den);
        return Normalized(
            Big{x.negative != y.negative, (x.num / g) * (y.num / h), (x.den / h) * (y.den / g)});
    }

    // By squaring, and given up as soon as a part passes the limit: numerator
    // and denominator have no common factor, so neither ever shrinks again.
    static std::optional<Number> BigPower(Big base, long long exponent) {
        if (exponent < 0) {
            if (base.num.zero()) throw std::runtime_error("division by zero");
            std::swap(base.num, base.den);
        }
        const bool negative = base.negative && (exponent % 2 != 0);
        Big        power{negative, Natural(1), Natural(1)};
        for (unsigned long long bits = Magnitude(exponent); bits != 0; bits >>= 1) {
            if (bits & 1) {
                power.num = power.num * base.num;
                power.den = power.den * base.den;
                if (Past(power)) return std::nullopt;
            }
            if (bits > 1) {
                base.num = base.num * base.num;
                base.den = base.den * base.den;
                if (Past(base)) return std::nullopt;
            }
        }
        return Normalized(std::move(power));
    }

    template <typename Part>
    static void Append(Part part, std::string& out) {
        out.append(reinterpret_cast<const char*>(&part), sizeof part);
    }

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

    // The double nearest p/q, by long division in binary: dividing the two as
    // doubles rounds twice once either has more than 53 bits. Fifty-five bits
    // are kept, the last two to round half to even with.
    static double Nearest(unsigned long long p, unsigned long long q) {
        if (p == 0) return 0;
        // Both exact as doubles, so their quotient is rounded once already.
        if (p <= 1ULL << 53 && q <= 1ULL << 53)
            return static_cast<double>(p) / static_cast<double>(q);
        const unsigned long long top55 = 1ULL << 55;
        unsigned long long       m = p / q, r = p % q;
        int                      exponent = 0;
        bool                     sticky   = false;
        for (; m >= top55; ++exponent) {
            sticky = sticky || (m & 1) != 0;
            m >>= 1;
        }
        for (; m < top55 / 2; --exponent) {
            r <<= 1;  // r < q < 2^63
            m <<= 1;
            if (r >= q) {
                r -= q;
                m |= 1;
            }
        }
        sticky          = sticky || r != 0 || (m & 1) != 0;
        const bool half = (m & 2) != 0;
        m >>= 2;
        if (half && (sticky || (m & 1) != 0)) ++m;
        return std::ldexp(static_cast<double>(m), exponent + 2);
    }

    // The same for naturals, whose quotient may be past a double's range or
    // below its normal one: 64 bits of it, then Rounded.
    static double Nearest(const Natural& p, const Natural& q) {
        if (p.zero()) return 0;
        const long shift  = 64 - (static_cast<long>(p.bits()) - static_cast<long>(q.bits()));
        const auto [m, r] = shift >= 0 ? DivMod(p.Shifted(static_cast<std::size_t>(shift)), q)
                                       : DivMod(p, q.Shifted(static_cast<std::size_t>(-shift)));
        // 64 or 65 bits: p/q is within a factor of two of 2^(bits p - bits q).
        const auto&   limbs    = m.limbs();
        std::uint64_t top64    = m.low();
        bool          sticky   = !r.zero();
        int           exponent = static_cast<int>(-shift);
        if (m.bits() > 64) {
            sticky = sticky || (top64 & 1) != 0;
            top64  = (static_cast<std::uint64_t>(limbs[2]) << 63) | (top64 >> 1);
            ++exponent;
        }
        return Rounded(top64, sticky, exponent);
    }

    // (m + something below one if sticky) * 2^exponent, with m's top bit set,
    // rounded half to even to a double -- with fewer bits below the normal
    // range, as a subnormal has.
    static double Rounded(std::uint64_t m, bool sticky, int exponent) {
        const int lead      = exponent + 63;
        const int precision = lead >= -1022 ? 53 : 53 - (-1022 - lead);
        if (precision <= 0) {
            const bool above_half = precision == 0 && (m > (1ULL << 63) || sticky);
            return above_half ? std::ldexp(1.0, -1074) : 0.0;
        }
        const int           drop = 64 - precision;
        std::uint64_t       kept = m >> drop;
        const std::uint64_t rest = m & ((1ULL << drop) - 1), half = 1ULL << (drop - 1);
        if (rest > half || (rest == half && (sticky || (kept & 1) != 0))) ++kept;
        return std::ldexp(static_cast<double>(kept), exponent + drop);
    }

    // Knuth's addition and multiplication of reduced fractions (TAOCP 4.5.1):
    // the common factors come out before anything is multiplied, so the
    // product checked for overflow is the reduced result itself, and the
    // sum's numerator is larger than the result's by the last gcd at most.
    // Whole numbers first: their gcds are all 1, and the 64-bit divisions
    // that find so cost integer matrices seven times the double code.
    // The result's parts, or false when they do not fit: parts rather than a
    // Number, whose copy is not free since it may hold a big one.
    static bool Sum(long long a, long long b, long long c, long long d, long long& num,
                    long long& den) {
        den = 1;
        if (b == 1 && d == 1) return Add(a, c, num);
        const long long g    = std::gcd(b, d);
        long long       left = 0, right = 0, t = 0;
        if (!Multiply(a, d / g, left) || !Multiply(c, b / g, right) || !Add(left, right, t)) {
            return false;
        }
        num = 0;
        if (t == 0) return true;
        const long long h = std::gcd(t, g);
        num               = t / h;
        return Multiply(b / g, d / h, den);
    }

    static bool Product(long long a, long long b, long long c, long long d, long long& num,
                        long long& den) {
        num = 0;
        den = 1;
        if (a == 0 || c == 0) return true;
        if (b == 1 && d == 1) return Multiply(a, c, num);
        const long long g = std::gcd(a, d);
        const long long h = std::gcd(c, b);
        return Multiply(a / g, c / h, num) && Multiply(b / h, d / g, den);
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

    // Past the thousand digits whatever the digits are, so not computed.
    static std::optional<Number> Literal(const char* begin, const char* end, double value) {
        // Most literals are short whole numbers, and 18 digits always fit.
        if (end - begin <= 18 &&
            std::all_of(begin, end, [](char c) { return c >= '0' && c <= '9'; })) {
            long long n = 0;
            for (const char* c = begin; c != end; ++c) n = n * 10 + (*c - '0');
            return Number(n);
        }
        std::string digits;
        long long   exponent = 0;
        const char* c        = begin;
        for (; c != end && *c != 'e' && *c != 'E'; ++c) {
            if (*c == '.') {
                exponent = -1;  // counted below, from the point on
                continue;
            }
            if (*c < '0' || *c > '9') return std::nullopt;
            digits += *c;
            if (exponent < 0) --exponent;
        }
        if (exponent < 0) ++exponent;
        if (digits.find_first_not_of('0') == std::string::npos) return Number(0);
        if (c != end) {
            const bool negative = *++c == '-';
            if (*c == '-' || *c == '+') ++c;
            long long written = 0;
            for (; c != end; ++c) {
                if (!Multiply(written, 10, written) || !Add(written, *c - '0', written))
                    return Approximate(value, true);
            }
            if (!Add(exponent, negative ? -written : written, exponent))
                return Approximate(value, true);
        }
        const Natural   mantissa = Natural::Parse(digits.data(), digits.data() + digits.size());
        const long long length   = static_cast<long long>(digits.size());
        if (exponent > 1000 || exponent < -1000 - length) return Approximate(value, true);
        if (exponent >= 0) {
            return Normalized(
                Big{false, mantissa * Pow10(static_cast<std::size_t>(exponent)), Natural(1)});
        }
        const Natural den = Pow10(static_cast<std::size_t>(-exponent));
        const Natural g   = Gcd(mantissa, den);
        return Normalized(Big{false, mantissa / g, den / g});
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

    // The same for naturals, in one division: p 10^k / q, with k large enough
    // that the quotient has `count` digits -- log10(2) lies between 0.30102
    // and 0.30103.
    static Digits Leading(const Natural& p, const Natural& q, int count) {
        const long k = count + 1 +
                       static_cast<long>(std::ceil(static_cast<double>(q.bits()) * 0.30103 -
                                                   static_cast<double>(p.bits() - 1) * 0.30102));
        const auto [whole, rest] = k >= 0 ? DivMod(p * Pow10(static_cast<std::size_t>(k)), q)
                                          : DivMod(p, q * Pow10(static_cast<std::size_t>(-k)));
        Digits lead;
        lead.digits   = whole.Decimal();
        lead.exponent = static_cast<int>(static_cast<long>(lead.digits.size()) - 1 - k);
        lead.rest     = !rest.zero();
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
                d = "1";
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
        if (x.big_ || y.big_) return Compare(x.Ratio(), y.Ratio());
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

    static int Compare(const Big& x, const Big& y) {
        const int sx = x.num.zero() ? 0 : x.negative ? -1 : 1;
        const int sy = y.num.zero() ? 0 : y.negative ? -1 : 1;
        if (sx != sy) return sx < sy ? -1 : 1;
        return sx * Natural::Compare(x.num * y.den, y.num * x.den);
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

    long long                  num_     = 0;  // when inexact, 1 if approximated
    long long                  den_     = 1;  // 0 marks an inexact number
    inexact_type               inexact_ = 0;  // the value when inexact, and zero otherwise
    std::shared_ptr<const Big> big_;          // the value when exact and past 64 bits
};

template <>
inline constexpr bool numeric_interface_parses<Number> = true;

#endif  // INKAMATH_NUMBER_HPP

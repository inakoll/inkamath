#ifndef INKAMATH_BIGNUM_HPP
#define INKAMATH_BIGNUM_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// A natural number of any size (MODERNIZATION.md, phase 13, step 2), in 32-bit
// limbs because the product of two fits in 64 bits on every compiler CI has.
// Schoolbook multiplication, Knuth's division and Lehmer's gcd: at the
// thousand digits exact numbers stop at, nothing cleverer has paid for itself.
class Natural {
public:
    using limb = std::uint32_t;
    using wide = std::uint64_t;

    Natural() = default;
    explicit Natural(wide n) {
        for (; n != 0; n >>= 32) limbs_.push_back(static_cast<limb>(n));
    }

    // Decimal digits and nothing else.
    static Natural Parse(const char* begin, const char* end) {
        Natural n;
        while (begin != end) {
            const char* chunk = begin + std::min<std::ptrdiff_t>(9, end - begin);
            limb        value = 0, scale = 1;
            for (; begin != chunk; ++begin) {
                value = value * 10 + static_cast<limb>(*begin - '0');
                scale *= 10;
            }
            n.MultiplyAdd(scale, value);
        }
        return n;
    }

    bool        zero() const { return limbs_.empty(); }
    std::size_t size() const { return limbs_.size(); }
    std::size_t bits() const {
        return zero() ? 0
                      : 32 * (size() - 1) + static_cast<std::size_t>(std::bit_width(limbs_.back()));
    }
    const std::vector<limb>& limbs() const { return limbs_; }

    // The value when it fits in 64 bits.
    bool fits() const { return size() <= 2; }
    wide low() const {
        return (size() > 1 ? static_cast<wide>(limbs_[1]) << 32 : 0) | (zero() ? 0 : limbs_[0]);
    }

    friend bool operator==(const Natural&, const Natural&) = default;

    static int Compare(const Natural& a, const Natural& b) {
        if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
        for (std::size_t i = a.size(); i-- > 0;) {
            if (a.limbs_[i] != b.limbs_[i]) return a.limbs_[i] < b.limbs_[i] ? -1 : 1;
        }
        return 0;
    }

    friend Natural operator+(const Natural& a, const Natural& b) {
        const Natural& longer  = a.size() < b.size() ? b : a;
        const Natural& shorter = a.size() < b.size() ? a : b;
        Natural        sum     = longer;
        wide           carry   = 0;
        for (std::size_t i = 0; i < sum.size() && (i < shorter.size() || carry != 0); ++i) {
            carry +=
                static_cast<wide>(sum.limbs_[i]) + (i < shorter.size() ? shorter.limbs_[i] : 0);
            sum.limbs_[i] = static_cast<limb>(carry);
            carry >>= 32;
        }
        if (carry != 0) sum.limbs_.push_back(static_cast<limb>(carry));
        return sum;
    }

    // a - b, which must not be negative.
    friend Natural operator-(const Natural& a, const Natural& b) {
        Natural difference = a;
        wide    borrow     = 0;
        for (std::size_t i = 0; i < difference.size() && (i < b.size() || borrow != 0); ++i) {
            const wide subtrahend = borrow + (i < b.size() ? b.limbs_[i] : 0);
            borrow                = difference.limbs_[i] < subtrahend ? 1 : 0;
            difference.limbs_[i] =
                static_cast<limb>((borrow << 32) + difference.limbs_[i] - subtrahend);
        }
        difference.Trim();
        return difference;
    }

    friend Natural operator*(const Natural& a, const Natural& b) {
        if (a.zero() || b.zero()) return Natural();
        Natural product;
        product.limbs_.assign(a.size() + b.size(), 0);
        for (std::size_t i = 0; i < a.size(); ++i) {
            wide carry = 0;
            for (std::size_t j = 0; j < b.size(); ++j) {
                // At most (2^32-1)^2 + 2(2^32-1), which is 2^64-1.
                carry += static_cast<wide>(a.limbs_[i]) * b.limbs_[j] + product.limbs_[i + j];
                product.limbs_[i + j] = static_cast<limb>(carry);
                carry >>= 32;
            }
            product.limbs_[i + b.size()] = static_cast<limb>(carry);
        }
        product.Trim();
        return product;
    }

    // The quotient and the remainder; b must not be zero.
    friend std::pair<Natural, Natural> DivMod(const Natural& a, const Natural& b) {
        if (Compare(a, b) < 0) return {Natural(), a};
        if (b.size() == 1) return a.DivModLimb(b.limbs_[0]);
        return a.DivModLong(b);
    }

    friend Natural operator/(const Natural& a, const Natural& b) { return DivMod(a, b).first; }
    friend Natural operator%(const Natural& a, const Natural& b) { return DivMod(a, b).second; }

    // Lehmer's: Euclid's steps are taken on the leading 64 bits while they
    // must agree with the full numbers' (Jebelean's condition), then applied to
    // the full numbers at once. Any such matrix has determinant +-1, so the gcd
    // survives even a wrong step; a round that makes no progress divides.
    friend Natural Gcd(Natural a, Natural b) {
        if (Compare(a, b) < 0) std::swap(a, b);
        while (!b.zero()) {
            if (a.fits()) {
                wide x = a.low(), y = b.low();
                while (y != 0) x = std::exchange(y, x % y);
                return Natural(x);
            }
            const std::size_t shift = a.bits() - 64;
            wide              x = a.Window(shift), y = b.Window(shift);
            // x and y stand for |ua*a - ub*b| and |va*a - vb*b|.
            constexpr wide most = 0xffffffffu;
            wide           ua = 1, ub = 0, va = 0, vb = 1;
            while (y != 0) {
                const wide q = x / y, r = x % y;
                if (q > (most - ub) / vb || (va != 0 && q > (most - ua) / va)) break;
                const wide wa = ua + q * va, wb = ub + q * vb;
                if (r < wb || y - r < wb + vb) break;
                ua = std::exchange(va, wa);
                ub = std::exchange(vb, wb);
                x  = std::exchange(y, r);
            }
            Natural na = Combination(ua, a, ub, b), nb = Combination(va, a, vb, b);
            if (Compare(na, nb) < 0) std::swap(na, nb);
            if (Compare(na, a) < 0) {
                a = std::move(na);
                b = std::move(nb);
            } else {
                Natural r = a % b;
                a         = std::move(b);
                b         = std::move(r);
            }
        }
        return a;
    }

    Natural Shifted(std::size_t bits) const {
        if (zero()) return *this;
        Natural           shifted;
        const std::size_t limbs = bits / 32;
        const unsigned    rest  = static_cast<unsigned>(bits % 32);
        shifted.limbs_.assign(limbs, 0);
        limb carry = 0;
        for (const limb l : limbs_) {
            shifted.limbs_.push_back(static_cast<limb>(l << rest) | carry);
            carry = rest == 0 ? 0 : static_cast<limb>(l >> (32 - rest));
        }
        if (carry != 0) shifted.limbs_.push_back(carry);
        return shifted;
    }

    std::string Decimal() const {
        if (zero()) return "0";
        std::vector<limb> chunks;
        for (Natural n = *this; !n.zero();) {
            auto [quotient, remainder] = n.DivModLimb(1000000000);
            chunks.push_back(remainder.zero() ? 0 : remainder.limbs_[0]);
            n = std::move(quotient);
        }
        std::string text = std::to_string(chunks.back());
        for (std::size_t i = chunks.size() - 1; i-- > 0;) {
            const std::string chunk = std::to_string(chunks[i]);
            text += std::string(9 - chunk.size(), '0') + chunk;
        }
        return text;
    }

private:
    void Trim() {
        while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back();
    }

    // The 64 bits from bit `shift` up.
    wide Window(std::size_t shift) const {
        const auto        at = [this](std::size_t i) -> wide { return i < size() ? limbs_[i] : 0; };
        const std::size_t i  = shift / 32;
        const unsigned    rest = static_cast<unsigned>(shift % 32);
        const wide        low  = at(i) | at(i + 1) << 32;
        return rest == 0 ? low : low >> rest | at(i + 2) << (64 - rest);
    }

    // |p*u - q*v|, for factors of at most 32 bits.
    static Natural Combination(wide p, const Natural& u, wide q, const Natural& v) {
        const std::size_t n = std::max(u.size(), v.size());
        Natural           d;
        d.limbs_.resize(n + 1);
        wide pu = 0, qv = 0, borrow = 0;
        for (std::size_t i = 0; i <= n; ++i) {
            pu += p * (i < u.size() ? u.limbs_[i] : 0);
            qv += q * (i < v.size() ? v.limbs_[i] : 0);
            const wide subtrahend = (qv & 0xffffffffu) + borrow;
            borrow                = (pu & 0xffffffffu) < subtrahend ? 1 : 0;
            d.limbs_[i]           = static_cast<limb>(pu - subtrahend);
            pu >>= 32;
            qv >>= 32;
        }
        if (borrow != 0) {  // negative, in two's complement: negate it
            wide carry = 1;
            for (limb& l : d.limbs_) {
                carry += static_cast<limb>(~l);
                l = static_cast<limb>(carry);
                carry >>= 32;
            }
        }
        d.Trim();
        return d;
    }

    void MultiplyAdd(limb factor, limb addend) {
        wide carry = addend;
        for (limb& l : limbs_) {
            carry += static_cast<wide>(l) * factor;
            l = static_cast<limb>(carry);
            carry >>= 32;
        }
        if (carry != 0) limbs_.push_back(static_cast<limb>(carry));
    }

    std::pair<Natural, Natural> DivModLimb(limb divisor) const {
        Natural quotient;
        quotient.limbs_.assign(size(), 0);
        wide remainder = 0;
        for (std::size_t i = size(); i-- > 0;) {
            const wide current = (remainder << 32) | limbs_[i];
            quotient.limbs_[i] = static_cast<limb>(current / divisor);
            remainder          = current % divisor;
        }
        quotient.Trim();
        return {quotient, Natural(remainder)};
    }

    // Knuth's algorithm D (TAOCP 4.3.1), as Hacker's Delight writes it: the
    // divisor is shifted until its top bit is set, which makes each estimated
    // quotient limb at most two too large.
    std::pair<Natural, Natural> DivModLong(const Natural& divisor) const {
        const std::size_t n = divisor.size(), m = size();
        const unsigned    s    = static_cast<unsigned>(std::countl_zero(divisor.limbs_.back()));
        const auto        high = [s](limb upper, limb lower) {
            return s == 0 ? upper : static_cast<limb>((upper << s) | (lower >> (32 - s)));
        };
        std::vector<limb> v(n), u(m + 1);
        for (std::size_t i = n - 1; i > 0; --i)
            v[i] = high(divisor.limbs_[i], divisor.limbs_[i - 1]);
        v[0] = static_cast<limb>(divisor.limbs_[0] << s);
        u[m] = s == 0 ? 0 : static_cast<limb>(limbs_[m - 1] >> (32 - s));
        for (std::size_t i = m - 1; i > 0; --i) u[i] = high(limbs_[i], limbs_[i - 1]);
        u[0] = static_cast<limb>(limbs_[0] << s);

        constexpr wide base = wide(1) << 32;
        Natural        quotient;
        quotient.limbs_.assign(m - n + 1, 0);
        for (std::size_t j = m - n + 1; j-- > 0;) {
            const wide top = (static_cast<wide>(u[j + n]) << 32) | u[j + n - 1];
            wide       q = top / v[n - 1], r = top % v[n - 1];
            while (q >= base || q * v[n - 2] > ((r << 32) | u[j + n - 2])) {
                --q;
                r += v[n - 1];
                if (r >= base) break;
            }
            // Multiply and subtract, with the borrow carried as a signed value.
            std::int64_t borrow = 0, t = 0;
            for (std::size_t i = 0; i < n; ++i) {
                const wide p = q * v[i];
                t            = static_cast<std::int64_t>(u[i + j]) - borrow -
                    static_cast<std::int64_t>(p & 0xffffffffu);
                u[i + j] = static_cast<limb>(t);
                borrow   = static_cast<std::int64_t>(p >> 32) - (t >> 32);
            }
            t        = static_cast<std::int64_t>(u[j + n]) - borrow;
            u[j + n] = static_cast<limb>(t);
            if (t < 0) {  // q was one too large: add the divisor back
                --q;
                wide carry = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    carry += static_cast<wide>(u[i + j]) + v[i];
                    u[i + j] = static_cast<limb>(carry);
                    carry >>= 32;
                }
                u[j + n] = static_cast<limb>(u[j + n] + carry);
            }
            quotient.limbs_[j] = static_cast<limb>(q);
        }
        quotient.Trim();

        Natural remainder;
        remainder.limbs_.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            remainder.limbs_[i] =
                s == 0 ? u[i] : static_cast<limb>((u[i] >> s) | (u[i + 1] << (32 - s)));
        }
        remainder.Trim();
        return {quotient, remainder};
    }

    std::vector<limb> limbs_;  // least significant first, and no leading zero limb
};

#endif  // INKAMATH_BIGNUM_HPP

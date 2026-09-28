#include <doctest/doctest.h>

#include "inkamath/bignum.hpp"

#include <cstdint>
#include <string>
#include <utility>

// Division is the part worth distrusting: Knuth's algorithm D has a branch
// that runs about once in 2^32 quotient digits, so the operands below are
// shaped to reach it rather than drawn uniformly.

TEST_SUITE_BEGIN("bignum");

namespace {

Natural N(const std::string& decimal) {
    return Natural::Parse(decimal.data(), decimal.data() + decimal.size());
}

// A deterministic generator, so a failure reproduces.
struct Random {
    std::uint64_t state = 0x9e3779b97f4a7c15u;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
    // Limbs that are mostly all ones or all zeros make the estimated quotient
    // digit wrong, which is what the correction steps are for.
    Natural natural(std::size_t limbs) {
        Natural n;
        for (std::size_t i = 0; i < limbs; ++i) {
            const std::uint64_t pick = next() % 4;
            const std::uint32_t limb = pick == 0   ? 0xffffffffu
                                       : pick == 1 ? 0u
                                                   : static_cast<std::uint32_t>(next());
            n                        = n.Shifted(32) + Natural(limb);
        }
        return n;
    }
};

}  // namespace

TEST_CASE("decimal text reads back") {
    CHECK(N("0").zero());
    CHECK(N("0").Decimal() == "0");
    CHECK(N("4294967296").Decimal() == "4294967296");
    CHECK(N("1000000000000000000000000000001").Decimal() == "1000000000000000000000000000001");
    CHECK(N("00012").Decimal() == "12");
    CHECK(Natural(1).Shifted(100).Decimal() == "1267650600228229401496703205376");
}

TEST_CASE("sums, differences and products") {
    const Natural a = N("340282366920938463463374607431768211455");  // 2^128 - 1
    CHECK((a + Natural(1)).Decimal() == "340282366920938463463374607431768211456");
    CHECK((a + Natural(1) - Natural(1)) == a);
    CHECK((a - a).zero());
    CHECK((a * a).Decimal() ==
          "115792089237316195423570985008687907852589419931798687112530834793049593217025");
    CHECK((a * Natural()).zero());
    CHECK(Natural::Compare(a, a + Natural(1)) < 0);
    CHECK(Natural::Compare(a + Natural(1), a) > 0);
    CHECK(Natural::Compare(a, a) == 0);
    CHECK(a.bits() == 128);
    CHECK((a + Natural(1)).bits() == 129);
}

TEST_CASE("division is multiplication undone") {
    Random random;
    for (int trial = 0; trial < 20000; ++trial) {
        const Natural a = random.natural(1 + random.next() % 12);
        Natural       b = random.natural(1 + random.next() % 6);
        if (b.zero()) b = Natural(1);
        const auto [q, r] = DivMod(a, b);
        REQUIRE(Natural::Compare(r, b) < 0);
        REQUIRE(q * b + r == a);
    }
}

TEST_CASE("gcds") {
    CHECK(Gcd(N("0"), N("5")).Decimal() == "5");
    CHECK(Gcd(N("5"), N("0")).Decimal() == "5");
    const Natural p = N("170141183460469231731687303715884105727");  // 2^127 - 1, prime
    const Natural q = N("618970019642690137449562111");              // 2^89 - 1, prime
    CHECK(Gcd(p * q, q * q) == q);
    CHECK(Gcd(p, q).Decimal() == "1");
}

// Lehmer's gcd takes its steps from the leading bits; Euclid's own steps are
// the reference it must agree with.
TEST_CASE("gcds agree with Euclid's") {
    const auto euclid = [](Natural a, Natural b) {
        while (!b.zero()) a = std::exchange(b, a % b);
        return a;
    };
    Random random;
    for (int trial = 0; trial < 5000; ++trial) {
        const Natural common = random.natural(1 + random.next() % 4);
        const Natural a      = random.natural(random.next() % 40) * common;
        const Natural b      = random.natural(random.next() % 40) * common;
        REQUIRE(Gcd(a, b) == euclid(a, b));
    }
    // Consecutive Fibonacci numbers make every quotient 1, the longest run.
    Natural f = Natural(1), g = Natural(1);
    for (int n = 0; n < 3000; ++n) f = std::exchange(g, f + g);
    CHECK(Gcd(f, g) == Natural(1));
    CHECK(Gcd(g, f * g) == g);
    // Powers of ten are the denominators of an exact decimal recurrence.
    Natural ten = Natural(1);
    for (int n = 0; n < 1000; ++n) ten = ten * Natural(10);
    const Natural odd = ten / Natural(4) + Natural(1);
    CHECK(Gcd(ten, odd) == euclid(ten, odd));
    CHECK(Gcd(ten * Natural(3), ten / Natural(8)) == ten / Natural(8));
}

TEST_SUITE_END();

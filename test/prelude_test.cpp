#include <doctest/doctest.h>

#include "inkamath/inkamath_prelude.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// The interpreter and a compiled step call the same prelude functions, so
// --check cannot see an error in them: they are held here to mpmath's values,
// data/prelude_reference.txt, written by prelude_reference.py, each within
// the worst error DESIGN.md states for it (C225).

TEST_SUITE_BEGIN("prelude");

namespace {

struct Reference {
    double x, y, d;
};

std::vector<Reference> table(const std::string& name) {
    std::ifstream          in(INKAMATH_TEST_DATA_DIR "/prelude_reference.txt");
    std::vector<Reference> rows;
    std::string            line, section;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string        x, y;
        double             d = 0;
        if (!(s >> x) || x[0] == '#') continue;
        if (!(s >> y >> d))
            section = x;
        else if (section == name)
            rows.push_back({std::strtod(x.c_str(), nullptr), std::strtod(y.c_str(), nullptr), d});
    }
    REQUIRE(!rows.empty());
    return rows;
}

// |got - exact| in units of the spacing of doubles at the exact value, y
// being the double nearest it and d its distance from y in those units.
double units(double got, double y, double d) {
    if (std::isinf(y)) return got == y ? 0 : INFINITY;
    int e = std::max(y == 0 ? -1074 : std::ilogb(y) - 52, -1074);
    if (e > -1074 && d != 0 && (d < 0) == (y > 0) && std::fabs(y) == std::ldexp(1.0, std::ilogb(y)))
        --e;
    return std::fabs(std::ldexp(got - y, -e) - d);
}

template <typename F>
void within(const std::string& name, const std::vector<Reference>& rows, F f, double bound) {
    double worst = 0, at = 0;
    for (const Reference& r : rows) {
        const double error = units(f(r.x), r.y, r.d);
        if (!(error <= worst)) worst = error, at = r.x;
    }
    std::ostringstream where;
    where << std::hexfloat << at;
    INFO(name << " is " << worst << " units off at " << where.str());
    CHECK(worst <= bound);
}

}  // namespace

TEST_CASE("exp") {
    std::vector<Reference> normal, subnormal;
    for (const Reference& r : table("exp"))
        (std::fabs(r.y) < 0x1p-1022 ? subnormal : normal).push_back(r);
    within("exp", normal, inkamath_prelude_exp, 1.31);
    within("exp below 2^-1022", subnormal, inkamath_prelude_exp, 0.91);
}

TEST_CASE("log") {
    within("log", table("log"), inkamath_prelude_log, 2.94);
}
TEST_CASE("tanh") {
    within("tanh", table("tanh"), inkamath_prelude_tanh, 2.97);
}
TEST_CASE("sin") {
    within("sin", table("sin"), inkamath_prelude_sin, 2.44);
}
TEST_CASE("cos") {
    within("cos", table("cos"), inkamath_prelude_cos, 2.45);
}

// The derivatives grad takes: the table's cos and -sin, at the same arguments.
TEST_CASE("sin and cos's parts") {
    std::vector<Reference> cosines = table("cos"), sines = table("sin");
    for (Reference& r : sines) r.y = -r.y, r.d = -r.d;
    within("sin's part", cosines, [](double x) { return inkamath_prelude_sin_dx(x, 1); }, 2.45);
    within("cos's part", sines, [](double x) { return inkamath_prelude_cos_dx(x, 1); }, 2.43);
}

TEST_CASE("ilogb") {
    for (const Reference& r : table("log")) {
        std::ostringstream where;
        where << std::hexfloat << r.x;
        INFO(where.str());
        CHECK(inkamath_prelude_ilogb(r.x) == std::ilogb(r.x));
    }
}

// The sweep that found prelude_reference.py's farthest arguments, against a
// 64-bit long double, as on x86 under GCC or Clang, whose libstdc++ draws
// the same doubles from these seeds. Minutes: --test-case=sweep --no-skip.
TEST_CASE("sweep" * doctest::skip()) {
    using L = long double;
    struct Range {
        const char* name;
        double (*f)(double);
        L (*exact)(L);
        double    lo, hi;
        long long count;
        unsigned  seed;
        bool      exponent;  // lo and hi bound the exponent, the mantissa uniform
    };
    const auto   of_exp  = [](L x) { return std::exp(x); };
    const auto   of_log  = [](L x) { return std::log(x); };
    const auto   of_tanh = [](L x) { return std::tanh(x); };
    const auto   of_sin  = [](L x) { return std::sin(x); };
    const auto   of_cos  = [](L x) { return std::cos(x); };
    const double quarter = 0.7853981633974483, turn = 6.283185307179586;
    const Range  ranges[] = {
        {"exp", inkamath_prelude_exp, of_exp, -708.39, 709.78, 400'000'000, 1, false},
        {"exp", inkamath_prelude_exp, of_exp, -745.13, -708.4, 100'000'000, 1, false},
        {"log", inkamath_prelude_log, of_log, 0.5, 2, 400'000'000, 8, false},
        {"log", inkamath_prelude_log, of_log, -1074, 1024, 200'000'000, 9, true},
        {"tanh", inkamath_prelude_tanh, of_tanh, -1, 1, 400'000'000, 15, false},
        {"tanh", inkamath_prelude_tanh, of_tanh, -20, 20, 200'000'000, 15, false},
        {"sin", inkamath_prelude_sin, of_sin, -quarter, quarter, 200'000'000, 22, false},
        {"sin", inkamath_prelude_sin, of_sin, -turn, turn, 200'000'000, 22, false},
        {"sin", inkamath_prelude_sin, of_sin, -1000, 1000, 200'000'000, 22, false},
        {"sin", inkamath_prelude_sin, of_sin, -0x1p20, 0x1p20, 200'000'000, 22, false},
        {"cos", inkamath_prelude_cos, of_cos, -quarter, quarter, 200'000'000, 29, false},
        {"cos", inkamath_prelude_cos, of_cos, -turn, turn, 200'000'000, 29, false},
        {"cos", inkamath_prelude_cos, of_cos, -1000, 1000, 200'000'000, 29, false},
        {"cos", inkamath_prelude_cos, of_cos, -0x1p20, 0x1p20, 200'000'000, 29, false},
    };
    for (const Range& r : ranges) {
        std::mt19937_64                        g(r.seed);
        std::uniform_real_distribution<double> u(r.lo, r.hi);
        std::vector<std::pair<double, double>> far;
        for (long long n = 0; n < r.count; ++n) {
            double x = u(g);
            if (r.exponent)
                x = std::ldexp(1 + std::ldexp(static_cast<double>(g() >> 12), -52),
                               static_cast<int>(std::floor(x)));
            const L v = r.exact(x);
            int     e = 0;
            (void)std::frexp(v, &e);
            const double error = v == 0 ? 0
                                        : static_cast<double>(std::fabs(
                                              std::ldexp(r.f(x) - v, -std::max(e - 53, -1074))));
            if (far.size() == 8 && error <= far.back().first) continue;
            far.emplace_back(error, x);
            std::sort(far.rbegin(), far.rend());
            if (far.size() > 8) far.pop_back();
        }
        std::printf("%s on [%.17g, %.17g]:", r.name, r.lo, r.hi);
        for (const auto& [error, x] : far) std::printf(" %.4f at %a", error, x);
        std::printf("\n");
    }
}

TEST_SUITE_END();

#include <doctest/doctest.h>

#include "inkamath/matrix.hpp"

#include <complex>
#include <string>

// Matrix is the one container left, and the two defects that lived longest in
// it -- a literal wider than two blocks, and a^n -- survived because the
// transcripts never went past 2x2 (MODERNIZATION.md, C22 and C23).

TEST_SUITE_BEGIN("matrix");

namespace {

using Scalar = std::complex<double>;
using Mat    = Matrix<Scalar>;

Mat make(size_t rows, size_t cols, std::initializer_list<double> cells) {
    Mat m{Extent{rows, cols}};
    size_t k = 0;
    for (double cell : cells) {
        m(k / cols + 1, k % cols + 1) = Scalar(cell);
        ++k;
    }
    return m;
}

std::string shown(const Mat& m) { return Mat::toString(m); }

}  // namespace

TEST_CASE("extent and construction") {
    CHECK(Mat().Size() == Extent{1, 1});
    CHECK(Mat(Scalar(7)).Size() == Extent{1, 1});
    CHECK(Mat(Extent{2, 3}).Size() == Extent{2, 3});

    // A default-constructed cell is zero, which mul relies on for its
    // accumulator.
    CHECK(shown(Mat(Extent{2, 2})) == "[0, 0;\n 0, 0]");
    CHECK(shown(Mat(Extent{2, 2}, Scalar(4))) == "[4, 4;\n 4, 4]");
}

TEST_CASE("a 1x1 matrix keeps its cell inline") {
    // The storage split is invisible from outside, including across every
    // copy and assignment that crosses it.
    Mat scalar(Scalar(3));
    Mat block = make(2, 2, {1, 2, 3, 4});

    Mat copy = scalar;
    CHECK(shown(copy) == "3");

    copy = block;
    CHECK(copy.Size() == Extent{2, 2});
    CHECK(shown(copy) == "[1, 2;\n 3, 4]");

    copy = scalar;
    CHECK(copy.Size() == Extent{1, 1});
    CHECK(shown(copy) == "3");

    Mat moved = std::move(block);
    CHECK(shown(moved) == "[1, 2;\n 3, 4]");
}

TEST_CASE("subscripts are 1-based and checked") {
    Mat m = make(2, 3, {1, 2, 3, 4, 5, 6});
    CHECK(m(1, 1) == Scalar(1));
    CHECK(m(2, 3) == Scalar(6));

    CHECK_THROWS_AS(m(0, 1), std::runtime_error);
    CHECK_THROWS_AS(m(1, 0), std::runtime_error);
    CHECK_THROWS_AS(m(3, 1), std::runtime_error);
    CHECK_THROWS_AS(m(1, 4), std::runtime_error);
}

TEST_CASE("rows are separated, not terminated") {
    CHECK(shown(Mat(Scalar(3))) == "3");
    CHECK(shown(make(1, 3, {1, 2, 3})) == "[1, 2, 3]");
    CHECK(shown(make(3, 1, {1, 2, 3})) == "[1;\n 2;\n 3]");
    CHECK(shown(make(2, 2, {1, 2, 3, 4})) == "[1, 2;\n 3, 4]");
}

TEST_CASE("element-wise operators need matching extents") {
    Mat a = make(2, 2, {1, 2, 3, 4});
    Mat b = make(2, 2, {10, 20, 30, 40});

    CHECK(shown(a + b) == "[11, 22;\n 33, 44]");
    CHECK(shown(b - a) == "[ 9, 18;\n 27, 36]");
    CHECK(shown(b / a) == "[10, 10;\n 10, 10]");
    CHECK(shown(-a) == "[-1, -2;\n -3, -4]");

    Mat wide = make(2, 3, {1, 2, 3, 4, 5, 6});
    CHECK_THROWS_AS(a + wide, std::runtime_error);
    CHECK_THROWS_AS(a - wide, std::runtime_error);
    CHECK_THROWS_AS(a / wide, std::runtime_error);
}

TEST_CASE("multiplication") {
    Mat a = make(2, 2, {1, 2, 3, 4});

    // A 1x1 operand multiplies every cell, from either side.
    CHECK(shown(Mat(Scalar(2)) * a) == "[2, 4;\n 6, 8]");
    CHECK(shown(a * Mat(Scalar(2))) == "[2, 4;\n 6, 8]");

    CHECK(shown(a * a) == "[ 7, 10;\n 15, 22]");
    CHECK(shown(make(1, 2, {1, 2}) * make(2, 1, {3, 4})) == "11");
    CHECK(shown(make(2, 1, {1, 2}) * make(1, 2, {3, 4})) == "[3, 4;\n 6, 8]");

    // Inner dimensions must agree.
    CHECK_THROWS_AS(make(1, 2, {1, 2}) * make(1, 2, {3, 4}), std::runtime_error);
}

TEST_CASE("powers are repeated multiplication") {
    // [1 1; 0 1] to the n is [1 n; 0 1], which names the exponent in the
    // answer: squaring the accumulator instead gave a^(2^(n-1)).
    Mat shift = make(2, 2, {1, 1, 0, 1});
    CHECK(shown(Mat::pow(shift, Mat(Scalar(0)))) == "[1, 0;\n 0, 1]");
    CHECK(shown(Mat::pow(shift, Mat(Scalar(1)))) == "[1, 1;\n 0, 1]");
    CHECK(shown(Mat::pow(shift, Mat(Scalar(2)))) == "[1, 2;\n 0, 1]");
    CHECK(shown(Mat::pow(shift, Mat(Scalar(3)))) == "[1, 3;\n 0, 1]");
    CHECK(shown(Mat::pow(shift, Mat(Scalar(7)))) == "[1, 7;\n 0, 1]");

    Mat a = make(2, 2, {1, 2, 3, 4});
    CHECK(shown(Mat::pow(a, Mat(Scalar(3)))) == shown(a * a * a));

    // No inverse, no root, and nothing but a square matrix has a power.
    CHECK_THROWS_AS(Mat::pow(a, Mat(Scalar(0.5))), std::runtime_error);
    CHECK_THROWS_AS(Mat::pow(a, Mat(Scalar(-1))), std::runtime_error);
    CHECK_THROWS_AS(Mat::pow(make(2, 3, {1, 2, 3, 4, 5, 6}), Mat(Scalar(2))),
                    std::runtime_error);
    CHECK_THROWS_AS(Mat::pow(a, make(1, 2, {1, 2})), std::runtime_error);

    // A 1x1 base is the scalar power, including the 0^0 that C5 fixed.
    CHECK(shown(Mat::pow(Mat(Scalar(0)), Mat(Scalar(0)))) == "1");
    CHECK(shown(Mat::pow(Mat(Scalar(2)), Mat(Scalar(10)))) == "1024");
}

TEST_CASE("the numeric interface is defined only where it means something") {
    Mat a = make(2, 2, {1, 2, 3, 4});

    CHECK(Mat::toInt(Mat(Scalar(42))) == 42);
    CHECK_THROWS_AS(Mat::toInt(a), std::runtime_error);
    CHECK_THROWS_AS(Mat::fact(a), std::runtime_error);
    CHECK_THROWS_AS(Mat::abs(a), std::runtime_error);

    CHECK(Mat::fact(Mat(Scalar(5))) == doctest::Approx(120.0));
    CHECK(Mat::abs(Mat(Scalar(-3))) == doctest::Approx(3.0));
}

TEST_SUITE_END();

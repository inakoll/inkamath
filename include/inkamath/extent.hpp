#ifndef H_EXTENT
#define H_EXTENT

#include <cstddef>

// The dimensions of a matrix, or of the matrix an expression evaluates to.
// One type rather than a pair, so that rows and columns cannot be swapped by
// a std::tie in the wrong order.
struct Extent
{
    size_t rows = 1;
    size_t cols = 1;

    size_t count() const {return rows * cols;}

    friend bool operator==(const Extent&, const Extent&) = default;
};

#endif // H_EXTENT

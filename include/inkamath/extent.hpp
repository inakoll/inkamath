#ifndef H_EXTENT
#define H_EXTENT

#include <cstddef>
#include <string>

// The dimensions of a matrix, or of the matrix an expression evaluates to.
// One type rather than a pair, so that rows and columns cannot be swapped by
// a std::tie in the wrong order.
struct Extent
{
    size_t rows = 1;
    size_t cols = 1;

    size_t count() const {return rows * cols;}

    // As the diagnostics write it: '2x3'.
    std::string toString() const {return std::to_string(rows) + "x" + std::to_string(cols);}

    friend bool operator==(const Extent&, const Extent&) = default;
};

#endif // H_EXTENT

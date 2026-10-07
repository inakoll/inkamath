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
    // A tensor's matrices, its slices; none for a matrix, which a tensor of
    // one slice is not (DESIGN.md, tensors of rank 3).
    size_t slices = 0;

    size_t count() const { return rows * cols * (slices ? slices : 1); }

    // As the diagnostics write it: '2x3', or '2x2x3' with its slices first.
    std::string toString() const {
        return (slices ? std::to_string(slices) + "x" : "") + std::to_string(rows) + "x" +
               std::to_string(cols);
    }
    std::string Described() const { return toString() + (slices ? " tensor" : " matrix"); }
    // 'a single value', or 'a 2x3 matrix'.
    std::string Called() const {
        return count() == 1 && !slices ? "a single value" : "a " + Described();
    }

    friend bool operator==(const Extent&, const Extent&) = default;
};

#endif // H_EXTENT

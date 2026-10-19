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
    bool   Single() const { return count() == 1 && !slices; }

    // What arithmetic cell by cell stretches two operands to, and a product.
    static Extent Wider(Extent a, Extent b) {
        if (a.slices != b.slices) return a.slices ? a : b;
        return a.Single() ? b : a;
    }
    static Extent Product(Extent a, Extent b) {
        if (a.Single()) return b;
        if (b.Single()) return a;
        return {a.rows, b.cols, a.slices > b.slices ? a.slices : b.slices};
    }

    // As the diagnostics write it: '2x3', or '2x2x3' with its slices first.
    std::string toString() const {
        return (slices ? std::to_string(slices) + "x" : "") + std::to_string(rows) + "x" +
               std::to_string(cols);
    }
    std::string Described() const { return toString() + (slices ? " tensor" : " matrix"); }
    // Why arithmetic cell by cell refuses two sizes, empty where one stretches
    // over the other: a single value over anything, a matrix over every slice.
    static std::string Unmet(Extent a, Extent b) {
        if ((a.rows == b.rows && a.cols == b.cols) || a.rows * a.cols == 1 || b.rows * b.cols == 1)
            return "";
        return "a " + a.Described() + " and a " + b.Described() +
               " have different sizes; write it by its cells, as " +
               (a.slices || b.slices ? "T[b,i,j]" : "A[i,j]") + " + c[1,j]";
    }
    std::string Called() const {
        return count() == 1 && !slices ? "a single value" : "a " + Described();
    }

    friend bool operator==(const Extent&, const Extent&) = default;
};

#endif // H_EXTENT

#ifndef INKAMATH_CONVERGENCE_HPP
#define INKAMATH_CONVERGENCE_HPP

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "inkamath/numeric_interface.hpp"

// When a run of values has reached its limit. One rule for the language: 'lim'
// walks the terms of a sequence with it, and a series without an upper bound
// its partial sums.
template <typename T>
class Convergence {
public:
    // Since phase 9 this cap is a judgement about how long to keep trying and
    // not a technical limit: 'lim' walks the terms upward, so each one finds
    // its predecessor memoised, and a hundred thousand of them cost 250ms and
    // no depth. Raising it was measured and rejected -- MODERNIZATION.md, C36.
    static constexpr size_t max_terms = 100;
    static constexpr double tolerance = 1E-10;

    // 'what' names the values when the reason they cannot be compared -- a
    // matrix has no absolute value, two terms have different sizes -- would
    // otherwise read as an internal error.
    explicit Convergence(std::string what) : what_(std::move(what)) {}

    // The next value; true when it is the limit. The first is never: one value
    // has nothing to be compared with.
    [[nodiscard]] bool Next(const T& value) {
        if (started_) {
            const Step step = [&]() {
                try {
                    return numeric_interface<T>::abs(value - previous_);
                } catch (const std::exception& reason) {
                    throw std::runtime_error(what_ + " has no limit: " + reason.what());
                }
            }();
            // '<=' and not '!(> tolerance)': a difference that is NaN
            // answers false to both, and must count as not converged.
            if (step <= tolerance && stepped_ && TailUnder(step, previous_step_)) return true;
            previous_step_ = step;
            stepped_       = true;
        }
        previous_ = value;
        started_  = true;
        return false;
    }

private:
    using Step = decltype(numeric_interface<T>::abs(std::declval<const T&>()));

    // A small step is not a small remainder. If the steps shrink by a factor
    // r each term, what is left of the series is about step*r/(1-r); for
    // 1/n^2, where r approaches 1, that is 1/n -- five orders of magnitude
    // above the step that would otherwise have been called convergence.
    // One step is no evidence at all, which is why 'stepped_' is required.
    static bool TailUnder(Step step, Step previous_step) {
        if (!(previous_step > 0)) return true;
        const Step ratio = step / previous_step;
        if (!(ratio < 1)) return false;
        return step * ratio / (1 - ratio) <= tolerance;
    }

    std::string what_;
    T           previous_{};
    Step        previous_step_{};
    bool        started_ = false;
    bool        stepped_ = false;
};

#endif  // INKAMATH_CONVERGENCE_HPP

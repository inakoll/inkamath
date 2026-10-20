#ifndef INKAMATH_CONVERGENCE_HPP
#define INKAMATH_CONVERGENCE_HPP

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "inkamath/numeric_interface.hpp"

// Nesting too deep, told apart from other failures because a recurrence can
// recover from it by filling its terms from the base up.
struct DepthExceeded : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// A walk that ran out, its values numbers that never settled, which a guard
// reading it takes for one that does not hold (DESIGN.md, a clause for a
// limit that does not converge); 'past' if its last was approximated (C68).
struct Unconverged : std::runtime_error {
    Unconverged(const std::string& what, bool approximated)
        : std::runtime_error(what), past(approximated) {}
    bool past;
};
// A term no clause gives, which an optional input's argument reading it
// directly makes absent, and an optional input absent, which a guard reading
// it takes for one that does not hold; each a plain refusal past the term
// 'from' (DESIGN.md, optional inputs).
struct NoTerm : std::runtime_error {
    NoTerm(const std::string& what, const void* term) : std::runtime_error(what), from(term) {}
    const void* from;
};
struct Absent : NoTerm {
    using NoTerm::NoTerm;
    bool inexact = false;  // where an inexact truth chose to read it (C241)
};

// When a run of values has reached its limit. One rule for the language: 'lim'
// walks the terms of a sequence with it, and a series without an upper bound
// its partial sums.
template <typename T>
class Convergence {
public:
    // Since phase 9 this cap is a judgement about how long to keep trying and
    // not a technical limit: 'lim' walks the terms upward, so each one finds
    // its predecessor memoised, and a hundred thousand of them cost 250ms and
    // no depth. Raising it was measured and rejected -- DESIGN.md, C36.
    static constexpr size_t max_terms = 100;
    static constexpr double tolerance = 1E-10;

    // 'what' names the values when the reason they cannot be compared -- a
    // matrix has no absolute value, two terms have different sizes -- would
    // otherwise read as an internal error.
    explicit Convergence(std::string what, std::string value = "a term")
        : what_(std::move(what)), value_(std::move(value)) {}

    // A limit is approached and not reached, so it is inexact even when every
    // value was exact and it lands on a whole number (DESIGN.md,
    // phase 13). A run of --check moves it by the remainder estimated where
    // the walk stopped, in the term's units (Number::inexact).
    T Reached(const T& value) const { return numeric_interface<T>::inexact(value, near_.left); }

    // The last value, in a refusal, marked as an answer would be (C168).
    static std::string Last(const T& value) {
        return numeric_interface<T>::toString(value) +
               (numeric_interface<T>::approximated(value) ? ", approximated past a thousand digits"
                                                          : "");
    }

    // The next value; true when it is the limit, within the tolerance of
    // each cell's own size. The first is never: one value has nothing to be
    // compared with. Near() says whether it was within the tolerance absolute
    // below 1; the first such is kept, the answer of a walk that never
    // closes on its own size, as a walk to 0 does not, or that ends past it
    // on a value refused (DESIGN.md, a tolerance floor for small limits).
    [[nodiscard]] bool Next(const T& value) {
        // A step cannot tell its NaN from a value refused, so neither is a
        // walk that runs out.
        if (T::Nan(value))
            throw std::runtime_error(what_ + " has no limit: " + value_ + " is " + Last(value));
        bool closed = near_now_ = false;
        if (started_) {
            const auto step = [&](double floor) {
                try {
                    return numeric_interface<T>::distance(value, previous_, floor);
                } catch (const std::exception& reason) {
                    throw std::runtime_error(what_ + " has no limit: " + reason.what());
                }
            };
            near_now_ = near_.Settles(step(1));
            closed    = close_.Settles(step(0));
            if (near_now_ && !kept_) kept_ = Reached(value);
        }
        previous_ = value;
        started_  = true;
        return closed;
    }
    [[nodiscard]] bool                    Near() const { return near_now_; }
    [[nodiscard]] const std::optional<T>& Kept() const { return kept_; }

private:
    using Step = decltype(numeric_interface<T>::distance(std::declval<const T&>(),
                                                         std::declval<const T&>(), 1));

    // A small step is not a small remainder. If the steps shrink by a factor
    // r each term, what is left of the series is about step*r/(1-r); for
    // 1/n^2, where r approaches 1, that is 1/n -- five orders of magnitude
    // above the step that would otherwise have been called convergence.
    // One step is no evidence at all, which is why 'stepped' is required.
    // After a step of 0 there is no ratio, and the last step is what is left.
    // At a step of 0 there is none either: a term may pause and move on, as
    // a binary digit of 0 does (C279), and the step before is what may be left.
    struct Steps {
        Step before{}, left{};
        bool stepped = false;
        // '<=' and not '!(> tolerance)': a step that is NaN must not settle.
        bool Settles(Step step) {
            const bool settles =
                step <= tolerance && stepped && (left = Remainder(step)) <= tolerance;
            before  = step;
            stepped = true;
            return settles;
        }
        Step Remainder(Step step) const {
            if (!(before > 0)) return step;
            if (!(step > 0)) return before;
            const Step ratio = step / before;
            if (!(ratio < 1)) return std::numeric_limits<Step>::infinity();
            return step * ratio / (1 - ratio);
        }
    };

    std::string what_, value_;
    T           previous_{};
    Steps            near_, close_;
    std::optional<T> kept_;
    bool             started_  = false;
    bool             near_now_ = false;
};

#endif  // INKAMATH_CONVERGENCE_HPP

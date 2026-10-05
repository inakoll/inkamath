# The interpreter's own error, estimated by --check (DESIGN.md, next in
# line). Each term is asked three times more, every rounding taken the other
# way with probability one half and every limit moved by the remainder it
# estimates, up, down and at random; how far those move is reported on the
# sequence's line: an estimate, not a bound. Every instance reports 'within
# 0' today, every term being one the step computes to the bit.
#
# A limit stopped short hides which side of its threshold a guard is on.
# 'half' sums to 1/2, a hundred billionth clear of the threshold, and is
# stopped at 1/2 - 2^-34, below it, so interpreter and step answer 0 where
# the exact answer is 1. Moved up by its remainder, 2^-34, it is 1/2 exactly
# and the guard holds.
#
#     tie: 100 steps from 0, against exact values until 0 and inexact ones from there
#     tie.y: within 0; the interpreter's terms about 5.8e-11 from the exact ones
#     tie.g: within 0; the interpreter's terms about 1 from the exact ones, past the tolerance from 0
tied(w = 1/10^11) = {
    half(a)_0 = 0
    half(a)_k = half(a)_(k-1) + a/2^(k+1)
    y_n = lim half(1)
    g_n | y_n >= 1/2 - w = 1
    g_n = 0
}
tie = tied()

# The logistic map at 4, from a fifth made inexact: its terms are past the
# tolerance from the exact ones at 26 and 0.95 from them at worst, by mpmath
# at 600 bits. The estimate's digits are the seeds'; simulated, it passes the
# tolerance at 23 to 26 and its largest is 0.98 to 1:
#
#     chaos: 100 steps from 0, against exact values until 0 and inexact ones from there
#     chaos.x: within 0; the interpreter's terms about <x> from the exact ones, past the tolerance from <s>
lmap(r = 4, s = 1/5) = {
    x_0 = s
    x_n = r*x_(n-1)*(1 - x_(n-1))
}
chaos = lmap(s = ~(1/5))

# The same map from 3/16, which a double holds: no exact number is made
# inexact, so only the roundings of its products can show that its terms are
# past the tolerance from the exact ones at 27, and 0.98 from them at worst.
# Simulated, the estimate passes the tolerance at 27 to 29 and its largest is
# 0.98 to 1:
#
#     dyad: 100 steps from 0, against exact values until 0 and inexact ones from there
#     dyad.x: within 0; the interpreter's terms about <x> from the exact ones, past the tolerance from <s>
dyad = lmap(s = ~(3/16))

# A power that is 0 is exact, whatever its rounding is said to be. Moved a
# unit below, the square of 'flat' would be negative, its root complex and
# its guard no comparison, so a run would give no term and the estimate be
# infinite. Its lines stay as they are:
#
#     flat: 100 steps from 0, against exact values until 0 and inexact ones from there
#     flat.r: within 0
#     flat.g: within 0
rooted(s = 0) = {
    r_n = (s^2)^(1/2)
    g_n | r_n > 0 = 1
    g_n = 0
}
flat = rooted(s = ~0)

# A fraction over a power of two whose numerator fits 53 bits is a double
# exactly, past 64 bits too: 'halved' passes 2^64 at 64, and its conversion
# is never moved. Its lines stay as they are:
#
#     halved: 100 steps from 0, against exact values until 0 and inexact ones from there
#     halved.x: within 0
#     halved.y: within 0
halving() = {
    x_0 = 1
    x_n = x_(n-1)/2
    y_n = x_n*~1
}
halved = halving()

# A limit 'grad' walks is moved as any other, its derivative by the remainder
# of its own steps. 'steep' doubles the derivative of 'bowl', 4n exactly; the
# interpreter's terms are 8n less what the series left, 1.2e-10 from the exact
# ones at 2, and 2.9e-11 at 1, never past the tolerance:
#
#     steep: 100 steps from 0, against exact values until 0 and inexact ones from there
#     steep.y: within 0; the interpreter's terms about <e> from the exact ones
bowl(w) = sum_(k=0) w^2/2^k
doubling(x_n = n) = {
    y_n = 2*x_n
}
steep = doubling(x_n = grad_(w = n) bowl(w))

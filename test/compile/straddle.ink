# A guard the interpreter's own error straddles (DESIGN.md).
# --check estimates that error by asking each term three times more, every
# rounding taken the other way at random and every limit moved by its
# remainder, up, down, then either way. Where a run takes another clause than
# the interpreter, the spread there is a clause's and not a rounding's: the
# first step at which one does is reported after the first line, with the
# margin of the guard that decided, as the interpreter computes it. The
# estimate is printed as before, and the exit status is the step's.

# A ReLU at its kink, reached by a limit. 'y' stops at 1/2 - 2^-34 and is
# moved by 2^-34, up to 1/2 in the first run and down to 1/2 - 2^-33 in the
# second. The kink is a ten-billionth below 1/2: the interpreter is 4.2e-11
# above it and takes the ReLU, the run moved down is below it and takes 0.
# The clauses meet at the kink, so the estimate still measures the term: 'r'
# is 1e-10 exactly, 5.8e-11 from the interpreter's, as the run moved up says.
#
#     knee: 100 steps from 0, against exact values until 0 and inexact ones from there
#     knee.r: at 0 a disturbed run takes 'r_n = 0' and the interpreter 'r_n | y_n > c = y_n - c'; the guard of the second is 4.2e-11 from its threshold
#     knee.y: within 0; the interpreter's terms about 5.8e-11 from the exact ones
#     knee.r: within 0; the interpreter's terms about 5.8e-11 from the exact ones
#
# With --float the line is the same, the runs being the interpreter's. The
# kink is 1/2 in float and 'y' at most 1/2, so the step takes 0 as the
# second run does, and its flip follows, at the same margin:
#
#     knee: 100 steps from 0 in float, against exact values until 0 and inexact ones from there
#     knee.r: at 0 a disturbed run takes 'r_n = 0' and the interpreter 'r_n | y_n > c = y_n - c'; the guard of the second is 4.2e-11 from its threshold
#     knee.r: at 0 the compiled step takes 'r_n = 0' and the interpreter 'r_n | y_n > c = y_n - c'; the guard of the second is 4.2e-11 from its threshold
#     knee.y: within <x>, <u> units of a float; the interpreter's terms about 5.8e-11 from the exact ones
#     knee.r: within 4.2e-11, 0.00035 units of a float; the interpreter's terms about 5.8e-11 from the exact ones
kinked(c = 1/2 - 1/10^10) = {
    half(a)_0 = 0
    half(a)_k = half(a)_(k-1) + a/2^(k+1)
    y_n = lim half(1)
    r_n | y_n > c = y_n - c
    r_n = 0
}
knee = kinked()

# A ReLU at its kink, reached by an inexact sum: ~(1/10) + ~(2/10) is
# 0.30000000000000004, 5.6e-17 above the double nearest 3/10, where the
# exact sum is 3/10 and the ReLU 0. A run that moves either conversion, or
# the threshold's, or rounds the sum the other way, lands at or below it
# about half the time, so the step at which one first does is the seeds',
# and so are the estimates' digits:
#
#     crease: 100 steps from 0, against exact values until 0 and inexact ones from there
#     crease.r: at <n> a disturbed run takes 'r_n = 0' and the interpreter 'r_n | s_n > c = s_n - c'; the guard of the second is 5.6e-17 from its threshold
#     crease.s: within 0; the interpreter's terms about <e> from the exact ones
#     crease.r: within 0; the interpreter's terms about <e> from the exact ones
creased(c = 3/10) = {
    s_n = ~(1/10) + ~(2/10)
    r_n | s_n > c = s_n - c
    r_n = 0
}
crease = creased()

# The same sum a fifth above its threshold: no run comes near it, and no
# line says one does.
#
#     plain: 100 steps from 0, against exact values until 0 and inexact ones from there
#     plain.s: within 0; the interpreter's terms about <e> from the exact ones
#     plain.r: within 0; the interpreter's terms about <e> from the exact ones
plain = creased(c = 1/10)

# A straddle and a flip in one instance, each reported once, the straddle
# first as it is the interpreter's. 'g' is straddled at 3, where n*y_n is
# 3/2 - 3*2^-34 in the interpreter and 3/2 in the run moved up: below 3 every
# run is below 3/2, past it every run above. 'h' is brink's guard
# (drift.ink), which the step flips at 1, on its threshold exactly. The exit
# status is the flip's.
#
#     both: 100 steps from 0, against exact values until 0 and inexact ones from there
#     both.g: at 3 a disturbed run takes 'g_n | n*y_n >= 3/2 = 1' and the interpreter 'g_n = 0'; the guard of the first is 1.7e-10 from its threshold
#     both.h: at 1 the compiled step takes 'h_n | d_n < c = 1' and the interpreter 'h_n = 0'; the guard of the first is exactly on its threshold
#     both.d: <x> at 9, where the interpreter gives 0.10000000000000001
#     both.h: 1 at 1, where the interpreter gives 0
#     both.y: within 0; the interpreter's terms about 5.8e-11 from the exact ones
#     both.g: within 0; the interpreter's terms about 1 from the exact ones, past the tolerance from 3
paired(c = 1/10) = {
    half(a)_0 = 0
    half(a)_k = half(a)_(k-1) + a/2^(k+1)
    y_n = lim half(1)
    g_n | n*y_n >= 3/2 = 1
    g_n = 0
    d_0 = c
    d_n = 10*d_(n-1) - 9*c
    h_n | d_n < c = 1
    h_n = 0
}
both = paired()

# tie's straddle through a function's guard, as README writes a ramp: H is a
# step, asked of 'y' less the threshold. Its interpreter's argument is
# 2^-34 - 10^-11 = -4.8e-11 and H takes 0, the run moved up's is +1e-11 and
# H takes 1, so the exact 'x' is n and the interpreter's 0. The call is
# 'x''s, first made at 1: x_0 is its base.
#
#     acc: 100 steps from 0, against exact values until 0 and inexact ones from there
#     acc.x: at 1 a disturbed run takes 'H(x) | x >= 0 = 1' and the interpreter 'H(x) = 0'; the guard of the first is 4.8e-11 from its threshold
#     acc.y: within 0; the interpreter's terms about 5.8e-11 from the exact ones
#     acc.x: within 0; the interpreter's terms about 99 from the exact ones, past the tolerance from 1
gated(w = 1/10^11) = {
    half(a)_0 = 0
    half(a)_k = half(a)_(k-1) + a/2^(k+1)
    H(x) | x >= 0 = 1
    H(x) = 0
    y_n = lim half(1)
    x_0 = 0
    x_n = x_(n-1) + H(y_n - (1/2 - w))
}
acc = gated()

# A function the input calls as well as the step. The interpreter asks the
# input before any term and a run within each, and their calls are paired
# all the same: every term exact, no run takes another clause.
#
#     given: 100 steps from 0, against exact values
#     given.x: within 0
U(x) | x >= 0 = 1
U(x) = 0
fedby(u_n) = {
    x_n = u_n + U(1)
}
given = fedby(u_n = U(-1))

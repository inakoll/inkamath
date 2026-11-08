# clamp from the prelude (DESIGN.md), compiled and held to
# the interpreter across both kinks. Worked out by hand, none recorded.
#
# 'ramp': x_n = n/4 - 1/2, so 2*x_n = n/2 - 1, a double exactly: below 0 to
# n = 1, at lo = 0 at n = 2, inside at 3, at hi = 1 at 4 and above from 5.
# y is the clamp, 0 to n = 2, 1/2 at 3 and 1 from 4. g is its derivative by
# x, forward: 2 at 3, 0 elsewhere, both kinks included, where max and min
# take their first argument's slope, the bound's. r is by x and hi at once,
# a matrix, so in reverse mode: [0, 0] to n = 2, [2, 0] at 3 and [0, 1] from
# 4, hi's slope where the clamp is hi, its kink included. Every term is
# exact, and a double, so the step's are the interpreter's, reported in the
# step's order, by name where none reads another:
#
#     ramp: 100 steps from 0, against exact values
#     ramp.g: within 0
#     ramp.r: within 0
#     ramp.y: within 0
kinks(x_n) = {
    y_n = clamp(2*x_n, 0, 1)
    g_n = grad_(t = x_n) clamp(2*t, 0, 1)
    r_n = grad_(v = [x_n, 1]) clamp(2*v[1,1], 0, v[1,2])
}
ramp = kinks(x_n = n/4 - 1/2)

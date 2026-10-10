# atan, atan2, asin, acos and acosh as the prelude writes them (DESIGN.md,
# next in line), compiled as sin and cos are: each a C function of the
# header's own, emitted once before the step with every function of the
# prelude it calls, and under grad a part function beside it. The step
# computes the doubles the interpreter computes, by the same operations, so
# every term agrees to the bit:
#
#     bea: 100 steps from 0, against exact values until 0 and inexact ones from there
#     bea.<s>: within 0; the interpreter's terms about <x> from the exact ones
#
# a line for each of its eight sequences. The program defines bea_atan,
# bea_atan2, bea_asin, bea_acos and bea_acosh, and calls no libm atan, asin,
# acos or acosh.
#
# h is the angle of a spiral, two turns through every quadrant and across
# the negative x axis; t crosses each threshold of atan's reduction; k runs
# from 1, where acosh takes its series, past 17/16; g's grad crosses atan's
# thresholds and atan2's positive x axis.
bearing(a = 1/8) = {
    x_n = cos(a*n)*(1 + a*n)
    y_n = sin(a*n)*(1 + a*n)
    h_n = atan2(y_n, x_n)
    t_n = atan(a*n - 6)
    s_n = asin(sin(a*n - 3)/2)
    c_n = acos(cos(a*n)/(1 + a))
    k_n = acosh(1 + a*a*n*n/4)
    g_n = grad_(u = a*n - 6) (atan(u) + asin(u/7) + atan2(u, 1))
}
bea = bearing(a = ~(1/8))

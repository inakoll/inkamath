# sin, cos, abs, max and min as the prelude writes them (DESIGN.md, next in
# line), compiled as exp, log and tanh are: each a C function of the
# header's own, emitted once before the step with every function of the
# prelude it calls, and under grad a part function beside it. The step
# computes the doubles the interpreter computes, by the same operations, so
# every term agrees to the bit:
#
#     tri: 100 steps from 0, against exact values until 0 and inexact ones from there
#     tri.<s>: within 0; the interpreter's terms about <x> from the exact ones
#
# a line for each of s, c, w, q, v, b, m, l and g, each estimate the seeds'.
# The program defines tri_sin, tri_cos, tri_abs, tri_max and tri_min, and
# the parts tri_sin_dx, tri_cos_dx, tri_abs_dx, tri_max_da and tri_min_db.
#
# w reaches 242,574, within 2^20; q and v are a pendulum stepped by Euler's
# rule from 1 radian; g crosses each kink at 12, where t is 0: max(t, 0) and
# min(0, t) tie and take their first argument's slope, and abs its x >= 0
# clause's, as interpreted.
waves(a = 1/4) = {
    s_n = sin(a*n*n - 1000)
    c_n = cos(a*n - 12)
    w_n = sin(a*n*n*n)
    q_0 = 1
    q_n = q_(n-1) + a*v_(n-1)
    v_0 = 0
    v_n = v_(n-1) - a*sin(q_(n-1))
    b_n = abs(s_n - c_n)
    m_n = max(s_n, c_n)
    l_n = min(s_n, c_n)
    g_n = grad_(t = a*n - 3) (max(t, 0)*cos(t) + min(0, t)*sin(t) + abs(t))
}
tri = waves(a = ~(1/4))

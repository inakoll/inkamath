# Second derivatives compiled (DESIGN.md): a double pendulum
# from its Lagrangian, M = d2L/dw2 by grad's order and C = d2L/dw dq by a
# Jacobian by cells of grad's gradient, beside the same equations written
# by hand, w' = M^-1*(dL/dq - C*w), semi-implicit Euler at h = 1/100. Each
# is held to the interpreter by --check, the derivatives to the bit as a
# first derivative is, so every term is within 0:
#
#     p: 100 steps from 0, against exact values until 1 and inexact ones from there
#     p.<s>: within 0 ...
#
# for M, C, G, w and q, and h's alike for k, s, M, C, G, w and q. The two
# agree to the digits the interpreter prints: q_100 = [-0.276479857066503;
# -1.04368092424307] by the hand-written recurrence in mpmath at 40 digits,
# M_1 and C_2 by mpmath's numerical derivatives of L besides
# (test/data/spec/hessian.ink).
#
# 'b', the second derivative of a definition in cases, each side of its
# threshold: 6q above 0, -2 below and 0 at 0, q stepping by 1/32 from
# -1/2, every term a dyadic exact in a double:
#
#     b: 100 steps from 0, against exact values
#     b.d: within 0, from 1
#     b.q: within 0
#
# 'k', a power whose second derivative is infinite at 0 where its first is
# not, t^(3/2), and one moving by the outer grad alone, t^(1/2): NaN at 3,
# where t = 0 and the interpreter refuses, as a first derivative's is.
#
# 'z', the same through a definition in cases constant below 0, h: each
# clause's parts by each grad where that clause has them, so no NaN below 0
# where the interpreter answers, and NaN at each jump of floor above it.
#
# 'c', nests at constant points folded exactly, as a first derivative's
# are: 0.1*3 is 3/10, where the comparison jumps and the interpreter
# refuses, NaN at every step; the others within 0.
lag(m1 = 1, m2 = 1, l1 = 1, l2 = 1, g = 10) = {
    L(q, w) = (m1 + m2)*l1^2*w[1]^2/2 + m2*l2^2*w[2]^2/2 + m2*l1*l2*w[1]*w[2]*cos(q[1] - q[2]) + (m1 + m2)*g*l1*cos(q[1]) + m2*g*l2*cos(q[2])
    q_0 = [1; 1/2]
    w_0 = [0; 0]
    M_n = grad_(v = w_(n-1))^2 L(q_(n-1), v)
    C_n[i<=2, j<=2] = (grad_(u = q_(n-1)) (grad_(v = w_(n-1)) L(u, v))[i])[j]
    G_n = grad_(u = q_(n-1)) L(u, w_(n-1))
    w_n = w_(n-1) + M_n^-1*(G_n - C_n*w_(n-1))/100
    q_n = q_(n-1) + w_n/100
}
hand(m1 = 1, m2 = 1, l1 = 1, l2 = 1, g = 10) = {
    q_0 = [1; 1/2]
    w_0 = [0; 0]
    k_n = m2*l1*l2*cos(q_(n-1)[1] - q_(n-1)[2])
    s_n = m2*l1*l2*sin(q_(n-1)[1] - q_(n-1)[2])
    M_n = [(m1 + m2)*l1^2, k_n; k_n, m2*l2^2]
    C_n = [-s_n*w_(n-1)[2], s_n*w_(n-1)[2]; -s_n*w_(n-1)[1], s_n*w_(n-1)[1]]
    G_n = [-s_n*w_(n-1)[1]*w_(n-1)[2] - (m1 + m2)*g*l1*sin(q_(n-1)[1]); s_n*w_(n-1)[1]*w_(n-1)[2] - m2*g*l2*sin(q_(n-1)[2])]
    w_n = w_(n-1) + M_n^-1*(G_n - C_n*w_(n-1))/100
    q_n = q_(n-1) + w_n/100
}
bend() = {
    pc(x) | x < 0 = -x^2
    pc(x) = x^3
    q_0 = -1/2
    q_n = q_(n-1) + 1/32
    d_n = grad_(x = q_(n-1))^2 pc(x)
}
cusp() = {
    x_0 = -1/2
    x_n = x_(n-1) + 1/4
    r_n = grad_(t = x_(n-1)^2)^2 t^(3/2)
    s_n = grad_(t = x_(n-1)^2) grad_(s = 1) s*t^(1/2)
}
kink() = {
    h(x) | x < 0 = 0
    h(x) = x
    x_0 = ~-0.5
    x_n = x_(n-1) + ~0.25
    d_n = grad_(t = x_(n-1))^2 (floor(h(t)) + t^3)
    r_n = grad_(t = x_(n-1))^2 h(t)^(1/2)
    u_n = grad_(t = x_(n-1))^2 h(t)^(3/2)
    s_n = grad_(t = x_(n-1)) grad_(s = 1) s*h(t)^(1/2)
}
fixed() = {
    y_0 = 0
    y_n = y_(n-1) + 1
    a_n = grad_(x = 1/10)^2 ((x*3 > 3/10) + x^3)
    d_n = grad_(x = 1/3)^2 x^3/(1 + x^2)
    f_n = grad_(x = 1/3) grad_(s = 2) s*x^3/(1 + x^2)
    g_n = grad_(v = [1/3; 1/7])^2 v[1]^3/(1 + v[2]^2)
}
p = lag()
h = hand()
b = bend()
k = cusp()
z = kink()
c = fixed()

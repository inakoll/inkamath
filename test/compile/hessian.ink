# Second derivatives compiled (DESIGN.md, next in line): a double pendulum
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
# Before, every model was refused, 'a derivative of a derivative, for now',
# and the order 'unexpected ^'.
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
p = lag()
h = hand()
b = bend()

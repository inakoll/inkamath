# A matrix inverse and power under grad compiled (DESIGN.md, next in line):
# B = A^-1's part -B*(A'*B), its parts in a nest -B*(A12*B + A2*B1 +
# A1*B2), and A^k's by the interpreter's squaring, each product's parts as
# a product's, so a robot's M(q)^-1*(tau - C - G) differentiates as it is
# written. Specified before it is built: today each model is refused, 'a
# derivative of a matrix power, for now'. Every number below is worked out
# apart from the interpreter and the compiler, none recorded.
#
# 'ex', dyadic points where every value the step computes is a double
# exactly, so that the step is held to the exact values themselves. M(t) =
# [1, t; t, 1 + t^2] has determinant 1 and inverse [1 + t^2, -t; -t, 1], and
# U(t) = I + t*N + t^2*N^2, N the 3x3 shift, has inverse I - t*N: so
#
#     a = [2t, -1; -1, 0]                 h = [2, 0; 0, 0]
#     b = [2t - 2; -1]                    c = [0, -1, 0; 0, 0, -1; 0, 0, 0]
#     g = M' = [0, 1; 1, 2t]              k = 2*N^2 = [0, 0, 2; 0, 0, 0; 0, 0, 0]
#
# and d, e, f and m the derivatives of M^2 = [1 + t^2, 2t + t^3; 2t + t^3,
# 1 + 3t^2 + t^4], of M^3 = [1 + 3t^2 + t^4, 3t + 4t^3 + t^5; 3t + 4t^3 +
# t^5, 1 + 6t^2 + 5t^4 + t^6] and of M^-2 = [1 + 3t^2 + t^4, -2t - t^3; -2t
# - t^3, 1 + t^2], by polynomials in fractions. x stays within (-1, 1), so
# Gauss-Jordan takes no other pivot; every operation of the step, the
# inverse's elimination, its parts and the squarings', in doubles and in
# fractions side by side at each of the 100 points, gives no double that is
# not its fraction. Each term from 1, where it first reads x_0:
#
#     ex: 100 steps from 0, against exact values
#     ex.<s>: within 0, from 1
#
# for a, b, c, d, e, f, g, h, k and m, and x within 0. p, at the constant
# point 1/3, is folded exactly, as a grad of constants is, [386/81,
# -356/81; -356/81, 58/27], so within 0 too, from 0; computed in the step,
# it was within 4.4e-16 on a prototype. At 1/2 and 1/3 the interpreter's
# own, written by hand in test/data/grad.ink and hessian.ink.
exact() = {
    M(t) = [1, t; t, 1 + t^2]
    U(t) = [1, t, t^2; 0, 1, t; 0, 0, 1]
    x_0 = -1/2
    x_n = x_(n-1) + 1/128
    a_n = grad_(t = x_(n-1)) M(t)^-1
    b_n = grad_(t = x_(n-1)) (M(t)^-1*[1; 2])
    c_n = grad_(t = x_(n-1)) U(t)^-1
    d_n = grad_(t = x_(n-1)) M(t)^2
    e_n = grad_(t = x_(n-1)) M(t)^3
    f_n = grad_(t = x_(n-1)) M(t)^-2
    g_n = grad_(t = x_(n-1)) M(t)^1
    h_n = grad_(t = x_(n-1))^2 M(t)^-1
    k_n = grad_(t = x_(n-1))^2 U(t)^-2
    m_n = grad_(t = x_(n-1))^2 M(t)^3
    p_n = grad_(t = 1/3) M(t)^-3
}
ex = exact()

# 'inv', state-dependent matrices at inexact points, where the
# interpreter's values are doubles: a 2x2 with a cosine and a 3x3 shaped as
# a three-link arm's mass matrix, symmetric and positive definite, each
# inverse alone and times a vector that moves, as M(q)^-1*(tau - C - G) is;
# and the powers 2, 3 and -2. Each part is the interpreter's operation for
# operation, so each term is within 0:
#
#     inv: 100 steps from 0, against exact values until 0 and inexact ones from there
#     inv.<s>: within 0, from 1; the interpreter's terms about <e> from the exact ones
#
# for a, b, c, d, e, f, g and h, and x within 0. By hand at 1/2, -B*M'*B
# with M' = [-sin(1/2), 1; 1, 0]: a_1 = [0.104095376, -0.136641857;
# -0.136641857, 0.0426557475], as test/data/grad.ink holds it.
arm() = {
    M(t) = [2 + cos(t), t; t, 3]
    N(t) = [5 + 2*cos(t), 2 + cos(t), 1; 2 + cos(t), 3, 1 + cos(t)/2; 1, 1 + cos(t)/2, 1]
    v(t) = [sin(t); 1]
    x_0 = ~1/2
    x_n = x_(n-1) + ~1/100
    a_n = grad_(t = x_(n-1)) M(t)^-1
    b_n = grad_(t = x_(n-1)) (M(t)^-1*v(t))
    c_n = grad_(t = x_(n-1)) N(t)^-1
    d_n = grad_(t = x_(n-1)) (N(t)^-1*[cos(t); 1; t])
    e_n = grad_(t = x_(n-1)) M(t)^2
    f_n = grad_(t = x_(n-1)) M(t)^3
    g_n = grad_(t = x_(n-1)) M(t)^-2
    h_n = grad_(t = x_(n-1)) N(t)^-2
}
inv = arm()

# 'nest', second derivatives: of the inverse, of the powers 3 and -2, of
# the 3x3 inverse and of the inverse times a vector, by grad's order, and a
# mixed partial whose base moves by both grads, the cross part
# -B*(P12*B + P2*B1 + P1*B2) in the interpreter's order. Within 0, as a
# nest's other parts are:
#
#     nest: 100 steps from 0, against exact values until 0 and inexact ones from there
#     nest.<s>: within 0, from 1; the interpreter's terms about <e> from the exact ones
#
# for a, b, c, d, e and f, and x within 0.
twice() = {
    M(t) = [2 + cos(t), t; t, 3]
    N(t) = [5 + 2*cos(t), 2 + cos(t), 1; 2 + cos(t), 3, 1 + cos(t)/2; 1, 1 + cos(t)/2, 1]
    P(s, t) = [2 + s*t, t; s, 3]
    x_0 = ~1/2
    x_n = x_(n-1) + ~1/100
    a_n = grad_(t = x_(n-1))^2 M(t)^-1
    b_n = grad_(t = x_(n-1))^2 M(t)^3
    c_n = grad_(t = x_(n-1))^2 M(t)^-2
    d_n = grad_(t = x_(n-1))^2 N(t)^-1
    e_n = grad_(s = x_(n-1)) grad_(t = ~1/3) P(s, t)^-1
    f_n = grad_(t = x_(n-1))^2 (M(t)^-1*[sin(t); 1])
}
nest = twice()

# 'many', a grad by several names: of a matrix, a pass for each name, and
# of a single value, one sweep back through the inverse and through the
# power -2, as the interpreter takes each. Within 0:
#
#     many: 100 steps from 0, against exact values until 0 and inexact ones from there
#     many.<s>: within 0, from 1; the interpreter's terms about <e> from the exact ones
#
# for g.x, g.y, h.x, h.y, k.x and k.y, and a and b within 0. For [1, x;
# y, 1 + x*y] at (1/2, 1/3), whose inverse is [1 + x*y, -x; -y, 1], g.x =
# [1/3, -1; 0, 0], g.y = [1/2, 0; -1, 0] and h, the gradient of 3 + x*y -
# 2x - y, (-5/3, -1/2), as test/data/gradmany.ink holds them.
names() = {
    Q(x, y) = [2 + cos(x), y; x*y, 3]
    a_0 = ~1/2
    a_n = a_(n-1) + ~1/100
    b_0 = ~1/3
    b_n = b_(n-1) - ~1/200
    g_n = grad_(x = a_(n-1), y = b_(n-1)) Q(x, y)^-1
    h_n = grad_(x = a_(n-1), y = b_(n-1)) ([1 1]*Q(x, y)^-1*[1; 2])
    k_n = grad_(x = a_(n-1), y = b_(n-1)) ([1 1]*Q(x, y)^-2*[1; 2])
}
many = names()

# 'back', gradients by a vector and by a matrix, in reverse mode: the
# inverse's part made of products and a negation, the sweep takes it back
# by their adjoints, -B^T*Bbar*B^T for A, as the interpreter's tape does,
# with no rule of its own. S needs a row exchange, its first column's
# largest cell its second. And a Hessian by the vector through the
# inverse, forward over forward. Within 0:
#
#     back: 100 steps from 0, against exact values until 0 and inexact ones from there
#     back.<s>: within 0, from 1; the interpreter's terms about <e> from the exact ones
#
# for a, b, c, d, e, f, g, h and k, and q and A within 0. The interpreter's
# power starts from the identity, its first factor's part I*B', which
# reverse mode records as a node: the step records it too, as the sum of
# the seed's adjoints is in that node's order; starting from B', f was
# within 1.4e-14 on a prototype. W^1 is no exception, its part I*W' a node
# as any first factor's, before the product that reads it, k. With R = [1, v1; v2, 1 + v1*v2] instead,
# at v = [1/2; 1/3], the gradient of [1 1]*R^-1*[1; 2] is [-5/3; -1/2], of [1
# 1]*R^-2*[1; 2] [-3; -1/6], and the Hessian of the first [0, 1; 1, 0], as
# test/data/reverse.ink and hessian.ink hold them.
sweep() = {
    R(q) = [2 + cos(q[1]), q[2]; q[1]*q[2], 3]
    S(q) = [q[1]/4, 1, 0; 2 + cos(q[2]), q[1], 1; 1, sin(q[2]), 3]
    q_0 = ~[1/2; 1/4]
    q_n = q_(n-1) + ~[1/100; -1/200]
    A_0 = ~[2 1/2; 1/3 3]
    A_n = A_(n-1) + ~[1/100 0; 0 -1/100]
    a_n = grad_(v = q_(n-1)) ([1 1]*R(v)^-1*[1; 2])
    b_n = grad_(v = q_(n-1)) ([1 1]*R(v)^2*[1; 2])
    c_n = grad_(v = q_(n-1)) ([1 1]*R(v)^-2*[1; 2])
    d_n = grad_(v = q_(n-1)) ([1 0 1]*S(v)^-1*[1; 1; 1])
    e_n = grad_(W = A_(n-1)) ([1 1]*W^-1*[1; 2])
    f_n = grad_(W = A_(n-1)) ([1 1]*W^3*[1; 2])
    g_n = grad_(v = q_(n-1)) ([1 1]*R(v)^-3*[1; 2])
    h_n = grad_(v = q_(n-1))^2 ([1 1]*R(v)^-1*[1; 2])
    k_n = grad_(W = A_(n-1)) ([1 1]*(W^1*(W*W))*[1; 2])
}
back = sweep()

# 'sing', a matrix singular at one point: x_4 is 0 exactly, so at 5 the
# interpreter refuses every grad, 'a singular matrix has no inverse', and
# the step's inverse is NaN in every cell (C308), and its parts with it; e,
# a running total of a's cell, is NaN from 5 on, where the interpreter
# refuses each term. Elsewhere [t, 1; 0, 1]^-1 = [1/t, -1/t; 0, 1], so a =
# [-1/t^2, 1/t^2; 0, 0], a_1 = [-1, 1; 0, 0], and d = [2/t^3, -2/t^3; 0, 0].
# A NaN where the interpreter refuses is agreement, so:
#
#     sing: 100 steps from 0, against exact values until 0 and inexact ones from there
#     sing.<s>: within 0, from 1
#
# for a, b, c and d, e and x within 0, each perhaps with the estimate of
# the interpreter's error. At a constant point a singular matrix refuses
# to compile, as the value alone does (test/cli.cmake).
pole() = {
    x_0 = ~-1
    x_n = x_(n-1) + ~1/4
    a_n = grad_(t = x_(n-1)) [t, 1; 0, 1]^-1
    b_n = grad_(t = x_(n-1)) [t, 1; 0, 1]^-2
    c_n = grad_(v = [x_(n-1); 1]) ([1 1]*[v[1], 1; 0, v[2]]^-1*[1; 1])
    d_n = grad_(t = x_(n-1))^2 [t, 1; 0, 1]^-1
    e_0 = 0
    e_n = e_(n-1) + a_n[1,1]
}
sing = pole()

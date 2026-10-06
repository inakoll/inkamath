# grad compiled (DESIGN.md, next in line): forward, as the interpreter takes
# it, each value compiled carrying its part, the derivative's cells beside its
# own, made by the interpreter's rules in its order. So the step's gradient is
# the interpreter's to the bit, and --check holds it so. Every number below is
# worked out apart from the interpreter: by hand, with exact fractions, with
# the step's operations in doubles and in floats, in C and in numpy, and with
# mpmath at 50 digits; none is recorded. Today each model is refused, 'cannot
# compile w: a derivative, for now'.
#
# 'line', least squares by gradient descent on data a line fits exactly,
# w = [1; 2]. Its terms are exact fractions, w_2 = [253/400; 537/400], and
# the step's are doubles with 0.1 for eta:
#
#     line: 100 steps from 0, against exact values
#     line.w: within 4.4e-16
#
# and in float, 'inkamath --check grad.ink line --float':
#
#     line: 100 steps from 0 in float, against exact values
#     line.w: within 2.7e-07, 1.9 units of a float
#
# Its header writes the gradient out: the part of each square is 2*u^1*u',
# pow included, as the interpreter takes it, u' being a cell of X, folded:
#
#     typedef struct lsq {
#         double eta;
#         long long index_;
#         double w[2][2][1];
#     } lsq;
#     ...
#     static inline void lsq_step(lsq* m_) {
#         ++m_->index_;
#         memcpy(m_->w[1], m_->w[0], sizeof m_->w[1]);
#         m_->w[0][0][0] = m_->index_ == 0 ? 0.0 : m_->w[1][0][0] - m_->eta * (2.0 * pow(1.0 * m_->w[1][0][0] + 0.0 * m_->w[1][1][0] - 1.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 1.0 * m_->w[1][1][0] - 3.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 2.0 * m_->w[1][1][0] - 5.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 3.0 * m_->w[1][1][0] - 7.0, 1.0) * 1.0 / 8.0);
#         m_->w[0][1][0] = m_->index_ == 0 ? 0.0 : m_->w[1][1][0] - m_->eta * (2.0 * pow(1.0 * m_->w[1][0][0] + 0.0 * m_->w[1][1][0] - 1.0, 1.0) * 0.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 1.0 * m_->w[1][1][0] - 3.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 2.0 * m_->w[1][1][0] - 5.0, 1.0) * 2.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 3.0 * m_->w[1][1][0] - 7.0, 1.0) * 3.0 / 8.0);
#     }
lsq(eta = 1/10) = {
    X = [1, 0; 1, 1; 1, 2; 1, 3]
    y = [1; 3; 5; 7]
    loss(v) = sum_(r=1)^4 (X[r]*v - y[r])^2/8
    w_0 = [0; 0]
    w_n = w_(n-1) - eta*grad_(v = w_(n-1)) loss(v)
}
line = lsq()

# 'fall', prelude.ink's logistic regression trained on its log loss, as a
# model: w by grad through the prelude's exp and log, and beside it u by the
# gradient written by hand (MANIFESTO.md, A paper conformance suite). Each
# is the interpreter's to the bit, the estimates the seeds':
#
#     fall: 100 steps from 0, against exact values until 1 and inexact ones from there
#     fall.u: within 0; the interpreter's terms about <e> from the exact ones
#     fall.w: within 0; the interpreter's terms about <e> from the exact ones
#
# The exact descent, by mpmath, is at w_10 = [-2.10089640888; 1.71241565141]
# and w_100 = [-6.37603325128; 4.42991214595].
#
# A function of the prelude called on a value that moves is a C function of
# the header's for its part, beside the one for its value, emitted once: named
# with 'd' and each parameter that moves, its parameters those of the
# function its part reads, then the part of each that moves. 'fall' has
# fall_expp_dr, fall_expk_dx, fall_exp_dx, fall_logp_dz, fall_logs_ds,
# fall_logm_dm, fall_logk_dx and fall_log_dx; ilogb's answer is a choice of
# constants, so it has no part and no such function. Two of them:
#
#     static inline double fall_exp_dx(double arg_x, double part_x) {
#         return isnan(arg_x) ? NAN : arg_x > 1000.0 ? 0.0 : isnan(arg_x) ? NAN : arg_x < -1000.0 ? 0.0 : fall_expk_dx(arg_x, (floor(arg_x * 1.4426950408889634 + 0.5) == arg_x * 1.4426950408889634 + 0.5 ? NAN : floor(arg_x * 1.4426950408889634 + 0.5)), part_x);
#     }
#     ...
#     static inline double fall_logs_ds(double arg_s, double part_s) {
#         return 2.0 * part_s * fall_logp(arg_s * arg_s) + 2.0 * arg_s * fall_logp_dz(arg_s * arg_s, part_s * arg_s + arg_s * part_s);
#     }
#
# fall_exp_dx is NaN where floor jumps, as the interpreter refuses it there.
descent(eta = 1/2) = {
    X = [1, 0; 1, 1; 1, 2; 1, 3]
    y = [0; 0; 1; 1]
    sig(z) = 1/(1 + exp(-z))
    loss(v) = 0 - sum_(r=1)^4 (y[r]*log(sig(X[r]*v)) + (1 - y[r])*log(1 - sig(X[r]*v)))
    hand(v) = sum_(r=1)^4 (sig(X[r]*v) - y[r])*X[r]'
    w_0 = [0; 0]
    w_n = w_(n-1) - eta*grad_(v = w_(n-1)) loss(v)
    u_0 = [0; 0]
    u_n = u_(n-1) - eta*hand(u_(n-1))
}
fall = descent()

# 'hinge', a ReLU by cells: each cell's part is its clause's, the clause the
# guard takes at the point, so the slope at 0 is the one after '<'. From n =
# 0, g is [0; 2], [0; 2], [1; 2], [1; 2], then [1; 0]:
#
#     hinge: 100 steps from 0, against exact values
#     hinge.g: within 0
rect(x_n) = {
    relu(z)[j<=2] = z[j]
    relu(z)[j<=2] | z[j] < 0 = 0
    a = [1, 2]
    g_n = grad_(v = [x_n - 2; 3 - x_n]) a*relu(v)
}
hinge = rect(x_n = n)

# 'edge', where the interpreter refuses for the point's sake, the step is
# NaN: floor at a whole number, f at every even n; a clause that holds only
# at the point, g at 2; a comparison read as a value where its sides meet, c
# at 2; a power whose derivative is infinite at 0, s at 2. Elsewhere f is
# floor(n/2), g is n, c is 0 below 2 and 1 above, and s is -1 below 2 and
# 1 above, its power C's pow of a double as the interpreter's is; and k, a
# gradient at a constant point, is folded, 12*n:
#
#     edge: 100 steps from 0, against exact values until 0 and inexact ones from there
#     edge.c: within 0
#     edge.f: within 0
#     edge.g: within 0
#     edge.k: within 0
#     edge.s: within 0; the interpreter's terms about <e> from the exact ones
#
# Each test is written in the value where the interpreter refuses, so every
# term reading it is NaN, as for any refusal, and a gradient is NaN where the
# value of what it differentiates is. The part f's step computes is
# '(floor(m_->x[0]) == m_->x[0] ? NAN : floor(m_->x[0])) * 1.0'.
kinks(x_n) = {
    q(t) = t^2
    q(t) | t == 1 = 1
    c_n = grad_(t = x_n) t*(t > 1)
    f_n = grad_(t = x_n) floor(t)*t
    g_n = grad_(t = x_n) q(t)
    k_n = n*grad_(t = 2) t^3
    s_n = grad_(t = x_n - 1) (t^2)^(1/2)
}
edge = kinks(x_n = n/2)

# 'clip', gradient descent on (v - 3)^2/2 whose step is clipped where the
# gradient is below -1: a guard reading a gradient is followed as any guard.
# Exact dyadic terms, 1/2, 1, 3/2, 2, then 3 - 2^(4-n), which the doubles
# hold until the halfway case at 56 rounds to 3, as the exact term does. At 5
# the gradient is -1, on the threshold in both, and neither flips:
#
#     clip: 100 steps from 0, against exact values
#     clip.w: within 0
clipped(eta = 1/2) = {
    loss(v) = (v - 3)^2/2
    w_0 = 0
    w_n = w_(n-1) - eta*grad_(v = w_(n-1)) loss(v)
    w_n | grad_(v = w_(n-1)) loss(v) < -1 = w_(n-1) + eta
}
clip = clipped()

# 'steer', newton.ink's backward Euler with the Newton step's derivative
# taken by grad inside the limit's function rather than written by hand. Its
# input is a double, so the interpreter's iterates are doubles as the step's:
#
#     steer: 100 steps from 0, against exact values until 1 and inexact ones from there
#     steer.x: within 0; the interpreter's terms about <e> from the exact ones
newton(dt = 1/10, u_n) = {
    r(s, xp, v) = s - xp - dt*(v - s^3)
    nw(xp, v)_0 = xp
    nw(xp, v)_k = nw(xp, v)_(k-1) - r(nw(xp, v)_(k-1), xp, v)/grad_(s = nw(xp, v)_(k-1)) r(s, xp, v)
    x_0 = 1
    x_n = lim nw(x_(n-1), u_n)
}
steer = newton(u_n = ~(1/2))

# What stays refused, each named in 'inkamath --compile' of a file of
#
#     a_n = grad_(t = x_n) grad_(s = t) s^3
#     b_n = grad_(t = x_n) lim p(t)
#     c_n = grad_(v = [x_n; 1]) 2*v
#     d_n = grad_(t = x_n) 2^t
#     f_n = grad_(t = x_n) t^x_n
#     g_n = grad_(t = x_n) [1 1]*[t 1; 0 t]^2*[1; 1]
#     h_n = grad_(t = x_n) 5
#     k_n = grad_(t = x_n) sq
#     m_n = grad_(t = x_n) amp(k = t).y
#     amp(k = 1) = {
#         y = 2*k
#     }
#     p(r)_0 = 1
#     p(r)_k = r*p(r)_(k-1)/4 + 1
#     sq = t^2
#     t = 3
#
# where the interpreter, given x_n = 2, answers a, b, f and g and refuses
# the rest in the words the compiler takes:
#
#     cannot compile a: a derivative of a derivative, for now
#     cannot compile b: a derivative of a limit, for now
#     cannot compile c: grad of a matrix with respect to a matrix is a Jacobian, which it does not give
#     cannot compile d: grad cannot differentiate a power whose exponent changes with t, unless its base is e
#     cannot compile f: a derivative of a power whose exponent is not a constant, for now
#     cannot compile g: a derivative of a matrix power, for now
#     cannot compile h: grad's expression does not read t
#     cannot compile k: sq reads the global t, which grad's t does not reach
#     cannot compile m: grad cannot differentiate through an instance yet
#
# on standard output, exiting 1.

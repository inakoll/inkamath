# A clause for a limit that does not converge, compiled (DESIGN.md): a guard
# reading a limit whose walk runs out does not hold, in the step as in the
# interpreter, and the clause after it is taken; where the walk is refused,
# or no clause comes after, the step is NaN as today. fallback_test.c holds
# each model to values computed apart from the interpreter, in Python's
# fractions and by README section 4's rule.

# A small MPC with a fallback clause: the plan minimises 2(x + v)^2 + v^2/2
# over |v| <= 1 by projected gradient with a step of 1/2, too long for the
# curvature 5. It settles where the bound holds it, at -1 from 5/2 and 3/2,
# and from 1/2 on cycles: the plant moves by -x/2 instead, so x_n is
# 2^-(n-1) and u_n -2^-n from n = 2, and solved_n is 1 at 0 and 1, 0 after.
# Every term is a double exactly, and every walk that settles stops on a
# step of 0, so a disturbed run moves none. Checked, it reports
#
#     planned: 100 steps from 0, against exact values until 0 and inexact ones from there
#     planned.<s>: within 0
#
# for x, u and solved, the step taking the interpreter's clause at each.
mpc(x0 = 5/2) = {
    clip(v) | v > 1 = 1
    clip(v) | v < -1 = -1
    clip(v) = v
    qp(s)_0 = 0
    qp(s)_k = clip(qp(s)_(k-1) - (5*qp(s)_(k-1) + 4*s)/2)
    u_n | abs(x_n + lim qp(x_n)) <= abs(x_n) = lim qp(x_n)
    u_n = -x_n/2
    solved_n | abs(x_n + lim qp(x_n)) <= abs(x_n) = 1
    solved_n = 0
    x_0 = x0
    x_n = x_(n-1) + u_(n-1)
}
planned = mpc()

# Newton's method within a step, its fallback the term before: from 0, on
# x^3 - 2x + c, it cycles 0, 1, 0, ... at c = 2, and at c = 1 converges at
# its sixth term, within 7e-24 of (sqrt(5) - 1)/2 (mpmath), whose double is
# 0.6180339887498949. So x_n is 1/2 at 0 and 1, and the root from 2 on.
# Checked, it reports
#
#     held: 100 steps from 0, against exact values until 1 and inexact ones from there
#     held.x: within <e>; the interpreter's terms about <e> from the exact ones
newton(c_n) = {
    G(x, c) = x^3 - 2*x + c
    dG(x) = 3*x^2 - 2
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - G(nv(c)_(k-1), c)/dG(nv(c)_(k-1))
    x_0 = 1/2
    x_n | abs(G(lim nv(c_n), c_n)) < 1/10^6 = lim nv(c_n)
    x_n = x_(n-1)
}
held = newton(c_n = 1 + mod(n, 2))

# A walk the interpreter refuses is not one that ran out: Newton's root
# from an exact 0 divides by 0, and in doubles its terms are inf and then
# NaN. The step is NaN, not the clause after.
rooted(c_n) = {
    hs(c)_0 = 0
    hs(c)_k = hs(c)_(k-1) - (hs(c)_(k-1)^2 - c)/(2*hs(c)_(k-1))
    y_n | lim hs(c_n) > 0 = lim hs(c_n)
    y_n = 0
}

# A limit in a sum's terms within a guard: a sum's bounds are constants, so
# it is written out, and each term's limit is read as the guard's own. From
# 0, on x^3 - 2x + jc, Newton converges for j = 1 and 2 at c = 1/2, to
# 0.2586520225 and 0.6180339887 (mpmath), and at c = 1 cycles for j = 2:
# y_n is 1 at even n, 0 at odd.
paired(c_n) = {
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - (nv(c)_(k-1)^3 - 2*nv(c)_(k-1) + c)/(3*nv(c)_(k-1)^2 - 2)
    y_n | sum_(j=1)^2 lim nv(j*c_n) < 2 = 1
    y_n = 0
}

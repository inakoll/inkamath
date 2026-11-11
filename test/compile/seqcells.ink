# A sequence with parameters walked (DESIGN.md, next in line): a term read
# as f(args)_K, where f is by cells, reaches a sequence by cells, or K is
# not a constant, compiles to a function of the header that walks k from
# the lowest base clause to K with a window of the terms before, as a
# limit's function walks, without its closing test. Every number below is
# worked out apart from the interpreter and the compiler, with Python's
# fractions and doubles, and none is recorded.

# 'heat': a stencil, the heat equation on 8 cells with reflecting ends,
# u(u0, c)_k by cells, each neighbour at max(i - 1, 1) and min(i + 1, 8),
# a place that folds at each cell. a is read at a constant index, b at the
# step's own, its walk from 0 again at each step, so that b_n = M^n u0, M
# the stencil's matrix. The bump's cells are p*(i - 1)*(8 - i)/16, their
# sum 7p/2, which every term keeps; at n = 7, where x is 1, a is
# [13/64; 45/128; 69/128; 21/32; 21/32; 69/128; 45/128; 13/64]. The step's
# doubles are the interpreter's, in its order, checked for 40 steps,
# 'inkamath --check seqcells.ink heat --steps 40':
#
#     heat: 40 steps from 0, against exact values until 0 and inexact ones from there
#     heat.a: within 0
#     heat.b: within 0
#
# and with --float within a hundred units of a float, Python's floats in
# the stencil's order 2.8 at most. Its
# header, 'inkamath --compile seqcells.ink stencil -o heat.h', defines one
# walk, heat_walk0, which a reads at 3 and b at the index.
bump(p)[i<=8] = p*(i - 1)*(8 - i)/16
u(u0, c)_0 = u0
u(u0, c)_k[i<=8] = u(u0, c)_(k-1)[i] + c*(u(u0, c)_(k-1)[max(i - 1, 1)] - 2*u(u0, c)_(k-1)[i] + u(u0, c)_(k-1)[min(i + 1, 8)])
stencil(x_n) = {
    a_n = u(bump(x_n), 1/4)_3
    b_n = u(bump(x_n), 1/4)_n
}
heat = stencil(x_n = ~(n/7))

# 'rim': two cells of one term read at a moving index are one call of the
# walk, which ends.h calls once. At n = 5, x = 5/7, e is
# 0.6222098214285714, (M^5 u0)[1] + (M^5 u0)[4]. Checked for 30 steps, as
# are idx and fixed; sens for 40.
#
#     rim: 30 steps from 0, against exact values until 0 and inexact ones from there
#     rim.e: within 0
ends(x_n) = {
    e_n = u(bump(x_n), 1/4)_n[1] + u(bump(x_n), 1/4)_n[4]
}
rim = ends(x_n = ~(n/7))

# 'sens': grad through the walk, forward mode, its parts carried in its
# window. g by a single value at a moving index is (M^n bump(1))[4], whose
# parts the interpreter takes exactly, a constant having none: 0.6005859375
# at n = 5 and 0.5572509765625 at 7. The step's doubles hold them to n = 36
# and round them past, 5.6e-17 from them at most, at 37, by Python's
# doubles in the stencil's order. v by the bump's 8 cells gives way to
# forward mode, as a limit does, a pass a cell: row 4 of M^3, [1/64, 3/32,
# 15/64, 5/16, 15/64, 3/32, 1/64, 0] at every n, each exact in a double.
#
#     sens: 40 steps from 0, against exact values until 0 and inexact ones from there
#     sens.g: within <e>, under 1e-15
#     sens.v: within 0
parts(x_n) = {
    g_n = grad_(p = x_n) u(bump(p), 1/4)_n[4]
    v_n = grad_(w = bump(x_n)) u(w, 1/4)_3[4]
}
sens = parts(x_n = ~(n/7))

# 'idx': the index's semantics. q(x)_k = x/2^k + 2 - 2^(1-k). a is walked
# from 0 at each step, n terms; b is NaN below the base, at n < 3, where the
# interpreter says 'q has no clause for index -3'; c is NaN at odd n, where
# it says 'an index must be a whole number, not 0.5'; d, a constant past the
# 1000 terms a step writes out, is walked where it was refused; and e, of a
# constant, walks in doubles where the interpreter's terms are exact
# fractions, 2.2e-16 from them at most, at n = 2, by Python's fractions:
#
#     idx: 30 steps from 0, against exact values until 0 and inexact ones from there
#     idx.a: within 0
#     idx.b: within 0
#     idx.c: within 0
#     idx.d: within 0
#     idx.e: within <e>, under 1e-15
q(x)_0 = x
q(x)_k = q(x)_(k-1)/2 + 1
walked(x_n) = {
    a_n = q(x_n)_n
    b_n = q(x_n)_(n - 3)
    c_n = q(x_n)_(n/2)
    d_n = q(x_n)_2000
    e_n = q(1/3)_n
}
idx = walked(x_n = ~(n/8))

# 'pair' and 'quad': the prelude's stability tests at a moving matrix,
# checked for 10 steps, 'inkamath --check seqcells.ink pair --steps 10'. G
# is [2x, 1/2; -1/4, x], its eigenvalues (3x +- (x^2 - 1/2)^(1/2))/2, a
# complex pair of modulus (2x^2 + 1/8)^(1/2) and real part 3x/2 where
# x^2 < 1/2; H, the 4x4's lower block, [-x, 1; -1, 2/3], a complex pair of
# modulus (1 - 2x/3)^(1/2) and real part 1/3 - x/2 throughout. x is
# (2n - 9)/10, -0.9 to 0.9 by 0.2, so that each transition is crossed: G's
# pair turns real between 0.7 and 0.9 either side; h is 1 where x < 0, n
# from 0 to 4, and 0 for the 4x4 at every x, H's real part above 0 below
# x = 2/3 and G's from 0; s is 1 where |x| < (7/16)^(1/2), n from 2 to 7,
# and for the 4x4, where H's modulus is below 1 too, from 5 to 7. hurwitz
# and schurcohn are taken in doubles by the interpreter as by the step, in
# the same order. rho and abscissa read the doubles as the fractions they
# are and bisect exactly (DESIGN.md, C275), and the step bisects in
# doubles: Python's doubles, in the prelude's order, are 6.7e-16 from the
# exact bisection at most for the 2x2 and 4.4e-16 for the 4x4, at n = 9:
#
#     pair: 10 steps from 0, against exact values until 0 and inexact ones from there
#     pair.G: within 0
#     pair.a: within <e>, under 1e-13
#     pair.h: within 0
#     pair.r: within <e>, under 1e-13
#     pair.s: within 0
#
# and quad's alike, these five lines and no other. rhou's and abscissau's
# 256 terms, which reach hurwitzr by cells, are walked, not written out:
# 'inkamath --compile seqcells.ink stab -o stab.h' is under 200,000 bytes
# and stab4's under 400,000, where written out they would be some 2.2 and
# 4.7 MB. abscissa compiles once abscissaq's recursion, which no
# polynomial whose first coefficient is 1 takes to its end, is NaN there
# rather than refused (DESIGN.md, the entry's prelude). Their values, by
# the eigenvalues' closed forms, are held to the interpreter's by
# cli.cmake's compile_seqcells_values.
stab(x_n) = {
    G_n = [2*x_n, 1/2; -1/4, x_n]
    h_n = hurwitz(charpoly(G_n))
    s_n = schurcohn(charpoly(G_n))
    r_n = rho(G_n)
    a_n = abscissa(G_n)
}
pair = stab(x_n = ~((2*n - 9)/10))
stab4(x_n) = {
    G_n = [2*x_n, 1/2, 0, 1/8; -1/4, x_n, 1/8, 0; 0, 0, -x_n, 1; 0, 0, -1, 2/3]
    h_n = hurwitz(charpoly(G_n))
    s_n = schurcohn(charpoly(G_n))
    r_n = rho(G_n)
    a_n = abscissa(G_n)
}
quad = stab4(x_n = ~((2*n - 9)/10))

# 'degen': C275's limit in the step, by the owner's decision. The step's
# rho and abscissa bisect in doubles, as compiled eig and smax do, their
# counts reading a characteristic polynomial in doubles, where the
# interpreter reads the doubles as the fractions they are. At x times the
# 4x4 identity, x 1/3's double, a fourfold eigenvalue, that polynomial's
# rounding parts the root, and the step's rho is 0.33338569110885224, by
# Python's doubles in the prelude's order, as the interpreter's own was
# 0.33339 before C275; the interpreter's is x, 0.33333333333333331. At
# every step, so '--check' reports the first and exits 1:
#
#     degen: 100 steps from 0, against exact values until 0 and inexact ones from there
#     degen.r: 0.33338569110885224 at 0, where the interpreter gives ~0.33333333333333331
#
# the line ending with the interpreter's estimate, if any, '; the
# interpreter's term about <e> from the exact one'.
fourfold(x_n) = {
    r_n = rho(x_n*[1, 0, 0, 0; 0, 1, 0, 0; 0, 0, 1, 0; 0, 0, 0, 1])
}
degen = fourfold(x_n = ~(1/3))

# 'fixed': a limit of a sequence by cells walks its cells as a step's term
# by cells is written, where 'j is not defined' refused it. Its limit is
# [2x; 4], each cell halving its distance to it; the walk stops by lim's
# rule in doubles as the interpreter's does:
#
#     fixed: 30 steps from 0, against exact values until 0 and inexact ones from there
#     fixed.y: within 0
hl(v)_0 = v
hl(v)_k[j<=2] = hl(v)_(k-1)[j]/2 + j*v[j]
halving(x_n) = {
    y_n = lim hl([x_n; 1])
}
fixed = halving(x_n = ~(n/7))

# What stays refused, each named by 'inkamath --compile' of a file of
#
#     a_n = w([x_n; 1; 2])_n
#     b_n = h(x_n)_n
#     c_n = r(x_n)_n
#     d_n = grad_(t = x_n)^2 q(t)_n
#     e_n = grad_(t = x_n) q(t)_(floor(t))
#     f_n = p(x_n)_n
#     g_n = q(x_n)_20000000
#     k_n = y(x_n)_(-1)
#     m_n = tq(x_n)_n
#     w(a)_0 = a
#     w(a[j<=m])_t[r<=m-t] = w(a)_(t-1)[r]
#     h(s)_0 = 0
#     h(s)_k = 2*h(s)_(floor(k/2)) + s
#     r(x)_0 = x
#     r(x)_k = r(x/2)_(k-1) + 1
#     q(x)_0 = x
#     q(x)_k = q(x)_(k-1)/2 + 1
#     p(x)_k = p(x)_(k-1)/2
#     y(x)_0 = [x; 1]
#     y(x)_k[j<=2] = y(x)_(k-1)[j]*x
#     tq(x)_0 = [1;; 0]
#     tq(x)_k = tq(x)_(k-1)*x/2 + [1;; x]
#
# where the interpreter, given x_n = n + 1/2, answers a at n < 3, b, c, d,
# m, and e where t is exact and its floor does not jump, and refuses f, g
# and k:
#
#     cannot compile a: a walk of w, whose terms change size
#     cannot compile b: h_(...): an index other than a whole multiple of k plus a constant
#     cannot compile c: r's term read with other arguments than its own
#     cannot compile d: a second derivative through a walk of q, for now
#     cannot compile e: an index that moves with t, for now
#     cannot compile f: p has no base clause to walk from
#     cannot compile g: q_20000000 is 20000000 terms from its base, and a fill stops at 10000000
#     cannot compile k: y has no clause for index -1
#     cannot compile m: a tensor in a walk of tq, for now

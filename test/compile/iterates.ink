# A sequence with parameters read at a constant index, compiled (DESIGN.md,
# next in line): f(x)_K is written out where it is read, as a sum with
# constant bounds is. Its terms are filled as the interpreter fills them,
# from the lowest base clause by the stride its reads give, each by the
# clause the interpreter takes at its index, a guard on the index folded;
# each term the term read reaches is a temporary of the step, as C140's are,
# and the term read is written where it is read. Every number below is
# worked out apart from the interpreter, with exact fractions, Python's
# doubles and numpy's floats; none is recorded.
#
# 'poly', the characteristic polynomial of test/data/signatures.ink by
# Faddeev-LeVerrier, as written there: fm and fc read each other, fc at its
# own index, the index a value, '/m', and det reads fc at n, a size its
# signature binds, 3 here. With A_n = [2, 2, 0; 1, 3, 1; 0, 1, n], c_n is
# [1; -(n + 5); 5n + 3; 2 - 4n] by the principal minors, d_n is 4n - 2 by
# permutations, and g_n, grad of det through both sequences, the cofactors
# [3n - 1, -n, 1; -2n, 2n, -2; 2, -2, 4], each cell by det at that cell 1
# less det at it 0. Every value the step computes, parts included, is a
# whole number below 1200, so exact in doubles and in floats:
#
#     poly: 100 steps from 0, against exact values
#     poly.A: within 0
#     poly.c: within 0
#     poly.d: within 0
#     poly.g: within 0
#
# and 'inkamath --check iterates.ink poly --float':
#
#     poly: 100 steps from 0 in float, against exact values
#     poly.A: within 0, 0 units of a float
#     poly.c: within 0, 0 units of a float
#     poly.d: within 0, 0 units of a float
#     poly.g: within 0, 0 units of a float
eigen(x_n) = {
    tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]
    id(n)[j<=n, k<=n] = j == k
    fm(A)_0 = 0*A
    fm(A[j<=n, k<=n])_m = A*fm(A)_(m-1) + fc(A)_(m-1)*id(n)
    fc(A)_0 = 1
    fc(A)_m = -tr(A*fm(A)_m)/m
    cp(A[j<=n, k<=n])[j<=n+1] = fc(A)_(j-1)
    det(A[j<=n, k<=n]) = (-1)^n*fc(A)_n
    A_n = [2, 2, 0; 1, 3, 1; 0, 1, x_n]
    c_n = cp(A_n)
    d_n = det(A_n)
    g_n = grad_(B = A_n) det(B)
}
poly = eigen(x_n = n)

# 'thirds', the same at x_n = ~(n/3), where the order of the operations
# shows: at 1, taken one after another as written, d is -0.6666666666666675
# and g's first cell -6.661338147750939e-16, where 4x - 2 and 3x - 1 at that
# x round to -0.6666666666666667 and -5.551115123125783e-17. Every value
# below is worked out in Python's doubles in the interpreter's order, a
# matrix product's sum from its first term and grad's part of a product
# a'*b + a*b', a matrix's at a time; the step takes the same order, so:
#
#     thirds: 100 steps from 0, against exact values until 0 and inexact ones from there
#     thirds.A: within 0; the interpreter's terms about <e> from the exact ones
#     thirds.c: within 0; the interpreter's terms about <e> from the exact ones
#     thirds.d: within 0; the interpreter's terms about <e> from the exact ones
#     thirds.g: within 0; the interpreter's terms about <e> from the exact ones
thirds = eigen(x_n = ~(n/3))

# 'plan', a model-predictive controller's few steps of projected gradient
# over a horizon of two moves, MANIFESTO.md's solver within a step with a
# fixed number of iterations: u(s)_k is the plan after k steps from 0 for
# the state s, its gradient by grad, each move clipped to [-1, 1] by a guard
# on its cell, tested in the step. The count, iters, is a parameter read
# where the step needs a constant, so it is compiled in, as a sum's bound
# is, and the header's first comment ends 'cannot change: iters.' The state
# is n/4 - 3: at -3 the plan is [1; 31/32], at 0 [0; 0], at 1 [-75/128;
# -57/256]; its first move is clipped up to -7/4 and from 7/4 on, both
# from 13/4. Every value, parts included, is a fraction over a power of
# two no larger than 2^16, exact in doubles:
#
#     plan: 100 steps from 0, against exact values
#     plan.y: within 0
mpc(eta = 1/8, iters = 4, x_n) = {
    H = [6, 2; 2, 4]
    F = [4; 2]
    J(v, s) = v'*H*v/2 + s*F'*v
    clip(v[j<=n])[j<=n] = v[j]
    clip(v[j<=n])[j<=n] | v[j] > 1 = 1
    clip(v[j<=n])[j<=n] | v[j] < -1 = -1
    u(s)_0 = [0; 0]
    u(s)_k = clip(u(s)_(k-1) - eta*grad_(v = u(s)_(k-1)) J(v, s))
    y_n = u(x_n)_iters
}
plan = mpc(x_n = n/4 - 3)

# 'graph', Kahan (2006), §10: 128 square roots, then 128 squares, which
# exactly give x back. r(x)_128 is read by s's base clause, one read
# filling another, so h_n is 256 terms, the interpreter's to the bit.
# Python's doubles, and numpy's floats, give 0 for every x = n/25 below 1
# and 1 from 1 on, where the exact terms are n/25. The interpreter's power
# of 1/2 is inexact wherever it is taken, 4^(1/2) and at 0 too, so its terms
# are from 0:
#
#     graph: 100 steps from 0, against exact values until 0 and inexact ones from there
#     graph.h: within 0; the interpreter's terms about <e> from the exact ones, past the tolerance from <n>
#
# the estimate and where it passes the tolerance being the disturbed runs',
# which no route apart from the interpreter gives. Its header,
# 'inkamath --compile iterates.ink roots -o roots.h', writes each term
# below the one read as a temporary, s_0 among them, and the one read where
# it is read:
#
#     const double t0_ = sqrt(0.0 + m_->x[0]);
#     const double t1_ = sqrt(0.0 + t0_);
#     ...
#     const double t126_ = sqrt(0.0 + t125_);
#     const double t127_ = sqrt(0.0 + t126_);
#     const double t128_ = pow(t127_, 2.0);
#     ...
#     m_->h[0] = pow(t254_, 2.0);
roots(x_n) = {
    r(x)_0 = x
    r(x)_k = r(x)_(k-1)^(1/2)
    s(x)_0 = r(x)_128
    s(x)_k = s(x)_(k-1)^2
    H(x) = s(x)_128
    h_n = H(x_n)
}
graph = roots(x_n = n/25)

# 'gaps', where the fill computes terms the read does not reach. h's term
# reads the one at half its index, floor(k/2), a constant at each term, so
# its stride is 1 and h_100 reaches h_50, h_25, h_12, h_6, h_3, h_1 and h_0
# alone: a_n = 127n. q reads two and three back, so its stride is 1 too,
# and q_7 does not reach the q_6 its fill computes: b_n = 5n + 2. Neither
# q_6 nor any of h's 93 others is written, a temporary nothing reads being a
# warning, which the step is built without:
#
#     gaps: 100 steps from 0, against exact values
#     gaps.a: within 0
#     gaps.b: within 0
reach(x_n) = {
    h(s)_0 = 0
    h(s)_k = 2*h(s)_(floor(k/2)) + s
    q(s)_0 = s
    q(s)_1 = 1
    q(s)_2 = 2*s
    q(s)_k = q(s)_(k-2) + q(s)_(k-3)
    a_n = h(x_n)_100
    b_n = q(x_n)_7
}
gaps = reach(x_n = n)

# 'memo', terms whose clauses name their index alike, each clause seeing
# its own index alone, as the interpreter binds it (C186): f_1 and f_2 are
# their index, 1 and 2, then each term 10 more, the model's c, so a_n is 22;
# q_3 is c, 10, as its base clause does not bind c, so e_n is 11. And
# terms kept apart where their arguments are written alike, by their exact
# value and their shape (C187): 1/2 and 1/2 + 2^-60 are one double, and g
# keeps 1/2 where it moves 2^-60 by 4^20, so b_n is n + 3/2 + 2^-19; h_1
# of a column and of a row is twice it, and s weighs each cell by its
# column, so d_n is (2n + 2) + (2n + 4), 4n + 6. A term read on the left
# of an 'and' is folded where the step starts, as a call is, so that x_-1,
# which its right reads, is never asked for (C188): t(n)_3 is 8n - 7, so
# l_n is 1 from 4 on, where x_(n-1) passes 2, and 0 before:
#
#     memo: 100 steps from 0, against exact values
#     memo.a: within 0
#     memo.b: within 0
#     memo.d: within 0
#     memo.e: within 0
#     memo.l: within 0
alike(c = 10, x_n) = {
    f(x)_0 = x
    f(x)_c | c < 3 = c
    f(x)_j | j > 0 = f(x)_(j-1) + c
    q(x)_0 = x
    q(x)_c = q(x)_(c-1) + 1
    q(x)_3 = c
    g(x)_0 = x
    g(x)_k = g(x)_(k-1)*4 - 3/2
    h(v)_0 = v
    h(v)_k = h(v)_(k-1)*2
    s(M[j<=r, k<=w]) = sum_(j=1)^r sum_(i=1)^w M[j,i]*i
    t(y)_0 = y
    t(y)_k = t(y)_(k-1)*2 - 1
    a_n = f(x_n)_4
    b_n = x_n + g(1/2)_20 + 2*g(1/2 + 2^-60)_20
    d_n = s(h([x_n; 1])_1) + s(h([x_n, 1])_1)
    e_n = q(x_n)_4
    l_n | t(n)_3 > 5 and x_(n-1) > 2 = 1
    l_n = 0
}
memo = alike(x_n = n)

# 'unread', a term the fill computes and no read reaches: g reads none of
# its own terms, so g(x)_4 is f(x)_4, f_2 times x, f_0 times x twice, x^2,
# and g_1 and g_3, which would read f_-1, are never asked for. A term the
# fill cannot compute is refused only where a read reaches it, as the
# interpreter's recursion does:
#
#     unread: 100 steps from 0, against exact values
#     unread.y: within 0
aside(x_n) = {
    f(x)_0 = 1
    f(x)_k = f(x)_(k-2)*x
    g(x)_0 = x
    g(x)_k = f(x)_k
    y_n = g(x_n)_4
}
unread = aside(x_n = n)

# 'lone', a function of the prelude given a number, which the header writes
# as a function of its own, holding no sequence with parameters nor the
# sizes its signature names: z, charpoly of one, was refused "a sequence
# with parameters in a limit's terms", and y, rhoe of one, "m is not
# defined". Each is written where it is called now (C213):
#
#     lone: 100 steps from 0, against exact values
#     lone.y: within 0
#     lone.z: within 0
single(x_n) = {
    y_n = rhoe(x_n)
    z_n = charpoly(x_n)[2]
}
lone = single(x_n = n + 1)

# What stays refused, each named in 'inkamath --compile' of a file of
#
#     a_n = r(x_n)_n
#     b_n = r(x_n)_1001
#     c_n = q(x_n)_3
#     d_n = z(x_n)_2
#     e_n = p(x_n)_2
#     f_n = w(x_n)_70
#     g_n = r(x_n)_(1/2)
#     h_n = lim nw(x_n)
#     u_n = y(x_n)_0
#     nw(a)_0 = a
#     nw(a)_k = nw(a)_(k-1)/2 + r(a)_2
#     p(x)_0 = x
#     p(x)_k = p(x)_(k+1)/2
#     q(x)_0 = x
#     q(x)_k = q(x)_(k-2) + 1
#     r(x)_0 = x
#     r(x)_k = r(x)_(k-1)/2 + 1
#     w(x)_0 = x
#     w(x)_k = w(x/2)_(k-1)
#     y(x)_0 = [x; 1]
#     y(x)_k[j<=2] = x*j
#     z(x)_0 = x
#     z(x)_k = z(x)_k/2
#
# where the interpreter, given x_n = 2, answers a, b, f, h and u, refuses c
# and g in the words the compiler takes, and runs out of depth for d and e:
#
#     cannot compile a: a sequence with parameters read at an index that is not a constant
#     cannot compile b: r_1001 is 1001 terms from its base, and a step writes out at most 1000
#     cannot compile c: q has no clause for index -1
#     cannot compile d: z is defined by itself
#     cannot compile e: p_(...): a term after the one being computed
#     cannot compile f: calls nested 64 deep, which a recursion its guards do not end would pass
#     cannot compile g: an index must be a whole number, not 0.5
#     cannot compile h: a sequence with parameters in a limit's terms, for now
#     cannot compile u: a sequence with parameters by cells, for now
#
# on standard output, exiting 1.

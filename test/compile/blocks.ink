# Block literals compiled (DESIGN.md, next in line): '[A, B; C, D]' laid out
# as the interpreter lays it out, each band as tall or as wide as its
# largest block, a single value stretched over its block, and each block's
# cells written into their place, so the header is the one the same literal
# written cell by cell gives. Every number below is worked out apart from
# the interpreter, with exact fractions and sympy in Python and Python's
# doubles in the step's order, numpy's and scipy's solvers beside them, then
# seen to be the interpreter's; none is recorded. Today every model is
# refused, 'a matrix built from matrices'.
#
# 'dare', the structure-preserving doubling algorithm for the discrete
# Riccati equation X = A'XA - A'XB(r + B'XB)^-1 B'XA + Q (Chu, Fan, Lin and
# Wang 2004): from A_0 = A, G_0 = B B'/r, H_0 = Q,
# W_k = (I + G_k H_k)^-1 and
#
#     A_(k+1) = A_k W_k A_k
#     G_(k+1) = G_k + A_k W_k G_k A_k'
#     H_(k+1) = H_k + A_k' H_k W_k A_k
#
# the three iterates packed into one term, d(r)_k = [A_k, G_k, H_k], read
# at a constant number of doublings, 6, compiled in, as iterates.ink's plan
# is. The plant is lqr.ink's double integrator, Q = I, and the weight r_n is
# 1 + n/4. Y is the same algorithm as three sequences with parameters,
# which compiles today, so D = X - Y is the packing's own cost: nothing, as
# laying the blocks out computes nothing. d(1)_1 is
#
#     [1, 3/2, 1/2, 1/2, 2, 1; 0, 1/2, 1/2, 3/2, 1, 5/2]
#
# and X_0 = H_6 rounds to [2.947122966707013, 2.3692054070924664;
# 2.3692054070924664, 4.61313426099618], within 1.4e-14 of scipy's
# solve_discrete_are, whose own error is that size. At r = 5/4 the equation's
# solution is rational, [3, 5/2; 5/2, 5], with the gain F = [2/5, 6/5], and
# X_1 rounds to it. For every n, H_6 is within 5e-18, relatively, of H_9, so
# 6 doublings solve the equation to a double's precision; the exact terms
# stay below 300 digits. The step rounds where the interpreter does not:
#
#     dare: 100 steps from 0, against exact values
#     dare.X: within 7.1e-15
#     dare.F: within 2.2e-16
#     dare.Y: within 7.1e-15
#     dare.D: within 0
riccati(doublings = 6, r_n) = {
    A = [1, 1; 0, 1]
    B = [0; 1]
    Q = [1, 0; 0, 1]
    I[j<=2, k<=2] = j == k
    a(S)[j<=2, k<=2] = S[j, k]
    g(S)[j<=2, k<=2] = S[j, k+2]
    h(S)[j<=2, k<=2] = S[j, k+4]
    w(S) = (I + g(S)*h(S))^(0-1)
    sda(S) = [a(S)*w(S)*a(S), g(S) + a(S)*w(S)*g(S)*a(S)', h(S) + a(S)'*h(S)*w(S)*a(S)]
    d(r)_0 = [A, B*B'/r, Q]
    d(r)_k = sda(d(r)_(k-1))
    wk(r)_k = (I + gk(r)_k*hk(r)_k)^(0-1)
    ak(r)_0 = A
    ak(r)_k = ak(r)_(k-1)*wk(r)_(k-1)*ak(r)_(k-1)
    gk(r)_0 = B*B'/r
    gk(r)_k = gk(r)_(k-1) + ak(r)_(k-1)*wk(r)_(k-1)*gk(r)_(k-1)*ak(r)_(k-1)'
    hk(r)_0 = Q
    hk(r)_k = hk(r)_(k-1) + ak(r)_(k-1)'*hk(r)_(k-1)*wk(r)_(k-1)*ak(r)_(k-1)
    X_n = h(d(r_n)_doublings)
    Y_n = hk(r_n)_doublings
    D_n = X_n - Y_n
    F_n = B'*X_n*A/(r_n + B'*X_n*B)
}
dare = riccati(r_n = 1 + n/4)

# 'smith', Smith's doubling for the Stein equation P = A P A' + I, the
# steady covariance of x_n = A x_(n-1) + e_n: A_(k+1) = A_k^2 and P_(k+1) =
# P_k + A_k P_k A_k', the two packed into one term under lim, which walks a
# single sequence, with A = [1/2, x; 0, 1/4]. The exact solution at x = 1 is
# [332/105, 32/105; 32/105, 16/15], at x = 0 [4/3, 0; 0, 16/15], and in
# general the solution of the linear system sympy solves; twelve doublings
# in doubles come within 2.3e-16 of it, relatively, at every x = n/4, and
# scipy's solve_discrete_lyapunov as close. x_n is inexact, so the
# interpreter's terms are doubles taken in the step's order, and the limit
# stops where the step's does, as steady.ink's power iteration does:
#
#     smith: 100 steps from 0, against exact values until 0 and inexact ones from there
#     smith.P: within 0; the interpreter's terms about <e> from the exact ones
stein(x_n) = {
    I[j<=2, k<=2] = j == k
    a(S)[j<=2, k<=2] = S[j, k]
    p(S)[j<=2, k<=2] = S[j, k+2]
    s(M)_0 = [M, I]
    s(M)_k = [a(s(M)_(k-1))*a(s(M)_(k-1)), p(s(M)_(k-1)) + a(s(M)_(k-1))*p(s(M)_(k-1))*a(s(M)_(k-1))']
    P_n = p(lim s([1/2, x_n; 0, 1/4]))
}
smith = stein(x_n = ~(n/4))

# 'deadbeat', integral action on lqr.ink's double integrator, the plant's
# matrices the model's parameters: the augmented system [A, 0; -C, 1] and
# [B; 0], its gain by Ackermann's formula, [0, 0, 1] times the inverse of the
# controllability matrix times Aa^3, the deadbeat polynomial. The
# controllability matrix [B, AB, A^2 B] is a sequence with parameters whose
# terms widen by a block, kr(M, N)_k being 3 x k, read at the size n its
# signature binds, 3; computed from the parameters, it is in the update.
# By sympy, the gain placing every eigenvalue of Aa - Ba Kf at 0 is
# [3, 3, -1]; the controllability matrix is [0, 1, 2; 1, 1, 1; 0, 0, -1],
# whose inverse [-1, 1, -1; 1, 0, 2; 0, 0, -1] Gauss-Jordan finds exactly in
# doubles. The reference r_n = floor(n/10) steps by 1 every ten steps, and
# the output settles on it in two: y_n = floor((n-2)/10) from 2, y_0 = y_1 =
# 0. Every value is a whole number below 30:
#
#     deadbeat: 100 steps from 0, against exact values
#     deadbeat.x: within 0
#     deadbeat.y: within 0
servo(A = [1, 1; 0, 1], B = [0; 1], C = [1, 0], r_n) = {
    kr(M, N)_1 = N
    kr(M, N)_k = [kr(M, N)_(k-1), M^(k-1)*N]
    ctrb(M[j<=n, k<=n], N) = kr(M, N)_n
    Aa = [A, 0; -C, 1]
    Ba = [B; 0]
    Kf = [0, 0, 1]*ctrb(Aa, Ba)^(0-1)*Aa^3
    x_0 = [0; 0; 0]
    x_n = (Aa - Ba*Kf)*x_(n-1) + [0; 0; r_n]
    y_n = [C, 0]*x_n
}
deadbeat = servo(r_n = floor(n/10))

# 'nested', blocks within blocks: w, a delay line whose newest sample is put
# above the three before it; M, a 4x4 block matrix beside w, w' below it and
# 1 in the corner, its zero block a single value stretched over 2x2; T, a
# tensor whose slices are block literals, each with a single value stretched
# over a 2x1 block; S, a short row among blocks, ended by a 0 stretched over
# 2x2. x_n is (-1)^n n/4, so at 3, x_3 = -3/4 and
#
#     w_3 = [-3/4; 1/2; -1/4; 0]
#     M_3 = [1/2, 1, -3/8, -3/4, -3/4; 0, 1/2, 0, -3/8, 1/2;
#            0, 0, 1/2, 1, -1/4; 0, 0, 0, 1/2, 0; -3/4, 1/2, -1/4, 0, 1]
#     T_3 = [1/2, 1, -3/4; 0, 1/2, -3/4;; 1/2, 1/2, 0; 1/2, 1, 1/2]
#     S_3 = [1/2, 1, -3/8, -3/4; 0, 1/2, 0, -3/8; 1/2, 1, 0, 0; 0, 1/2, 0, 0]
#     y_3 = 59/8
#
# as numpy's block and sympy's BlockMatrix lay them out. Every value to 99
# is a multiple of 1/16 below 2^10, which doubles and floats hold:
#
#     nested: 100 steps from 0, against exact values
#     nested.S: within 0
#     nested.w: within 0
#     nested.M: within 0
#     nested.T: within 0
#     nested.y: within 0
#
# and in float, 'inkamath --check blocks.ink nested --float', each 'within 0,
# 0 units of a float'.
stacked(x_n) = {
    A = [1/2, 1; 0, 1/2]
    w_0 = [0; 0; 0; 0]
    w_n = [x_n; [w_(n-1)[1]; w_(n-1)[2]; w_(n-1)[3]]]
    M_n = [[A, x_n*A; 0, A], w_n; w_n', 1]
    T_n = [A, w_n[1];; w_n[2], A']
    S_n = [A, x_n*A; A]
    y_n = [1, 1, 1, 1, 1]*M_n*[1; 2; 3; 4; 5]
}
nested = stacked(x_n = (-1)^n*n/4)

# 'sloped', grad through blocks, each block's part laid out as its value is,
# 0 where a block has none. g: with u = [1; 2; 3; 4], u'[tA, t^2; 0, At]u
# is 160t + 21t^2, t^2 and 0 each stretched over 2x2, so g_n = 160 + 42x_n.
# c, by the matrix M = x_n A: u'[M, M'; 0, MM]u, whose gradient sympy gives
# as [78x + 4, 135x + 8; 110x + 6, 188x + 12], at x_3 = 3/4 [125/2, 437/4;
# 177/2, 153]. With x_n = n/4, every value is a multiple of 1/4 below 2^20:
#
#     sloped: 100 steps from 0, against exact values
#     sloped.c: within 0
#     sloped.g: within 0
slopes(x_n) = {
    A = [1, 2; 3, 4]
    u = [1; 2; 3; 4]
    g_n = grad_(t = x_n) u'*[t*A, t^2; 0, A*t]*u
    c_n = grad_(M = x_n*A) u'*[M, M'; 0, M*M]*u
}
sloped = slopes(x_n = n/4)

# The layout, cell by cell: 'inkamath --compile aug.ink -o aug.h' of a file
# aug.ink holding
#
#     A = [1, 1; 0, 1]
#     B = [0; 1]
#     C = [1, 0]
#     K = [3, 3]
#     ki = 1
#     Aa = [A, 0; -C, 1]
#     Ba = [B; 0]
#     Kf = [K, -ki]
#     x_0 = [0; 0; 0]
#     x_n = (Aa - Ba*Kf)*x_(n-1) + [0; 0; r_n]
#     y_n = [C, 0]*x_n
#
# writes, byte for byte, the header of a file aug.ink in which each literal
# is written cell by cell, as compiles today:
#
#     Aa = [A[1,1], A[1,2], 0; A[2,1], A[2,2], 0; -C[1,1], -C[1,2], 1]
#     Ba = [B[1,1]; B[2,1]; 0]
#     Kf = [K[1,1], K[1,2], -ki]
#     y_n = [C[1,1], C[1,2], 0]*x_n
#
# each block's cells read where they are, a parameter's from its field, with
# no array, copy or loop for the literal; among its lines
#
#     m_->x[0][2][0] = m_->index_ == 0 ? 0.0 : (0.0 - m_->C[0][0] + (0.0 - 0.0 * m_->K[0][0])) * m_->x[1][0][0] + (0.0 - m_->C[0][1] + (0.0 - 0.0 * m_->K[0][1])) * m_->x[1][1][0] + (1.0 + (0.0 - 0.0 * (0.0 - m_->ki))) * m_->x[1][2][0] + m_->r[0];
#     m_->y[0] = m_->C[0][0] * m_->x[0][0][0] + m_->C[0][1] * m_->x[0][1][0] + 0.0 * m_->x[0][2][0];
#
# What stays refused, each named in 'inkamath --compile' of a file of
#
#     a_n = [T, x_n]
#     b_n = [A, [x_n, 1]]
#     c_0 = x_0
#     c_n = [c_(n-1), x_n]
#     d_n = lim e(x_n)
#     e(r)_0 = [I, I]
#     e(r)_k = [(I + r*f(e(r)_(k-1)))^(0-1), I]
#     f(S)[j<=2, k<=2] = S[j, k]
#     A = [1, 2; 3, 4]
#     I[j<=2, k<=2] = j == k
#     T = [1;; 2]
#     x_n = n
#
# where the interpreter refuses a in the words below, gives b_2 as [1, 2, 2,
# 1; 3, 4, 1, 1], the row [2, 1] continued by its corner, which C41 keeps
# and records as no meaning, c_3 as [0, 1, 2, 3], a term that widens at
# every step, and d_2 as [1/2, 0, 1, 0; 0, 1/2, 0, 1], its 1/2 inexact:
#
#     cannot compile a: a tensor cannot be a block of a literal, only a matrix can
#     cannot compile b: a block that does not fill its band
#     cannot compile c: its clauses have different shapes
#     cannot compile d: a matrix inverse outside a sequence
#
# on standard output, exiting 1. Today a, b and d are refused as 'a matrix
# built from matrices'; d stays refused until a limit's function has
# temporaries (DESIGN.md, next in line), which is what the doubling
# algorithm for the Riccati equation needs under lim.

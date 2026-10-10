# Temporaries in a limit's function (DESIGN.md, next in line): a limit's
# iterate declares what it computes once, as a step does, so a matrix
# inverse, a call nested in an argument, a product's operands and a
# sequence with parameters read at a constant are each written once per
# iterate. Every number below is worked out apart from the interpreter,
# with mpmath, scipy, sympy and Python's doubles and fractions, the
# limit's stopping rule transcribed from convergence.hpp; none is recorded.
# Today 'kinetic', 'jacobian' and 'doubling' are refused, "a matrix inverse
# inside a limit's terms", 'inset', "a sequence with parameters in a
# limit's terms, for now", and 'layered', "a limit of matrices inside a
# limit's terms".
#
# 'kinetic', backward Euler on Robertson's kinetics (Robertson 1966; Hairer
# and Wanner II, IV.1), robertson.ink's 'rober' with each step's Newton
# iteration under lim rather than read at 12, its matrix inverse written
# as the paper writes it. y_0 is inexact, so the interpreter's terms are
# doubles taken in the step's order, and each limit stops where the step's
# does. The roots of z - y - h f(z), found in mpmath to 60 digits, are
#
#     y_1   = [0.99615133310359166; 3.5651160504271875e-05; 0.0038130157359040646]
#     y_100 = [0.84200607353507096; 1.6284288113351236e-05; 0.15797764217681569]
#
# their sum 1 within 1e-60. Python's doubles, Newton under the limit's rule
# and numpy's inverse, come within 5.1e-16 of them, relatively, in every
# cell to 100, in 3 to 12 iterations a step:
#
#     kinetic: 100 steps from 0, against exact values until 0 and inexact ones from there
#     kinetic.y: within 0; the interpreter's terms about <e> from the exact ones
k1 = 0.04
k2 = 3*10^7
k3 = 10^4
f(y) = [-k1*y[1] + k3*y[2]*y[3]; k1*y[1] - k3*y[2]*y[3] - k2*y[2]*y[2]; k2*y[2]*y[2]]
J(y) = [-k1, k3*y[3], k3*y[2]; k1, -k3*y[3] - 2*k2*y[2], -k3*y[2]; 0, 2*k2*y[2], 0]
Jg(y)[j<=3, k<=3] = (grad_(v = y) f(v)[j])[k]
I = [1 0 0; 0 1 0; 0 0 1]
backward(h = 1/10, y0 = ~[1; 0; 0]) = {
    nw(yp)_0 = yp
    nw(yp)_k = nw(yp)_(k-1) - (I - h*J(nw(yp)_(k-1)))^-1*(nw(yp)_(k-1) - yp - h*f(nw(yp)_(k-1)))
    y_0 = y0
    y_n = lim nw(y_(n-1))
}
kinetic = backward()

# 'jacobian', the same with the Jacobian taken by grad inside the limit's
# function, each cell's part a temporary as a step's is (C140). Its
# Jacobian is J's to the bit, k2*y2 + k2*y2 being 2*k2*y2 exactly, so its
# terms are kinetic's, the interpreter's and Python's alike:
#
#     jacobian: 100 steps from 0, against exact values until 0 and inexact ones from there
#     jacobian.y: within 0; the interpreter's terms about <e> from the exact ones
graded(h = 1/10, y0 = ~[1; 0; 0]) = {
    nw(yp)_0 = yp
    nw(yp)_k = nw(yp)_(k-1) - (I - h*Jg(nw(yp)_(k-1)))^-1*(nw(yp)_(k-1) - yp - h*f(nw(yp)_(k-1)))
    y_0 = y0
    y_n = lim nw(y_(n-1))
}
jacobian = graded()

# 'doubling', blocks.ink's structure-preserving doubling for the discrete
# Riccati equation (Chu, Fan, Lin and Wang 2004) under lim instead of read
# at 6 doublings: its three iterates packed into one term, W_k = (I + G_k
# H_k)^-1 a temporary of the iterate. r_n is inexact, so the interpreter's
# terms are doubles in the step's order. At r = 1, X rounds to scipy's
# solve_discrete_are, [2.947122966707013, 2.3692054070924664;
# 2.3692054070924664, 4.61313426099618]; at r = 5/4 the solution is
# rational by sympy, [3, 5/2; 5/2, 5], with the gain F = [2/5, 6/5]. Python's
# doubles under the limit's rule stop after 6 to 8 doublings, within 2.3e-14
# of scipy, relatively, for every r_n to 99:
#
#     doubling: 100 steps from 0, against exact values until 0 and inexact ones from there
#     doubling.X: within 0; the interpreter's terms about <e> from the exact ones
#     doubling.F: within 0; the interpreter's terms about <e> from the exact ones
riccati(r_n) = {
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
    X_n = h(lim d(r_n))
    F_n = B'*X_n*A/(r_n + B'*X_n*B)
}
doubling = riccati(r_n = ~(1 + n/4))

# 'inset', iterates.ink's refused h: a sequence with parameters read at a
# constant in a limit's terms, r(a)_2 = a/4 + 3/2, its terms temporaries of
# the iterate as they are of a step. The limit is a/2 + 3. x_n = n is
# exact, and so is every term the walk takes: each is a dyadic rational of
# at most 47 bits, which a double holds, so the step's doubles are the
# interpreter's fractions and stop at the same term, within 9.9e-11 of
# n/2 + 3, as Python's fractions and doubles both find:
#
#     inset: 100 steps from 0, against exact values until 0 and inexact ones from there
#     inset.h: within 0; the interpreter's terms about <e> from the exact ones
halving(x_n) = {
    r(x)_0 = x
    r(x)_k = r(x)_(k-1)/2 + 1
    nw(a)_0 = a
    nw(a)_k = nw(a)_(k-1)/2 + r(a)_2
    h_n = lim nw(x_n)
}
inset = halving(x_n = n)

# 'layered', a limit of matrices inside a limit's terms, given a matrix
# argument: lim s(v) tends to 2v, so with v = [a; 1] each term of o adds
# 2a + 2 and the outer limit is 4(2a + 2)/3, 8(n + 3)/9 at a = n/3. a is
# inexact, so the interpreter's terms are doubles in the step's order;
# Python's doubles under the limit's rule, both walks, come within 3.3e-11
# of 8(n + 3)/9, relatively, to 99:
#
#     layered: 100 steps from 0, against exact values until 0 and inexact ones from there
#     layered.q: within 0; the interpreter's terms about <e> from the exact ones
layers(x_n) = {
    s(v)_0 = v
    s(v)_k = s(v)_(k-1)/2 + v
    o(a)_0 = a
    o(a)_k = o(a)_(k-1)/4 + [1, 1]*lim s([a; 1])
    q_n = lim o(x_n)
}
layered = layers(x_n = ~(n/3))

# The header, each value written once per iterate: 'inkamath --compile
# cramer.ink -o cramer.h' of a file cramer.ink holding k1, k2, k3, f, J and
# I as above and
#
#     c(j) = mod(j - 1, 3) + 1
#     adj(A)[j<=3, k<=3] = A[c(k+1), c(j+1)]*A[c(k+2), c(j+2)] - A[c(k+1), c(j+2)]*A[c(k+2), c(j+1)]
#     det3(A) = (A[1]*adj(A))[1,1]
#     solve(A, b) = adj(A)*b/det3(A)
#     h = 1/10
#     nw(yp)_0 = yp
#     nw(yp)_k = nw(yp)_(k-1) - solve(I - h*J(nw(yp)_(k-1)), nw(yp)_(k-1) - yp - h*f(nw(yp)_(k-1)))
#     y_0 = [1; 0; 0]
#     y_n = lim nw(y_(n-1))
#
# the inverse by Cramer's rule, which compiles today to 237 KB, 'm_->h *'
# written 2,169 times. Each call nested in solve's arguments is a temporary
# of the iterate, so h is read once per cell of h*J(z) and of h*f(z): 'm_->h *'
# 12 times, and the header under 32 KB. With J replaced by Jg, 12.9 MB
# today, 'm_->h *' 12 times too and the header under 32 KB. With solve(A,
# b) replaced by A^-1*b, as kinetic writes it, refused today, the header
# holds 'm_->h *' 12 times, 'cramer_inverse3_(' twice, its definition and
# its one call, in the limit's function, and is under 32 KB; compiled with
# --float, it holds no 'double', and 'float v0_[3][3] = ' declares the
# inverse's array, each value of the iterate a float as the step's are.
#
# What moves elsewhere: iterates.ink's h and blocks.ink's d compile, so
# 'cannot compile h: a sequence with parameters in a limit's terms, for
# now' and 'cannot compile d: a matrix inverse inside a limit's terms' leave
# the refusals 'compile_iterates_refused' and 'compile_blocks_refused' in
# test/cli.cmake expect, every other line staying.

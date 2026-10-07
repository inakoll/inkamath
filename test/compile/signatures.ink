# A size bound by a function's signature, compiled (DESIGN.md, next in line):
# a call is compiled where it is made, at its argument's shape, so each size
# its signature names is a constant there and folds, and a sum to n unrolls
# as one to 3 does. tr is called at 3x3 and at 2x2. Every number below is
# worked out apart from the interpreter, none recorded. Before the sized
# parameters were read, they were dropped (C150), and 'inkamath --compile'
# refused the model: 'cannot compile c: c2 takes no arguments'.
#
# The header 'inkamath --compile signatures.ink invariants' writes is the one
# written with every size a number, byte for byte, as sized.ink's is, from a
# file of the same name that test/CMakeLists.txt writes:
#
#     invariants() = {
#         tr(M) = sum_(j=1)^3 M[j,j]
#         tr2(M) = sum_(j=1)^2 M[j,j]
#         c2(M) = (tr(M)^2 - tr(M*M))/2
#         sq(v) = sum_(j=1)^3 v[j]^2
#         outer(x, y)[j<=3, k<=2] = x[j]*y[k]
#         ...
#         u_n = tr2([n, 1; 1, n])
#         ...
#     }
#
# its other lines as below. Its terms are whole numbers below 10^8 at every
# step to 99, t_n = 5 - n, u_n = 2n, c_n = 5 - 6n, v_n = [n^2 + 2; 3n + 3;
# -n], s_n = (n^2 + 2)^2 + (3n + 3)^2 + n^2, o_n = [v_n, -v_n] and g_n =
# 2*A_n', so the step is exact:
#
#     inv: 100 steps from 0, against exact values
#     inv.A: within 0
#     inv.c: within 0
#     inv.g: within 0
#     inv.t: within 0
#     inv.u: within 0
#     inv.v: within 0
#     inv.o: within 0
#     inv.s: within 0
invariants() = {
    tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]
    c2(M[j<=n, k<=n]) = (tr(M)^2 - tr(M*M))/2
    sq(v[j<=n]) = sum_(j=1)^n v[j]^2
    outer(x[j<=m], y[k<=n])[j<=m, k<=n] = x[j]*y[k]
    A_n = [2, n, 0; 1, 3, 1; 0, 1, -n]
    v_n = A_n*[1; n; 2]
    t_n = tr(A_n)
    u_n = tr([n, 1; 1, n])
    c_n = c2(A_n)
    s_n = sq(v_n)
    o_n = outer(v_n, [1; -1])
    g_n = grad_(B = A_n) tr(B*B)
}
inv = invariants()

# What 'inkamath --compile' refuses of a file of its own, in the
# interpreter's words, its shapes being static (test/cli.cmake):
#
#     tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]
#     w_n = tr([n, 1, 2; 3, 4, 5])
#
#     cannot compile w: tr takes M[j<=n, k<=n], not a 2x3 matrix
#
# The characteristic polynomial of test/data/signatures.ink, cp and det,
# stays refused, its fc(A)_n a function's sequence read at an index:
# 'cannot compile d: a sequence with parameters'. Every header in
# test/compile/expected stays byte for byte as it is.

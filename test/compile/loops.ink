# Loops in generated code (DESIGN.md, next in line), specified before it is
# built: test/cli.cmake's loops parts compile these models and hold their
# headers to the loops each increment writes, every value as before.
#
# 'spin': a product of 27 multiplications or more read in place, A*x_(n-1);
# transposed, A'*x_n; an operand made an array, (A + x_n)*x_n; a tensor by a
# matrix, slice by slice; and B*v_(n-1), 4 multiplications, written out.
spin(A = [0, -1, 0; 1, 0, 0; 0, 0, 1], B = [0, 1; 1, 0]) = {
    v_0 = [1; 2]
    v_n = B*v_(n-1)
    w_0 = [1, 0, 0; 0, 1, 0; 0, 0, 1;; 0, 1, 0; 1, 0, 0; 0, 0, 1]
    w_n = w_(n-1)*A
    x_0 = [1, 2, 3; 4, 5, 6; 7, 8, 9]
    x_n = A*x_(n-1)
    y_n = A'*x_n
    z_n = (A + x_n)*x_n
}

# 'solve': a product of an inverse, which may be NaN, read by name where a
# decision tests it; x, whose cells are each the first's at its place, and
# its whole-term NaN test.
solve(A = [2, 1, 0; 1, 2, 1; 0, 1, 2], x0 = [1, 0, 0; 0, 1, 0; 0, 0, 1]) = {
    s_n | (A^-1*x_(n-1))[1, 1] > 0 = 1
    s_n = 0
    x_0 = x0
    x_n = A^-1*x_(n-1)
}

# The sweep's product G*X', G the adjoint of W*X: in 'dense' every adjoint
# there, in 'gated' each where its unit's clause holds, in 'diag' only on
# the diagonal, which stays written out.
dense(eta = 1/4, W0 = [1, 0, 0; 0, 1, 0; 0, 0, 1]/2, X_n[i<=3, j<=3]) = {
    L(W, X) = sum_(i=1)^3 sum_(j=1)^3 tanh((W*X)[i, j])
    W_0 = W0
    W_n = W_(n-1) - eta*grad_(V = W_(n-1)) L(V, X_n)
}
gated(eta = 1/4, X_n[i<=3, j<=3]) = {
    r(z) | z > 0 = z
    r(z) = 0
    L(W, X) = sum_(i=1)^3 sum_(j=1)^3 r((W*X)[i, j])
    W_0 = [1, 0, 0; 0, 1, 0; 0, 0, 1]/2
    W_n = W_(n-1) - eta*grad_(V = W_(n-1)) L(V, X_n)
}
diag(eta = 1/4, X_n[i<=3, j<=3]) = {
    L(W, X) = sum_(i=1)^3 tanh((W*X)[i, i])
    W_0 = [1, 0, 0; 0, 1, 0; 0, 0, 1]/2
    W_n = W_(n-1) - eta*grad_(V = W_(n-1)) L(V, X_n)
}

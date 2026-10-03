# Limits of matrices compiled (DESIGN.md, next in line): at every step the
# steady state of a two-state chain whose switching rate is the input, and the
# dominant direction of a matrix the input builds, found by power iteration,
# the matrix passed to the limit as an array.
chain(u_n) = {
    walk(p)_0 = [1 0]
    walk(p)_k = walk(p)_(k-1)*[1-p, p; 1/4, 3/4]
    s_n = lim walk(u_n)
    pw(A)_0 = [1; 1]
    pw(A)_k = A*pw(A)_(k-1)/((A*pw(A)_(k-1))'*(A*pw(A)_(k-1)))^(1/2)
    d_n = lim pw([2, u_n; u_n, 1])
}
mark = chain(u_n = (n + 1)/(n + 2))

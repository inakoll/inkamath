# A recurrent cell, h_t = tanh(W h_(t-1) + U x_t + b), as a paper writes it
# (MANIFESTO.md, A paper conformance suite): tanh of each cell, read by one
# index out of the column the affine map gives, and tanh itself from exp,
# its series under lim, compiled as a loop.
cell(x_n) = {
    ex(z)_0 = 1
    ex(z)_1 = 1 + z
    ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k
    th(z) = 1 - 2/(lim ex(2*z) + 1)
    W = [1/2, -1/4; 1/4, 1/2]
    U = [1; -1/2]
    b = [0; 1/10]
    h_0 = [0; 0]
    h_n[j<=2] = th((W*h_(n-1) + U*x_n + b)[j])
}
rnn = cell(x_n = 1/(n+1))

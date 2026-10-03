# Scaled dot-product attention, one head (MANIFESTO.md, A paper conformance
# suite): softmax over each row of Q K^T / sqrt(d), its exp a limit compiled
# as a loop, then the rows weighting V. Per head and per batch it would need
# a tensor of rank 3, which the language does not have.
attend(d = 2) = {
    ex(z)_0 = 1
    ex(z)_1 = 1 + z
    ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k
    Q_n = [1 0; 0 1; 1 1]*n/10
    K_n = [1 1; 0 1; 1 0]
    V_n = [1 2; 3 4; 5 6]
    S_n = Q_n*K_n'/d^(1/2)
    A_n[i<=3, j<=3] = lim ex(S_n[i,j])/sum_(c=1)^3 lim ex(S_n[i,c])
    O_n = A_n*V_n
}
head = attend()

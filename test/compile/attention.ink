# Scaled dot-product attention, one head (MANIFESTO.md, A paper conformance
# suite): softmax over each row of Q K^T / sqrt(d), then the rows weighting
# V, the softmax's size read from S as the paper leaves it unwritten. Its sum
# still names the count of keys, 3, as the paper's sum over j does not. Per
# head and per batch is a tensor of rank 3: test/compile/tensor.ink's 'heads'.
attend(d = 2) = {
    Q_n = [1 0; 0 1; 1 1]*n/10
    K_n = [1 1; 0 1; 1 0]
    V_n = [1 2; 3 4; 5 6]
    S_n = Q_n*K_n'/d^(1/2)
    A_n[i,j] = exp(S_n[i,j])/sum_(c=1)^3 exp(S_n[i,c])
    O_n = A_n*V_n
}
head = attend()

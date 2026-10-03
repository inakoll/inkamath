# Softmax and cross-entropy (MANIFESTO.md, A paper conformance suite), with
# exp its series and log Newton's method on it, a limit inside a limit's
# terms, both compiled as loops. The logits shrink as the steps go on, so the
# distribution flattens and the loss for the third class grows towards log 3.
head(t = 1) = {
    ex(z)_0 = 1
    ex(z)_1 = 1 + z
    ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k
    lg(a)_0 = 0
    lg(a)_k = lg(a)_(k-1) - 1 + a/lim ex(lg(a)_(k-1))
    z_n[r<=3] = [1; 2; 3][r]*t/(n + 1)
    q_n[r<=3] = lim ex(z_n[r])
    s_n[r<=3] = q_n[r]/sum_(c=1)^3 q_n[c]
    loss_n = 0 - lim lg(s_n[3])
}
cls = head()

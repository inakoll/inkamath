# Two formulas equal exactly, one of which floating point breaks.

# Running variance of a stream far from zero (Welford 1962; Knuth, TAOCP 2,
# 4.2.2): the textbook E[x^2] - E[x]^2 cancels catastrophically, and parts
# even in double at its first step, where Welford's update holds:
#
#     moments.naive: 0.12249999865889549 at 1, where the interpreter gives 0.1225
stats(x_n) = {
    S1_0 = x_0
    S1_n = S1_(n-1) + x_n
    S2_0 = x_0^2
    S2_n = S2_(n-1) + x_n^2
    naive_n = S2_n/(n+1) - (S1_n/(n+1))^2
    mu_0 = x_0
    mu_n = mu_(n-1) + (x_n - mu_(n-1))/(n+1)
    M2_0 = 0
    M2_n = M2_(n-1) + (x_n - mu_(n-1))*(x_n - mu_n)
    welford_n = M2_n/(n+1)
}
moments = stats(x_n = 4096 + mod(7*n, 11)/10)

# Softmax over attention scores that grow with n (Vaswani et al. 2017, 3.2.1),
# as written and shifted by its row's largest score (Goodfellow, Bengio &
# Courville, Deep Learning, 4.1). In float, exp of the written one overflows
# from n = 63, and inf over inf is NaN; the shifted one holds:
#
#     peak.A[3,1]: -nan at 63, where the interpreter gives 1
scores(d = 2) = {
    Q_n = [1 0; 0 1; 1 1]*n
    K = [1 1; 0 1; 1 0]
    S_n = Q_n*K'/d^(1/2)
    top(z, i) = max(z[i,1], max(z[i,2], z[i,3]))
    A_n[i,j] = exp(S_n[i,j])/sum_(c=1)^3 exp(S_n[i,c])
    B_n[i,j] = exp(S_n[i,j] - top(S_n, i))/sum_(c=1)^3 exp(S_n[i,c] - top(S_n, i))
}
peak = scores()

# The same regression trained with Adam (MANIFESTO.md, A paper conformance
# suite): the two moments sequences of their own, the second cell by cell,
# each corrected for its bias by the power of its rate at the step.
adam(eta = 1/10, b1 = 9/10, b2 = 999/1000, eps = 1/10^8) = {
    sig(z) = 1/(1 + exp(-z))
    X = [1, 0, 0; 1, 0, 1; 1, 1, 0; 1, 1, 1]
    y = [0; 0; 0; 1]
    p_n[r<=4] = sig(X[r]*w_n)
    g_n = X'*(p_n - y)/4
    a_0 = [0; 0; 0]
    a_n = b1*a_(n-1) + (1 - b1)*g_(n-1)
    q_0 = [0; 0; 0]
    q_n[j<=3] = b2*q_(n-1)[j] + (1 - b2)*g_(n-1)[j]^2
    w_0 = [0; 0; 0]
    w_n[j<=3] = w_(n-1)[j] - eta*a_n[j]/(1 - b1^n)/((q_n[j]/(1 - b2^n))^(1/2) + eps)
}
fit = adam()

# Logistic regression trained with momentum, as a paper writes it (MANIFESTO.md,
# A paper conformance suite): the velocity a sequence beside the weights, the
# gradient one step behind it, and exp the prelude's.
heavy(eta = 1, beta = 9/10) = {
    sig(z) = 1/(1 + exp(-z))
    X = [1, 0, 0; 1, 0, 1; 1, 1, 0; 1, 1, 1]
    y = [0; 0; 0; 1]
    p_n[r<=4] = sig(X[r]*w_n)
    g_n = X'*(p_n - y)/4
    v_0 = [0; 0; 0]
    v_n = beta*v_(n-1) + g_(n-1)
    w_0 = [0; 0; 0]
    w_n = w_(n-1) - eta*v_n
}
ball = heavy()

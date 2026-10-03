# Logistic regression trained by gradient descent, as a paper writes it
# (MANIFESTO.md, A paper conformance suite): an epoch is a step, the data are
# read a row at a time, X[r]*w a row times a column, and the gradient is
# written by hand. exp is its series, a limit compiled as a loop, folded into
# a recurrence on the sum since a limit's terms read only their own. Samples
# are r rather than a paper's i, which is the imaginary unit here.
logit(eta = 2) = {
    ex(z)_0 = 1
    ex(z)_1 = 1 + z
    ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k
    sig(z) = 1/(1 + lim ex(0-z))
    X = [0 0; 0 1; 1 0; 1 1]
    y = [0; 0; 0; 1]
    p_n[r<=4] = sig(X[r]*w_n + b_n)
    w_0 = [0; 0]
    b_0 = 0
    w_n[j<=2] = w_(n-1)[j] - eta*sum_(r=1)^4 (p_(n-1)[r] - y[r])*X[r,j]/4
    b_n = b_(n-1) - eta*sum_(r=1)^4 (p_(n-1)[r] - y[r])/4
}
gate = logit()

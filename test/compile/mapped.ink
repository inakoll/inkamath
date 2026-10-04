# logistic.ink's regression with its sigmoid applied by a map, as the paper
# writes it: compiled cell by cell, each cell the call, and held to the
# interpreter by --check (DESIGN.md, next in line). exp is the prelude's, e^x.
logit(eta = 2) = {
    sig(z) = 1/(1 + exp(-z))
    X = [0 0; 0 1; 1 0; 1 1]
    y = [0; 0; 0; 1]
    p_n = sig.(X*w_n + b_n)
    w_0 = [0; 0]
    b_0 = 0
    w_n = w_(n-1) - eta*X'*(p_(n-1) - y)/4
    b_n = b_(n-1) - eta*[1 1 1 1]*(p_(n-1) - y)/4
}
mapped = logit()

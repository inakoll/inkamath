# logistic.ink's regression with its sigmoid a function of cells whose size
# is inferred (DESIGN.md, next in line): compiled where it is called, its
# size the extent of the value read, so the header is the one written with
# sig(z)[i<=4, j<=1], byte for byte, and held to the interpreter by --check.
logit(eta = 2) = {
    sig(z)[i,j] = 1/(1 + exp(-z[i,j]))
    X = [0 0; 0 1; 1 0; 1 1]
    y = [0; 0; 0; 1]
    p_n = sig(X*w_n + b_n)
    w_0 = [0; 0]
    b_0 = 0
    w_n = w_(n-1) - eta*X'*(p_(n-1) - y)/4
    b_n = b_(n-1) - eta*[1 1 1 1]*(p_(n-1) - y)/4
}
sized = logit()

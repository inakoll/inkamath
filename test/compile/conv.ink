# A small convolutional network's forward pass compiled (Goodfellow, Bengio
# and Courville 2016, §9.5): a 4x4 image padded by zeros, cross-correlated
# with two 3x3 kernels, a bias each, then a ReLU, 2x2 max pooling and a dense
# head, on a stream of images. Every number below is worked out apart from
# the interpreter, in exact fractions in Python, and then seen to be the
# interpreter's; none is recorded.
#
#     s_0 = 1/2, s_1 = 19/2, s_2 = 93/2
#
# Every value of the hundred steps, partial sums included, is a multiple of
# 1/4 below 2^10, which doubles and floats hold, so whatever the order of its
# operations the step is exact:
#
#     see: 100 steps from 0, against exact values
#     see.Xp: within 0
#     see.Z: within 0
#     see.M: within 0
#     see.s: within 0
#
# and in float, 'inkamath --check conv.ink see --float', each "within 0, 0
# units of a float".
infer(K = [2 -1 2; -1 1 2; 0 0 0;; 2 0 0; 2 -1 2; 1 0 2], d = [1/4; -1/2], W = [1 -1; 2 0;; -1 1; 0 1], a = 1/2, X_n[i<=4, j<=4]) = {
    Xp_n[i<=6, j<=6] | i > 1 and i < 6 and j > 1 and j < 6 = X_n[i-1, j-1]
    Xp_n[i<=6, j<=6] = 0
    Z_n[o<=2, i<=4, j<=4] = d[o] + sum_(u=1)^3 sum_(v=1)^3 K[o,u,v]*Xp_n[i+u-1, j+v-1]
    relu(z)[o,i,j] | z[o,i,j] > 0 = z[o,i,j]
    relu(z)[o,i,j] = 0
    pool(r)[o<=2, i<=2, j<=2] = max(max(r[o,2*i-1,2*j-1], r[o,2*i-1,2*j]), max(r[o,2*i,2*j-1], r[o,2*i,2*j]))
    M_n = pool(relu(Z_n))
    s_n = a + sum_(o=1)^2 sum_(i=1)^2 sum_(j=1)^2 W[o,i,j]*M_n[o,i,j]
}
see = infer(X_n = [1 0 2 0; 3 3 3 3; 1 0 3 0; 3 3 0 3]*(mod(n, 3) - 1) + [1 1 1 1; 0 0 0 0; 1 1 1 1; 0 0 0 0])

# 'learn', such a network trained by grad: a 2x2 kernel K over a 3x3 image
# padded by zeros, a bias d, ReLU, 2x2 max pooling and a dense head W and a,
# each by gradient descent on (s - y)^2/2 with eta = 1/1024, for two steps,
# then held: the interpreter's exact terms lengthen at each step, and a
# hundred of them would take minutes to check. The gradients worked out by
# hand, by backpropagation in exact fractions:
#
#     E_0 = 2025/32
#     E_1 = 85423597133654025/2^51
#     K_1 = [3871/4096, -1069/1024; 7967/4096, -135/4096]
#     a_2 = 16510108669/2^35
#
# No pre-activation is 0, and every value on the way to a weight is a dyadic
# number of at most 50 significant bits, which a double holds, so the
# weights are exact; the loss, a square of such numbers, rounds:
#
#     learn: 100 steps from 0, against exact values
#     learn.K: within 0
#     learn.W: within 0
#     learn.a: within 0
#     learn.d: within 0
#     learn.E: within <e>
train(eta = 1/1024, X_n[i<=3, j<=3], y_n) = {
    pad(X)[i<=5, j<=5] | i > 1 and i < 5 and j > 1 and j < 5 = X[i-1, j-1]
    pad(X)[i<=5, j<=5] = 0
    conv(K, X)[i<=4, j<=4] = sum_(u=1)^2 sum_(v=1)^2 K[u,v]*pad(X)[i+u-1, j+v-1]
    relu(z)[i,j] | z[i,j] > 0 = z[i,j]
    relu(z)[i,j] = 0
    pool(r)[i<=2, j<=2] = max(max(r[2*i-1,2*j-1], r[2*i-1,2*j]), max(r[2*i,2*j-1], r[2*i,2*j]))
    s(K, d, W, a, X) = a + sum_(i=1)^2 sum_(j=1)^2 W[i,j]*pool(relu(conv(K, X) + d))[i,j]
    L(K, d, W, a, X, t) = (s(K, d, W, a, X) - t)^2/2
    K_0 = [1 -1; 2 0]
    d_0 = 1/4
    W_0 = [1 -1; 2 1]
    a_0 = 1/2
    K_n | n > 2 = K_(n-1)
    K_n = K_(n-1) - eta*grad_(G = K_(n-1)) L(G, d_(n-1), W_(n-1), a_(n-1), X_n, y_n)
    d_n | n > 2 = d_(n-1)
    d_n = d_(n-1) - eta*grad_(e = d_(n-1)) L(K_(n-1), e, W_(n-1), a_(n-1), X_n, y_n)
    W_n | n > 2 = W_(n-1)
    W_n = W_(n-1) - eta*grad_(U = W_(n-1)) L(K_(n-1), d_(n-1), U, a_(n-1), X_n, y_n)
    a_n | n > 2 = a_(n-1)
    a_n = a_(n-1) - eta*grad_(c = a_(n-1)) L(K_(n-1), d_(n-1), W_(n-1), c, X_n, y_n)
    E_n = L(K_n, d_n, W_n, a_n, X_n, y_n)
}
learn = train(X_n = [0 2 2; 2 1 3; 2 1 2], y_n = 1)

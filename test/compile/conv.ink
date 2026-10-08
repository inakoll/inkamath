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

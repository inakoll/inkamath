# A gradient with respect to a tensor compiled (DESIGN.md, next in line):
# forward, one pass for each cell of the tensor, slice after slice and row
# by row, that cell seeded with 1 and every other with 0, as the interpreter
# seeds it; a single value's gradient is shaped as the tensor. Every number
# below is worked out apart from the interpreter and the compiler; none is
# recorded. Today 'inkamath --compile tensorgrad.ink' refuses each model,
# "a derivative with respect to a tensor, for now".

# 'saliency', the input gradient of a small convolutional network over a
# batch (Goodfellow, Bengio and Courville 2016, §9.5): conv.ink's 'learn'
# with its weights held, its input a batch of two 3x3 images, and
# G_n = grad_(V = X_n) L(V), slice b the gradient with respect to image b,
# eighteen passes. The images repeat every three steps. By backpropagation
# by hand in exact fractions, and again by central differences of exact
# fractions, which agree where no pre-activation is 0 and no pool ties, as
# none does:
#
#     G_0 = [25/2 -25/4 25/4; 0 -25/2 0; 25/2 -25/2 25/4;;
#            49/4 -49/4 -49/4; 49/2 0 -49/4; 49/2 -49/2 49/2]
#     G_1 = [41/2 -41/4 41/4; 41/2 -41 41/4; 41 0 41/2;;
#            33/4 -33/4 -33/4; 33 -33/2 -33/4; 33 0 33/2]
#     G_2 = [89/4 -89/4 -89/4; 89 -89/2 -89/4; 89 0 89/2;;
#            53/2 0 -53/2; 53/2 -53/4 -53/4; 53 53/2 0]
#
# Every value on the way, parts included, is a multiple of 1/32 below 2^9,
# which doubles and floats hold, so
#
#     saliency: 100 steps from 0, against exact values
#     saliency.G: within 0
#
# and 'inkamath --check tensorgrad.ink saliency --float':
#
#     saliency: 100 steps from 0 in float, against exact values
#     saliency.G: within 0, 0 units of a float
probe(K = [1 -1; 2 0], d = 1/4, W = [1 -1; 2 1], a = 1/2, X_n[b<=2, i<=3, j<=3]) = {
    pad(X)[b<=2, i<=5, j<=5] | i > 1 and i < 5 and j > 1 and j < 5 = X[b, i-1, j-1]
    pad(X)[b<=2, i<=5, j<=5] = 0
    conv(X)[b<=2, i<=4, j<=4] = sum_(u=1)^2 sum_(v=1)^2 K[u,v]*pad(X)[b, i+u-1, j+v-1]
    relu(z)[b,i,j] | z[b,i,j] > 0 = z[b,i,j]
    relu(z)[b,i,j] = 0
    pool(r)[b<=2, i<=2, j<=2] = max(max(r[b,2*i-1,2*j-1], r[b,2*i-1,2*j]), max(r[b,2*i,2*j-1], r[b,2*i,2*j]))
    s(X, b) = a + sum_(i=1)^2 sum_(j=1)^2 W[i,j]*pool(relu(conv(X) + d))[b,i,j]
    L(X) = (s(X, 1) - 1)^2/2 + (s(X, 2) + 1)^2/2
    G_n = grad_(V = X_n) L(V)
}
saliency = probe(X_n = [-1 0 1; 2 0 2; 1 3 2;; 2 1 2; -1 0 -1; 1 3 -1]*(mod(n, 3) - 1) + [2 2 1; 1 2 1; 2 1 2;; 0 0 2; 2 2 2; 1 1 1])

# 'attend', attention's W^Q trained (Vaswani et al., 2017, section 3.2),
# the seed DESIGN.md's Tensors compiled left between attention compiled and
# its training: tensor.ink's 'heads' as small as it stays multi-head over a
# batch, two sequences of two tokens of width 2 and two heads of width 1,
# so sqrt(d_k) = 1. W^Q, stacked by head, a 2x2x1 tensor and the step's own
# term, descends J = sum of G[b,t,c]*O[b,t,c], conv.ink's J, by eta = 1/4.
# By mpmath at 50 digits, the gradient by central differences:
#
#     WQ_1  = [1.00757088017110; -1.0078125;; -0.0151563660062998; 1.99265613399370]
#     WQ_99 = [1.72836091191366; -1.77343134819407;; -1.52364072642791; 1.24073759047894]
#
# The interpreter's exp is a double's, so as for 'heads', <e> below 1e-13:
#
#     attend: 100 steps from 0, against exact values until 1 and inexact ones from there
#     attend.WQ: within <e>; the interpreter's terms about <e> from the exact ones
attn(eta = 1/4, X_n[b<=2, t<=2, c<=2]) = {
    WK = [1; -1;; 2; 1]
    WV = [2; 1;; -1; 1]
    WO = [1 -1;; 2 1]
    G = [1 -1; 0 1;; -1 0; 1 1]
    sm(z)[b,t,s] = exp(z[b,t,s])/sum_(r=1)^2 exp(z[b,t,r])
    head(Q, h, x) = sm((x*Q[h])*(x*WK[h])')*(x*WV[h])
    O(Q, x) = sum_(h=1)^2 head(Q, h, x)*WO[h]
    J(Q, x) = sum_(b=1)^2 sum_(t=1)^2 sum_(c=1)^2 G[b,t,c]*O(Q, x)[b,t,c]
    WQ_0 = [1; -1;; 0; 2]
    WQ_n = WQ_(n-1) - eta*grad_(Q = WQ_(n-1)) J(Q, X_n)
}
attend = attn(X_n = [1 0; 1 1;; 0 1; 1 -1]/2)

# 'small', a batch of two 2x2 states, each pulled toward its input turned a
# quarter back and toward the other's negation: E is the sum over the cells
# of (V*R - X)^2/2, R = [0 1; -1 0], plus that of V[1,i,j]*V[2,i,j]. Since
# R*R' = I its gradient's slice b is V[b] - X[b]*R' + V[3-b], so a seed in
# one slice reaches the other's part. g is the gradient at the parameter P,
# a field; s descends it from its own last term, the step's state, by
# eta = 1/2; o is with respect to a tensor of one cell, [n;;], and is
# [2*n;;], a tensor and not a single value, as the interpreter keeps the
# rank. In exact fractions, by that form and again by central differences,
# exact for a quadratic:
#
#     g_0  = [2 -1; 0 3;; 3 0; -1 2]
#     g_99 = [2 98; 0 3;; 3 0; -100 2]
#     s_1  = [0 -1; 0 1/2;; 1/2 0; 0 0]
#     s_3  = [1/2 -9/4; -5/4 0;; 0 1/4; 9/4 1/2]
#     s_99 = [49/2 -4953/4; -4949/4 -24;; -24 4753/4; 5145/4 49/2]
#
# Every term and part is a multiple of 1/4 below 2^11, exact in a double and
# a float:
#
#     small: 100 steps from 0, against exact values
#     small.g: within 0
#     small.o: within 0
#     small.s: within 0
#
# and 'inkamath --check tensorgrad.ink small --float':
#
#     small: 100 steps from 0 in float, against exact values
#     small.g: within 0, 0 units of a float
#     small.o: within 0, 0 units of a float
#     small.s: within 0, 0 units of a float
pull(eta = 1/2, P = [1 -1; 0 2;; 2 0; -1 1], X_n[b<=2, i<=2, j<=2]) = {
    R = [0 1; -1 0]
    E(V, X) = sum_(b=1)^2 sum_(i=1)^2 sum_(j=1)^2 (V*R - X)[b,i,j]^2/2 + sum_(i=1)^2 sum_(j=1)^2 V[1,i,j]*V[2,i,j]
    g_n = grad_(V = P) E(V, X_n)
    s_0 = P
    s_n = s_(n-1) - eta*grad_(V = s_(n-1)) E(V, X_n)
    o_n = grad_(v = [n;;]) v[1,1,1]^2
}
small = pull(X_n = [n 1; 0 -1;; 1 0; -1 n])

# What stays refused, in the interpreter's words: a gradient with respect to
# a tensor that is not a single value is a Jacobian. h and k are refused
# today as the models above are. A file of its own:
#
#     T = [1 2; 3 4;; 5 6; 7 8]
#     h_n = grad_(V = n*T) 2*V
#     k_n = grad_(V = n*T) V[1]
#     w_n = grad_(v = [n 1]) v*T
#
#     cannot compile h: grad of a tensor with respect to a tensor is a Jacobian, which it does not give
#     cannot compile k: grad of a matrix with respect to a tensor is a Jacobian, which it does not give
#     cannot compile w: grad of a tensor with respect to a matrix is a Jacobian, which it does not give
#
# w is refused today in words that miscall its tensor a matrix: "grad of a
# matrix with respect to a matrix" (C307).

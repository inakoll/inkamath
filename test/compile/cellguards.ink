# Guards on cells at run time (DESIGN.md, next in line): a definition by
# cells whose guard reads a value that moves is compiled, each cell the chain
# of its clauses that the step tests in the interpreter's order, and under
# grad each cell carries its clause's part. Every number below is worked out
# apart from the interpreter, by hand or with exact fractions in Python, and
# then seen to be the interpreter's; none is recorded. Today each model is
# refused, 'a guard on cells that is not a constant', or under grad 'a
# derivative through a definition by cells, for now'.
#
# 'fit', a network of two ReLU units trained by grad through the ReLU written
# by its cells, sizes.ink's network on other data, the loss L beside it. Unit
# 2 is dead on the first two samples and revives at the first step, after
# which the two fit them exactly and both are dead on the third, whose target
# is 0, so the loss is 93/4, 281/16, then 0 from the second step:
#
#     W_0 = [-3/2, 0; 2, 2]
#     W_1 = [-7/4, -1; -9/2, -5/4]
#     W_n = [-35/8, -19/8; -15/8, 1/8], from n = 2
#
# No pre-activation is ever 0, and every value the step computes, part or
# term, is a multiple of 1/32 below 64, which doubles and floats hold, so
# whatever the order of its operations the step is exact:
#
#     fit: 100 steps from 0, against exact values
#     fit.W: within 0
#     fit.L: within 0
#
# and in float, 'inkamath --check cellguards.ink fit --float':
#
#     fit: 100 steps from 0 in float, against exact values
#     fit.W: within 0, 0 units of a float
#     fit.L: within 0, 0 units of a float
#
# No guard in it is an equality and nothing else writes NaN, so its header
# tests nothing for it.
relunet(eta = 1/2) = {
    X = [0, -1; -1, 0; 2, 1]
    y = [3; 3; 0]
    b = [1; 1/2]
    v = [1, -1]
    relu(z)[i] | z[i] > 0 = z[i]
    relu(z)[i] = 0
    net(M, x) = v*relu(M*x + b)
    loss(M) = sum_(r=1)^3 (net(M, X[r]') - y[r])^2/2
    W_0 = [-3/2, 0; 2, 2]
    W_n = W_(n-1) - eta*grad_(M = W_(n-1)) loss(M)
    L_n = loss(W_n)
}
fit = relunet()

# 'layer', net.ink's network with its layer written as on paper, h_n =
# relu(z_n), the ReLU a function by cells. Its cells are the chains net.ink's
# term by cells gives, so its step computes h by net.h's two lines:
#
#     m_->h[0][0][0] = m_->z[0][0][0] < 0.0 ? 0.0 : m_->z[0][0][0];
#     m_->h[0][1][0] = m_->z[0][1][0] < 0.0 ? 0.0 : m_->z[0][1][0];
#
# Its input, a quarter of n with alternating sign, keeps every term a
# multiple of 1/16 below 64, y_5 being -5/8, so each sequence is exact:
#
#     layer: 100 steps from 0, against exact values
#     layer.w: within 0
#     layer.z: within 0
#     layer.h: within 0
#     layer.y: within 0
net(x_n) = {
    W = [1/2, -1/4, 1/4; -1/2, 1, 1/2]
    v = [1, -1/2]
    relu(z)[j] | z[j] < 0 = 0
    relu(z)[j] = z[j]
    w_0[j<=3, k<=1] = 0
    w_n[j<=3, k<=1] = w_(n-1)[j-1, 1]
    w_n[j<=3, k<=1] | j == 1 = x_n
    z_n = W*w_n
    h_n = relu(z_n)
    y_n = v*h_n
}
layer = net(x_n = (-1)^n*n/4)

# 'kink', each cell its clause's part at the threshold, t being n - 2, of
# [1 1] times a function of [t; 1] whose second cell is constant. c, the
# one-line ReLU z*(z > 0) of sizes.ink: 0 below 2, NaN at 2, where its
# comparison jumps and the interpreter refuses, then 1. g, guarded by '> 0':
# 0 to 2, where the clause taken is 0 and has no part, then 1. h, README's
# order, '< 0' first: 0 below 2, then 1, at 2 too. e, of [t; t - 1], each
# cell 0 where it is 0 by an equality and its square elsewhere: 4t - 2, so
# 4n - 10, but NaN at 2 and 3, where cell 1 and then cell 2 take a clause
# that holds at the point alone, as the interpreter refuses, 'pk[1,1] takes a
# clause at t = 0 that holds only there':
#
#     kink: 100 steps from 0, against exact values
#     kink.c: within 0
#     kink.e: within 0
#     kink.g: within 0
#     kink.h: within 0
kinks(x_n) = {
    rl(z)[i,j] = z[i,j]*(z[i,j] > 0)
    up(z)[i] | z[i] > 0 = z[i]
    up(z)[i] = 0
    rd(z)[i] | z[i] < 0 = 0
    rd(z)[i] = z[i]
    pk(z)[i] | z[i] == 0 = 0
    pk(z)[i] = z[i]^2
    c_n = grad_(t = x_n) [1 1]*rl([t; 1])
    g_n = grad_(t = x_n) [1 1]*up([t; 1])
    h_n = grad_(t = x_n) [1 1]*rd([t; 1])
    e_n = grad_(t = x_n) [1 1]*pk([t; t - 1])
}
kink = kinks(x_n = n - 2)

# 'crest', drift.ink's rift with its threshold a function by cells, g_n =
# below(d_n). --check follows the clauses of a sequence's terms, not those of
# a function called, so the flip at 1 that rift reports is not, and the
# values part as they do there, d by its tenfold drift from 9 and g from 1,
# where the doubles' d_1, 0.09999999999999998, is below a tenth:
#
#     crest: 100 steps from 0, against exact values
#     crest.d[1,1]: <d> at 9, where the interpreter gives 0.10000000000000001
#     crest.g[1,1]: 1 at 1, where the interpreter gives 0
#
# its program failing.
cut(c = 1/10) = {
    below(z)[j] | z[j] < c*j = 1
    below(z)[j] = 0
    d_0 = [c; 2*c]
    d_n = 10*d_(n-1) - 9*[c; 2*c]
    g_n = below(d_n)
}
crest = cut()

# 'mask', a guard on cells reading a parameter: k is a field the host may
# assign and M is computed from it where the parameters are, as any value
# reading them. Today the guard folds k's value into M and k is compiled in,
# 'Compiled in, as a size, a bound or a lag cannot change: k.', with no
# field; its header now declares 'double k;'. With k = 3 every cell of M is
# 1, so y_n is 7n:
#
#     mask: 100 steps from 0, against exact values
#     mask.y: within 0
masked(k = 2, x_n) = {
    M[i<=3] | i <= k = 1
    M[i<=3] = 0
    y_n = M'*[x_n; 2*x_n; 4*x_n]
}
mask = masked(k = 3, x_n = n)

# What stays refused, each named in 'inkamath --compile' of a file of
#
#     a_n = grad_(v = [x_n; 1]) up(v)
#     c_n[j<=2] = j*x_n
#     c_2[1] | x_2 > 0 = 5
#     up(z)[i] | z[i] > 0 = z[i]
#     up(z)[i] = 0
#     x_n = n - 1
#
# where the interpreter refuses a in the compiler's words and answers c_2,
# [5; 2], as x_2 is 1:
#
#     cannot compile a: grad of a matrix with respect to a matrix is a Jacobian, which it does not give
#     cannot compile c: a guarded cell of one term
#
# on standard output, exiting 1. Today a is refused as 'a guard on cells
# that is not a constant'.

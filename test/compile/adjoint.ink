# The compiler's adjoint step (DESIGN.md, next in line): a gradient with
# respect to more than one cell compiled in reverse mode, as the interpreter
# takes it. The body is written once, each value carrying a node of a tape
# the compiler keeps where forward mode carries a part, and the gradient is
# a sweep back over that tape, written as temporaries of the step: the
# values a node multiplies by kept from the body's, each node's adjoint
# accumulated from the body's node back to the seed's in the interpreter's
# order. So the step's gradient is the interpreter's to the bit where every
# value it multiplies by is a double there too, and its header grows with
# the body, not with the seed's cells. Every number below is worked out
# apart from the interpreter and the compiler, by mpmath at 50 digits, the
# gradient by hand or by a complex step; none is recorded. Before it each
# gradient was forward mode's, a pass per cell, and parts from the
# interpreter's by rounding. Each report is the first increment's but
# 'gate''s, the second's.

# 'fit4', a layer of four tanh units trained on its square error by grad
# with respect to its 4x4 weights, from an inexact start, on inputs in
# thirds. By backpropagation written out, (tanh(z) - y)*(1 - tanh(z)^2)
# times x', z = W*x, and by a complex step of 1e-45 for each cell:
#
#     W_1  = [0.446083239004 0 0.0539167609955 0.107833521991;
#             0.0208333333333 0.5 -0.0208333333333 -0.0416666666667;
#             0.00325381078621 0 0.496746189214 -0.00650762157242;
#             0.0286930976446 0 -0.0286930976446 0.442613804711]
#     W_99 = [0.172426760334 -0.124856730384 -0.0784588234266 0.124257685855;
#             -0.148400725501 0.125674513559 0.100895561703 -0.125029199237;
#             -0.113300049751 0.124419577379 0.137313345551 -0.124967027319;
#             0.118515634432 -0.125024389132 -0.130852530059 0.125607446377]
#
# Every value its sweep multiplies by is a double in the interpreter too, a
# whole number or a power of two, which a double holds, or a cell of x,
# exact there but meeting only doubles, which round it to the double the
# step holds; so the step's gradient is the interpreter's operation for
# operation, where forward mode's passes take x's cells in another order:
#
#     fit4: 100 steps from 0, against exact values until 0 and inexact ones from there
#     fit4.W: within 0; the interpreter's terms about <e> from the exact ones
#
# Its header, 'inkamath --compile adjoint.ink layer -o layer.h', calls tanh's
# part function once for each of the four units, at a part of 1, its slope,
# where forward mode calls it for each unit in each of sixteen passes:
# 'layer_tanh_dx(' written 5 times, its definition and four slopes. The
# sweep reads each square's 2*u^1 and each slope, kept from the body, and
# each cell of the gradient is a unit's adjoint times a cell of x, unit 4's
# first, as the sweep reaches it first:
#
#     const double t15_ = 2.0 * pow(t13_ + (0.0 - m_->y[0][3][0]), 1.0);
#     const double t16_ = 0.5 * t15_;
#     const double t17_ = t16_ * t14_;
#     ...
#     m_->W[0][3][3] = m_->index_ == 0 ? 0.5 : m_->W[1][3][3] + (0.0 - m_->eta * (isnan(t24_) ? NAN : t17_ * m_->x[0][3][0]));
layer(eta = 1/4, x_n[i<=4], y_n[i<=4]) = {
    L(W, x, y) = sum_(i=1)^4 (tanh((W*x)[i]) - y[i])^2/2
    W_0 = ~[1 0 0 0; 0 1 0 0; 0 0 1 0; 0 0 0 1]/2
    W_n = W_(n-1) - eta*grad_(V = W_(n-1)) L(V, x_n, y_n)
}
fit4 = layer(x_n = [mod(n, 5) - 2; mod(2*n, 5) - 2; mod(3*n, 5) - 2; mod(4*n, 5) - 2]/3, y_n = [1/2; -1/4; 1/8; -1/16])

# 'wide', the same layer sixteen units wide, 256 weights: forward mode
# writes its body once for each, a header of 485,423 bytes before, and
# reverse mode once, with a sweep of its size. Its header,
# 'inkamath --compile adjoint.ink wide -o wide.h', is under 100,000 bytes,
# and 'wide_tanh_dx(' is written 17 times, its definition and sixteen
# slopes.
wide(eta = 1/4, x_n[i<=16], y_n[i<=16]) = {
    L(W, x, y) = sum_(i=1)^16 (tanh((W*x)[i]) - y[i])^2/2
    W_0 = ~[1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0; 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0; 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0; 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0; 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0; 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0; 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0; 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0; 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0; 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0; 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0; 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0; 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0; 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0; 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0; 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1]/2
    W_n = W_(n-1) - eta*grad_(V = W_(n-1)) L(V, x_n, y_n)
}

# 'token', a piece of the exploration's transformer (Vaswani et al., 2017):
# a sequence of three one-hot tokens embedded by E, normalised by layer
# norm without gain or bias, unembedded by U and scored by cross-entropy
# against the sequence reversed, E trained. Layer norm and the softmax are
# definitions by cells, each cell placed into its matrix's node as the
# interpreter places it; exp and log are the prelude's, by their slopes. By
# a complex step of 1e-40 for each cell of the gradient:
#
#     E_1  = [0.884564739664 -0.716568017639 0.303402426788 0.278600851186;
#             0.5 1 -1 0;
#             0.0650279347178 0.249862803384 0.456763817651 -1.02165455575;
#             -0.391542455129 -0.0228332558242 0.764805865278 0.899569845675]
#     E_99 = [0.919383651036 -1.17555300707 1.31006886828 -0.303899512247;
#             -0.0656039238776 1.34402367074 -1.5807664368 0.802346689932;
#             0.406640058972 -0.401076679644 1.18213944514 -1.43770282447;
#             -0.872670985488 0.870067186223 -0.516309860745 1.76891366001]
#
# E's second row is never read at step 1, so it keeps its start. U's cells
# and 1/100000 are exact in the interpreter, and the step folds each to the
# double the interpreter rounds it to where it meets one:
#
#     token: 100 steps from 0, against exact values until 0 and inexact ones from there
#     token.E: within 0; the interpreter's terms about <e> from the exact ones
#
# Its header, 'inkamath --compile adjoint.ink norm -o norm.h', 450,343
# bytes before, is under 100,000: 'norm_exp_dx(' written 13 times, its
# definition and the slope of each of the twelve scores, and 'norm_log_dx('
# 4 times, its definition and one for each token.
norm(eta = 1/2, X_n[t<=3, v<=4], Y_n[t<=3, v<=4]) = {
    U = [1 -1 0 1/2; 0 1 -1 0; 1/2 0 1 -1; -1 1/2 0 1]
    mu(x)[t<=3] = sum_(j=1)^4 x[t,j]/4
    cen(x, m)[t<=3, j<=4] = x[t,j] - m[t]
    rs(z)[t<=3] = 1/(sum_(j=1)^4 z[t,j]^2/4 + 1/100000)^(1/2)
    ln(z, r)[t<=3, j<=4] = z[t,j]*r[t]
    lse(z)[t<=3] = log(sum_(v=1)^4 exp(z[t,v]))
    ce(z, y) = sum_(t=1)^3 (lse(z)[t] - sum_(v=1)^4 y[t,v]*z[t,v])/3
    L(E, X, Y) = ce(ln(cen(X*E, mu(X*E)), rs(cen(X*E, mu(X*E))))*U, Y)
    E_0 = ~[1 -1/2 0 1/4; 1/2 1 -1 0; 0 1/4 1/2 -1; -1/4 0 1 1/2]
    E_n = E_(n-1) - eta*grad_(A = E_(n-1)) L(A, X_n, Y_n)
}
oh(k)[t<=3, v<=4] = mod(k + t, 4) + 1 == v
ho(k)[t<=3, v<=4] = mod(k + 4 - t, 4) + 1 == v
token = norm(X_n = oh(n), Y_n = ho(n))

# 'gate', the second increment's: three ReLU units by cells and a fixed
# readout, W trained from an inexact start. The guard chooses each unit's
# clause at the point as in forward mode, and the sweep adds a clause's
# adjoints only where it is taken. By hand, (v*relu(z) - y)*v[i]*(z[i] > 0)
# times x', z = W*x, no pre-activation nearer 0 than 0.00028:
#
#     W_1  = [1 -1 0.333333333333; -0.5 0.956018518519 0.192669753086;
#             0.142857142857 0.526388888889 -0.995601851852]
#     W_99 = [0.935465038725 -1.06453496127 0.202881253766;
#             0.00848828931398 -0.0499521390992 -0.0436149433609;
#             -0.221600607004 1.36591464511 -0.815971931417]
#
# A guard reading a double chooses by an inexact truth, so the interpreter
# marks the clause it takes, and so does the step:
#
#     gate: 100 steps from 0, against exact values until 0 and inexact ones from there
#     gate.W: within 0; the interpreter's terms about <e> from the exact ones
#
# Until the second increment its gradient stays forward mode's, the body
# reaching a guard.
relugate(eta = 1/4, x_n[i<=3], y_n) = {
    relu(z)[i] | z[i] > 0 = z[i]
    relu(z)[i] = 0
    v = [1, -1/3, 1/5]
    L(M, x, y) = (v*relu(M*x) - y)^2/2
    W_0 = ~[1 -1 1/3; -1/2 1 1/5; 1/7 1/2 -1]
    W_n = W_(n-1) - eta*grad_(M = W_(n-1)) L(M, x_n, y_n)
}
gate = relugate(x_n = [mod(n, 3) - 1; 1; mod(2*n, 5)/3 - 1/2], y_n = mod(n, 4)/4)

# 'sinabs', C329: a session takes sin and abs of a double by their slopes,
# and so does the step. --check's exact run walks sin's definition for its
# guards, which calls abs on the same value: that call remembered, the
# body's own abs found it, recorded before the root's nodes, and its
# adjoint was summed last, where a session's is first.
#
#     sinabs: 100 steps from 0, against exact values until 0 and inexact ones from there
#     sinabs.W: within 0; the interpreter's terms about <e> from the exact ones
slopes(eta = 1/16, x_n[i<=2]) = {
    L(W, x) = sin((W*x)[1]) + ((W*x)[1]^2 + 1)^(1/2) + abs((W*x)[1])
    W_0 = ~[1 -1; 1/2 1]
    W_n = W_(n-1) - eta*grad_(V = W_(n-1)) L(V, x_n)
}
sinabs = slopes(x_n = ~[mod(n, 3) - 1; 1/3])

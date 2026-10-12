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
# 'gate3''s and 'aside''s, the second's.

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
# first, as the sweep reaches it first, after W*x's four cells tested for
# NaN in one flag, t4_:
#
#     const double t19_ = t17_ + (0.0 - m_->y[0][3][0]);
#     const double t20_ = 2.0 * pow(t19_, 1.0);
#     const double t21_ = 0.5 * t20_;
#     const double t22_ = t21_ * t18_;
#     ...
#     m_->W[0][3][3] = m_->index_ == 0 ? 0.5 : m_->W[1][3][3] + (0.0 - m_->eta * (isnan(t29_) ? NAN : t22_ * m_->x[0][3][0]));
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
#
# Each matrix it tests for NaN is tested once, a flag declared where first
# tested and read at each cell's read (DESIGN.md, a matrix read by its
# cells tested for NaN once), test/cli.cmake's compile_nan_once: 'isnan('
# written fewer than 150 times, 2,011 before, and norm.h under 40,000
# bytes, 59,117 before. By count 94: 16 for the gradient's cells, each
# testing the loss, 16 for the term's own test, 17 in the prelude's exp,
# log and ilogb, and 45 for the six matrices read by their cells, X*E and
# its means, the centred cells and their scales, the logits and their
# log-sum-exps, 12 and 3 cells by turns.
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

# 'gate3', the second increment's: three ReLU units by cells and a fixed
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
#     gate3: 100 steps from 0, against exact values until 0 and inexact ones from there
#     gate3.W: within 0; the interpreter's terms about <e> from the exact ones
relugate(eta = 1/4, x_n[i<=3], y_n) = {
    relu(z)[i] | z[i] > 0 = z[i]
    relu(z)[i] = 0
    v = [1, -1/3, 1/5]
    L(M, x, y) = (v*relu(M*x) - y)^2/2
    W_0 = ~[1 -1 1/3; -1/2 1 1/5; 1/7 1/2 -1]
    W_n = W_(n-1) - eta*grad_(M = W_(n-1)) L(M, x_n, y_n)
}
gate3 = relugate(x_n = [mod(n, 3) - 1; 1; mod(2*n, 5)/3 - 1/2], y_n = mod(n, 4)/4)

# 'under', gate3's units over a fixed W and U trained beneath them, so a
# cell of U's adjoint is there where any unit's clause is taken. By hand,
# e*(W'*(v'.*(z > 0)))*x', z = W*U*x, e = v*relu(z) - y, no pre-activation
# nearer 0 than 0.0039:
#
#     U_1  = [1 -0.467346041194 0.00544232646762;
#             0.333333333333 0.960974537037 -0.256504243827;
#             0 0.155399470899 0.992566578483]
#     U_99 = [0.703720112177 -0.0210902152444 -0.120825339494;
#             0.513782511076 0.225035443646 -0.256009481807;
#             -0.0391518069697 -1.23205680101 0.826294405244]
#
#     under: 100 steps from 0, against exact values until 0 and inexact ones from there
#     under.U: within 0; the interpreter's terms about <e> from the exact ones
#
# Each condition is written once (DESIGN.md, the reverse sweep's conditions
# written once), test/cli.cmake's compile_sweep_conditions: ' != 0)'
# written fewer than 20 times in hidden.h, 39 before. By count 15: 3, one
# for each unit's adjoint where its clause is taken, 9 for W'*(...), each of
# its three cells summing the three units, and 3 for two flags, the first
# two units' clauses and the third's beside them, which U's 9 cells read
# where each wrote all three.
hidden(eta = 1/4, x_n[i<=3], y_n) = {
    relu(z)[i] | z[i] > 0 = z[i]
    relu(z)[i] = 0
    v = [1, -1/3, 1/5]
    W = [1 -1 1/3; -1/2 1 1/5; 1/7 1/2 -1]
    L(M, x, y) = (v*relu(W*(M*x)) - y)^2/2
    U_0 = ~[1 -1/2 0; 1/3 1 -1/4; 0 1/5 1]
    U_n = U_(n-1) - eta*grad_(M = U_(n-1)) L(M, x_n, y_n)
}
under = hidden(x_n = [mod(n, 3) - 1; 1; mod(2*n, 5)/3 - 1/2], y_n = mod(n, 4)/4)

# 'aside', the second increment's matrix written whole beside a cell's own
# clause (C136): k's first cell is its own, its second the whole's, 2*z,
# which the sweep reaches as a cell of the whole's node. By hand at 50
# digits, e*[z[2]; z[1] + 2] times x', e = z[1]*z[2] + 2*z[2] - y, z = W*x,
# and step 1 again by central differences:
#
#     W_1  = [1.00607638889 -0.5; 0.278645833333 1]
#     W_99 = [0.559688031712 -0.616481484209;
#             0.0155347422454 -0.261044498279]
#
#     aside: 100 steps from 0, against exact values until 0 and inexact ones from there
#     aside.W: within 0; the interpreter's terms about <e> from the exact ones
beside(eta = 1/8, x_n[i<=2], y_n) = {
    k(z) = 2*z
    k(z)[1] = z[1]*z[2]
    L(M, x, y) = (k(M*x)[1] + k(M*x)[2] - y)^2/2
    W_0 = ~[1 -1/2; 1/3 1]
    W_n = W_(n-1) - eta*grad_(M = W_(n-1)) L(M, x_n, y_n)
}
aside = beside(x_n = [mod(n, 5)/2 - 1; mod(3*n, 7)/3 - 1], y_n = mod(n, 3)/3)

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

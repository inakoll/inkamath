# Reverse mode (DESIGN.md, next in line): a single value's gradient with
# respect to a matrix or a tensor of more than one cell, at a real point, is
# one evaluation recording its parts on a tape and one sweep back, where
# forward mode evaluates the body once per cell. Same notation, same answers:
# exact values are those of the closed forms, by sympy; doubles are mpmath's
# or NumPy's to the digits shown. Written by hand, never recorded.
>> grad_(W = [1 2; 3 4]) [1 1]*W*[1; 2]
[1, 2;
 1, 2]

>> grad_(W = [1 2; 3 4]) [1 1]*W^2*[1; 2]
[ 9, 19;
 11, 23]

>> grad_(W = [1 2; 3 4]) [1 1]*W^-1*[1; 2]
[0,  0.25;
 0, -0.25]

>> grad_(W = [1 2; 3 4]) [1 1]*(W/3)'*[1; 2]
[~0.333333333, ~0.333333333;
 ~0.666666667, ~0.666666667]

>> frac grad_(W = [1/2 1/3; 1/5 1/7]) [1 1]*W^-1*[1; 1]
[-480,   756;
 1400, -2205]

>> grad_(T = [1 2; 3 4;; 5 6; 7 8]) [1 1]*T[1]*T[2]*[1; 1]
[11, 15;
 11, 15;;
  4,  4;
  6,  6]

# README's, section 4.
>> grad_(v = [1; 2]) v'*v
[2;
 4]

# Sums and products unrolled, a local of the line and one of a call.
>> grad_(v = [1 2 3]) sum_(k=1)^3 v[1,k]^k
[1, 4, 27]

>> grad_(v = [1 2 3]) prod_(k=1)^3 v[1,k]
[6, 3, 2]

>> grad_(v = [1 2]) ((a = v*v') + a^2)
[22, 44]

>> f(w) = (h = w*[1; 2]) + h^2
f(w) = (h = w*[1; 2]) + h^2

>> grad_(w = [3 4]) f(w)
[23, 46]

# By cells, each cell's clause chosen at the point: a ReLU at 0 takes the
# clause its guard leaves, slope 1 here.
>> sq(v)[j<=3] = v[j]^2
sq(v)[j<=3] = v[j]^2

>> grad_(v = [1; 2; 3]) [3 2 1]*sq(v)
[6;
 8;
 6]

>> rl(v)[j<=3] = v[j]
rl(v)[j<=3] = v[j]

>> rl(v)[j<=3] | v[j] < 0 = 0
rl(v)[j<=3] | v[j] < 0 = 0

>> grad_(v = [1; 0; -2]) [1 2 3]*rl(v)
[1;
 2;
 0]

# Remembered terms: a term remembered as a value does not answer for one
# with its parts, and each term with its parts is evaluated once.
>> x(W)_0 = [1; 1]
x(W)_0 = [1; 1]

>> x(W)_n = W*x(W)_(n-1)
x(W)_n = W*x(W)_(n-1)

>> frac [1 1]*x([1/2 1; 0 1/3])_3
149/216

>> frac grad_(W = [1/2 1; 0 1/3]) [1 1]*x(W)_3
[ 25/12, 19/36;
 145/36,   3/2]

# The prelude's functions of a double, by their slopes: e and 1/2, and
# 1 - tanh(3)^2.
>> grad_(v = [~1 ~2]) (exp(v[1,1]) + log(v[1,2]))
[~2.71828183, ~0.5]

>> grad_(v = [1 2]) tanh(v*[1; 1])
[~0.00986603717, ~0.00986603717]

# A cell is inexact where a path to it meets a double. A cell no path
# reaches is an exact 0, and a cell whose paths meet only exact numbers is
# exact, where forward mode's pass for it multiplied a 0 by a double.
>> grad_(W = [1 2; 3 4]) [1 1]*W*[~1; 2]
[~1, 2;
 ~1, 2]

>> grad_(v = [1 2]) (v[1,1]*~2 + v[1,2]*3)
[~2, 3]

>> grad_(W = [1 2; 3 4]) W[1,1]*~2.5
[~2.5, 0;
    0, 0]

>> c(m)[j <= 1, k <= 2] = m[j,k]*[~2 3][j,k]
c(m)[j <= 1, k <= 2] = m[j,k]*[~2 3][j,k]

>> grad_(v = [1 2]) c(v)[1,2]
[0, 3]

# One cell is one pass either way, and stays forward.
>> grad_(v = [2;;]) v[1,1,1]^3
[12;;]

# The count: forward mode evaluates the body once per cell, so ten cells
# take ten times the sum's 120,000 steps, past a line's million; reverse
# mode once.
>> grad_(w = 3) (w*w + sum_(k=1)^120000 k)
6

>> grad_(w = [1 2 3 4 5 6 7 8 9 10]) (w*w' + sum_(k=1)^120000 k)
[2, 4, 6, 8, 10, 12, 14, 16, 18, 20]

# A tiny transformer, trimmed from the exploration's tf.ink (Vaswani et al.
# 2017, pre-layer-norm): one block of two heads of width 2, width 4, a
# vocabulary of 4, one sequence of three tokens and its reverse as target.
# Weights in eighths, exact in doubles too. The loss, two gradients and the
# loss after one step of 1/10 on every weight are NumPy's, by the tape of
# the exploration's rev.py, itself held to central differences.
>> nv = 4
nv = 4

>> nt = 3
nt = 3

>> nd = 4
nd = 4

>> mu(x)[b,t] = sum_(j=1)^nd x[b,t,j]/nd
mu(x)[b,t] = sum_(j=1)^nd x[b,t,j]/nd

>> cen(x, m)[b,t,j] = x[b,t,j] - m[b,t]
cen(x, m)[b,t,j] = x[b,t,j] - m[b,t]

>> rs(z)[b,t] = 1/(sum_(j=1)^nd z[b,t,j]^2/nd + 1/100000)^(1/2)
rs(z)[b,t] = 1/(sum_(j=1)^nd z[b,t,j]^2/nd + 1/100000)^(1/2)

>> lnc(z, r, g, c)[b,t,j] = g[1,j]*z[b,t,j]*r[b,t] + c[1,j]
lnc(z, r, g, c)[b,t,j] = g[1,j]*z[b,t,j]*r[b,t] + c[1,j]

>> lns(z, g, c) = lnc(z, rs(z), g, c)
lns(z, g, c) = lnc(z, rs(z), g, c)

>> lnorm(x, g, c) = lns(cen(x, mu(x)), g, c)
lnorm(x, g, c) = lns(cen(x, mu(x)), g, c)

>> ex(z)[b,t,s] = exp(z[b,t,s])
ex(z)[b,t,s] = exp(z[b,t,s])

>> den(e)[b,t] = sum_(r=1)^nt e[b,t,r]
den(e)[b,t] = sum_(r=1)^nt e[b,t,r]

>> nrm(e, d)[b,t,s] = e[b,t,s]/d[b,t]
nrm(e, d)[b,t,s] = e[b,t,s]/d[b,t]

>> sm(e) = nrm(e, den(e))
sm(e) = nrm(e, den(e))

>> head(a, Q, K, Vv, k) = sm(ex((a*Q[k])*(a*K[k])'/2))*(a*Vv[k])
head(a, Q, K, Vv, k) = sm(ex((a*Q[k])*(a*K[k])'/2))*(a*Vv[k])

>> att(a, Q, K, Vv, O) = sum_(h=1)^2 head(a, Q, K, Vv, h)*O[h]
att(a, Q, K, Vv, O) = sum_(h=1)^2 head(a, Q, K, Vv, h)*O[h]

>> relu(z)[b,t,f] | z[b,t,f] > 0 = z[b,t,f]
relu(z)[b,t,f] | z[b,t,f] > 0 = z[b,t,f]

>> relu(z)[b,t,f] = 0
relu(z)[b,t,f] = 0

>> addr(z, r)[b,t,f] = z[b,t,f] + r[1,f]
addr(z, r)[b,t,f] = z[b,t,f] + r[1,f]

>> ff(a, W1, c1, W2, c2) = addr(relu(addr(a*W1, c1))*W2, c2)
ff(a, W1, c1, W2, c2) = addr(relu(addr(a*W1, c1))*W2, c2)

>> blk(x, Q, K, Vv, O, W1, c1, W2, c2, G, Bb) = (y = x + att(lnorm(x, G[1], Bb[1]), Q, K, Vv, O)) + ff(lnorm(y, G[2], Bb[2]), W1, c1, W2, c2)
blk(x, Q, K, Vv, O, W1, c1, W2, c2, G, Bb) = (y = x + att(lnorm(x, G[1], Bb[1]), Q, K, Vv, O)) + ff(lnorm(y, G[2], Bb[2]), W1, c1, W2, c2)

>> lse(z)[b,t] = log(sum_(v=1)^nv exp(z[b,t,v]))
lse(z)[b,t] = log(sum_(v=1)^nv exp(z[b,t,v]))

>> ce(z[b<=p, t<=q, v<=w], y) = sum_(b=1)^p sum_(t=1)^q (lse(z)[b,t] - sum_(v=1)^w y[b,t,v]*z[b,t,v])/(p*q)
ce(z[b<=p, t<=q, v<=w], y) = sum_(b=1)^p sum_(t=1)^q (lse(z)[b,t] - sum_(v=1)^w y[b,t,v]*z[b,t,v])/(p*q)

>> loss(X, Y, E, P, Q, K, Vv, O, W1, c1, W2, c2, G, Bb, U) = ce(lnorm(blk(X*E + P, Q, K, Vv, O, W1, c1, W2, c2, G, Bb), G[3], Bb[3])*U, Y)
loss(X, Y, E, P, Q, K, Vv, O, W1, c1, W2, c2, G, Bb, U) = ce(lnorm(blk(X*E + P, Q, K, Vv, O, W1, c1, W2, c2, G, Bb), G[3], Bb[3])*U, Y)

>> E0 = [0.25 -0.5 0.5 -0.875; -0.75 -0.625 0.375 -0.875; 1 -0.25 -0.875 -0.75; 0.625 0.625 -0.75 -0.125]
E0 = [0.25 -0.5 0.5 -0.875; -0.75 -0.625 0.375 -0.875; 1 -0.25 -0.875 -0.75; 0.625 0.625 -0.75 -0.125]

>> P0 = [-0.75 0.625 -0.875 -0.625; -0.125 -0.875 0.5 -0.875; -0.125 -0.875 -0.5 0.125]
P0 = [-0.75 0.625 -0.875 -0.625; -0.125 -0.875 0.5 -0.875; -0.125 -0.875 -0.5 0.125]

>> Q0 = [0.625 -0.5; -0.625 0.125; -0.375 -0.625; -0.25 0.375;; -0.625 -0.75; -0.875 -0.25; 0.875 0.625; 0.25 0.75]
Q0 = [0.625 -0.5; -0.625 0.125; -0.375 -0.625; -0.25 0.375;; -0.625 -0.75; -0.875 -0.25; 0.875 0.625; 0.25 0.75]

>> K0 = [0.75 0.375; 0.125 -0.125; -0.375 -0.125; -0.75 0.125;; 1 0.875; 0.25 0.75; 0.125 -0.75; -0.625 1]
K0 = [0.75 0.375; 0.125 -0.125; -0.375 -0.125; -0.75 0.125;; 1 0.875; 0.25 0.75; 0.125 -0.75; -0.625 1]

>> Vv0 = [0.625 -0.375; 0.25 -0.5; 0.875 0.625; -0.875 -0.75;; 0.25 0.25; 0.375 0.875; 0.75 -0.75; -0.75 0]
Vv0 = [0.625 -0.375; 0.25 -0.5; 0.875 0.625; -0.875 -0.75;; 0.25 0.25; 0.375 0.875; 0.75 -0.75; -0.75 0]

>> O0 = [0.875 -0.75 -0.875 0.125; 0.75 0.125 0.5 0.375;; -1 0.75 0.375 -0.375; -0.625 0.875 -0.875 -0.25]
O0 = [0.875 -0.75 -0.875 0.125; 0.75 0.125 0.5 0.375;; -1 0.75 0.375 -0.375; -0.625 0.875 -0.875 -0.25]

>> W10 = [0.125 -0.5 -0.125 0.5; 0.5 0.875 -0.75 -0.375; 0.75 0.5 0 -0.5; 0.625 0 0.625 0.375]
W10 = [0.125 -0.5 -0.125 0.5; 0.5 0.875 -0.75 -0.375; 0.75 0.5 0 -0.5; 0.625 0 0.625 0.375]

>> W20 = [0.5 -0.125 -0.5 -0.75; -0.375 -0.5 -0.125 -0.125; -1 0.875 -0.375 0; 0.125 -1 -0.5 0.625]
W20 = [0.5 -0.125 -0.5 -0.75; -0.375 -0.5 -0.125 -0.125; -1 0.875 -0.375 0; 0.125 -1 -0.5 0.625]

>> U0 = [0.375 0.25 -0.5 1; -0.875 0.75 0.5 0.5; 0.5 0.5 -0.625 0.875; 0.5 -0.875 -0.25 -0.75]
U0 = [0.375 0.25 -0.5 1; -0.875 0.75 0.5 0.5; 0.5 0.5 -0.625 0.875; 0.5 -0.875 -0.25 -0.75]

>> c10 = [0 0 0 0]
c10 = [0 0 0 0]

>> c20 = [0 0 0 0]
c20 = [0 0 0 0]

>> G0 = [1 1 1 1;; 1 1 1 1;; 1 1 1 1]
G0 = [1 1 1 1;; 1 1 1 1;; 1 1 1 1]

>> Bb0 = [0 0 0 0;; 0 0 0 0;; 0 0 0 0]
Bb0 = [0 0 0 0;; 0 0 0 0;; 0 0 0 0]

>> X = [0 1 0 0; 0 0 0 1; 1 0 0 0;;]
X = [0 1 0 0; 0 0 0 1; 1 0 0 0;;]

>> Y = [1 0 0 0; 0 0 0 1; 0 1 0 0;;]
Y = [1 0 0 0; 0 0 0 1; 0 1 0 0;;]

>> loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)
~2.95929024

>> grad_(A = Q0) loss(X, Y, E0, P0, A, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)
[~-0.0767740432,  ~-0.059784138;
   ~0.201562393,   ~0.119130257;
  ~0.0619377371,  ~0.0401917886;
  ~-0.186726087, ~-0.0995379074;;
  ~0.0715460743, ~-0.0676330906;
 ~-0.0266895869,  ~0.0898332103;
 ~-0.0279833467,  ~0.0197974097;
 ~-0.0168731408, ~-0.0419975294]

>> grad_(A = U0) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, A)
[ ~0.753840184,  ~-0.39075984,   ~0.045397874, ~-0.408478218;
 ~-0.837681934,  ~0.545012278, ~-0.0298716088,  ~0.322541264;
 ~-0.630248762,  ~0.328525791, ~0.00121517432,  ~0.300507797;
  ~0.714090512, ~-0.482778229, ~-0.0167414395, ~-0.214570843]

>> E1 = E0 - grad_(A = E0) loss(X, Y, A, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10
E1 = E0 - grad_(A = E0) loss(X, Y, A, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10

>> P1 = P0 - grad_(A = P0) loss(X, Y, E0, A, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10
P1 = P0 - grad_(A = P0) loss(X, Y, E0, A, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10

>> Q1 = Q0 - grad_(A = Q0) loss(X, Y, E0, P0, A, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10
Q1 = Q0 - grad_(A = Q0) loss(X, Y, E0, P0, A, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10

>> K1 = K0 - grad_(A = K0) loss(X, Y, E0, P0, Q0, A, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10
K1 = K0 - grad_(A = K0) loss(X, Y, E0, P0, Q0, A, Vv0, O0, W10, c10, W20, c20, G0, Bb0, U0)/10

>> Vv1 = Vv0 - grad_(A = Vv0) loss(X, Y, E0, P0, Q0, K0, A, O0, W10, c10, W20, c20, G0, Bb0, U0)/10
Vv1 = Vv0 - grad_(A = Vv0) loss(X, Y, E0, P0, Q0, K0, A, O0, W10, c10, W20, c20, G0, Bb0, U0)/10

>> O1 = O0 - grad_(A = O0) loss(X, Y, E0, P0, Q0, K0, Vv0, A, W10, c10, W20, c20, G0, Bb0, U0)/10
O1 = O0 - grad_(A = O0) loss(X, Y, E0, P0, Q0, K0, Vv0, A, W10, c10, W20, c20, G0, Bb0, U0)/10

>> W11 = W10 - grad_(A = W10) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, A, c10, W20, c20, G0, Bb0, U0)/10
W11 = W10 - grad_(A = W10) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, A, c10, W20, c20, G0, Bb0, U0)/10

>> c11 = c10 - grad_(A = c10) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, A, W20, c20, G0, Bb0, U0)/10
c11 = c10 - grad_(A = c10) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, A, W20, c20, G0, Bb0, U0)/10

>> W21 = W20 - grad_(A = W20) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, A, c20, G0, Bb0, U0)/10
W21 = W20 - grad_(A = W20) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, A, c20, G0, Bb0, U0)/10

>> c21 = c20 - grad_(A = c20) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, A, G0, Bb0, U0)/10
c21 = c20 - grad_(A = c20) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, A, G0, Bb0, U0)/10

>> G1 = G0 - grad_(A = G0) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, A, Bb0, U0)/10
G1 = G0 - grad_(A = G0) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, A, Bb0, U0)/10

>> Bb1 = Bb0 - grad_(A = Bb0) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, A, U0)/10
Bb1 = Bb0 - grad_(A = Bb0) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, A, U0)/10

>> U1 = U0 - grad_(A = U0) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, A)/10
U1 = U0 - grad_(A = U0) loss(X, Y, E0, P0, Q0, K0, Vv0, O0, W10, c10, W20, c20, G0, Bb0, A)/10

>> loss(X, Y, E1, P1, Q1, K1, Vv1, O1, W11, c11, W21, c21, G1, Bb1, U1)
~1.60260877

# What grad refuses, it refuses in the same words.
>> grad_(W = [1 2; 3 4]) W*[1; 1]
error: grad of a matrix with respect to a matrix is a Jacobian, which it does not give

>> grad_(W = [1 2; 3 4]) W*[1 2; 3 4;; 5 6; 7 8]
error: grad of a tensor with respect to a matrix is a Jacobian, which it does not give

>> grad_(T = [1;; 2]) T
error: grad of a tensor with respect to a tensor is a Jacobian, which it does not give

>> grad_(v = [1 2]) 3
error: grad's expression does not read v

>> hv(t) = t*v
hv(t) = t*v

>> grad_(v = [1 2]) hv(v[1,1])
error: hv reads the global v, which grad's v does not reach

>> grad_(v = [1 2]) floor(v[1,1]*v[1,2])
error: floor jumps at v = [1, 2]

>> grad_(v = [1 1]) (v[1,1] > v[1,2])
error: a comparison jumps at v = [1, 1]

>> eq(v)[j <= 1, k <= 2] | v[j,k] == 1 = 0
eq(v)[j <= 1, k <= 2] | v[j,k] == 1 = 0

>> eq(v)[j <= 1, k <= 2] = v[j,k]
eq(v)[j <= 1, k <= 2] = v[j,k]

>> grad_(v = [1 2]) eq(v)*[1; 1]
error: eq[1,1] takes a clause at v = [1, 2] that holds only there

>> grad_(v = [1 2]) [5 7 9][1, v[1,1]]
error: an index jumps at v = [1, 2]

>> grad_(v = [2 3]) sum_(k=1)^(v[1,1]) k*v[1,2]
error: the sum's bound jumps at v = [2, 3]

>> gr(x)[j <= x] = j
gr(x)[j <= x] = j

>> grad_(v = [2 1]) [1 1]*gr(v[1,1])
error: the size of gr jumps at v = [2, 1]

>> grad_(v = [1 2]) v[1,1]^v[1,2]
error: grad cannot differentiate a power whose exponent changes with v, unless its base is e

>> grad_(v = [0 1]) (v[1,1]^(1/2) + v[1,2])
error: a power's derivative is infinite at v = [0, 1]

>> grad_(v = [1 2]) [1 1]*eig([v[1,1] 0; 0 v[1,2]])
error: grad cannot differentiate eig yet

>> bis(x)_0 = [0; 1]
bis(x)_0 = [0; 1]

>> bis(x)_n = bis(x)_(n-1) + (bis(x)_(n-1)[2] - bis(x)_(n-1)[1])/2*([1; 0] - ((bis(x)_(n-1)[1] + bis(x)_(n-1)[2])/2 > x)*[1; 1])
bis(x)_n = bis(x)_(n-1) + (bis(x)_(n-1)[2] - bis(x)_(n-1)[1])/2*([1; 0] - ((bis(x)_(n-1)[1] + bis(x)_(n-1)[2])/2 > x)*[1; 1])

>> grad_(v = [1/3 1]) [1 1]*lim bis(v[1,1])*v[1,2]
error: grad cannot differentiate lim bis, whose terms step with v at a comparison or a floor

>> decay(a = 1/2) = {
..     s_0 = 1
..     s_n = a*s_(n-1)
.. }
decay(a = 1/2) = { ... }

>> grad_(v = [1/2 1]) decay(a = v[1,1]).s_2*v[1,2]
error: grad cannot differentiate through an instance yet

# Where reverse mode cannot do as forward mode does, forward mode does it:
# a limit or an endless sum whose terms move, whose derivative converges or
# not as its own terms do; a complex value; a grad inside. The answers, and
# the refusal, are those of forward mode: 1/(2*sqrt(4)) and 1/(2*sqrt(9));
# the derivative at 0 diverging as in grad.ink; -2*sin(1) and cos(1); v's
# cells swapped; 8/9 and 1/3 of the geometric series; and at a complex
# point, 2*i and i^2.
>> r(x)_0 = 1
r(x)_0 = 1

>> r(x)_n = (r(x)_(n-1) + x/r(x)_(n-1))/2
r(x)_n = (r(x)_(n-1) + x/r(x)_(n-1))/2

>> grad_(v = [4 9]) (lim r(v[1,1]) + lim r(v[1,2]))
[~0.25, ~0.166666667]

>> grad_(v = [0 1]) (lim r(v[1,1]) + v[1,2])
error: the derivative of r did not converge within 100 terms (last term ~4.225502e+29)

>> grad_(v = [1/2 1]) (sum_(k=1) (v[1,1]/2)^k*v[1,2])
[~0.888888889, ~0.333333333]

>> grad_(v = [1 2]) (re(e^(i*v[1,1]))*v[1,2])
[~-1.68294197, ~0.540302306]

>> grad_(v = [1 2]) grad_(x = 3) x*v[1,1]*v[1,2]
[2, 1]

# Giving way keeps nothing of the walk but its steps: hh(a), remembered by
# the walk, is walked again, and the clause an inexact truth chose marks the
# answer as forward mode's (C241).
>> hh(x) | x > 0 = 1
hh(x) | x > 0 = 1

>> hh(x) = 2
hh(x) = 2

>> grad_(v = [1 2]) ((a = ~0.5) + hh(a) + v[1,1] + grad_(y = 1) y*v[1,2])
[~1, ~1]

>> grad_(v = [i 1]) v[1,1]^2*v[1,2]
[~(i*2), ~-1]

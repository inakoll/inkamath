# A gradient with respect to several names at once (DESIGN.md, next in
# line): 'grad_(x = a, y = b) body', the nabla's subscript naming every
# parameter, as a training step writes it once for them all. Its answer has
# a part for each name, and each part is what grad by that name alone gives,
# the others held at their points: a definition of the grad is read by its
# parts after a point, as an instance's names are. At a real point, outside
# another grad, it is one evaluation and one sweep back however many names it
# has. Exact values are sympy's, the net's also by backpropagation written
# out in fractions; doubles are sympy's rounded, and the transformer's
# NumPy's, its gradients by a complex step of each cell. Written by hand,
# never recorded.
>> g = grad_(x = 2, y = 3) x^2*y
g = grad_(x = 2, y = 3) x^2*y

>> g.x
12

>> g.y
4

>> g.x == grad_(x = 2) x^2*3
1

>> tex ?g
g = \left.\nabla_{x,y} x^2\,y\right|_{x=2,y=3}

# A grad by several names is a definition, read by its parts: it has no
# value of its own, and is refused whole as an instance is. Its body is a
# term, as a sum's, so w's '+ 1' is outside it.
>> g
error: g is a gradient by several names; read one of its parts (g.x)

>> g.z
error: g.z is not defined

# An index or arguments the definition does not take are refused first,
# as any name's.
>> g_1
error: g is not a sequence

>> g(1)
error: g takes no arguments

>> g.x(1)
error: g.x takes no arguments

>> grad_(x = 2, y = 3) x*y
error: a grad by several names is a definition's whole right side, its parts read after a point, as g.x

>> w = grad_(x = 2, y = 3) x*y + 1
error: a grad by several names is a definition's whole right side, its parts read after a point, as g.x

# A local is read by no point, so it holds no grad by several names.
>> (lg = grad_(x = 2, y = 3) x*y) + 1
error: a local cannot be a gradient by several names

>> lw = (lg = grad_(x = 2, y = 3) x*y) + 1
error: a local cannot be a gradient by several names

>> grad_(x = 2, x = 3) x^2
error: grad names x twice

# A matrix, a single value and a tensor are each a name's point, and its
# part has its shape: a 1x1 matrix is a single value, so there is no
# mixture to refuse.
>> k = grad_(W = [1 2; 3 4], b = 5) ([1 1]*W*[1; 2] + b^2)
k = grad_(W = [1 2; 3 4], b = 5) ([1 1]*W*[1; 2] + b^2)

>> k.W
[1, 2;
 1, 2]

>> k.b
10

>> t = grad_(T = [1 2; 3 4;; 5 6; 7 8], v = [1; 1]) [1 1]*T[1]*T[2]*v
t = grad_(T = [1 2; 3 4;; 5 6; 7 8], v = [1; 1]) [1 1]*T[1]*T[2]*v

>> t.T
[11, 15;
 11, 15;;
  4,  4;
  6,  6]

>> t.v
[62;
 72]

# Two names of one shape at one point are two seeds, though the same
# operation on the same nodes is one node (C323).
>> h = grad_(A = [1 2], B = [1 2]) A*B'
h = grad_(A = [1 2], B = [1 2]) A*B'

>> h.A
[1, 2]

>> h.B
[1, 2]

# A term or a function: the arguments with the definition's name and the
# index with the part, as 'lowpass(a = 1/2).v_3' reads.
>> q_n = grad_(x = n, y = 1/n) x^2*y
q_n = grad_(x = n, y = 1/n) x^2*y

>> q.x_3
2

>> q.y_3
9

>> q.x
error: q.x is a sequence; index it (q.x_0)

>> q_3
error: q is a gradient by several names; read one of its parts (q.x_3)

>> q_0 = 1
error: q is a gradient by several names, and takes no other clause

>> r_n | n > 1 = grad_(x = n, y = 1) x*y
error: a gradient by several names takes no guard and no cells

>> qa_0 = 1
qa_0 = 1

>> qa_n = grad_(x = n, y = 1) x*y
error: qa is a gradient by several names, and takes no other clause

>> gs(a) = grad_(x = a, y = 2*a) x*y^3
gs(a) = grad_(x = a, y = 2*a) x*y^3

>> gs(1).x
8

>> gs(1).y
12

>> gs(1)
error: gs is a gradient by several names; read one of its parts (gs(...).x)

>> gs(1).z
error: gs(...).z is not defined

# A term of a function: the index is offered in the call's own form.
>> qs(a)_n = grad_(x = a*n, y = n) x^2*y
qs(a)_n = grad_(x = a*n, y = n) x^2*y

>> qs(2).x_3
36

>> qs(2).x
error: qs.x is a sequence; index it (qs(2).x_0)

# Every point is read where the grad is written, before any name is bound,
# and the names only in the body; a local of the body is bound once.
>> p = 5
p = 5

>> m = grad_(p = 1, y = p) p*y
m = grad_(p = 1, y = p) p*y

>> m.p
5

>> m.y
1

>> la = grad_(x = 2, y = 3) ((a = x*y) + a^2)
la = grad_(x = 2, y = 3) ((a = x*y) + a^2)

>> la.x
39

>> la.y
26

# What grad refuses by one name it refuses by each, in the same words, and
# a message that names the point names every name at its own.
>> nr = grad_(x = 2, y = 3) x^2
nr = grad_(x = 2, y = 3) x^2

>> nr.x
error: grad's expression does not read y

>> hv(s) = s*v
hv(s) = s*v

>> hg = grad_(u = 1, v = 2) hv(u)
hg = grad_(u = 1, v = 2) hv(u)

>> hg.u
error: hv reads the global v, which grad's v does not reach

# Every name is grad's in the body, not only the one asked: the body's x
# is grad's, and the global x reading y is never read (C407).
>> y = 5
y = 5

>> x = y + 1
x = y + 1

>> gy = grad_(x = 1, y = 2) (x*y)
gy = grad_(x = 1, y = 2) (x*y)

>> gy.x
2

>> gy.y
1

>> clear x
clear x

>> clear y
clear y

>> fl = grad_(x = 2, y = 3) floor(x*y)
fl = grad_(x = 2, y = 3) floor(x*y)

>> fl.x
error: floor jumps at x = 2, y = 3

# The whole grad, a part by a name the jump does not read too (C341).
>> fb = grad_(a = 0, b = 1) [a; floor(a)*b]
fb = grad_(a = 0, b = 1) [a; floor(a)*b]

>> fb.b
error: floor jumps at a = 0, b = 1

>> pw = grad_(x = 2, y = 3) x^y
pw = grad_(x = 2, y = 3) x^y

>> pw.y
error: grad cannot differentiate a power whose exponent changes with x or y, unless its base is e

>> jw = grad_(W = [1 2], b = 1) W*b
jw = grad_(W = [1 2], b = 1) W*b

>> jw.b
error: grad of a matrix with respect to a matrix is a Jacobian, which it does not give

# A part read under another grad is read as a value where it does not move,
# and refused where it does.
>> grad_(z = 1) z*gs(2).x
64

>> grad_(z = 1) gs(z).x
error: grad cannot differentiate a gradient by several names yet

# A part's name is not the name of a grad it is read under: gs(1).x reads
# no x of pg's.
>> pg = grad_(x = 1, y = 2) x*y*gs(1).x
pg = grad_(x = 1, y = 2) x*y*gs(1).x

>> pg.x
16

>> pg.y
8

# Single values alone are one sweep too. Forward mode walks tanh for t's
# exact part (C139), 254 calls deep, and refuses, as grad by t alone does;
# reverse mode takes the header's slope, 1 - tanh(1/2)^2, and u's part is
# tanh(1/2).
>> dive(k, s) = dive(k - 1, s)
dive(k, s) = dive(k - 1, s)

>> dive(k, s) | k < 1 = tanh(s)
dive(k, s) | k < 1 = tanh(s)

>> grad_(t = ~0.5) dive(254, t)
error: evaluation nests more than 256 references deep

>> d = grad_(t = ~0.5, u = 1) dive(254, t)*u
d = grad_(t = ~0.5, u = 1) dive(254, t)*u

>> d.t
~0.786447733

>> d.u
~0.462117157

# Where reverse mode gives way, forward mode takes a pass for each cell of
# each name, in the order written: a grad inside, a limit whose terms move,
# a complex point, and a matrix's derivatives by single values, which grad
# by one single value gives. 2y and 2x; 1/(2*sqrt(4)) and 1/(2*sqrt(9));
# 2xy and x^2 at i; y and 2x, x and 0.
>> n2 = grad_(x = 2, y = 3) (x*grad_(s = y) s^2)
n2 = grad_(x = 2, y = 3) (x*grad_(s = y) s^2)

>> n2.x
6

>> n2.y
4

>> rt(x)_0 = 1
rt(x)_0 = 1

>> rt(x)_n = (rt(x)_(n-1) + x/rt(x)_(n-1))/2
rt(x)_n = (rt(x)_(n-1) + x/rt(x)_(n-1))/2

>> lm = grad_(a = 4, b = 9) (lim rt(a) + lim rt(b))
lm = grad_(a = 4, b = 9) (lim rt(a) + lim rt(b))

>> lm.a
~0.25

>> lm.b
~0.166666667

>> c = grad_(x = i, y = 2) x^2*y
c = grad_(x = i, y = 2) x^2*y

>> c.x
~(i*4)

>> c.y
~-1

>> jm = grad_(x = 2, y = 3) [x*y, x^2]
jm = grad_(x = 2, y = 3) [x*y, x^2]

>> jm.x
[3, 4]

>> jm.y
[2, 0]

# A model's parameters as points: a grad's name reaches its body, not a
# definition of the model that reads the parameter, which is refused as a
# global is. (3w + c - 1)^2 at 2 and 1: 36 and 12.
>> fit(w = 2, c = 1) = {
..     L(s, e) = (3*s + e - 1)^2
..     g = grad_(w = w, c = c) L(w, c)
.. }
fit(w = 2, c = 1) = { ... }

>> fit().g.w
36

>> fit().g.c
12

# Each instance has parts of its own: at 3 and 1, and at 3 and 5.
>> fit(w = 3).g.w
54

>> fit(w = 3, c = 5).g.c
26

>> fit2(w = 2, c = 1) = {
..     L(s) = (3*s + c - 1)^2
..     g = grad_(w = w, c = c) L(w)
.. }
fit2(w = 2, c = 1) = { ... }

>> fit2().g.w
error: fit2(...).L reads the global c, which grad's c does not reach

# Two layers, a ReLU between them by its cells, x = [1; -2], target 1/2.
# The pre-activations 1, -1, 3/2 choose each unit's clause; the error is
# 13/4; by hand, W2's part is 13/4 times the units, the units' 13/4 times
# W2 where they pass, and W1's that times x'. One step of 1/10 on all four.
>> act(z)[j<=3] = z[j]
act(z)[j<=3] = z[j]

>> act(z)[j<=3] | z[j] < 0 = 0
act(z)[j<=3] | z[j] < 0 = 0

>> L2(W1, b1, W2, b2) = (W2*act(W1*[1; -2] + b1) + b2 - 1/2)^2/2
L2(W1, b1, W2, b2) = (W2*act(W1*[1; -2] + b1) + b2 - 1/2)^2/2

>> two = grad_(W1 = [2 1; -1 1; 1/2 -1], b1 = [1; 2; -1], W2 = [1/2 -1 2], b2 = 1/4) L2(W1, b1, W2, b2)
two = grad_(W1 = [2 1; -1 1; 1/2 -1], b1 = [1; 2; -1], W2 = [1/2 -1 2], b2 = 1/4) L2(W1, b1, W2, b2)

>> two.W1
[1.625, -3.25;
     0,     0;
   6.5,   -13]

>> two.b1
[1.625;
     0;
   6.5]

>> two.W2
[3.25, 0, 4.875]

>> two.b2
3.25

>> frac L2([2 1; -1 1; 1/2 -1] - two.W1/10, [1; 2; -1] - two.b1/10, [1/2 -1 2] - two.W2/10, 1/4 - two.b2/10)
833569/5120000

# The transformer of reverse.ink, trimmed from the exploration's (Vaswani
# et al. 2017, pre-layer-norm): one block of two heads, its thirteen weights
# one grad's names where they were thirteen grads of fifteen arguments. The
# gradient of W^Q is reverse.ink's; the loss after one step of 1/10 on
# every weight, and after two in a model whose term is the grad, NumPy's;
# the second step's gradient of c2 too.
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

>> dJ = grad_(E = E0, P = P0, Q = Q0, K = K0, Vv = Vv0, O = O0, W1 = W10, c1 = c10, W2 = W20, c2 = c20, G = G0, Bb = Bb0, U = U0) loss(X, Y, E, P, Q, K, Vv, O, W1, c1, W2, c2, G, Bb, U)
dJ = grad_(E = E0, P = P0, Q = Q0, K = K0, Vv = Vv0, O = O0, W1 = W10, c1 = c10, W2 = W20, c2 = c20, G = G0, Bb = Bb0, U = U0) loss(X, Y, E, P, Q, K, Vv, O, W1, c1, W2, c2, G, Bb, U)

>> dJ.Q
[~-0.0767740432,  ~-0.059784138;
   ~0.201562393,   ~0.119130257;
  ~0.0619377371,  ~0.0401917886;
  ~-0.186726087, ~-0.0995379074;;
  ~0.0715460743, ~-0.0676330906;
 ~-0.0266895869,  ~0.0898332103;
 ~-0.0279833467,  ~0.0197974097;
 ~-0.0168731408, ~-0.0419975294]

>> loss(X, Y, E0 - dJ.E/10, P0 - dJ.P/10, Q0 - dJ.Q/10, K0 - dJ.K/10, Vv0 - dJ.Vv/10, O0 - dJ.O/10, W10 - dJ.W1/10, c10 - dJ.c1/10, W20 - dJ.W2/10, c20 - dJ.c2/10, G0 - dJ.G/10, Bb0 - dJ.Bb/10, U0 - dJ.U/10)
~1.60260877

>> train(eta = 1/10) = {
..     E_0 = E0
..     P_0 = P0
..     Q_0 = Q0
..     K_0 = K0
..     Vv_0 = Vv0
..     O_0 = O0
..     W1_0 = W10
..     c1_0 = c10
..     W2_0 = W20
..     c2_0 = c20
..     G_0 = G0
..     Bb_0 = Bb0
..     U_0 = U0
..     g_n = grad_(E = E_(n-1), P = P_(n-1), Q = Q_(n-1), K = K_(n-1), Vv = Vv_(n-1), O = O_(n-1), W1 = W1_(n-1), c1 = c1_(n-1), W2 = W2_(n-1), c2 = c2_(n-1), G = G_(n-1), Bb = Bb_(n-1), U = U_(n-1)) loss(X, Y, E, P, Q, K, Vv, O, W1, c1, W2, c2, G, Bb, U)
..     E_n = E_(n-1) - eta*g.E_n
..     P_n = P_(n-1) - eta*g.P_n
..     Q_n = Q_(n-1) - eta*g.Q_n
..     K_n = K_(n-1) - eta*g.K_n
..     Vv_n = Vv_(n-1) - eta*g.Vv_n
..     O_n = O_(n-1) - eta*g.O_n
..     W1_n = W1_(n-1) - eta*g.W1_n
..     c1_n = c1_(n-1) - eta*g.c1_n
..     W2_n = W2_(n-1) - eta*g.W2_n
..     c2_n = c2_(n-1) - eta*g.c2_n
..     G_n = G_(n-1) - eta*g.G_n
..     Bb_n = Bb_(n-1) - eta*g.Bb_n
..     U_n = U_(n-1) - eta*g.U_n
..     J_n = loss(X, Y, E_n, P_n, Q_n, K_n, Vv_n, O_n, W1_n, c1_n, W2_n, c2_n, G_n, Bb_n, U_n)
.. }
train(eta = 1/10) = { ... }

>> run = train()
run = train()

>> run.J_1
~1.60260877

>> run.J_2
~1.16986363

>> run.g.c2_2
[~-0.0800354967, ~0.0916809774, ~-0.161322507, ~0.149677026]

>> run.g.c2
error: g.c2 is a sequence; index it (g.c2_0)

# A part whose index moves is refused as a part whose arguments move, an
# instance's as the session's: run's arguments do not move.
>> grad_(z = 1) run.g.c2_(z + 1)
error: grad cannot differentiate a gradient by several names yet

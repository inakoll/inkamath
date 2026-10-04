# grad of a definition by cells (DESIGN.md, next in line): each cell's
# clause, chosen as evaluating it chooses, differentiated at its row and
# column, and the parts assembled into the definition's shape. What is then a
# gradient or a Jacobian is decided as for any value. Written by hand, never
# recorded: the exact values are derived, the approximate ones computed apart.
>> sq(v)[j<=3] = v[j]^2
sq(v)[j<=3] = v[j]^2

>> grad_(t = 2) sq([t; 2*t; 3])
[ 4;
 16;
  0]

>> grad_(v = [1; 2; 3]) [3 2 1]*sq(v)
[6;
 8;
 6]

>> grad_(v = [1; 2; 3]) sq(v)
error: grad of a matrix with respect to a matrix is a Jacobian, which it does not give

# Softmax by its cells. Of two values it is the sigmoid of their difference,
# so its slope is the sigmoid's in grad.ink; at 0 each of three is 1/3.
>> sm(z, n)[j<=n] = exp(z[j])/sum_(c=1)^n exp(z[c])
sm(z, n)[j<=n] = exp(z[j])/sum_(c=1)^n exp(z[c])

>> grad_(z = [1; 2]) sm(z, 2)[1]
[ ~0.196611933;
 ~-0.196611933]

>> grad_(z = [0; 0; 0]) sm(z, 3)[1]
[ ~0.222222222;
 ~-0.111111111;
 ~-0.111111111]

# Normalised by a sum instead of exp, it is exact, and its cells' sum is 1,
# whose gradient is exactly 0.
>> nm(z)[j<=3] = z[j]/sum_(c=1)^3 z[c]
nm(z)[j<=3] = z[j]/sum_(c=1)^3 z[c]

>> frac grad_(z = [1; 2; 3]) nm(z)[1]
[ 5/36;
 -1/36;
 -1/36]

>> grad_(z = [1; 2; 3]) sum_(j=1)^3 nm(z)[j]
[0;
 0;
 0]

# A guard chooses each cell's clause at the point, one side of a threshold:
# with v[j] < 0 a cell at 0 has slope 1, with v[j] <= 0 slope 0.
>> rl(v)[j<=3] = v[j]
rl(v)[j<=3] = v[j]

>> rl(v)[j<=3] | v[j] < 0 = 0
rl(v)[j<=3] | v[j] < 0 = 0

>> grad_(t = 1) rl([t; 0-t; t - 1])
[1;
 0;
 1]

>> grad_(v = [2; -1; 0]) [1 2 3]*rl(v)
[1;
 0;
 3]

>> rl0(v)[j<=3] = v[j]
rl0(v)[j<=3] = v[j]

>> rl0(v)[j<=3] | v[j] <= 0 = 0
rl0(v)[j<=3] | v[j] <= 0 = 0

>> grad_(t = 1) rl0([t; 0-t; t - 1])
[1;
 0;
 0]

# Cells that read cells of other definitions, each call in a frame of its
# own, so their rows' names do not meet: the square's slope plus the ReLU's.
>> cs(v)[j<=3] = sq(v)[j] + rl(v)[j]
cs(v)[j<=3] = sq(v)[j] + rl(v)[j]

>> grad_(t = 1) cs([t; 0-t; t - 1])
[3;
 2;
 1]

# A cell may read a term at its row, or a limit: pq(a)_j is a^j, and the
# limit of hl(a*j) is (a*j)^2.
>> pq(a)_0 = 1
pq(a)_0 = 1

>> pq(a)_n = a*pq(a)_(n-1)
pq(a)_n = a*pq(a)_(n-1)

>> qa(a)[j<=3] = pq(a)_j
qa(a)[j<=3] = pq(a)_j

>> grad_(a = 3) qa(a)
[ 1;
  6;
 27]

>> hl(a)_0 = 0
hl(a)_0 = 0

>> hl(a)_n = a^2
hl(a)_n = a^2

>> lc(a)[j<=2] = lim hl(a*j)
lc(a)[j<=2] = lim hl(a*j)

>> grad_(a = 3) lc(a)
[ 6;
 24]

# A clause that holds at the point only is refused, naming the cell. A
# cell's place never moves, so a guard on it is no jump.
>> sp(v)[j<=2] = v[j]^2
sp(v)[j<=2] = v[j]^2

>> sp(v)[j<=2] | v[j] == 1 = 1
sp(v)[j<=2] | v[j] == 1 = 1

>> grad_(t = 3) sp([2*t; t])
[24;
  6]

>> grad_(t = 1) sp([2*t; t])
error: sp[2,1] takes a clause at t = 1 that holds only there

>> dg(x)[j<=2, k<=2] = 0
dg(x)[j<=2, k<=2] = 0

>> dg(x)[j<=2, k<=2] | j == k = x^2
dg(x)[j<=2, k<=2] | j == k = x^2

>> grad_(x = 3) dg(x)
[6, 0;
 0, 6]

# A clause for one cell beats the others, and the matrix written whole gives
# the cells no clause does, with their slopes.
>> oc(x)[j<=2, k<=2] = x*j*k
oc(x)[j<=2, k<=2] = x*j*k

>> oc(x)[1,2] = x^3
oc(x)[1,2] = x^3

>> grad_(x = 2) oc(x)
[1, 12;
 2,  4]

>> wh(x) = [x 0; 0 x]
wh(x) = [x 0; 0 x]

>> wh(x)[1,2] = x^2
wh(x)[1,2] = x^2

>> grad_(x = 3) wh(x)
[1, 6;
 0, 1]

# A clause for one cell may be guarded; at 1 its comparison's sides meet and
# move, and its value, false, gives the slope.
>> og(x)[j<=2] = x*j
og(x)[j<=2] = x*j

>> og(x)[2] | x > 1 = x^3
og(x)[2] | x > 1 = x^3

>> grad_(x = 2) og(x)
[ 1;
 12]

>> grad_(x = 1) og(x)
[1;
 2]

# A size may come from a parameter, or read the name where it does not move.
# A size is a whole number, so one that moves is at a jump.
>> pw(x, n)[j<=n] = x^j
pw(x, n)[j<=n] = x^j

>> grad_(x = 2) pw(x, 3)
[ 1;
  4;
 12]

>> grad_(x = 2) grad_(y = x) pw(y, 3)
[ 0;
  2;
 12]

>> fl(x)[j<=floor(x)] = x*j
fl(x)[j<=floor(x)] = x*j

>> grad_(x = 5/2) fl(x)
[1;
 2]

>> grad_(x = 2) fl(x)
error: floor jumps at x = 2

>> gr(n)[j<=n] = j
gr(n)[j<=n] = j

>> grad_(x = 2) gr(x)
error: the size of gr jumps at x = 2

>> bad(x)[j<=2] = [x x]
bad(x)[j<=2] = [x x]

>> grad_(x = 1) bad(x)
error: a cell of bad must be a single value, not a 1x2 matrix

# A term by its cells, through its recurrence; a base term and a cell of
# every term that both give a cell ask which, as evaluating them does.
>> h(a)_0[j<=2] = j
h(a)_0[j<=2] = j

>> h(a)_n[j<=2] = a*h(a)_(n-1)[j]
h(a)_n[j<=2] = a*h(a)_(n-1)[j]

>> grad_(a = 2) h(a)_3
[12;
 24]

>> g(a)_0[j<=2] = 1
g(a)_0[j<=2] = 1

>> g(a)_n[j<=2] = a*g(a)_(n-1)[j]
g(a)_n[j<=2] = a*g(a)_(n-1)[j]

>> g(a)_n[2] = n*a
g(a)_n[2] = n*a

>> grad_(a = 3) g(a)_2
error: g_0 and g_n[2,1] both give row 2, column 1 of g_0; write g_0[2,1] to say which

>> g(a)_0[2] = 1
g(a)_0[2] = 1

>> grad_(a = 3) g(a)_2
[6;
 2]

# A term's cell is named with its index, as evaluating it names the term.
>> qe(a)_n[j<=2] = a*j*n
qe(a)_n[j<=2] = a*j*n

>> qe(a)_n[j<=2] | a*j*n == 2 = 0
qe(a)_n[j<=2] | a*j*n == 2 = 0

>> grad_(a = 1) qe(a)_2
error: qe_2[1,1] takes a clause at a = 1 that holds only there

# A limit is walked from the highest base term, here written by its cells:
# below it the sequence has no term.
>> m(a)_2[j<=2] = j
m(a)_2[j<=2] = j

>> m(a)_n[j<=2] = a*j^2
m(a)_n[j<=2] = a*j^2

>> grad_(a = 3) lim m(a)
[1;
 4]

# A clause for one cell is no base term, so the walk starts below it: ob_6
# divides by zero at 3, and the limit, reached at ob_4, never reads it.
>> ob(a)_2[j<=2] = j
ob(a)_2[j<=2] = j

>> ob(a)_n[j<=2] = a*j^2
ob(a)_n[j<=2] = a*j^2

>> ob(a)_6[1] = 1/(a - 3)
ob(a)_6[1] = 1/(a - 3)

>> lim ob(3)
[ 3;
 12]

>> grad_(a = 3) lim ob(a)
[1;
 4]

# Without parameters a definition by cells reads only globals, which grad's
# name does not reach: a constant, or refused as any definition is.
>> N[j<=2, k<=2] = j + k
N[j<=2, k<=2] = j + k

>> layer(w)[j<=2] = N[j]*w
layer(w)[j<=2] = N[j]*w

>> grad_(w = [1; 1]) [1 1]*layer(w)
[5;
 7]

>> M[j<=2] = j*x
M[j<=2] = j*x

>> grad_(x = 1) M
error: M reads the global x, which grad's x does not reach

# A tensor by its cells, slice by slice. A slice's name is bound in its
# clause as a row's is, so it is not the global of grad's name.
>> G(x)[b<=2, j<=1, k<=2] = x^b*k
G(x)[b<=2, j<=1, k<=2] = x^b*k

>> grad_(x = 3) G(x)
[1,  2;;
 6, 12]

>> grad_(b = 3) G(b)
[1,  2;;
 6, 12]

>> tg(x)[b<=2, j<=1, k<=2] = x*b*k
tg(x)[b<=2, j<=1, k<=2] = x*b*k

>> tg(x)[b<=2, j<=1, k<=2] | b == k = x^2
tg(x)[b<=2, j<=1, k<=2] | b == k = x^2

>> grad_(x = 3) tg(x)
[6, 2;;
 2, 6]

>> tq(x)[b<=2, j<=2, k<=1] = x*b*j
tq(x)[b<=2, j<=2, k<=1] = x*b*j

>> tq(x)[b<=2, j<=2, k<=1] | x*j == 2 = 0
tq(x)[b<=2, j<=2, k<=1] | x*j == 2 = 0

>> grad_(x = 1) tq(x)
error: tq[1,2,1] takes a clause at x = 1 that holds only there

>> Gc[b<=2, j<=1, k<=1] = b
Gc[b<=2, j<=1, k<=1] = b

>> grad_(b = 2) b*Gc
[1;;
 2]

# Inside an instance, as anything there.
>> net(a = 1) = {
..     y[j<=2] = a*j
.. }
net(a = 1) = { ... }

>> grad_(x = 1) net(a = x).y
error: grad cannot differentiate through an instance yet

# Attention's queries trained: one head, three tokens of width 2, softmax by
# its cells. The gradient with respect to W^Q beside the one written by hand
# through softmax's Jacobian, which finite differences confirm to 3e-10.
>> X = [1 0; 0 1; 1 1]
X = [1 0; 0 1; 1 1]

>> K = X*[0 1; 1 1]
K = X*[0 1; 1 1]

>> V = X*[1 2; 0 1]
V = X*[1 2; 0 1]

>> Y = [1 -1; 0 2; 1 0]
Y = [1 -1; 0 2; 1 0]

>> S(w) = X*w*K'/2
S(w) = X*w*K'/2

>> A(w)[t<=3, s<=3] = exp(S(w)[t,s])/sum_(r=1)^3 exp(S(w)[t,r])
A(w)[t<=3, s<=3] = exp(S(w)[t,s])/sum_(r=1)^3 exp(S(w)[t,r])

>> loss(w) = sum_(t=1)^3 sum_(c=1)^2 (A(w)*V)[t,c]*Y[t,c]
loss(w) = sum_(t=1)^3 sum_(c=1)^2 (A(w)*V)[t,c]*Y[t,c]

>> G2 = Y*V'
G2 = Y*V'

>> dS(w)[t<=3, s<=3] = A(w)[t,s]*(G2[t,s] - sum_(r=1)^3 A(w)[t,r]*G2[t,r])
dS(w)[t<=3, s<=3] = A(w)[t,s]*(G2[t,s] - sum_(r=1)^3 A(w)[t,r]*G2[t,r])

>> hand(w) = X'*dS(w)*K/2
hand(w) = X'*dS(w)*K/2

>> W0 = [1 0; 1 1]
W0 = [1 0; 1 1]

>> grad_(w = W0) loss(w)
[~-0.0648506377, ~-0.0276412633;
   ~0.016918008,   ~0.496136974]

>> dW = grad_(w = W0) loss(w) - hand(W0)
dW = grad_(w = W0) loss(w) - hand(W0)

>> sum_(j=1)^2 sum_(k=1)^2 dW[j,k]^2 < 1/10^24
1

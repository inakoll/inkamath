# Sizes inferred (DESIGN.md, next in line): an index written without its
# bound takes it from the matrices the clause reads at that index alone,
# z[i,j] giving i the rows of z and j its columns, every such read agreeing.
# So a function of cells is one line, of a matrix of any size, and a layer is
# written where it is used. Written by hand, never recorded: the exact values
# are derived, the approximate ones computed apart with mpmath.

# --- one line, any size ------------------------------------------------------

>> rl(z)[i,j] = z[i,j]*(z[i,j] > 0)
rl(z)[i,j] = z[i,j]*(z[i,j] > 0)

>> rl([1 -2; 0 3])
[1, 0;
 0, 3]

>> rl([1 -1])
[1, 0]

# Remembered by its arguments, their shapes included: the same cells as a
# column.
>> rl([1; -1])
[1;
 0]

# A single value is read as a 1x1 matrix is, so it gives one.
>> rl(-3)
0

>> rl(5)
5

>> ?rl
rl(z)[i,j] = z[i,j]*(z[i,j] > 0)

# Set as a paper sets it: no range where none is written.
>> tex ?rl
\operatorname{rl}(z)_{i,j} = z_{i,j}\,(z_{i,j} > 0)

>> act(z)[i,j] = tanh(z[i,j])
act(z)[i,j] = tanh(z[i,j])

>> act([0 1/2])
[0, ~0.462117157]

>> act(1/2)
~0.462117157

>> tex ?act
\operatorname{act}(z)_{i,j} = \operatorname{tanh}(z_{i,j})

# --- a layer where it is used ------------------------------------------------

>> X = [1 2; -1 1; 2 -1]
X = [1 2; -1 1; 2 -1]

>> W = [1 -1; 1/2 1]
W = [1 -1; 1/2 1]

>> b = -1/2
b = -1/2

>> Z = X*W + b
Z = X*W + b

>> h[i,j] = Z[i,j]*(Z[i,j] > 0)
h[i,j] = Z[i,j]*(Z[i,j] > 0)

>> h
[1.5, 0.5;
   0, 1.5;
   1,   0]

>> tex ?h
h_{i,j} = Z_{i,j}\,(Z_{i,j} > 0)

# The size is Z's when h is read, so it follows Z.
>> Z = [1 -1]
Z = [1 -1]

>> h
[1, 0]

# A clause for one cell gives no size, as now: it overrides a cell of the
# size inferred, and one outside it says so as reading it would.
>> h[1,1] = 7
h[1,1] = 7

>> h
[7, 0]

>> h[2,1] = 5
h[2,1] = 5

>> h
error: row 2, column 1 is outside a 1x2 matrix

# --- what a read gives -------------------------------------------------------

# One index on a matrix reads a row, so a column takes the rows, and a row
# vector is read through its transpose, as on paper.
>> dbl(v)[i] = 2*v[i]
dbl(v)[i] = 2*v[i]

>> dbl([1; 2; 3])
[2;
 4;
 6]

>> dbl([1 2 3])
error: a cell of dbl must be a single value, not a 1x3 matrix

>> dbl([1 2 3]')
[2;
 4;
 6]

# A read gives the indices written alone in it: z[i,1] gives i the rows,
# and its number nothing.
>> col1(z)[i] = z[i,1]
col1(z)[i] = z[i,1]

>> col1([1 2; 3 4])
[1;
 3]

# A bound written is the bound, beside one inferred, and is not checked
# against the reads: it may take fewer rows than z has.
>> top(z)[i<=2, j] = z[i,j]
top(z)[i<=2, j] = z[i,j]

>> top([1 2; 3 4; 5 6])
[1, 2;
 3, 4]

>> top([1 2])
error: row 2, column 1 is outside a 1x2 matrix

>> tex ?top
\operatorname{top}(z)_{i,j} = z_{i,j}, \quad 1 \le i \le 2

# A column of z is a row of the cell's, as written.
>> tp(z)[i,j] = z[j,i]
tp(z)[i,j] = z[j,i]

>> tp([1 2 3])
[1;
 2;
 3]

# Each index from the matrix it reads; a sum's index is the sum's, bounded
# as on paper.
>> mm(A, B)[i,j] = sum_(k=1)^2 A[i,k]*B[k,j]
mm(A, B)[i,j] = sum_(k=1)^2 A[i,k]*B[k,j]

>> mm([1 2; 3 4], [1; 1])
[3;
 7]

# The matrix read may be any expression that reads no index of the cell.
>> sf(x)[i] = (2*x + 1)[i]
sf(x)[i] = (2*x + 1)[i]

>> sf([1; 2])
[3;
 5]

# A read may call a definition whose size is inferred in turn.
>> ab(z)[i,j] = rl(z)[i,j] + rl(-z)[i,j]
ab(z)[i,j] = rl(z)[i,j] + rl(-z)[i,j]

>> ab([1 -2; 0 3])
[1, 2;
 0, 3]

# But not the definition itself, whose size reading it would need first: its
# other reads give it.
>> g(z)[i,j] | z[i,j] > 1 = g(z/2)[i,j]
g(z)[i,j] | z[i,j] > 1 = g(z/2)[i,j]

>> g(z)[i,j] = z[i,j]^2
g(z)[i,j] = z[i,j]^2

>> g([2 4; 3 0])
[     1, 1;
 0.5625, 0]

# Every read of an index agrees, or it is refused naming two that do not:
# taking the first would cut b short or read past it, by the order written.
>> add(a, b)[i,j] = a[i,j] + b[i,j]
add(a, b)[i,j] = a[i,j] + b[i,j]

>> add([1 2], [3 4])
[4, 6]

>> add([1 2], [3 4 5])
error: a[i,j] and b[i,j] give j different sizes, 2 and 3

>> add([1 2], 3)
error: a[i,j] and b[i,j] give j different sizes, 2 and 1

>> dg(z)[i] = z[i,i]
dg(z)[i] = z[i,i]

>> dg([1 2; 3 4])
[1;
 4]

>> dg([1 2 3])
error: z[i,i] gives i different sizes, 1 and 3

# An index that no matrix is read at alone has no size.
>> M[r,c] = r + c
M[r,c] = r + c

>> M
error: M has no size, as nothing reads a matrix at r alone; write it as M[r<=rows, c<=cols]

>> sh(z)[i] = z[i+1]
sh(z)[i] = z[i+1]

>> sh([1; 2; 3])
error: sh has no size, as nothing reads a matrix at i alone; write it as sh(z)[i<=rows]

# Nor does a matrix that reads a name bound in the clause, a sum's index: it
# is another matrix at each k.
>> ex3(A)[i,j] = sum_(k=0)^3 (A^k)[i,j]
ex3(A)[i,j] = sum_(k=0)^3 (A^k)[i,j]

>> ex3([1 1; 0 1])
error: ex3 has no size, as nothing reads a matrix at i alone; write it as ex3(A)[i<=rows, j<=cols]

# The bounds written are kept.
>> tl(z)[i<=2, j] = i
tl(z)[i<=2, j] = i

>> tl([1 2])
error: tl has no size, as nothing reads a matrix at j alone; write it as tl(z)[i<=2, j<=cols]

# --- guards ------------------------------------------------------------------

# A guard's reads give a size as the value's do, so the ReLU of README.md
# section 4 needs no bound.
>> relu(z)[i,j] = z[i,j]
relu(z)[i,j] = z[i,j]

>> relu(z)[i,j] | z[i,j] < 0 = 0
relu(z)[i,j] | z[i,j] < 0 = 0

>> relu([1 -2; 0 3])
[1, 0;
 0, 3]

>> tex ?relu
\operatorname{relu}(z)_{i,j} = \begin{cases} 0 & \text{if } z_{i,j} < 0 \\ z_{i,j} & \text{otherwise} \end{cases}

# A guard alone gives it, and a cell no clause gives is 0, as now.
>> sgn(z)[i,j] | z[i,j] > 0 = 1
sgn(z)[i,j] | z[i,j] > 0 = 1

>> sgn([1 -2; 0 3])
[1, 0;
 0, 1]

# A clause that reads nothing takes the size of the clauses that do: a
# causal mask.
>> cm(s)[i,j] = s[i,j]
cm(s)[i,j] = s[i,j]

>> cm(s)[i,j] | j > i = 0
cm(s)[i,j] | j > i = 0

>> cm([1 2; 3 4])
[1, 0;
 3, 4]

>> ms(s)[i,j] | j > i = 0
ms(s)[i,j] | j > i = 0

>> ms([1 2; 3 4])
error: ms has no size, as nothing reads a matrix at i alone; write it as ms(s)[i<=rows, j<=cols]

# Each index takes its size from the clauses that give it one, and the
# clauses agree.
>> pk(a, b)[i,j] = a[i,1]
pk(a, b)[i,j] = a[i,1]

>> pk(a, b)[i,j] | i == j = b[1,j]
pk(a, b)[i,j] | i == j = b[1,j]

>> pk([1; 2], [5 6])
[5, 1;
 2, 6]

>> pd(a, b)[i,j] = a[i,j]
pd(a, b)[i,j] = a[i,j]

>> pd(a, b)[i,j] | i == j = b[i,j]
pd(a, b)[i,j] | i == j = b[i,j]

>> pd([1 2; 3 4], [1 2 3])
error: the clauses of pd give it different sizes

# A read is evaluated for its size before any cell, so in a clause no cell
# takes as well, and its steps count against the budget: with i<=2 written,
# lg(1) is [0; 0].
>> lg(n)[i] = 0
lg(n)[i] = 0

>> lg(n)[i] | n < 0 = ([1; 1]*sum_(k=1)^(10^7) 1)[i]
lg(n)[i] | n < 0 = ([1; 1]*sum_(k=1)^(10^7) 1)[i]

>> lg(1)
error: evaluation gave up after 1000000 steps

# --- terms -------------------------------------------------------------------

# A recurrent layer: the function of cells takes each term's shape.
>> R = [1/2 -1; 1 1/2]
R = [1/2 -1; 1 1/2]

>> u_0 = [0; 0]
u_0 = [0; 0]

>> u_n = rl(R*u_(n-1) + [1; -1])
u_n = rl(R*u_(n-1) + [1; -1])

>> u_4
[1.375;
     1]

# A term by its cells, its size read at its index.
>> q_n[i] = u_n[i]^2
q_n[i] = u_n[i]^2

>> q_4
[1.890625;
        1]

# A base term's size is its own, as when written.
>> q_0[i] = 1
q_0[i] = 1

>> q_0
error: q_0 has no size, as nothing reads a matrix at i alone; write it as q_0[i<=rows]

# A term may take its size from the one before.
>> cn_0 = [0 0]
cn_0 = [0 0]

>> cn_n[i,j] = cn_(n-1)[i,j] + j
cn_n[i,j] = cn_(n-1)[i,j] + j

>> cn_3
[3, 6]

# An earlier term is not the definition itself, with arguments too.
>> pw(x)_0 = [1; 2]
pw(x)_0 = [1; 2]

>> pw(x)_n[i] = x*pw(x)_(n-1)[i]
pw(x)_n[i] = x*pw(x)_(n-1)[i]

>> pw(3)_2
[ 9;
 18]

# A term read at a sum's index gives nothing, as any read of a sum's index.
>> w_0 = [1; 2]
w_0 = [1; 2]

>> w_n = 2*w_(n-1)
w_n = 2*w_(n-1)

>> cs_n[i] = sum_(m=0)^n w_m[i]
cs_n[i] = sum_(m=0)^n w_m[i]

>> cs_2
error: cs_2 has no size, as nothing reads a matrix at i alone; write it as cs_n[i<=rows]

# --- tensors -----------------------------------------------------------------

>> rl3(z)[s,i,j] = z[s,i,j]*(z[s,i,j] > 0)
rl3(z)[s,i,j] = z[s,i,j]*(z[s,i,j] > 0)

>> rl3([1 -1;; -2 2])
[1, 0;;
 0, 2]

# Two indices do not read a tensor, which is said as reading it says it.
>> rl([1 -1;; -2 2])
error: a 2x1x2 tensor takes one index or three, not two

>> bias(z)[s,i,j] = z[s,i,j] + s
bias(z)[s,i,j] = z[s,i,j] + s

>> bias([1 2;; 3 4])
[2, 3;;
 5, 6]

>> T = [1 2; 3 4;; 5 6; 7 8]
T = [1 2; 3 4;; 5 6; 7 8]

# Three indices on a tensor, the sum's among them, give s the slices.
>> trc(P)[s] = sum_(j=1)^2 P[s,j,j]
trc(P)[s] = sum_(j=1)^2 P[s,j,j]

>> trc(T)
[ 5;
 13]

# One index on a tensor reads a slice, which gives s the slices as well.
>> tr2(P)[s] = sum_(j=1)^2 P[s][j,j]
tr2(P)[s] = sum_(j=1)^2 P[s][j,j]

>> tr2(T)
[ 5;
 13]

>> fs(P)[i,j] = P[1][i,j]
fs(P)[i,j] = P[1][i,j]

>> fs(T)
[1, 2;
 3, 4]

# A slice at a sum's index is another at each k, so it gives nothing.
>> ps(P)[i,j] = sum_(k=1)^2 P[k][i,j]
ps(P)[i,j] = sum_(k=1)^2 P[k][i,j]

>> ps(T)
error: ps has no size, as nothing reads a matrix at i alone; write it as ps(P)[i<=rows, j<=cols]

# --- a call given a matrix where single values are needed --------------------

# The innermost call written in the session that was given a value of the
# refused shape is named, with that shape.
>> tanh([0; 1/2])
error: tanh needs single values, not a 2x1 matrix; write it by its cells

>> exp([0 1; 2 3])
error: exp needs single values, not a 2x2 matrix; write it by its cells

>> ex(z)[i,j] = exp(z[i,j])
ex(z)[i,j] = exp(z[i,j])

>> ex([0 1; 2 3])
[         1, ~2.71828183;
 ~7.3890561, ~20.0855369]

# Outside a call, an operator keeps its words; e^A is the matrix
# exponential on paper, and stays refused.
>> e^[0 1; 2 3]
error: a matrix cannot be an exponent

>> [1 2] < 3
error: a comparison needs single values, not a 1x2 matrix

# No longer a comparison the session never wrote.
>> log([2 10])
error: log needs single values, not a 1x2 matrix; write it by its cells

# exp is written in sig, so it is the innermost call the session wrote.
>> sig(z) = 1/(1 + exp(-z))
sig(z) = 1/(1 + exp(-z))

>> sig([0 1 -1])
error: exp needs single values, not a 1x3 matrix; write it by its cells

>> sg(z)[i,j] = sig(z[i,j])
sg(z)[i,j] = sig(z[i,j])

>> sg([0 1 -1])
[0.5, ~0.731058579, ~0.268941421]

# A guard, and a tensor.
>> nonzero(x) | x = 1
nonzero(x) | x = 1

>> nonzero(x) | x == 0 = 0
nonzero(x) | x == 0 = 0

>> nonzero([1 2])
error: nonzero needs single values, not a 1x2 matrix; write it by its cells

>> nz(x) = 0
nz(x) = 0

>> nz(x) | x = 1
nz(x) | x = 1

>> nz([7;;])
error: nz needs single values, not a 1x1x1 tensor; write it by its cells

>> th3(z)[s,i,j] = tanh(z[s,i,j])
th3(z)[s,i,j] = tanh(z[s,i,j])

>> th3([0;; 1/2])
[           0;;
 ~0.462117157]

# A value of a shape no call was given keeps the operator's words.
>> big(x) = x*x' > 0
big(x) = x*x' > 0

>> big([1; 2])
error: a comparison needs single values, not a 2x2 matrix

# mod divides by a single value: by a matrix, b*floor(a/b) would be a
# product of matrices, which it never means.
>> mod([7 8 9], 3)
[1, 2, 0]

>> mod([7 8; 9 10], [3 3; 3 3])
error: mod needs a single value to divide by, not a 2x2 matrix; write it by its cells

>> mod(7, [3 4 5])
error: mod needs a single value to divide by, not a 1x3 matrix; write it by its cells

>> md(a, b)[i,j] = mod(a[i,j], b[i,j])
md(a, b)[i,j] = mod(a[i,j], b[i,j])

>> md([7 8; 9 10], [3 4; 5 6])
[1, 0;
 4, 4]

>> md([7 8], [3 4 5])
error: a[i,j] and b[i,j] give j different sizes, 2 and 3

# --- grad --------------------------------------------------------------------

# A size read is a shape, which never moves with grad's name.
>> grad_(v = [3; -1]) [1 1]*rl(v)
[1;
 0]

# z*(z > 0) has no slope at 0, and says so; the guarded ReLU takes its
# guard's side, as gradcells.ink's does.
>> grad_(t = 0) [1 1]*rl([t; 1])
error: a comparison jumps at t = 0

>> grad_(t = 0) [1 1]*relu([t; 1])
1

# --- a ReLU network trained one step -----------------------------------------

# Two layers, the first the one-line ReLU, trained by grad on a squared loss
# over three samples, none at 0 before the layer. Exact throughout: the
# gradient is held to the one written by hand through the ReLU's slope, and
# the loss before and after the step and the new weights are derived by hand.
>> X = [1 2; -1 1; 2 -1]
X = [1 2; -1 1; 2 -1]

>> t = [1; 0; 2]
t = [1; 0; 2]

>> b = [1/2; -1]
b = [1/2; -1]

>> v = [1 -1]
v = [1 -1]

>> net(W, x) = v*rl(W*x + b)
net(W, x) = v*rl(W*x + b)

>> loss(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])^2/2
loss(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])^2/2

>> A = [1 -1; 1/2 1]
A = [1 -1; 1/2 1]

>> loss(A)
4.25

>> grad_(W = A) loss(W)
[  3, -1.5;
 2.5,    5]

# The ReLU's slope by its cells: v' and the layer read at i agree on 2.
>> dz(W, x)[i] = v'[i]*((W*x + b)[i] > 0)
dz(W, x)[i] = v'[i]*((W*x + b)[i] > 0)

>> hand(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])*dz(W, X[r]')*X[r]
hand(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])*dz(W, X[r]')*X[r]

>> (grad_(W = A) loss(W)) == hand(A)
1

>> A1 = A - grad_(W = A) loss(W)/4
A1 = A - grad_(W = A) loss(W)/4

>> A1
[  0.25, -0.625;
 -0.125,  -0.25]

>> loss(A1)
0.5703125

# A layer that applies a function of single values to the whole names it.
>> rs(x) = x
rs(x) = x

>> rs(x) | x < 0 = 0
rs(x) | x < 0 = 0

>> bad(W, x) = v*rs(W*x + b)
bad(W, x) = v*rs(W*x + b)

>> bad(A, X[1]')
error: rs needs single values, not a 2x1 matrix; write it by its cells

# --- softmax and cross-entropy trained one step ------------------------------

# Softmax as the paper writes it: j from z, and the sum bounded by the
# number of classes, K, as the page bounds it. One step of a linear
# classifier, grad's gradient held to (p - y) x' written by hand; the values
# are mpmath's, and NumPy's to the eight digits it shows.
>> sm(z, K)[j] = exp(z[j])/sum_(c=1)^K exp(z[c])
sm(z, K)[j] = exp(z[j])/sum_(c=1)^K exp(z[c])

>> sm([1; 2; 3], 3)
[~0.0900305732;
  ~0.244728471;
  ~0.665240956]

>> tex ?sm
\operatorname{sm}(z, K)_j = \frac{\operatorname{exp}(z_{j})}{\sum_{c=1}^{K} \operatorname{exp}(z_{c})}

>> ce(z, k, K) = -log(sm(z, K)[k])
ce(z, k, K) = -log(sm(z, K)[k])

>> S = [1 0; 0 1; 1 1]
S = [1 0; 0 1; 1 1]

>> y = [1; 3; 2]
y = [1; 3; 2]

>> Id = [1 0 0; 0 1 0; 0 0 1]
Id = [1 0 0; 0 1 0; 0 0 1]

>> L(W) = sum_(r=1)^3 ce(W*S[r]', y[r], 3)
L(W) = sum_(r=1)^3 ce(W*S[r]', y[r], 3)

>> G(W) = sum_(r=1)^3 (sm(W*S[r]', 3) - Id[y[r]]')*S[r]
G(W) = sum_(r=1)^3 (sm(W*S[r]', 3) - Id[y[r]]')*S[r]

>> ss(D) = sum_(j=1)^3 sum_(k=1)^2 D[j,k]^2
ss(D) = sum_(j=1)^3 sum_(k=1)^2 D[j,k]^2

>> W0 = [1 0; 0 1; 1/2 -1/2]
W0 = [1 0; 0 1; 1/2 -1/2]

>> L(W0)
~3.50663326

>> ss((grad_(W = W0) L(W)) - G(W0)) < 1/10^16
1

>> W1 = W0 - grad_(W = W0) L(W)/2
W1 = W0 - grad_(W = W0) L(W)/2

>> W1
[ ~1.03560041, ~-0.326771348;
 ~0.195678739,  ~0.974574741;
 ~0.268720855, ~-0.147803393]

>> L(W1)
~2.91415591

# A function applied to each cell, said at the call: f.(x), after Julia's dot
# (DESIGN.md, next in line). Without the dot a call is the function of the
# whole value, as it was: x^2 squares a matrix, and e^A, on paper the matrix
# exponential, stays refused. Written by hand, never recorded: the exact
# values are derived, the approximate ones computed apart with mpmath.

# --- the prelude -------------------------------------------------------------

>> exp.([0 1; 2 3])
[         1, ~2.71828183;
 ~7.3890561, ~20.0855369]

# Without the dot, the call that needed single values is named, with its map.
>> exp([0 1; 2 3])
error: exp needs single values, not a 2x2 matrix; exp.(x) maps it over the cells

>> e^[0 1; 2 3]
error: a matrix cannot be an exponent

# Of a single value, a map is the call.
>> exp.(0)
1

>> log.([2 10])
[~0.693147181, ~2.30258509]

# No longer a comparison the session never wrote.
>> log([2 10])
error: log needs single values, not a 1x2 matrix; log.(x) maps it over the cells

# Each cell is a call of its own, and fails as that call does.
>> log.([2 0])
error: division by zero

>> tanh.([0; 1/2])
[           0;
 ~0.462117157]

# The innermost call refused is named: tanh reads its argument whole and
# hands it to exp.
>> tanh([0; 1/2])
error: exp needs single values, not a 2x1 matrix; exp.(x) maps it over the cells

# floor is already a function of cells, and what is written from it with a
# single value is cell by cell already: the dot changes nothing there.
>> floor.([1/2 -1/2])
[0, -1]

>> floor([1/2 -1/2])
[0, -1]

>> ceil.([1/2 -1/2])
[1, 0]

>> mod.([7 8 9], 3)
[1, 2, 0]

>> mod([7 8 9], 3)
[1, 2, 0]

# Of two matrices, mod as written multiplies them, b*floor(a/b) being a
# product; the map is mod of each pair of cells.
>> mod([7 8; 9 10], [3 3; 3 3])
[-8, -7;
 -6, -5]

>> mod.([7 8; 9 10], [3 4; 5 6])
[1, 0;
 4, 4]

# A single value meets every cell, as it meets an operator; by name as by
# position.
>> mod.(7, [3 4 5])
[1, 3, 2]

>> mod.(b = 3, a = [7 8])
[1, 2]

# Not stretched: NumPy's broadcasting is rejected here as for the operators.
>> mod.([7 8], [3; 4])
error: these matrices have different sizes

# --- functions of the session ------------------------------------------------

>> sq(x) = x^2
sq(x) = x^2

>> sq([1 2; 3 4])
[ 7, 10;
 15, 22]

>> sq.([1 2; 3 4])
[1,  4;
 9, 16]

>> sig(z) = 1/(1 + exp(-z))
sig(z) = 1/(1 + exp(-z))

>> sig.([0 1 -1])
[0.5, ~0.731058579, ~0.268941421]

>> sig([0 1 -1])
error: exp needs single values, not a 1x3 matrix; exp.(x) maps it over the cells

# Mapped inside, it takes a matrix whole, '+' and '/' with a single value
# being cell by cell already.
>> sg(z) = 1/(1 + exp.(-z))
sg(z) = 1/(1 + exp.(-z))

>> sg([0 1 -1])
[0.5, ~0.731058579, ~0.268941421]

# A definition in cases chooses its clause cell by cell.
>> relu(x) = x
relu(x) = x

>> relu(x) | x < 0 = 0
relu(x) | x < 0 = 0

>> relu.([1 -2; 0 3])
[1, 0;
 0, 3]

>> relu([1 -2; 0 3])
error: relu needs single values, not a 2x2 matrix; relu.(x) maps it over the cells

# A bare guard, as conditional.ink's nonzero, whose refusal moves to these
# words.
>> nz(x) = 0
nz(x) = 0

>> nz(x) | x = 1
nz(x) | x = 1

>> nz.([5 0])
[1, 0]

>> nz([5 0])
error: nz needs single values, not a 1x2 matrix; nz.(x) maps it over the cells

# Every matrix given is mapped; one meant whole is read by a function of
# fewer arguments.
>> aff(x, w) = [x 1]*w
aff(x, w) = [x 1]*w

>> aff.([1 2], [2; 1])
error: these matrices have different sizes

>> af(x) = aff(x, [2; 1])
af(x) = aff(x, [2; 1])

>> af.([1 2])
[3, 5]

# Each cell must be a single value.
>> col(x) = [x; 2*x]
col(x) = [x; 2*x]

>> col.([1 2])
error: a cell of col must be a single value, not a 2x1 matrix

# Only a function maps; a value is refused as its call is.
>> s(x)_n = x*n
s(x)_n = x*n

>> s.([1 2])_3
error: s is a sequence, and only a function maps over cells

>> a = 2
a = 2

>> a.([1 2])
error: a takes no arguments

# A recurrent layer with no size written: the map takes its shape.
>> R = [1/2 -1; 1 1/2]
R = [1/2 -1; 1 1/2]

>> h_0 = [0; 0]
h_0 = [0; 0]

>> h_n = relu.(R*h_(n-1) + [1; -1])
h_n = relu.(R*h_(n-1) + [1; -1])

>> h_4
[1.375;
     1]

# --- tensors -----------------------------------------------------------------

>> sq.([1 2;; 3 4])
[1,  4;;
 9, 16]

>> relu.([1 -1;; -2 2])
[1, 0;;
 0, 2]

# A tensor keeps its rank, of one cell as of any.
>> exp.([0;;])
[1;;]

# A matrix meets every slice, another tensor its slices in turn, as in an
# operator.
>> mod.([7 8;; 9 10], [3 4])
[1, 0;;
 0, 2]

>> mod.([7 8;; 9 10], [3 4;; 5 6])
[1, 0;;
 4, 4]

>> mod.([7 8;; 9 10], [3 4;; 5 6;; 7 8])
error: a 2x1x2 tensor and a 3x1x2 tensor have different numbers of slices

>> mod.([7 8;; 9 10], [3 4 5])
error: these matrices have different sizes

>> relu([1 -1;; -2 2])
error: relu needs single values, not a 2x1x2 tensor; relu.(x) maps it over the cells

# --- grad --------------------------------------------------------------------

# Each cell differentiated as its call is; a gradient or a Jacobian is
# decided as for any value.
>> grad_(x = 3) sq.([x 2*x])
[6, 24]

>> grad_(z = [1; 2]) [1 1]*sq.(z)
[2;
 4]

>> grad_(z = [1; 2]) sq.(z)
error: grad of a matrix with respect to a matrix is a Jacobian, which it does not give

>> grad_(z = [0; 1]) [1 1]*sig.(z)
[        0.25;
 ~0.196611933]

>> grad_(z = [0; 1]) [1 1]*sig(z)
error: exp needs single values, not a 2x1 matrix; exp.(x) maps it over the cells

# A ReLU's slope at 0 is its guard's: x < 0 does not hold there, so the
# clause x gives 1.
>> grad_(z = [0; -1; 2]) [1 1 1]*relu.(z)
[1;
 0;
 1]

>> q2(x) = 1
q2(x) = 1

>> q2(x) | x^2 == 0 = 5
q2(x) | x^2 == 0 = 5

>> grad_(t = 1) [1 1]*q2.([t - 1; t])
error: q2 takes a clause at t = 1 that holds only there

>> grad_(T = [1 2;; 3 4]) sq.(T)[2,1,2]
[0, 0;;
 0, 8]

# --- a ReLU network trained one step -----------------------------------------

# Two layers, the first a ReLU as the paper writes it, trained by grad on a
# squared loss over three samples. Exact throughout: the gradient is held to
# the one written by hand through the ReLU's slope, and the loss before and
# after the step, the gradient and the new weights are derived by hand.
>> X = [1 2; -1 1; 2 -1]
X = [1 2; -1 1; 2 -1]

>> t = [1; 0; 2]
t = [1; 0; 2]

>> b = [1/2; -1]
b = [1/2; -1]

>> v = [1 -1]
v = [1 -1]

>> net(W, x) = v*relu.(W*x + b)
net(W, x) = v*relu.(W*x + b)

>> loss(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])^2/2
loss(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])^2/2

>> A = [1 -1; 1/2 1]
A = [1 -1; 1/2 1]

>> loss(A)
4.25

>> grad_(W = A) loss(W)
[  3, -1.5;
 2.5,    5]

>> step(x) = 1
step(x) = 1

>> step(x) | x < 0 = 0
step(x) | x < 0 = 0

>> mul(p, q) = p*q
mul(p, q) = p*q

>> hand(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])*mul.(v', step.(W*X[r]' + b))*X[r]
hand(W) = sum_(r=1)^3 (net(W, X[r]') - t[r])*mul.(v', step.(W*X[r]' + b))*X[r]

>> (grad_(W = A) loss(W)) == hand(A)
1

>> A1 = A - grad_(W = A) loss(W)/4
A1 = A - grad_(W = A) loss(W)/4

>> A1
[  0.25, -0.625;
 -0.125,  -0.25]

>> loss(A1)
0.5703125

# A layer that forgot its dot names its activation, not its loss.
>> bad(W, x) = v*relu(W*x + b)
bad(W, x) = v*relu(W*x + b)

>> bad(A, X[1]')
error: relu needs single values, not a 2x1 matrix; relu.(x) maps it over the cells

# --- softmax and cross-entropy trained one step ------------------------------

# Softmax as the paper writes it, its sum bounded by the number of classes,
# where gradcells.ink needs a definition by cells of that size, and
# cross-entropy as -log of it. One step of a linear classifier, grad's
# gradient held to (p - y) x' written by hand; the values are mpmath's, and
# NumPy's to the eight digits it shows.
>> sm(z, K) = exp.(z)/sum_(c=1)^K exp(z[c])
sm(z, K) = exp.(z)/sum_(c=1)^K exp(z[c])

>> sm([1; 2; 3], 3)
[~0.0900305732;
  ~0.244728471;
  ~0.665240956]

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

# --- tex ---------------------------------------------------------------------

# Set as a paper sets sigma(z), meaning each cell.
>> tex ?net
\operatorname{net}(W, x) = v\,\operatorname{relu}(W\,x + b)

>> tex ?sm
\operatorname{sm}(z, K) = \frac{\operatorname{exp}(z)}{\sum_{c=1}^{K} \operatorname{exp}(z_{c})}

# The derivative of an expression with respect to a name at a point,
# grad_(x = point) expression (DESIGN.md, differentiation; MANIFESTO.md,
# Differentiation). The name is bound as a sum binds its index: the point is
# read where grad is written, the name only in the expression, which reaches
# as far as a sum's does. What the expression reads through other definitions
# is differentiated with it, rule by rule, and the answer is exact where its
# parts are.
>> grad_(x = 2) x^3
12

>> f(x) = x^2 + 3*x
f(x) = x^2 + 3*x

>> grad_(x = 1) f(x)
5

>> frac grad_(x = 3) 1/x
-1/9

>> x = 100
x = 100

>> grad_(x = 3) x^2
6

>> x
100

>> grad_(x = x/50) x^2
4

# An expression that does not read the name is more likely a slip than a
# constant; one that reads it and is flat answers 0.
>> grad_(x = 1) 5
error: grad's expression does not read x

>> c(x) = 7
c(x) = 7

>> grad_(x = 1) c(x)
0

# A definition reads its own globals, which grad's name does not reach, so
# a derivative through one would be silently 0; it is refused.
>> g = x^2
g = x^2

>> grad_(x = 3) g
error: g reads the global x, which grad's x does not reach

>> shift(t) = t + x
shift(t) = t + x

>> grad_(x = 3) shift(x)
error: shift reads the global x, which grad's x does not reach

# A clause's own names are not globals, its slice's no more than its row's
# (DESIGN.md, C77).
>> Gs[b<=2, j<=1, k<=1] = b
Gs[b<=2, j<=1, k<=1] = b

>> grad_(b = 2) b*Gs
[1;;
 2]

# A size is read as a clause is, so one that reads the global is refused too
# (C79).
>> gz[j<=x] = j
gz[j<=x] = j

>> grad_(x = 2) x*gz
error: gz reads the global x, which grad's x does not reach

# A sum term by term, and a sequence through its recurrence. A term already
# remembered is not an answer for its derivative.
>> grad_(x = 1) sum_(k=1)^3 k*x^k
14

>> p(x)_0 = x
p(x)_0 = x

>> p(x)_k = p(x)_(k-1)*x
p(x)_k = p(x)_(k-1)*x

>> p(2)_3
16

>> grad_(x = 2) p(x)_3
32

# A definition in cases is differentiated in the clause that holds at the
# point, which is one side of a threshold: a ReLU written with x < 0 has
# slope 1 at 0, written with x <= 0 slope 0, as PyTorch has.
>> relu(x) = x
relu(x) = x

>> relu(x) | x < 0 = 0
relu(x) | x < 0 = 0

>> grad_(x = 2) relu(x)
1

>> grad_(x = 0-2) relu(x)
0

>> grad_(x = 0) relu(x)
1

>> relu0(x) = x
relu0(x) = x

>> relu0(x) | x <= 0 = 0
relu0(x) | x <= 0 = 0

>> grad_(x = 0) relu0(x)
0

# A clause that holds at a single point has no side, and its slope is not the
# function's: q is x + 1 everywhere, so its derivative at 1 is 1, not 0.
>> q(x) = (x^2 - 1)/(x - 1)
q(x) = (x^2 - 1)/(x - 1)

>> q(x) | x == 1 = 2
q(x) | x == 1 = 2

>> grad_(x = 3) q(x)
1

>> grad_(x = 1) q(x)
error: q takes a clause at x = 1 that holds only there

# floor and a comparison are flat, except where they jump.
>> grad_(x = 3/2) floor(x)
0

>> grad_(x = 2) floor(x)
error: floor jumps at x = 2

>> grad_(x = 2) (x > 1)
0

>> grad_(x = 1) (x > 1)
error: a comparison jumps at x = 1

# Whether the sides move is what tells, not their slopes: at a tangent the
# slopes agree and the comparison still jumps (DESIGN.md, C73).
>> grad_(x = 0) (x^2 > 0)
error: a comparison jumps at x = 0

>> grad_(x = 0) grad_(y = x) (y^2 > 0)
error: a comparison jumps at y = 0

>> q2(x) = 1
q2(x) = 1

>> q2(x) | x^2 == 0 = 5
q2(x) | x^2 == 0 = 5

>> grad_(x = 0) q2(x)
error: q2 takes a clause at x = 0 that holds only there

>> grad_(x = 1) 2^((x - 1)^2)
error: grad cannot differentiate a power whose exponent changes with x, unless its base is e

# Wrong on purpose (DESIGN.md, C73): floor(x^2) is 0 near 0, so flat, but a
# whole number whose argument moves is taken for a jump.
>> grad_(x = 0) floor(x^2)
error: floor jumps at x = 0

# A power, where its derivative is finite, and with an exponent that changes
# with the name only where its base is e: any other base needs a logarithm.
>> grad_(x = 4) x^(1/2)
0.25

>> grad_(x = 0) x^(1/2)
error: a power's derivative is infinite at x = 0

>> grad_(x = 0) (x^2)^(1/2)
error: a power's derivative is infinite at x = 0

>> grad_(x = 0) x^0
0

>> grad_(x = 0) x^1
1

>> grad_(x = 3/2) x^floor(x)
1

>> grad_(x = 0) e^(2*x)
2

>> grad_(x = 1) 2^x
error: grad cannot differentiate a power whose exponent changes with x, unless its base is e

>> grad_(x = 2) x^x
error: grad cannot differentiate a power whose exponent changes with x, unless its base is e

# A complex derivative is the derivative, not its conjugate.
>> grad_(z = 1 + i) z^2
2+i*2

# With respect to a matrix, a single value's gradient is shaped as the
# matrix; with respect to a single value, a matrix's is shaped as the matrix.
>> grad_(v = [1; 2]) v'*v
[2;
 4]

>> grad_(v = [3; 4]) [1 2]*v
[1;
 2]

>> grad_(r = [3 4]) r*[1; 2]
[1, 2]

>> grad_(x = 3) [x; x^2]
[1;
 6]

# A matrix power is a product, whose factors do not commute, and its inverse
# is differentiated as one.
>> grad_(x = 1) [1 x; 2 3]^2
[2, 4;
 0, 2]

>> grad_(x = 1) [1 x; 2 3]^(0-1)
[ 6, -3;
 -4,  2]

# A limit's derivative is the limit of its terms' derivatives, taken until
# both the terms and their derivatives have converged; exp and the sigmoid
# written as series are differentiated as written.
>> ex(z)_0 = 1
ex(z)_0 = 1

>> ex(z)_1 = 1 + z
ex(z)_1 = 1 + z

>> ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k
ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k

>> grad_(x = 1) lim ex(x)
~2.71828183

>> sig(z) = 1/(1 + lim ex(0-z))
sig(z) = 1/(1 + lim ex(0-z))

>> grad_(z = 1) sig(z)
~0.196611933

# Newton's square root converges at 0, where the root has no derivative, and
# the derivatives of its terms say so by diverging.
>> sq(x)_0 = 1
sq(x)_0 = 1

>> sq(x)_n = (sq(x)_(n-1) + x/sq(x)_(n-1))/2
sq(x)_n = (sq(x)_(n-1) + x/sq(x)_(n-1))/2

>> grad_(x = 4) lim sq(x)
0.25

>> grad_(x = 0) lim sq(x)
error: the derivative of sq did not converge within 100 terms (last term ~4.225502e+29)

# Wrong on purpose (DESIGN.md, C72): h tends to 0 for every x,
# so the derivative of its limit is 0, but every term's derivative at 0 is 1.
# The terms converge too slowly near 0 for the rule to hold, and no check at
# the point can tell.
>> h(x)_0 = x
h(x)_0 = x

>> h(x)_n = h(x)_(n-1)/(1 + x^2)
h(x)_n = h(x)_(n-1)/(1 + x^2)

>> grad_(x = 0) lim h(x)
1

# A partial is taken one name at a time, and a derivative is an expression
# like any other: it can be differentiated, and defined as a function of its
# point.
>> grad_(x = 1) grad_(y = 2) x*y^2
4

>> grad_(x = 1) grad_(y = x) y^3
6

>> df(x) = grad_(t = x) f(t)
df(x) = grad_(t = x) f(t)

>> df(1)
5

>> tex ?df
\operatorname{df}(x) = \left.\frac{\partial}{\partial t} f(t)\right|_{t=x}

>> f(x) = x^3
f(x) = x^3

>> df(1)
3

>> s2(x) = x^2 - 2
s2(x) = x^2 - 2

>> ds(x) = grad_(t = x) s2(t)
ds(x) = grad_(t = x) s2(t)

>> r_0 = 1
r_0 = 1

>> r_n = r_(n-1) - s2(r_(n-1))/ds(r_(n-1))
r_n = r_(n-1) - s2(r_(n-1))/ds(r_(n-1))

>> lim r
~1.41421356

# An exact gradient check: least squares, its gradient by hand beside the
# one grad gives, equal to the last digit, and gradient descent taken with
# each, equal at every step.
>> X = [1 0; 1 1; 1 2]
X = [1 0; 1 1; 1 2]

>> y = [1; 2; 2]
y = [1; 2; 2]

>> loss(v) = sum_(i=1)^3 (X[i]*v - y[i])^2
loss(v) = sum_(i=1)^3 (X[i]*v - y[i])^2

>> hand(v) = 2*sum_(i=1)^3 (X[i]*v - y[i])*X[i]'
hand(v) = 2*sum_(i=1)^3 (X[i]*v - y[i])*X[i]'

>> grad_(v = [0; 0]) loss(v)
[-10;
 -12]

>> frac grad_(v = [1/2; 1/3]) loss(v)
[   -5;
 -17/3]

>> grad_(v = [1/2; 1/3]) loss(v) == hand([1/2; 1/3])
1

>> w_0 = [0; 0]
w_0 = [0; 0]

>> w_n = w_(n-1) - grad_(v = w_(n-1)) loss(v)/8
w_n = w_(n-1) - grad_(v = w_(n-1)) loss(v)/8

>> u_0 = [0; 0]
u_0 = [0; 0]

>> u_n = u_(n-1) - hand(u_(n-1))/8
u_n = u_(n-1) - hand(u_(n-1))/8

>> w_20 == u_20
1

# A bound name shadows a sequence of its name, as a sum's index does.
>> grad_(w = 3) w^2
6

# What it does not give, it says.
>> grad_(v = [1; 1]) [1 2; 3 4]*v
error: grad of a matrix with respect to a matrix is a Jacobian, which it does not give

>> decay(a = 1/2) = {
..     s_0 = 1
..     s_n = a*s_(n-1)
.. }
decay(a = 1/2) = { ... }

>> grad_(x = 1/2) decay(a = x).s_2
error: grad cannot differentiate through an instance yet

>> grad_(p_1 = 3) x^2
error: grad takes a name at a point, as 'grad_(x = 2) x^3'

>> grad_(3) x^2
error: grad takes a name at a point, as 'grad_(x = 2) x^3'

>> grad_(x) x^2
error: grad takes a name at a point, as 'grad_(x = 2) x^3'

>> grad(x^2, x = 3)
error: grad takes a name at a point, as 'grad_(x = 2) x^3'

>> grad = 1
error: grad is reserved, so it cannot be defined

# A base of e is the built-in one only.
>> e = 3
e = 3

>> grad_(x = 0) e^x
error: grad cannot differentiate a power whose exponent changes with x, unless its base is e

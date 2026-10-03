# The derivative of an expression with respect to a name at a point,
# grad(expression, x = point), as a paper writes the partial derivative there
# (DESIGN.md, next in line; MANIFESTO.md, Differentiation). The name is bound
# for the expression as a parameter is, so it shadows a global of its name.
# What the expression reads through other definitions is differentiated with
# it, rule by rule, and the answer is exact where its parts are.
>> grad(x^3, x = 2)
12

>> f(x) = x^2 + 3*x
f(x) = x^2 + 3*x

>> grad(f(x), x = 1)
5

>> frac grad(1/x, x = 3)
-1/9

>> x = 100
x = 100

>> grad(x^2, x = 3)
6

>> x
100

>> grad(5, x = 1)
0

# A sum term by term, and a sequence through its recurrence.
>> grad(sum_(k=1)^3 k*x^k, x = 1)
14

>> p(x)_0 = x
p(x)_0 = x

>> p(x)_k = p(x)_(k-1)*x
p(x)_k = p(x)_(k-1)*x

>> grad(p(x)_3, x = 2)
32

# A definition in cases is differentiated in the case the point takes: a
# ReLU's slope is 0 below its threshold and 1 from it on. floor and a
# comparison are flat.
>> relu(x) | x < 0 = 0
relu(x) | x < 0 = 0

>> relu(x) = x
relu(x) = x

>> grad(relu(x), x = 2)
1

>> grad(relu(x), x = 0-2)
0

>> grad(relu(x), x = 0)
1

>> grad(floor(x), x = 3/2)
0

>> grad(x > 1, x = 2)
0

# With respect to a matrix, a single value's gradient is shaped as the matrix.
>> grad(v'*v, v = [1; 2])
[2;
 4]

>> grad([1 2]*v, v = [3; 4])
[1;
 2]

# A limit's derivative is the limit of its terms' derivatives, so exp and
# the sigmoid written as series are differentiated as written.
>> ex(z)_0 = 1
ex(z)_0 = 1

>> ex(z)_1 = 1 + z
ex(z)_1 = 1 + z

>> ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k
ex(z)_k = ex(z)_(k-1) + (ex(z)_(k-1) - ex(z)_(k-2))*z/k

>> grad(lim ex(x), x = 0)
~1

>> sig(z) = 1/(1 + lim ex(0-z))
sig(z) = 1/(1 + lim ex(0-z))

>> grad(sig(z), z = 0)
~0.25

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

>> grad(loss(v), v = [0; 0])
[-10;
 -12]

>> grad(loss(v), v = [1/2; 1/3]) == hand([1/2; 1/3])
1

>> w_0 = [0; 0]
w_0 = [0; 0]

>> w_n = w_(n-1) - grad(loss(v), v = w_(n-1))/8
w_n = w_(n-1) - grad(loss(v), v = w_(n-1))/8

>> u_0 = [0; 0]
u_0 = [0; 0]

>> u_n = u_(n-1) - hand(u_(n-1))/8
u_n = u_(n-1) - hand(u_(n-1))/8

>> w_20 == u_20
1

# What it does not give, it says.
>> grad(x^x, x = 2)
error: grad cannot differentiate a power whose exponent depends on x

>> grad([1 2; 3 4]*v, v = [1; 1])
error: grad of a matrix with respect to a matrix is a Jacobian, which it does not give

>> grad(x^2, 3)
error: grad takes a name at a point, as 'grad(f(x), x = 2)'

>> grad = 1
error: grad is reserved, so it cannot be defined

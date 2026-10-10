# grad through a local (DESIGN.md, next in line). A local binds the value of
# its right-hand side once, for the rest of the line (phase 8); under grad
# that value carries its derivatives, as every value there does, so reading
# the local reads them. Nothing else of phase 8 moves: left to right, not
# visible before its binding, shadowing globals and parameters, the line its
# extent. Expected values are sympy's, each local substituted by hand into
# the closed form it names; none was taken from inkamath.

# In a function, the case the refusal stopped, in grad's own body, and in an
# argument, which binds in the caller's frame and not in f's ('(t = x)' alone
# would name f's parameter t).
>> f(x) = (t = 2*x) + t*t
f(x) = (t = 2*x) + t*t

>> f(3)
42

>> grad_(x = 3) f(x)
26

>> grad_(x = 3) ((t = 2*x) + t*t)
26

>> grad_(x = 3) (f(1*(t = x)) + t)
27

# One bound in a call dies with it, remembered or not.
>> grad_(x = 3) (f(x) + f(x) + t)
error: t is not defined

# Read more than once, and one local read by the next.
>> cube(x) = (t = x + 1)*t*t
cube(x) = (t = x + 1)*t*t

>> grad_(x = 2) cube(x)
27

>> g(x) = (u = x*x) + (v = u + 1) + v
g(x) = (u = x*x) + (v = u + 1) + v

>> grad_(x = 3) g(x)
18

# A local shadows a parameter from its binding on. grad's own name and a
# sum's index it cannot: the fix that refuses them in both evaluators lands
# before this item.
>> h(x) = x*((x = 10) + x)
h(x) = x*((x = 10) + x)

>> h(3)
60

>> grad_(x = 3) h(x)
20

>> grad_(x = 2) x*((x = 5) + x)
error: x is grad's variable, so a local cannot define it

>> grad_(x = 1) sum_(k=1)^3 x*k*((k = 2) + k)
error: k is the sum's index, so a local cannot define it

# A local that does not move with the name has no part, and one bound in the
# point is read in the body, as the point is read around grad.
>> c7(x) = (c = 7) + c*x
c7(x) = (c = 7) + c*x

>> grad_(x = 1) c7(x)
7

>> grad_(x = (p = 3)) x*x*p
18

# Bound in a guard, read by the clause it chooses.
>> pos(x) | (t = 2*x) > 0 = t*t
pos(x) | (t = 2*x) > 0 = t*t

>> pos(x) = 0
pos(x) = 0

>> grad_(x = 3) pos(x)
24

>> grad_(x = -1) pos(x)
0

# In a term, and in each term a limit walks: Newton's root with its last
# term named, whose derivative at 4 is 1/(2*sqrt(4)).
>> s(x)_0 = 1
s(x)_0 = 1

>> s(x)_n = (step = x^n) + s(x)_(n-1) + step
s(x)_n = (step = x^n) + s(x)_(n-1) + step

>> grad_(x = 2) s(x)_2
10

>> nr(x)_0 = 1
nr(x)_0 = 1

>> nr(x)_n = (p = nr(x)_(n-1)) - (p*p - x)/(2*p)
nr(x)_n = (p = nr(x)_(n-1)) - (p*p - x)/(2*p)

>> grad_(x = 4) lim nr(x)
~0.25

# In a sum's body, and after it, holding its last term's value and parts,
# as (sum_(k=1)^3 (t = k)) + t is 9.
>> grad_(x = 2) sum_(k=1)^3 (t = k*x)*t
56

>> grad_(x = 2) ((sum_(k=1)^3 (t = k*x)) + t)
9

# Bound in a sum's bound, which grad leaves to the evaluator: n is 3 from
# there on, as the evaluator has it, and not the x bound before it.
>> grad_(x = 2) ((n = x) + (sum_(k=1)^((n = 3)) k) + n*x)
4

# Matrices and tensors carry their parts as single values do.
>> A = [1 2; 3 4]
A = [1 2; 3 4]

>> grad_(x = 2) ((M = x*A) + M*M)
[29, 42;
 63, 92]

>> grad_(v = [1; 2]) ((w = A*v)'*w)
[ 76;
 108]

>> K = [1 2; 3 4;; 5 6; 7 8]
K = [1 2; 3 4;; 5 6; 7 8]

>> grad_(x = 2) ((T = x*K)[2,1,2]*T[1,2,1])
72

# A gradient with respect to a matrix is one evaluation at the point, though
# it takes a pass per cell: the second cell's pass does not find the local
# the first bound, and the line reads the local after grad.
>> w = 1
w = 1

>> (grad_(v = [1; 2]) (w*v'*v + 0*(w = 5))) + w
[7;
 9]

# What a pass puts back is what its local shadowed, here a local of the line
# before grad, and not the global behind it.
>> (w = 2) + (grad_(v = [1; 2]) (w*v'*v + 0*(w = 5))) + w
[11;
 15]

>> clear w
clear w

# Two layers, each named where it is computed, as a convolutional network
# names its pooled activations; without locals, each layer is a function of
# every parameter before it, and the gradients are the same.
>> relu(v)[j<=2] = v[j]
relu(v)[j<=2] = v[j]

>> relu(v)[j<=2] | v[j] < 0 = 0
relu(v)[j<=2] | v[j] < 0 = 0

>> u0 = [1; 2]
u0 = [1; 2]

>> b1 = [1/4; -1]
b1 = [1/4; -1]

>> W1 = [1 -2; 1/2 1]
W1 = [1 -2; 1/2 1]

>> W2 = [3 -1]
W2 = [3 -1]

>> loss(A, B) = (r = B*(z = relu(A*u0 + b1)) - 2)*r + z'*z/10
loss(A, B) = (r = B*(z = relu(A*u0 + b1)) - 2)*r + z'*z/10

>> layer(A) = relu(A*u0 + b1)
layer(A) = relu(A*u0 + b1)

>> loss0(A, B) = (B*layer(A) - 2)*(B*layer(A) - 2) + layer(A)'*layer(A)/10
loss0(A, B) = (B*layer(A) - 2)*(B*layer(A) - 2) + layer(A)'*layer(A)/10

>> loss(W1, W2)
12.475

>> grad_(P = W1) loss(P, W2)
[  0,    0;
 7.3, 14.6]

>> (grad_(P = W1) loss(P, W2)) == grad_(P = W1) loss0(P, W2)
1

>> grad_(Q = W2) loss(W1, Q)
[0, -10.5]

>> (grad_(Q = W2) loss(W1, Q)) == grad_(Q = W2) loss0(W1, Q)
1

# Nested: a local bound outside an inner grad is read inside it with its
# outer parts; a mixed partial through one; and one bound inside an inner
# grad is read after it at the inner point, moving with the outer name.
>> grad_(x = 1) ((t = x*x)*grad_(y = 2) t*y*y)
16

>> grad_(x = 1) grad_(y = 2) ((t = x*y)*t)
8

>> grad_(x = 3) ((grad_(y = 2) (s = x*y)*y) + s)
6

# Runge-Kutta's stages written as locals, each read by the next, and the
# step's derivative with respect to its start.
>> lg(y) = y - y^2
lg(y) = y - y^2

>> rk(u, dt) = u + dt/6*((a = lg(u)) + 2*(b = lg(u + dt/2*a)) + 2*(c = lg(u + dt/2*b)) + lg(u + dt*c))
rk(u, dt) = u + dt/6*((a = lg(u)) + 2*(b = lg(u + dt/2*a)) + 2*(c = lg(u + dt/2*b)) + lg(u + dt*c))

>> frac rk(1/2, 1/10)
281846054463086586882133/536870912000000000000000

>> frac grad_(u = 1/2) rk(u, 1/10)
2510301413390836485599/2516582400000000000000

# The extent is the line: a local bound under grad is read after it, at the
# point, and is gone by the next line; before its binding it is not there.
>> (grad_(x = 3) ((t = 2*x) + t*t)) + t
32

>> t
error: t is not defined

>> grad_(x = 3) (t + (t = 2*x))
error: t is not defined

# A local shadows a global from its binding on, so a global of the local's
# name that reads the global of grad's name is not reached through it, in
# the order of evaluation: a guard before its clause, a sum's body before
# what follows it. Read before the binding, it is, and is refused as any
# such definition is.
>> x = 100
x = 100

>> t = x^2
t = x^2

>> grad_(x = 3) f(x)
26

>> grad_(x = 3) ((t = 2*x) + t*t)
26

>> grad_(x = 3) pos(x)
24

>> grad_(x = 2) ((sum_(k=1)^3 (t = k*x)) + t)
9

>> ft(x) = t + (t = 2*x)
ft(x) = t + (t = 2*x)

>> grad_(x = 3) ft(x)
error: t reads the global x, which grad's x does not reach

>> clear t
clear t

>> clear x
clear x

# What stays refused. A local with parameters binds an expression, not a
# value, and grad does not follow it; a local is a value, and a call of one
# is the evaluator's error, as (a = 3) + a(2) is.
>> grad_(x = 3) ((lf(y) = y*x) + lf(x))
error: grad cannot differentiate a local function

>> (a = 3) + grad_(x = 2) a(x)
error: a takes no arguments

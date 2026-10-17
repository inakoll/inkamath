# grad's order (DESIGN.md): 'grad_(x = a)^n body', the superscript after
# the subscript, as sum's. With respect to a single value, the n-th
# derivative, n grads of x nested; with respect to a vector, at order 2, the
# Hessian. Every value was worked out apart from the interpreter, as its
# specification: exact ones by hand and in Python's fractions, inexact ones
# by mpmath at 40 digits, its numerical derivatives besides.
>> grad_(x = 2)^2 x^3
12

>> grad_(x = 1/2)^4 x^5
60

>> grad_(x = 1)^8 x^8
40320

# Nesting by another name, around or inside, is as before.
>> grad_(x = 1) grad_(y = 2) x^2*y^3
24

>> grad_(x = 1)^2 grad_(y = 3) x^2*y^3
54

# A function of its point, written as it was given.
>> d2(t) = grad_(u = t)^2 u^3
d2(t) = grad_(u = t)^2 u^3

>> frac d2(1/3)
2

>> ?d2
d2(t) = grad_(u = t)^2 u^3

>> tex ?d2
\operatorname{d2}(t) = \left.\frac{\partial^{2}}{\partial u^{2}} u^3\right|_{u=t}

# Through the prelude, a matrix and the clause that holds at the point.
>> grad_(x = 1/2)^2 sin(x)
~-0.479425539

>> grad_(x = 0)^5 exp(2*x)
~32

>> grad_(x = 1)^2 [x^3, x^2]
[6, 2]

>> pc(x) | x < 0 = -x^2
pc(x) | x < 0 = -x^2

>> pc(x) = x^3
pc(x) = x^3

>> [grad_(x = -1/2)^2 pc(x), grad_(x = 0)^2 pc(x), grad_(x = 1)^2 pc(x)]
[-2, 0, 6]

# With respect to a vector, a column or a row, the Hessian, n by n: the
# Jacobian by cells of the gradient, README section 4, its cell (j, i) the
# cell (i, j).
>> f(v) = v[1]^2*v[2] + v[2]^3
f(v) = v[1]^2*v[2] + v[2]^3

>> grad_(v = [1; 2])^2 f(v)
[4,  2;
 2, 12]

>> H(x)[i<=2, j<=2] = (grad_(u = x) (grad_(v = u) f(v))[i])[j]
H(x)[i<=2, j<=2] = (grad_(u = x) (grad_(v = u) f(v))[i])[j]

>> H([1; 2]) == grad_(v = [1; 2])^2 f(v)
1

>> grad_(v = [1 2])^2 v*v'
[2, 0;
 0, 2]

# The Hessian of a Lagrangian, l = 1/2: 12(x - 2)^2 + 2 + 2l, -4 and 8.
>> Lg(v, l) = (v[1] - 2)^4 + (v[1] - 2*v[2])^2 + l*(v[1]^2 - v[2])
Lg(v, l) = (v[1] - 2)^4 + (v[1] - 2*v[2])^2 + l*(v[1]^2 - v[2])

>> grad_(v = [3; 1])^2 Lg(v, 1/2)
[15, -4;
 -4,  8]

# Newton's method on v1^4/4 + v2^2 - v1*v2, its minimum at
# [2^(-1/2); 2^(-3/2)], solved exactly at each step.
>> newton(x0 = [1; 1]) = {
..     f(v) = v[1]^4/4 + v[2]^2 - v[1]*v[2]
..     x_0 = x0
..     x_n = x_(n-1) - (grad_(v = x_(n-1))^2 f(v))^-1*grad_(v = x_(n-1)) f(v)
.. }
newton(x0 = [1; 1]) = { ... }

>> nt = newton()
nt = newton()

>> frac nt.x_1
[4/5;
 2/5]

>> frac nt.x_3
[67108864/94852805;
 33554432/94852805]

# A double pendulum's mass matrix and Coriolis from its Lagrangian, as
# test/compile/hessian.ink compiles them. M_1 at q_0 = [1; 1/2]; C_2 and
# q_100 those of the recurrence written by hand, in mpmath.
>> lag(m1 = 1, m2 = 1, l1 = 1, l2 = 1, g = 10) = {
..     L(q, w) = (m1 + m2)*l1^2*w[1]^2/2 + m2*l2^2*w[2]^2/2 + m2*l1*l2*w[1]*w[2]*cos(q[1] - q[2]) + (m1 + m2)*g*l1*cos(q[1]) + m2*g*l2*cos(q[2])
..     q_0 = [1; 1/2]
..     w_0 = [0; 0]
..     M_n = grad_(v = w_(n-1))^2 L(q_(n-1), v)
..     C_n[i<=2, j<=2] = (grad_(u = q_(n-1)) (grad_(v = w_(n-1)) L(u, v))[i])[j]
..     G_n = grad_(u = q_(n-1)) L(u, w_(n-1))
..     w_n = w_(n-1) + M_n^-1*(G_n - C_n*w_(n-1))/100
..     q_n = q_(n-1) + w_n/100
.. }
lag(m1 = 1, m2 = 1, l1 = 1, l2 = 1, g = 10) = { ... }

>> p = lag()
p = lag()

>> p.M_1
[           2, ~0.877582562;
 ~0.877582562,            1]

>> p.C_2
[~-0.0201420959,  ~0.0201420959;
  ~0.0490735055, ~-0.0490735055]

>> p.q_100
[~-0.276479857;
  ~-1.04368092]

# Coriolis without its cells: C*w is the derivative in the direction w,
# one pass by w's cells. lag's L at q = [1; 1/2], w = [3/10; -7/10]: by
# hand, sin(1/2)*(w[2] - w[1])*[w[2]; w[1]].
>> Lp(q, w) = 2*w[1]^2/2 + w[2]^2/2 + w[1]*w[2]*cos(q[1] - q[2]) + 20*cos(q[1]) + 10*cos(q[2])
Lp(q, w) = 2*w[1]^2/2 + w[2]^2/2 + w[1]*w[2]*cos(q[1] - q[2]) + 20*cos(q[1]) + 10*cos(q[2])

>> grad_(e = 0) grad_(v = [3/10; -7/10]) Lp([1; 1/2] + e*[3/10; -7/10], v)
[ ~0.335597877;
 ~-0.143827662]

# Refused when read, so where it is defined: an order below 2, not whole or
# not written as a number, and one by several names; for now, an order by a
# matrix and past 2 by a vector. A first derivative's refusals and nesting's
# stay.
>> grad_(x = 2)^1 x^3
error: grad's order is a whole number, 2 or more; a first derivative is written without one

>> w = grad_(x = 2)^1 x^3
error: grad's order is a whole number, 2 or more; a first derivative is written without one

>> grad_(x = 2)^0 x^3
error: grad's order is a whole number, 2 or more; a first derivative is written without one

>> grad_(x = 2)^2.5 x^3
error: grad's order is a whole number, 2 or more; a first derivative is written without one

>> grad_(x = 2)^(1/2) x^3
error: grad's order is written as a number

>> e(k) = grad_(x = 2)^k x^5
error: grad's order is written as a number

>> grad_(x = 2, y = 3)^2 x^2*y
error: a grad by several names takes no order, for now

>> grad_(v = [1 2; 3 4])^2 v[1,1]*v[2,2]
error: a derivative of order 2 with respect to a matrix, for now

>> grad_(v = [1; 2])^3 f(v)
error: a derivative of order 3 with respect to a vector, for now

>> grad_(v = [1; 2])^2 [v[1]^2; v[2]]
error: grad of a matrix with respect to a matrix is a Jacobian, which it does not give

>> grad_(x = 1)^9 x^9
error: grad nests more than 8 deep

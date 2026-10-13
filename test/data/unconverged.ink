# A clause for a limit that does not converge (DESIGN.md): a guard that
# reads a limit that does not converge does not hold, so the clause after
# it is tried, and its answer is inexact. Every value was worked out apart
# from the interpreter: the terms in Python's fractions, walked by README
# section 4's rule written again in Python, the roots in mpmath at 50
# digits.

# Newton's method on F(x) = x^3 - 2x + 2. From -2 it converges to the real
# root, -1.76929235423863 by mpmath, at its fifth term; from 0 its terms are
# 0, 1, 0, 1, ... exactly, F(0)/F'(0) being 2/-2 and F(1)/F'(1) 1/1, so the
# hundredth is 0.
>> F(x) = x^3 - 2*x + 2
F(x) = x^3 - 2*x + 2

>> dF(x) = 3*x^2 - 2
dF(x) = 3*x^2 - 2

>> nw(x)_0 = x
nw(x)_0 = x

>> nw(x)_k = nw(x)_(k-1) - F(nw(x)_(k-1))/dF(nw(x)_(k-1))
nw(x)_k = nw(x)_(k-1) - F(nw(x)_(k-1))/dF(nw(x)_(k-1))

>> lim nw(-2)
~-1.76929235

>> lim nw(0)
error: nw did not converge within 100 terms (last term 0)

# A guard reading the limit: the root, where Newton finds one that F takes
# to within a millionth of 0, and otherwise the guess. The guess is inexact
# there: whether a walk settled is read from its steps as doubles, and
# within a hundred terms, as a clause a guard reading a double chose is
# (C241). From 1/10 the terms fall into the cycle, past a thousand digits
# from the eighth, nw(1/10)_7 (Python's fractions), and the guess is
# marked as a clause chosen by a truth so read is (C68).
>> root(x) | abs(F(lim nw(x))) < 1/10^6 = lim nw(x)
root(x) | abs(F(lim nw(x))) < 1/10^6 = lim nw(x)

>> root(x) = x
root(x) = x

>> root(-2)
~-1.76929235

>> root(0)
~0

>> root(1/10)
~0.1  # approximated past a thousand digits

# A local the guard binds, which the clause's value reads.
>> loc(x) | abs((z = lim nw(x))^3 - 2*z + 2) < 1/10^6 = z
loc(x) | abs((z = lim nw(x))^3 - 2*z + 2) < 1/10^6 = z

>> loc(x) = x
loc(x) = x

>> loc(-2)
~-1.76929235

>> loc(0)
~0

# Where no clause after it applies, the refusal is the limit's, as today.
>> h(x) | abs(F(lim nw(x))) < 1/10^6 = lim nw(x)
h(x) | abs(F(lim nw(x))) < 1/10^6 = lim nw(x)

>> h(0)
error: nw did not converge within 100 terms (last term 0)

# Read as the guard reads it: on the right of 'or', only where the left
# has not decided; on the left, the guard does not hold whatever its right
# side says.
>> v(x) | x < 1 or lim nw(x) < 0 = 1
v(x) | x < 1 or lim nw(x) < 0 = 1

>> v(x) = 2
v(x) = 2

>> v(0)
1

>> v(1)
~2

>> w(x) | lim nw(x) < 0 or x < 1 = 1
w(x) | lim nw(x) < 0 or x < 1 = 1

>> w(x) = 2
w(x) = 2

>> w(0)
~2

# Through a function the guard calls.
>> ok(x) = abs(F(lim nw(x))) < 1/10^6
ok(x) = abs(F(lim nw(x))) < 1/10^6

>> r(x) | ok(x) = lim nw(x)
r(x) | ok(x) = lim nw(x)

>> r(x) = x
r(x) = x

>> r(0)
~0

>> r(-2)
~-1.76929235

# Through a definition the guard calls that has no clause after its own
# guard: its refusal is the limit's, so the guard calling it does not hold.
>> ho(x) | h(x) < 0 = 1
ho(x) | h(x) < 0 = 1

>> ho(x) = 2
ho(x) = 2

>> ho(-2)
~1

>> ho(0)
~2

# A guard that did not hold for a limit that ran out leaves the guards after
# it their own truth, and the clause that then holds is inexact: sw(1)'s
# terms are 0, 2, 0, ... and sw(-1)'s -2, 0, -2, ..., the hundredth 0.
>> sw(x)_n = x + (-1)^n
sw(x)_n = x + (-1)^n

>> fa(x) | lim sw(x) > 0 = 1
fa(x) | lim sw(x) > 0 = 1

>> fa(x) | x > 0 = 2
fa(x) | x > 0 = 2

>> fa(x) = 3
fa(x) = 3

>> fa(1)
~2

>> ga(x) | lim sw(x) > 0 = 1
ga(x) | lim sw(x) > 0 = 1

>> ga(x) | x > 0 = 2
ga(x) | x > 0 = 2

>> ga(1)
~2

# A second guard that reads a limit that runs out, and a third that holds;
# where none holds, the refusal is the limit's.
>> tri(x) | lim sw(x) > 0 = 1
tri(x) | lim sw(x) > 0 = 1

>> tri(x) | lim sw(x) < 0 = 2
tri(x) | lim sw(x) < 0 = 2

>> tri(x) | x > 0 = 3
tri(x) | x > 0 = 3

>> tri(1)
~3

>> tri(-1)
error: sw did not converge within 100 terms (last term 0)

# The same in a function an outer guard calls: tri(1) answers, and tri(-1)
# is refused in the limit's words, so the outer guard does not hold.
>> co(x) | tri(x) > 2 = 10
co(x) | tri(x) > 2 = 10

>> co(x) = 20
co(x) = 20

>> co(1)
~10

>> co(-1)
~20

# Not through a term the guard reads: y_0 is refused, and a refused term is
# not a limit that did not converge. Nor through another limit's terms,
# each of which is refused where nw(0) is; from -2, t's terms tend to twice
# the root.
>> y_n = lim nw(n)
y_n = lim nw(n)

>> z_n | y_n < 0 = 1
z_n | y_n < 0 = 1

>> z_n = 2
z_n = 2

>> z_0
error: nw did not converge within 100 terms (last term 0)

>> t(x)_0 = 0
t(x)_0 = 0

>> t(x)_k = t(x)_(k-1)/2 + lim nw(x)
t(x)_k = t(x)_(k-1)/2 + lim nw(x)

>> q(x) | lim t(x) < 0 = 1
q(x) | lim t(x) < 0 = 1

>> q(x) = 2
q(x) = 2

>> q(-2)
~1

>> q(0)
error: nw did not converge within 100 terms (last term 0)

# A series without an upper bound is such a limit: its partial sums 2^(k+1)
# - 1 from 2, and 2 - 2^-k from 1/2.
>> geo(x) | sum_(k=0) x^k > 0 = sum_(k=0) x^k
geo(x) | sum_(k=0) x^k > 0 = sum_(k=0) x^k

>> geo(x) = 0
geo(x) = 0

>> geo(1/2)
~2

>> geo(2)
~0

# In a sum's terms, each limit is read as the guard's own: from 1/2 the
# first is nw(0), from -1 they are nw(-3/2) and nw(-2), both the root.
>> two(x) | sum_(j=1)^2 abs(F(lim nw(x - j/2))) < 1/10^6 = 1
two(x) | sum_(j=1)^2 abs(F(lim nw(x - j/2))) < 1/10^6 = 1

>> two(x) = 2
two(x) = 2

>> two(-1)
~1

>> two(1/2)
~2

# So is a limit's derivative under grad: Heron's root of x, whose slope
# at 0 grows without end (grad.ink).
>> sq(x)_0 = 1
sq(x)_0 = 1

>> sq(x)_n = (sq(x)_(n-1) + x/sq(x)_(n-1))/2
sq(x)_n = (sq(x)_(n-1) + x/sq(x)_(n-1))/2

>> d(x) | grad_(y = x) lim sq(y) > 0 = 1
d(x) | grad_(y = x) lim sq(y) > 0 = 1

>> d(x) = 2
d(x) = 2

>> d(4)
~1

>> d(0)
~2

# grad of a definition whose guard reads a limit that does not converge
# at the point is refused: no clause is chosen there that grad can follow.
>> grad_(y = 0) root(y)
error: grad cannot differentiate root at y = 0, whose guard reads a limit that does not converge

# A term the walk cannot take is a refusal, not a limit that does not
# converge: from an exact 0, Newton's root of 2 divides by 0.
>> hs(c)_0 = 0
hs(c)_0 = 0

>> hs(c)_k = hs(c)_(k-1) - (hs(c)_(k-1)^2 - c)/(2*hs(c)_(k-1))
hs(c)_k = hs(c)_(k-1) - (hs(c)_(k-1)^2 - c)/(2*hs(c)_(k-1))

>> hr(c) | lim hs(c) > 0 = lim hs(c)
hr(c) | lim hs(c) > 0 = lim hs(c)

>> hr(c) = 0
hr(c) = 0

>> hr(2)
error: division by zero

# From an inexact 0, as a step computes it, the first term is ~inf and the
# second inf - inf/inf, NaN: refused there too, since a step cannot tell
# its NaN from a term the interpreter refuses. It ran to a hundred terms
# and said it did not converge.
>> hs(c)_0 = ~0
hs(c)_0 = ~0

>> lim hs(2)
error: hs has no limit: a term is -nan

>> hr(2)
error: hs has no limit: a term is -nan

# So are a matrix term with a NaN cell, a NaN base term, and a NaN partial
# sum, ~inf and -~inf added, whose terms are numbers.
>> P_0 = [0, 1]
P_0 = [0, 1]

>> P_k = [0/~0, 1]
P_k = [0/~0, 1]

>> lim P
error: P has no limit: a term is [-nan, 1]

>> b_0 = 0/~0
b_0 = 0/~0

>> b_n = b_(n-1)/2
b_n = b_(n-1)/2

>> lim b
error: b has no limit: a term is -nan

>> sum_(k=0) (-1)^k*~1e308*10
error: the sum has no limit: a partial sum is -nan

>> hs(c)_0 = 1
hs(c)_0 = 1

>> hr(2)
~1.41421356

# An infinite term is a number: a walk through one does not converge
# (sequences.ink), and the clause after is tried.
>> o_0 = ~1e308
o_0 = ~1e308

>> o_n = o_(n-1) + o_(n-1)
o_n = o_(n-1) + o_(n-1)

>> lim o
error: o did not converge within 100 terms (last term ~inf)

>> p(x) | lim o > 0 = 1
p(x) | lim o > 0 = 1

>> p(x) = 2
p(x) = 2

>> p(0)
~2

# A small MPC with a fallback clause. The plan minimises 2(x + v)^2 +
# v^2/2, that is 5/2 v^2 + 4 x v and a constant, over |v| <= 1, by
# projected gradient with a step of 1/2, too long for the curvature 5: the
# error is multiplied by -3/2 at each term. Where the bound holds the plan,
# |x| >= 5/4, it settles, at -1 from 5/2 and 3/2; inside, 0 apart, it
# never does, -1, 1/2, -1, ... from x = 1/2. The plant moves by the plan
# where it exists and brings x no further from 0, and otherwise by -x/2,
# so x_n is 2^-(n-1) from n = 2, u_n -2^-n, and solved_n says which.
>> mpc(x0 = 5/2) = {
..     clip(v) | v > 1 = 1
..     clip(v) | v < -1 = -1
..     clip(v) = v
..     qp(s)_0 = 0
..     qp(s)_k = clip(qp(s)_(k-1) - (5*qp(s)_(k-1) + 4*s)/2)
..     u_n | abs(x_n + lim qp(x_n)) <= abs(x_n) = lim qp(x_n)
..     u_n = -x_n/2
..     solved_n | abs(x_n + lim qp(x_n)) <= abs(x_n) = 1
..     solved_n = 0
..     x_0 = x0
..     x_n = x_(n-1) + u_(n-1)
.. }
mpc(x0 = 5/2) = { ... }

>> m = mpc()
m = mpc()

>> m.u_0
~-1

>> m.x_2
~0.5

>> m.u_2
~-0.25

>> m.solved_1
~1

>> m.solved_2
~0

>> m.x_3
~0.25

>> m.x_20
~1.90734863e-06

# Without the fallback, as today: the plan from x_2 is refused, and the
# hundredth term of its cycle is 1/2.
>> bare(x0 = 5/2) = {
..     clip(v) | v > 1 = 1
..     clip(v) | v < -1 = -1
..     clip(v) = v
..     qp(s)_0 = 0
..     qp(s)_k = clip(qp(s)_(k-1) - (5*qp(s)_(k-1) + 4*s)/2)
..     u_n = lim qp(x_n)
..     x_0 = x0
..     x_n = x_(n-1) + u_(n-1)
.. }
bare(x0 = 5/2) = { ... }

>> bare().u_2
error: qp did not converge within 100 terms (last term ~0.5)

# Newton's method within a step, its fallback the term before: c_n is 2 at
# odd n, where Newton cycles from 0, and 1 at even n, where it converges at
# its sixth term to the root (sqrt(5) - 1)/2, 0.618033988749895 by mpmath.
>> newton(c_n) = {
..     G(x, c) = x^3 - 2*x + c
..     dG(x) = 3*x^2 - 2
..     nv(c)_0 = 0
..     nv(c)_k = nv(c)_(k-1) - G(nv(c)_(k-1), c)/dG(nv(c)_(k-1))
..     x_0 = 1/2
..     x_n | abs(G(lim nv(c_n), c_n)) < 1/10^6 = lim nv(c_n)
..     x_n = x_(n-1)
.. }
newton(c_n) = { ... }

>> s = newton(c_n = 1 + mod(n, 2))
s = newton(c_n = 1 + mod(n, 2))

>> s.x_1
~0.5

>> s.x_2
~0.618033989

>> s.x_3
~0.618033989

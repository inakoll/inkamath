# A maximum, minimum, argmax or argmin over an index, written as sum_ is
# (DESIGN.md, next in line). Every value was worked out by hand from the
# definitions: the term max(a, b) folded from the first keeps, or its index,
# and checked where the prelude can say it by max(a, b) nested.

# The largest term and the smallest, and the first index of each.
>> max_(k=1)^4 [1/2; 3; -1; 3][k]
3

>> argmax_(k=1)^4 [1/2; 3; -1; 3][k]
2

>> min_(k=1)^4 [1/2; 3; -1; -1][k]
-1

>> argmin_(k=1)^4 [1/2; 3; -1; -1][k]
3

# Of exact terms, the exact term: 1/4, 2/7, 1/4, 4/19.
>> frac max_(k=1)^4 k/(k^2 + 3)
2/7

>> argmax_(k=1)^4 k/(k^2 + 3)
2

# 1/2, 1/2, 3/8, 1/4, 5/32: a tie, which the first takes.
>> max_(k=1)^5 k/2^k
0.5

>> argmax_(k=1)^5 k/2^k
1

>> argmin_(k=1)^5 (k - 3)^2
3

# One term is itself, compared with nothing.
>> max_(k=2)^2 k/4
0.5

>> max_(k=1)^1 ~0.25
~0.25

>> argmin_(k=7)^7 k
7

# Two terms or more: a comparison read an inexact one, so the term it keeps
# is inexact, and its index, as a clause a guard chose by one (C241). Over
# two terms, max(a, b).
>> max_(k=1)^2 [1; ~0.5][k]
~1

>> max(1, ~0.5)
~1

>> max_(k=1)^3 [1/2; ~0.25; 1/3][k]
~0.5

>> argmax_(k=1)^3 [1/2; ~0.25; 1/3][k]
~1

>> max_(k=1)^3 [1; 1/~0; 2][k]
~inf

>> argmax_(k=1)^3 [1; 1/~0; 2][k]
~2

# Equals, the first: an exact 1/2 and its double, and the two zeros of a
# double, -0 not below +0.
>> argmax_(k=1)^2 [1/2; ~0.5][k]
~1

>> max_(k=1)^2 [~-10^-400; ~0][k]
~-0

>> max_(k=1)^2 [~0; ~-10^-400][k]
~0

>> min_(k=1)^2 [~0; ~-10^-400][k]
~0

>> argmin_(k=1)^2 [~-10^-400; ~0][k]
~1

# The body is a term, as a sum's, and the bounds are the scope's.
>> max_(k=1)^3 k + 10
13

>> max_(k=1)^3 k + k
error: k is bound in the maximum's body alone, which ended at the '+'

>> n = 3
n = 3

>> max_(k=1)^n -(k - 2)^2
0

>> argmax_(n=1)^n -(n - 2)^2
2

# Nested, a maximin, and inside a sum: rows [1 4] and [3 2].
>> max_(i=1)^2 min_(j=1)^2 [1 4; 3 2][i, j]
2

>> argmax_(i=1)^2 min_(j=1)^2 [1 4; 3 2][i, j]
2

>> sum_(i=1)^2 max_(j=1)^2 [1 4; 3 2][i, j]
7

# An argmax is an index, to read a cell at.
>> v = [3; 1; 4; 1; 5; 9; 2; 6]
v = [3; 1; 4; 1; 5; 9; 2; 6]

>> argmax_(k=1)^8 v[k]
6

>> v[argmax_(k=1)^8 v[k]]
9

>> top(u[k<=m]) = argmax_(j=1)^m u[j]
top(u[k<=m]) = argmax_(j=1)^m u[j]

>> top(v)
6

>> v[argmin_(k=1)^8 v[k]]
1

# But an inexact one is not an index, as no other is.
>> w = [1/2; ~0.75; 1/4]
w = [1/2; ~0.75; 1/4]

>> argmax_(k=1)^3 w[k]
~2

>> w[argmax_(k=1)^3 w[k]]
error: an index must be exact, and ~2 was approximated

>> argmax_(k=1)^3 w[k] == 2
~1

# A matrix body is refused, as max(a, b) refuses one: a maximum by cells is
# written by its cells, a max-pooling of a window of two.
>> max_(k=1)^2 [1 2; 3 4][k]
error: the maximum needs single values, not a 1x2 matrix; write it by its cells

>> pool(x)[i<=2] = max_(k=1)^2 x[2*i - 2 + k]
pool(x)[i<=2] = max_(k=1)^2 x[2*i - 2 + k]

>> pool([1; 5; -2; 0])
[5;
 0]

# No terms is no maximum, where a sum of none is 0; and no last index no
# reduction, where a sum without one is a series.
>> sum_(k=3)^1 k
0

>> max_(k=3)^1 k
error: the maximum has no terms: k runs from 3 to 1

>> argmin_(k=1)^0 k
error: the argmin has no terms: k runs from 1 to 0

>> max_(k=1) 1/k
error: expected '^' and the last index, as in max_(k=1)^n

# NaN is no number, and a complex number no real one, as a comparison says,
# in the reduction's words: each term is ranked, one alone too.
>> nn = 1/~0 - 1/~0
nn = 1/~0 - 1/~0

>> max_(k=1)^2 [1; nn][k]
error: the maximum needs a number, not -nan

>> argmin_(k=1)^1 nn
error: the argmin needs a number, not -nan

>> max_(k=1)^2 k*i
error: the maximum needs real numbers, not ~(i)

# The words stay names: max(a, b) the prelude's, and argmax anything.
>> max(2, 3)
3

>> argmax = 4
argmax = 4

>> argmax_(k=1)^argmax k^2
4

# grad through a maximum takes the slope of the term kept, the first of
# equals, as max(a, b) takes a's: at 4 the terms are 3, 4, 3, at 3 they
# are 2, 2, 0.
>> grad_(x = 4) max_(k=1)^3 (k*x - k^2)
2

>> grad_(x = 3) max_(k=1)^3 (k*x - k^2)
1

>> grad_(x = 3) max(max(x - 1, 2*x - 4), 3*x - 9)
1

>> grad_(x = 3) min_(k=1)^3 (k*x - k^2)
3

>> grad_(v = [3; 1; 3]) max_(k=1)^3 v[k]
[1;
 0;
 0]

>> grad_(x = 1) max_(k=1)^2 [x; ~2][k]
~0

# On the real line alone, as max is (C205): a term whose slope leaves it,
# or one moving with a complex variable, refused where they answered ~(i)
# and ~1.
>> grad_(x = 1) max_(k=1)^2 [i*x - i; -1][k]
error: the maximum has no derivative where a term leaves the real line at x = 1

>> grad_(z = i) min_(k=1)^2 [1; z - i][k]
error: the minimum has no complex derivative at z = ~(i)

# An argmax is constant but where a tie moves, a jump, as a comparison's:
# the terms 2, 2 and x tie at 2 where x moves, and at 1 where nothing does.
>> grad_(x = 4) argmax_(k=1)^3 (k*x - k^2)
0

>> grad_(x = 3) argmax_(k=1)^3 (k*x - k^2)
error: the argmax jumps at x = 3

>> t(k, x) = 2
t(k, x) = 2

>> t(k, x) | k == 3 = x
t(k, x) | k == 3 = x

>> grad_(x = 1) argmax_(k=1)^3 t(k, x)
0

>> grad_(x = 2) argmax_(k=1)^3 t(k, x)
error: the argmax jumps at x = 2

# A tie that does not reach the term kept moves nothing: at 2, x ties with 2
# below 5, which stays the largest, and above 0, which stays the smallest.
>> grad_(x = 2) argmax_(k=1)^3 [x; 2; 5][k]
0

>> grad_(x = 2) argmax_(k=1)^3 [2; x; 5][k]
0

>> grad_(x = 2) argmin_(k=1)^3 [x; 2; 0][k]
0

>> grad_(x = 2) argmin_(k=1)^3 [2; x; 0][k]
0

# An argmax steps as a comparison does, so a limit whose terms read one
# moving is refused as grad.ink's bisection is (C215), where it answered
# [0; 0]: this one halves toward x too.
>> bis(x)_0 = [0; 1]
bis(x)_0 = [0; 1]

>> bis(x)_n = bis(x)_(n-1) + (bis(x)_(n-1)[2] - bis(x)_(n-1)[1])/2*([1; 0] - (argmax_(k=1)^2 [x; (bis(x)_(n-1)[1] + bis(x)_(n-1)[2])/2][k] - 1)*[1; 1])
bis(x)_n = bis(x)_(n-1) + (bis(x)_(n-1)[2] - bis(x)_(n-1)[1])/2*([1; 0] - (argmax_(k=1)^2 [x; (bis(x)_(n-1)[1] + bis(x)_(n-1)[2])/2][k] - 1)*[1; 1])

>> grad_(x = 1/3) lim bis(x)
error: grad cannot differentiate lim bis, whose terms step with x at a comparison or a floor

# tex sets them as a paper does.
>> f(x) = max_(k=1)^3 x^k + 1
f(x) = max_(k=1)^3 x^k + 1

>> tex ?f
f(x) = \max_{k=1}^{3} x^k + 1

>> h(x) = min_(k=0)^n (x - k)^2
h(x) = min_(k=0)^n (x - k)^2

>> tex ?h
h(x) = \min_{k=0}^{n} (x - k)^2

>> tex ?top
\operatorname{top}(u \in \mathbb{R}^{m}) = \operatorname*{arg\,max}_{j=1}^{m} u_{j}

>> g(u) = argmin_(j=1)^2 u[j]
g(u) = argmin_(j=1)^2 u[j]

>> tex ?g
g(u) = \operatorname*{arg\,min}_{j=1}^{2} u_{j}

# README's: a classifier's accuracy over a batch, each row's logits, its
# prediction their first largest; written before as prod_(v=1)^c (Z[b,v] <=
# Z[b,y[b]]), which takes a tied target as predicted. Rows 1 and 2 tie.
>> acc(Z[b<=m, j<=c], y) = sum_(b=1)^m (argmax_(v=1)^c Z[b,v] == y[b])/m
acc(Z[b<=m, j<=c], y) = sum_(b=1)^m (argmax_(v=1)^c Z[b,v] == y[b])/m

>> Z = [1/2 3 -1 3; 2 0 2 1; -1 -2 -3 -1/2]
Z = [1/2 3 -1 3; 2 0 2 1; -1 -2 -3 -1/2]

>> frac acc(Z, [2; 3; 4])
2/3

>> frac acc(Z, [2; 1; 4])
1

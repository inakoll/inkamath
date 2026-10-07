# A size bound by a function's signature (DESIGN.md, next in line): a
# parameter states its size by bounds, as a model's input does, and a bound
# that is a name is bound by the call to the argument's size, a whole number
# constant for that call. Every value below is worked out by hand or with
# exact fractions apart from the interpreter, none recorded. Today a parameter
# that is not a name is dropped in silence (C150): the definitions echo, and
# their calls are refused, 'tr takes no arguments', or answer without it.

# The trace, M an n by n matrix: the call binds n to M's size.
>> tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]
tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]

>> tr([1 2; 3 4])
5

>> tr([2 2 0; 1 3 1; 0 1 4])
9

# A single value is a 1x1 matrix, as a size inferred reads it.
>> tr(7)
7

>> ?tr
tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]

# Set as a model's input is.
>> tex ?tr
\operatorname{tr}(M \in \mathbb{R}^{n \times n}) = \sum_{j=1}^{n} M_{j,j}

# A size that disagrees is refused before the body, naming the parameter as
# written and what it was given.
>> tr([1 2 3; 4 5 6])
error: tr takes M[j<=n, k<=n], not a 2x3 matrix

>> tr([1 2; 3 4;; 5 6; 7 8])
error: tr takes M[j<=n, k<=n], not a 2x2x2 tensor

>> tr
error: tr expects 1 argument, got 0

# The size is a number in the body: a sum's bound, a divisor, an index, a
# cell's bound.
>> mean(v[j<=n]) = sum_(j=1)^n v[j]/n
mean(v[j<=n]) = sum_(j=1)^n v[j]/n

>> mean([1; 2; 6])
3

>> mean(v = [2; 4])
3

>> tex ?mean
\operatorname{mean}(v \in \mathbb{R}^{n}) = \sum_{j=1}^{n} \frac{v_{j}}{n}

# One bound is a column, and a row is not one, as for a model's input.
>> mean([1 2 6])
error: mean takes v[j<=n], not a 1x3 matrix

>> last(v[j<=n]) = v[n]
last(v[j<=n]) = v[n]

>> last([4; 5; 6])
6

# A parameter's indices bind nothing, so a cell's may be the same letters.
>> rev(v[j<=n])[j<=n] = v[n+1-j]
rev(v[j<=n])[j<=n] = v[n+1-j]

>> rev([1; 2; 3])
[3;
 2;
 1]

>> size(M[j<=m, k<=n]) = [m n]
size(M[j<=m, k<=n]) = [m n]

>> size([1 2 3; 4 5 6])
[2, 3]

>> size(5)
[1, 1]

# The memo keys a call by its arguments' shapes too.
>> size([1 2])
[1, 2]

>> size([1; 2])
[2, 1]

>> jk(M[j<=m, k<=n]) = j
jk(M[j<=m, k<=n]) = j

>> jk([1 2])
error: j is not defined

# A name in two parameters is one size.
>> dot(x[j<=n], y[j<=n]) = sum_(j=1)^n x[j]*y[j]
dot(x[j<=n], y[j<=n]) = sum_(j=1)^n x[j]*y[j]

>> dot([1; 2; 3], [4; 5; 6])
32

>> dot([1; 2], [1; 2; 3])
error: dot takes x[j<=n] and y[j<=n], not a 2x1 matrix and a 3x1 matrix

>> outer(x[j<=m], y[k<=n])[j<=m, k<=n] = x[j]*y[k]
outer(x[j<=m], y[k<=n])[j<=m, k<=n] = x[j]*y[k]

>> outer([1; 2], [3; 4; 5])
[3, 4,  5;
 6, 8, 10]

# A bound may be a number.
>> cross(a[j<=3], b[j<=3]) = [a[2]*b[3] - a[3]*b[2]; a[3]*b[1] - a[1]*b[3]; a[1]*b[2] - a[2]*b[1]]
cross(a[j<=3], b[j<=3]) = [a[2]*b[3] - a[3]*b[2]; a[3]*b[1] - a[1]*b[3]; a[1]*b[2] - a[2]*b[1]]

>> cross([1; 2; 3], [4; 5; 6])
[-3;
  6;
 -3]

>> cross([1; 2], [4; 5; 6])
error: cross takes a[j<=3], not a 2x1 matrix

>> cross(1, [4; 5; 6])
error: cross takes a[j<=3], not a single value

# Three bounds are a tensor, slices first.
>> slices(T[b<=p, j<=m, k<=n]) = sum_(c=1)^p T[c]
slices(T[b<=p, j<=m, k<=n]) = sum_(c=1)^p T[c]

>> slices([1 2; 3 4;; 5 6; 7 8])
[ 6,  8;
 10, 12]

>> shape(T[b<=p, j<=m, k<=n]) = [p m n]
shape(T[b<=p, j<=m, k<=n]) = [p m n]

>> shape([1 2;; 3 4;; 5 6])
[3, 1, 2]

>> shape([1 2; 3 4])
error: shape takes T[b<=p, j<=m, k<=n], not a 2x2 matrix

>> tex ?shape
\operatorname{shape}(T \in \mathbb{R}^{p \times m \times n}) = \begin{bmatrix} p & m & n \end{bmatrix}

# A size is bound as a parameter is, so the session's n is not read, and a
# definition means the same before and after one is defined.
>> n = 10
n = 10

>> cnt(v[j<=n]) = n
cnt(v[j<=n]) = n

>> cnt([1; 1])
2

>> tr([1 2; 3 4])
5

>> n
10

# The prelude's names are hidden as the session's are.
>> ne(v[j<=e]) = e
ne(v[j<=e]) = e

>> ne([1; 1; 1])
3

# A default is held to its size and gives it; a default may read a size its
# argument gave.
>> pad(v[j<=n] = [1; 2]) = n
pad(v[j<=n] = [1; 2]) = n

>> pad()
2

>> pad([1; 2; 3])
3

>> tex ?pad
\operatorname{pad}(v \in \mathbb{R}^{n} = \begin{bmatrix} 1 \\ 2 \end{bmatrix}) = n

>> scale(v[j<=n], s = n) = s*v
scale(v[j<=n], s = n) = s*v

>> scale([1; 2])
[2;
 4]

>> scale([1; 2], 3)
[3;
 6]

>> pd(v[j<=3] = [1; 2]) = v
pd(v[j<=3] = [1; 2]) = v

>> pd()
error: pd takes v[j<=3], not a 2x1 matrix

# The sizes are the definition's, as its parameters' names are: stated in one
# clause, they hold for every clause. The characteristic polynomial by
# Faddeev-LeVerrier, highest power first, with no size measured.
>> id(n)[j<=n, k<=n] = j == k
id(n)[j<=n, k<=n] = j == k

>> fm(A)_0 = 0*A
fm(A)_0 = 0*A

>> fm(A[j<=n, k<=n])_m = A*fm(A)_(m-1) + fc(A)_(m-1)*id(n)
fm(A[j<=n, k<=n])_m = A*fm(A)_(m-1) + fc(A)_(m-1)*id(n)

>> fc(A)_0 = 1
fc(A)_0 = 1

>> fc(A)_m = -tr(A*fm(A)_m)/m
fc(A)_m = -tr(A*fm(A)_m)/m

>> cp(A[j<=n, k<=n])[j<=n+1] = fc(A)_(j-1)
cp(A[j<=n, k<=n])[j<=n+1] = fc(A)_(j-1)

>> det(A[j<=n, k<=n]) = (-1)^n*fc(A)_n
det(A[j<=n, k<=n]) = (-1)^n*fc(A)_n

>> cp([2 2 0; 1 3 1; 0 1 4])
[  1;
  -9;
  23;
 -14]

>> det([2 2 0; 1 3 1; 0 1 4])
14

>> det([1 2; 3 4])
-2

>> det(5)
5

>> det([1 2 3; 4 5 6])
error: det takes A[j<=n, k<=n], not a 2x3 matrix

>> ?fm
fm(A)_0 = 0*A
fm(A[j<=n, k<=n])_m = A*fm(A)_(m-1) + fc(A)_(m-1)*id(n)

>> tex ?fm
\operatorname{fm}(A)_0 = 0\,A
\operatorname{fm}(A \in \mathbb{R}^{n \times n})_m = A\,\operatorname{fm}(A)_{m-1} + \operatorname{fc}(A)_{m-1}\,\operatorname{id}(n)

>> fm(A[r<=p, c<=p])_m = A
error: fm takes A of size n x n, so a clause cannot take it of size p x p; write 'clear fm' first

# Clauses agree on their sizes, not on the names of their indices.
>> sz(v[j<=n]) | n == 1 = 0
sz(v[j<=n]) | n == 1 = 0

>> sz(v[i<=n]) = n
sz(v[i<=n]) = n

>> sz([1; 2])
2

>> sz(3)
0

# A sequence of a function reads its size in any clause, and its limit is
# its terms'.
>> on(n)[j<=n] = 1
on(n)[j<=n] = 1

>> pw(A[j<=n, k<=n])_0 = on(n)
pw(A[j<=n, k<=n])_0 = on(n)

>> pw(A)_m = A*pw(A)_(m-1)/(A*pw(A)_(m-1))[n]
pw(A)_m = A*pw(A)_(m-1)/(A*pw(A)_(m-1))[n]

>> lim pw([4 1 1; 1 4 1; 1 1 4])
[1;
 1;
 1]

# A guard reads the sizes, and each call of a recursion binds its own.
>> drop(v[j<=n])[j<=n-1] = v[j]
drop(v[j<=n])[j<=n-1] = v[j]

>> tot(v[j<=n]) = v[n] + tot(drop(v))
tot(v[j<=n]) = v[n] + tot(drop(v))

>> tot(v) | n == 1 = v[1]
tot(v) | n == 1 = v[1]

>> tot([1; 2; 3])
6

>> tot(5)
5

# Under grad a size is a constant, the shape never moving.
>> grad_(A = [1 2; 3 4]) tr(A*A)
[2, 6;
 4, 8]

>> grad_(A = [1 2; 3 4]) det(A)
[ 4, -3;
 -2,  1]

>> grad_(t = 1) det([1 2; 3 4] + t*id(2))
7

>> grad_(t = 3) t*cnt([t; t])
2

# A size's name is its own: not a parameter's, nor the index's, nor a
# cell's index.
>> bad(n, x[j<=n]) = n
error: bad has a parameter and a size named n

>> bad(x[j<=n])_n = x[n]
error: bad has an index and a size named n

>> bad(x[j<=k])[k<=2] = x[1]
error: bad has a cell's index and a size named k

# A size is bound once a parameter is, so a default reads only the sizes
# stated before it.
>> late(s = n, v[j<=n] = [1; 2]) = s
error: late's default for s reads the size n, not stated before it

>> late(v[j<=n] = [1; n]) = v
error: late's default for v reads the size n, not stated before it

# A parameter is a name or a name and its size; anything else, dropped in
# silence before (C150), is refused.
>> bad(2) = 3
error: a parameter is a name, as 'x' or 'x = 1', or a name and its size, as 'v[j<=n]'

>> bad(x, 2) = x
error: a parameter is a name, as 'x' or 'x = 1', or a name and its size, as 'v[j<=n]'

>> bad(x[j]) = x
error: a parameter is a name, as 'x' or 'x = 1', or a name and its size, as 'v[j<=n]'

>> bad(x[2]) = x
error: a parameter is a name, as 'x' or 'x = 1', or a name and its size, as 'v[j<=n]'

>> bad(x_n) = 1
error: a parameter is a name, as 'x' or 'x = 1', or a name and its size, as 'v[j<=n]'

>> bad(x[j<=n+1]) = x
error: a size is a whole number or a name, as 'v[j<=3]' or 'v[j<=n]'

>> bad(x[j<=0]) = x
error: a size must be at least 1, not 0

>> bad(x[j<=-1]) = x
error: a size must be at least 1, not -1

>> bad(1)
error: bad is not defined

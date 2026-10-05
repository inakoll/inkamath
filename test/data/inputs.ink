# A model's input of more than one cell (DESIGN.md, next in line; C83). The
# model states an input's size in its signature, by bounds, as a definition
# by cells states its own, and every term of the input, whether the
# argument, a default or a history gives it, has that size or is refused
# where it is read. An input whose size is not stated takes any, as before.

>> dot(x_n[j<=2]) = {
..     y_n = [1 2]*x_n
.. }
dot(x_n[j<=2]) = { ... }

>> ?dot
dot(x_n[j<=2]) = {
    y_n = [1 2]*x_n
}

# Typeset as a paper states it, the size after the input.
>> tex ?dot
\operatorname{dot}(x_n \in \mathbb{R}^{2}):
    y_n = \begin{bmatrix} 1 & 2 \end{bmatrix}\,x_n

>> v = dot(x_n = [n; 1])
v = dot(x_n = [n; 1])

>> v.y_3
5

>> v.x_3
[3;
 1]

# A cell of the input is read as any matrix's is, one index a row.
>> dif(x_n[j<=2]) = {
..     y_n = x_n[1] - x_n[2]
.. }
dif(x_n[j<=2]) = { ... }

>> dif(x_n = [n^2; n]).y_3
6

# A size inferred from the input's cells is the stated size, read where the
# argument is held to it.
>> pos(x_n[j<=3]) = {
..     g_n[j] = x_n[j]*(x_n[j] > 0)
.. }
pos(x_n[j<=3]) = { ... }

>> pos(x_n = [n - 2; 3 - n; -1]).g_1
[0;
 2;
 0]

>> pos(x_n = [n; n]).g_1
error: pos(...).x_1 is a 2x1 matrix, where pos takes a 3x1 matrix

# Each input is held to its own size.
>> two(a_n[j<=2], b_n[j<=3]) = {
..     y_n = [1 1]*a_n + [1 1 1]*b_n
.. }
two(a_n[j<=2], b_n[j<=3]) = { ... }

>> two(a_n = [n; 1], b_n = [1; n; n^2]).y_2
10

>> two(a_n = [n; 1], b_n = [n; 1]).y_2
error: two(...).b_2 is a 2x1 matrix, where two takes a 3x1 matrix

# Defining an instance evaluates nothing, so an argument of another size is
# refused where a term is read, naming the term and both sizes: a single
# value, a longer column, and a row, as a missing transpose is anywhere.
>> w = dot(x_n = n)
w = dot(x_n = n)

>> w.y_1
error: w.x_1 is a single value, where dot takes a 2x1 matrix

>> w.x_1
error: w.x_1 is a single value, where dot takes a 2x1 matrix

>> dot(x_n = [n; 1; 0]).y_1
error: dot(...).x_1 is a 3x1 matrix, where dot takes a 2x1 matrix

>> dot(x_n = [n 1]).y_1
error: dot(...).x_1 is a 1x2 matrix, where dot takes a 2x1 matrix

# Two bounds are rows and columns, a matrix at each index; a column may be
# written with both, and is set as written.
>> trace(x_n[j<=2, k<=2]) = {
..     t_n = x_n[1,1] + x_n[2,2]
..     d_n = x_n*[1; -1]
.. }
trace(x_n[j<=2, k<=2]) = { ... }

>> g = trace(x_n = [n, 1; 2, n^2])
g = trace(x_n = [n, 1; 2, n^2])

>> g.t_3
12

>> g.d_3
[ 2;
 -7]

>> trace(x_n = [n 1]).t_0
error: trace(...).x_0 is a 1x2 matrix, where trace takes a 2x2 matrix

>> sq(x_n[j<=2, k<=2]) = {
..     y_n = x_n*x_n
.. }
sq(x_n[j<=2, k<=2]) = { ... }

>> tex ?sq
\operatorname{sq}(x_n \in \mathbb{R}^{2 \times 2}):
    y_n = x_n\,x_n

>> col(x_n[j<=2, k<=1]) = {
..     y_n = [1 2]*x_n
.. }
col(x_n[j<=2, k<=1]) = { ... }

>> col(x_n = [n; 1]).y_2
4

>> tex ?col
\operatorname{col}(x_n \in \mathbb{R}^{2 \times 1}):
    y_n = \begin{bmatrix} 1 & 2 \end{bmatrix}\,x_n

# A bound may read the model's parameters, as a cell's bound may.
>> avg(d = 3, x_n[j<=d]) = {
..     y_n = sum_(j=1)^d x_n[j]/d
.. }
avg(d = 3, x_n[j<=d]) = { ... }

>> avg(x_n = [n; 2*n; 3*n]).y_4
8

>> avg(d = 2, x_n = [n; 3*n]).y_4
8

>> avg(x_n = [n; 3*n]).y_4
error: avg(...).x_4 is a 2x1 matrix, where avg takes a 3x1 matrix

>> tex ?avg
\operatorname{avg}(d = 3, x_n \in \mathbb{R}^{d}):
    y_n = \sum_{j=1}^{d} \frac{x_{n,j}}{d}

# Or a name the model defines.
>> wide(x_n[j<=N]) = {
..     N = 2
..     y_n = [1 1]*x_n
.. }
wide(x_n[j<=N]) = { ... }

>> wide(x_n = [n; 1]).y_3
4

>> wide(x_n = n).y_3
error: wide(...).x_3 is a single value, where wide takes a 2x1 matrix

# But not its index: a size that moved from term to term could not be
# compiled, so the interpreter refuses it too, where it is written.
>> grow(x_n[j<=n+1]) = {
..     y_n = sum_(j=1)^(n+1) x_n[j]
.. }
error: x is an input of grow, so its size cannot read the index n

# A bound is a size, at least 1, as a cell's is.
>> none(x_n[j<=0]) = {
..     y_n = x_n
.. }
none(x_n[j<=0]) = { ... }

>> none(x_n = n).y_0
error: a size must be at least 1, not 0

# Three bounds are a tensor, slices first. Compiled, it is refused.
>> bat(x_n[b<=2, j<=1, k<=2]) = {
..     y_n = x_n[2]*[1; 1]
.. }
bat(x_n[b<=2, j<=1, k<=2]) = { ... }

>> bat(x_n = [1 2;; 3 n]).y_5
8

>> bat(x_n = [3 n]).y_5
error: bat(...).x_5 is a 1x2 matrix, where bat takes a 2x1x2 tensor

>> tex ?bat
\operatorname{bat}(x_n \in \mathbb{R}^{2 \times 1 \times 2}):
    y_n = x_{n,2}\,\begin{bmatrix} 1 \\ 1 \end{bmatrix}

# One cell stated is a single value, and held to it.
>> one(x_n[j<=1]) = {
..     y_n = 2*x_n
.. }
one(x_n[j<=1]) = { ... }

>> one(x_n = n).y_3
6

>> one(x_n = [n; n]).y_3
error: one(...).x_3 is a 2x1 matrix, where one takes a single value

# A default is an argument, and held to the size as one is.
>> rest(x_n[j<=2] = [0; 1]) = {
..     y_n = [1 2]*x_n
.. }
rest(x_n[j<=2] = [0; 1]) = { ... }

>> rest().y_0
2

>> rest(x_n = 1).y_0
error: rest(...).x_0 is a single value, where rest takes a 2x1 matrix

# Its size is set before its default, as written.
>> tex ?rest
\operatorname{rest}(x_n \in \mathbb{R}^{2} = \begin{bmatrix} 0 \\ 1 \end{bmatrix}):
    y_n = \begin{bmatrix} 1 & 2 \end{bmatrix}\,x_n

>> nil(x_n[j<=2] = 0) = {
..     y_n = [1 2]*x_n
.. }
nil(x_n[j<=2] = 0) = { ... }

>> nil().y_0
error: nil(...).x_0 is a single value, where nil takes a 2x1 matrix

# A history gives terms of the input, so of its size: a single value is not
# stretched to a column.
>> turn(x_n[j<=2]) = {
..     x_n | n < 0 = [1; -1]
..     c_n = [0 1; 1 0]*x_(n-1)
.. }
turn(x_n[j<=2]) = { ... }

>> r = turn(x_n = [n; n^2])
r = turn(x_n = [n; n^2])

>> r.c_0
[-1;
  1]

>> r.c_3
[4;
 2]

>> flat(x_n[j<=2]) = {
..     x_n | n < 0 = 0
..     c_n = x_(n-1)
.. }
flat(x_n[j<=2]) = { ... }

>> flat(x_n = [n; n]).c_0
error: flat(...).x_-1 is a single value, where flat takes a 2x1 matrix

>> flat(x_n = [n; n]).c_1
[0;
 0]

# A history by cells gives every term, its guard choosing cells, so it is
# refused as a clause that always applies is, the size stated or not (C89).
>> sw(x_n) = {
..     x_n[j<=2] | n < 0 = 0
..     c_n = [0 1; 1 0]*x_(n-1)
.. }
error: x is an input of sw, so its body can give it only a history: a term or a guarded clause

>> sw(x_n[j<=2]) = {
..     x_n[j<=2] | n < 0 = 0
..     c_n = [0 1; 1 0]*x_(n-1)
.. }
error: x is an input of sw, so its body can give it only a history: a term or a guarded clause

# An instance within a model is held to its own model's size.
>> outer(u_n) = {
..     inner = dot(x_n = [u_n; 1])
..     y_n = inner.y_n
.. }
outer(u_n) = { ... }

>> outer(u_n = n).y_2
4

>> lone(u_n) = {
..     inner = dot(x_n = u_n)
..     y_n = inner.y_n
.. }
lone(u_n) = { ... }

>> lone(u_n = n).y_2
error: lone(...).inner.x_2 is a single value, where dot takes a 2x1 matrix

# An input whose size is not stated takes any, as before.
>> dbl(x_n) = {
..     y_n = 2*x_n
.. }
dbl(x_n) = { ... }

>> dbl(x_n = [n; 1]).y_1
[2;
 2]

>> dbl(x_n = n).y_1
2

# A size is stated by bounds only: an index with none, or a place, states
# none, and a parameter's size is its default's.
>> bad(x_n[j]) = {
..     y_n = x_n
.. }
error: a model's parameter is a name, as 'k = 2', or an input, as 'x_n' or 'x_n[j<=2]'

>> bad(x_n[2]) = {
..     y_n = x_n
.. }
error: a model's parameter is a name, as 'k = 2', or an input, as 'x_n' or 'x_n[j<=2]'

>> bad(k[j<=2] = [1; 2], x_n) = {
..     y_n = k*x_n
.. }
error: a model's parameter is a name, as 'k = 2', or an input, as 'x_n' or 'x_n[j<=2]'

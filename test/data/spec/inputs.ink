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

# And its index, as a term's by cells may, so each term has its own size.
# Compiled, such a size is refused.
>> grow(x_n[j<=n+1]) = {
..     y_n = sum_(j=1)^(n+1) x_n[j]
.. }
grow(x_n[j<=n+1]) = { ... }

>> c_n[j<=n+1] = j
c_n[j<=n+1] = j

>> grow(x_n = c_n).y_3
10

>> grow(x_n = [n; n]).y_3
error: grow(...).x_3 is a 2x1 matrix, where grow takes a 4x1 matrix

# Three bounds are a tensor, slices first. Compiled, it is refused.
>> bat(x_n[b<=2, j<=1, k<=2]) = {
..     y_n = x_n[2]*[1; 1]
.. }
bat(x_n[b<=2, j<=1, k<=2]) = { ... }

>> bat(x_n = [1 2;; 3 n]).y_5
8

>> bat(x_n = [3 n]).y_5
error: bat(...).x_5 is a 1x2 matrix, where bat takes a 2x1x2 tensor

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

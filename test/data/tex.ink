# 'tex' of a definition by cells and of a model (DESIGN.md, tex of cells),
# so that a layer can be read against the page it came from. An entry is set
# as a paper defines a matrix, by its entry at a row and a column, with their
# range after it; a clause for one cell is a line of its own before the rest,
# as a base clause is. A model is its head, then its definitions a line each,
# indented as '?' shows them.
>> M[j<=2, k<=2] = j + k
M[j<=2, k<=2] = j + k

>> tex ?M
M_{j,k} = j + k, \quad 1 \le j \le 2,\ 1 \le k \le 2

>> M[j<=2, k<=2] | j == k = 1
M[j<=2, k<=2] | j == k = 1

>> tex ?M
M_{j,k} = \begin{cases} 1 & \text{if } j = k \\ j + k & \text{otherwise} \end{cases}, \quad 1 \le j \le 2,\ 1 \le k \le 2

>> M[1,2] = 5
M[1,2] = 5

>> tex ?M
M_{1,2} = 5
M_{j,k} = \begin{cases} 1 & \text{if } j = k \\ j + k & \text{otherwise} \end{cases}, \quad 1 \le j \le 2,\ 1 \le k \le 2

# A bound that is a name is set as one; a matrix written whole is a line of
# its own, before the cells that override it.
>> n = 3
n = 3

>> P[j<=n, k<=n] = j*k
P[j<=n, k<=n] = j*k

>> tex ?P
P_{j,k} = j\,k, \quad 1 \le j \le n,\ 1 \le k \le n

>> B = [1 2; 3 4]
B = [1 2; 3 4]

>> B[2,1] = 0
B[2,1] = 0

>> tex ?B
B = \begin{bmatrix} 1 & 2 \\ 3 & 4 \end{bmatrix}
B_{2,1} = 0

# A column by one index has one subscript, and a term's cell shares the
# term's, as reading one does.
>> v[j<=3] = j^2
v[j<=3] = j^2

>> tex ?v
v_j = j^2, \quad 1 \le j \le 3

>> tap_0[j<=3] = 0
tap_0[j<=3] = 0

>> tap_n[j<=3] = tap_(n-1)[j-1]
tap_n[j<=3] = tap_(n-1)[j-1]

>> tap_n[j<=3] | j == 1 = n^2
tap_n[j<=3] | j == 1 = n^2

>> tex ?tap
\mathit{tap}_{0,j} = 0, \quad 1 \le j \le 3
\mathit{tap}_{n,j} = \begin{cases} n^2 & \text{if } j = 1 \\ \mathit{tap}_{n-1,j-1} & \text{otherwise} \end{cases}, \quad 1 \le j \le 3

>> tap_n[2] = 7
tap_n[2] = 7

>> tex ?tap
\mathit{tap}_{0,j} = 0, \quad 1 \le j \le 3
\mathit{tap}_{n,2} = 7
\mathit{tap}_{n,j} = \begin{cases} n^2 & \text{if } j = 1 \\ \mathit{tap}_{n-1,j-1} & \text{otherwise} \end{cases}, \quad 1 \le j \le 3

# A model: its head, its parameters with their defaults and its inputs with
# their index, then each definition as 'tex' sets it alone.
>> gain(k = 2, b = 1/2, x_n) = {
..     y_n = k*x_n + b
..     s_0 = 0
..     s_n = s_(n-1) + y_n
.. }
gain(k = 2, b = 1/2, x_n) = { ... }

>> tex ?gain
\operatorname{gain}(k = 2, b = \frac{1}{2}, x_n):
    y_n = k\,x_n + b
    s_0 = 0
    s_n = s_{n-1} + y_n

# An instance names its input with its index, as it was written.
>> g = gain(x_n = n)
g = gain(x_n = n)

>> tex ?g
g = \operatorname{gain}(x_n = n)

# A recurrent layer, h_n = th(W h_(n-1) + U x_n), its activation taken cell
# by cell.
>> layer(x_n) = {
..     th(z) = z/(1 + z^2)
..     W = [1/2, -1/4; 1/4, 1/2]
..     U = [1; -1/2]
..     h_0 = [0; 0]
..     h_n[j<=2] = th((W*h_(n-1) + U*x_n)[j])
.. }
layer(x_n) = { ... }

>> tex ?layer
\operatorname{layer}(x_n):
    \operatorname{th}(z) = \frac{z}{1 + z^2}
    W = \begin{bmatrix} \frac{1}{2} & \frac{-1}{4} \\ \frac{1}{4} & \frac{1}{2} \end{bmatrix}
    U = \begin{bmatrix} 1 \\ \frac{-1}{2} \end{bmatrix}
    h_0 = \begin{bmatrix} 0 \\ 0 \end{bmatrix}
    h_{n,j} = \operatorname{th}((W\,h_{n-1} + U\,x_n)_{j}), \quad 1 \le j \le 2

# What it still cannot set, it says.
>> outer(a = 1) = {
..     inner(b = 2) = { c = a + b }
.. }
outer(a = 1) = { ... }

>> tex ?outer
error: tex cannot show a model inside a model yet

# A base term written after a guarded clause is tried after it (C45), so it
# is a case in its place, not a line above the cases that would say it holds
# (C85); one written before every guard is a line of its own, as it holds.
>> yb_n | n < 0 = 0
yb_n | n < 0 = 0

>> yb_(-1) = 3
yb_(-1) = 3

>> yb_n = n
yb_n = n

>> yb_(-1)
0

>> tex ?yb
\mathit{yb}_n = \begin{cases} 0 & \text{if } n < 0 \\ 3 & \text{if } n = -1 \\ n & \text{otherwise} \end{cases}

>> ya_0 = 5
ya_0 = 5

>> ya_n | n < 0 = 0
ya_n | n < 0 = 0

>> ya_n = n
ya_n = n

>> tex ?ya
\mathit{ya}_0 = 5
\mathit{ya}_n = \begin{cases} 0 & \text{if } n < 0 \\ n & \text{otherwise} \end{cases}

# A comparison that is a whole right-hand side keeps its brackets, as in a
# product: 'I_{j,k} = j = k' reads as a chain.
>> I[j<=2, k<=2] = j == k
I[j<=2, k<=2] = j == k

>> tex ?I
I_{j,k} = (j = k), \quad 1 \le j \le 2,\ 1 \le k \le 2

>> pos(x) = x > 0 and x < 1
pos(x) = x > 0 and x < 1

>> tex ?pos
\operatorname{pos}(x) = (x > 0 \land x < 1)

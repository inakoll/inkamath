# A definition as typeset mathematics (DESIGN.md, next in line; MANIFESTO.md,
# Reading). This was the specification, and every entry passes as it was
# written. 'tex ?name' gives what '?name' gives, as LaTeX, so that a
# transcription can be read against the page it came from. It renders what was
# parsed rather than what was typed: the clauses for one index each on a line,
# and those for every index, guarded or not, as one definition in cases.
>> s_0 = 1
s_0 = 1

>> s_n = s_(n-1)/2
s_n = s_(n-1)/2

>> tex ?s
s_0 = 1
s_n = \frac{s_{n-1}}{2}

# A product is a thin space, or a dot before a digit; a subtraction is one
# whatever it was parsed as.
>> f(x, y) = x^2 + 3*y - 1
f(x, y) = x^2 + 3*y - 1

>> tex ?f
f(x, y) = x^2 + 3\,y - 1

>> p = 2*3*x^(n+1)
p = 2*3*x^(n+1)

>> tex ?p
p = 2 \cdot 3\,x^{n + 1}

# A name of one letter is itself, a Greek one its letter, and any other is
# set as one italic word.
>> alpha = 1/10
alpha = 1/10

>> tex ?alpha
\alpha = \frac{1}{10}

>> umax = 3/2
umax = 3/2

>> tex ?umax
\mathit{umax} = \frac{3}{2}

# A definition in cases, as a clamp is written on paper.
>> v_n = 2*n
v_n = 2*n

>> u_n | v_n > umax = umax
u_n | v_n > umax = umax

>> u_n | v_n < -umax = -umax
u_n | v_n < -umax = -umax

>> u_n = v_n
u_n = v_n

>> tex ?u
u_n = \begin{cases} \mathit{umax} & \text{if } v_n > \mathit{umax} \\ -\mathit{umax} & \text{if } v_n < -\mathit{umax} \\ v_n & \text{otherwise} \end{cases}

>> w_0 = 0
w_0 = 0

>> w_n | n > 2 and w_(n-1) <= 1 = 1
w_n | n > 2 and w_(n-1) <= 1 = 1

>> w_n = w_(n-1) + 1/2
w_n = w_(n-1) + 1/2

>> tex ?w
w_0 = 0
w_n = \begin{cases} 1 & \text{if } n > 2 \land w_{n-1} \le 1 \\ w_{n-1} + \frac{1}{2} & \text{otherwise} \end{cases}

# Sums, factorials, limits and floors as a paper writes them.
>> h_n = sum_(k=1)^n 1/k
h_n = sum_(k=1)^n 1/k

>> tex ?h
h_n = \sum_{k=1}^{n} \frac{1}{k}

>> c(n, k) = !n/(!k*!(n-k))
c(n, k) = !n/(!k*!(n-k))

>> tex ?c
c(n, k) = \frac{n!}{k!\,(n - k)!}

>> l = lim s
l = lim s

>> tex ?l
l = \lim_{n \to \infty} s_n

>> gl = lim g.y
gl = lim g.y

>> tex ?gl
\mathit{gl} = \lim_{n \to \infty} g.y_n

>> m(x) = floor(x/2) + (x + 1)^2
m(x) = floor(x/2) + (x + 1)^2

>> tex ?m
m(x) = \lfloor \frac{x}{2} \rfloor + (x + 1)^2

# A function of more than one letter is an operator's name.
>> sq(x) = x^2
sq(x) = x^2

>> r = sq(3)
r = sq(3)

>> tex ?r
r = \operatorname{sq}(3)

# Matrices, a transpose, a cell.
>> A = [1 2; 3 4]
A = [1 2; 3 4]

>> tex ?A
A = \begin{bmatrix} 1 & 2 \\ 3 & 4 \end{bmatrix}

>> q = x'*A*x
q = x'*A*x

>> tex ?q
q = x^\mathsf{T}\,A\,x

>> d = A[1,2]
d = A[1,2]

>> tex ?d
d = A_{1,2}

# 'tex' shows a definition, and is reserved, as 'frac' is.
>> tex 1+1
error: tex shows a definition, as 'tex ?name'

>> tex ?nothing
error: nothing is not defined

>> tex = 3
error: tex is reserved, so it cannot be defined

# What it cannot set yet, it says so rather than printing something else.
>> z = ~(1/3)
z = ~(1/3)

>> tex ?z
error: tex cannot show '~', which has no form on paper

# A model it sets now, its head then its definitions (tex.ink).
>> gain(k = 2, x_n) = { y_n = k*x_n }
gain(k = 2, x_n) = { ... }

>> tex ?gain
\operatorname{gain}(k = 2, x_n):
    y_n = k\,x_n

# A cell of a term shares the term's subscript, as a paper writes x_{n,j}:
# two subscripts in a row are not LaTeX at all. A cell of anything else that
# already carries one is bracketed.
>> nxt_n = s_(n-1)[2]
nxt_n = s_(n-1)[2]

>> tex ?nxt
\mathit{nxt}_n = s_{n-1,2}

>> lead = q[1,1]'[1]
lead = q[1,1]'[1]

>> tex ?lead
\mathit{lead} = (q_{1,1}^\mathsf{T})_{1}

# A sum that ends a product needs no brackets: it reaches to the end, as on
# paper.
>> ga = alpha*sum_(k=1)^3 k
ga = alpha*sum_(k=1)^3 k

>> tex ?ga
\mathit{ga} = \alpha\,\sum_{k=1}^{3} k

# A decimal typed is exact, and set in full however many its digits (C96).
>> c = 1.4426950408889634
c = 1.4426950408889634

>> tex ?c
c = 1.4426950408889634

>> g(x) = x*0.12345678901
g(x) = x*0.12345678901

>> tex ?g
g(x) = x \cdot 0.12345678901

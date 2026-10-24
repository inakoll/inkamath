# A definition written inside an expression binds a LOCAL: a name that lives
# to the end of the line and is invisible outside it. This is what the 2014
# design meant by "l'assignation etant une expression comme une autre", and
# what C29 found had never worked (DESIGN.md, phase 8).

# The binding happens before the rest of the expression reads it, and the
# bound expression is also the value of the binding.
>> (t = 3) + t
6

# It does not survive the line.
>> t
error: t is not defined

# A local is bound where it is written, which is what lets it capture a
# parameter -- the case two lines cannot express, because the second line
# would be a global that cannot see x.
>> f(x) = (t = 2*x) + t
f(x) = (t = 2*x) + t

>> f(5)
20

# Locals chain: one can read another bound earlier in the same expression.
>> g(x) = (u = x*x) + (v = u + 1) + v
g(x) = (u = x*x) + (v = u + 1) + v

>> g(3)
29

# Left to right, so a local is not visible before its own binding.
>> t + (t = 3)
error: t is not defined

# A local shadows a global for the rest of the line, and only for that.
>> a = 1
a = 1

>> (a = 2) + a
4

>> a
1

# But not what its clause binds, a parameter, an index or a cell's place:
# h(1) was 20, an index's local read another term or ran 256 deep, and a
# cell's gave every cell alike (C405).
>> h(x) = (x = 10) + x
error: x is a parameter of h, so the clause cannot bind it again

>> hg(x) | (x = 9) > 10 = 1
error: x is a parameter of hg, so the clause cannot bind it again

>> hs(x)_n = (x = 3) + n
error: x is a parameter of hs, so the clause cannot bind it again

>> hv_n = (n = 3) + 1
error: n is the index of hv_n, so the clause cannot bind it again

>> hx_0 = 0
hx_0 = 0

>> hx_n = (n = 3) + hx_(n-1)
error: n is the index of hx_n, so the clause cannot bind it again

>> hM[j<=2] = (j = 5) + j
error: j is a cell's place of hM, so the clause cannot bind it again

>> hT[s<=2, j<=2, k<=2] = (s = 1) + k
error: s is a cell's place of hT, so the clause cannot bind it again

# An argument given by name binds nothing of the clause's.
>> hd(x) = x
hd(x) = x

>> hc(x) = hd(x = 2) + x
hc(x) = hd(x = 2) + x

>> hc(1)
3

# A model's parameter is the model's, read by its other definitions, so a
# local in one of them shadows it there alone.
>> hm(s = 10, u_n) = {
..     y_n = (s = u_n)*s
..     z_n = s + y_n
.. }
hm(s = 10, u_n) = { ... }

>> hm(u_n = n).z_3
19

# A local in a sequence clause lives for the evaluation of that term.
>> s(x)_0 = 1
s(x)_0 = 1

>> s(x)_n = (step = x^n) + s(x)_(n-1) + step
s(x)_n = (step = x^n) + s(x)_(n-1) + step

>> s(2)_2
13

# So it does in each term a limit walks, or the limit would not be the limit
# of the terms an index gives: this one read the previous term's c (C66).
>> c = 100
c = 100

>> w_0 = 0
w_0 = 0

>> w_n = w_(n-1)/2 + c + 0*(c = 1)
w_n = w_(n-1)/2 + c + 0*(c = 1)

>> lim w
~200

>> w_60
~200

# Parentheses alone do not make a local: a line that is only a definition is
# a definition, however it is written.
>> (b = 7)
(b = 7)

>> b
7

# '?' prints a definition back, and a local is not one -- it is gone by the
# time the next line is read.
>> ?t
error: t is not defined

# A sum's or a product's index, and grad's variable, an outer grad's too in
# the frame an inner one shares, are the construct's own: a local of one of
# their names is refused, as one of i is (C299).
>> sum_(k=1)^3 ((k = 2) + k)
error: k is the sum's index, so a local cannot define it

>> prod_(k=1)^3 ((k = 2) + k)
error: k is the product's index, so a local cannot define it

>> grad_(x = 1) x + sum_(k=1)^3 ((k = 2) + k)
error: k is the sum's index, so a local cannot define it

>> grad_(x = 1) sum_(k=1)^3 x*k*((k = 2) + k)
error: k is the sum's index, so a local cannot define it

>> grad_(x = 2) x*((x = 5) + x)
error: x is grad's variable, so a local cannot define it

>> grad_(x = 1) grad_(y = 2) ((x = 3) + y)
error: x is grad's variable, so a local cannot define it

>> (sum_(k=1)^3 k) + (k = 2) + k
10

# A local is a value. With parameters it would be a function, which cannot
# capture what its line binds (C29) and read a global in its stead, so it is
# refused, with a global of its parameter's name or without (C303).
>> (lf(y) = y + 1) + lf(2)
error: a local cannot take parameters

>> y = 1
y = 1

>> (lf(y) = y + 1) + lf(2)
error: a local cannot take parameters

>> clear y
clear y

>> lfx(x) = (lf(y = 0) = y + x) + lf(1)
lfx(x) = (lf(y = 0) = y + x) + lf(1)

>> lfx(5)
error: a local cannot take parameters

>> grad_(x = 3) ((lf(y) = y*x) + lf(x))
error: a local cannot take parameters

>> grad_(x = 3) x + (lf(y = 1) = y)
error: a local cannot take parameters

# An empty list neither, which adds nothing: it was read as no list at all
# (C314).
>> (lg() = 1) + lg()
error: a local cannot take parameters

>> grad_(x = 1) ((lg() = x) + lg)
error: a local cannot take parameters

# Nor an index, which is a parameter: s_3 read the global n for its index,
# and was 40 with n = 1, or n was not defined (C309).
>> (s_n = n) + s_3
error: a local cannot take an index

>> n = 1
n = 1

>> (s_n = 10*n) + s_3
error: a local cannot take an index

>> grad_(x = 2) ((s_n = n*x) + s_3)
error: a local cannot take an index

>> clear n
clear n

# A local by cells is evaluated in a frame of its own, as a call is, so it
# takes the values its line binds there along: it read neither x nor t,
# "not defined" (C310).
>> h2(x) = (M[j<=2] = j*x) + M
h2(x) = (M[j<=2] = j*x) + M

>> h2(3)
[ 6;
 12]

>> (t = 2) + (M[j<=2] = j*t) + M
[ 6;
 10]

# Each cell is evaluated in a frame of its own, as each term is (C66): a
# later cell read an earlier one's local, so the answer hung on the order of
# the cells, [7; 7] here and refused with the guard at j == 2 (C406).
>> N[j<=2] | j == 1 = (t = 7)
N[j<=2] | j == 1 = (t = 7)

>> N[j<=2] = t
N[j<=2] = t

>> N
error: t is not defined

>> P[j<=2] | j == 2 = (t = 7)
P[j<=2] | j == 2 = (t = 7)

>> P[j<=2] = t
P[j<=2] = t

>> P
error: t is not defined

>> tc_n[j<=2] | j == 1 = (t = n)
tc_n[j<=2] | j == 1 = (t = n)

>> tc_n[j<=2] = t
tc_n[j<=2] = t

>> tc_1
error: t is not defined

>> fc(x)[j<=2] | j == 1 = (t = x)
fc(x)[j<=2] | j == 1 = (t = x)

>> fc(x)[j<=2] = t
fc(x)[j<=2] = t

>> fc(3)
error: t is not defined

>> grad_(x = 1) fc(x)
error: t is not defined

# A cell still reads its parameters, and its own locals.
>> fd(x)[j<=2] = (t = x*j) + t
fd(x)[j<=2] = (t = x*j) + t

>> fd(3)
[ 6;
 12]

>> grad_(x = 3) fd(x)
[2;
 4]

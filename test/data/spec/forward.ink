# The forward form, x_(t+1) = f(x_t, u_t) (DESIGN.md, next in line),
# specified before it is built and never recorded: every value below is
# worked out by hand or with Python's fractions, apart from the interpreter.
# A clause written at its index plus a whole number k is read as its shift,
# t read as t - k in its right side and guard, so every answer is the one
# the shift written by hand gives; what shows the clause shows it as
# written.

# README section 4's sequence, written as a paper writes it. s_20 is 2^-20,
# and lim stops where README's does, at 2^-34.
>> s_0 = 1
s_0 = 1

>> s_(n+1) = s_n/2
s_(n+1) = s_n/2

>> frac s_20
1/1048576

>> s_20
~9.53674316e-07

>> lim s
~5.82076609e-11

>> ?s
s_0 = 1
s_(n+1) = s_n/2

>> tex ?s
s_0 = 1
s_{n+1} = \frac{s_n}{2}

# One clause is shown by a constant index, as ?s_n is refused.
>> ?s_(n+1)
error: n is not defined

# README section 4's factorial: below its lowest base case it is not
# defined, whichever way the recurrence is written.
>> fact_(n+1) = fact_n*(n+1)
fact_(n+1) = fact_n*(n+1)

>> fact_5
error: evaluation nests more than 256 references deep

>> fact_0 = 1
fact_0 = 1

>> fact_5
120

# k = 2, Fibonacci's recurrence as number theory writes it. F_100 by
# Python's integers.
>> F_0 = 0
F_0 = 0

>> F_1 = 1
F_1 = 1

>> F_(n+2) = F_(n+1) + F_n
F_(n+2) = F_(n+1) + F_n

>> F_10
55

>> F_100
354224848179261915075

>> tex ?F
F_0 = 0
F_1 = 1
F_{n+2} = F_{n+1} + F_n

# The index plus a whole number from 1, written as one, and nothing else
# that reads a name nothing defines: one sentence for every other form.
# Behind its index, at it plus 0 or with the number first, a clause says
# what x_t says.
>> y_(n-1) = 1
error: a clause is written at a whole number, as y_0, at its index, as y_n, or ahead of it, as y_(n+1), not at y_(n-1)

>> y_(n+0) = 1
error: a clause is written at a whole number, as y_0, at its index, as y_n, or ahead of it, as y_(n+1), not at y_(n+0)

>> y_(1+n) = 1
error: a clause is written at a whole number, as y_0, at its index, as y_n, or ahead of it, as y_(n+1), not at y_(1+n)

>> y_(2*n) = 1
error: a clause is written at a whole number, as y_0, at its index, as y_n, or ahead of it, as y_(n+1), not at y_(2*n)

>> y_(n+1/2) = 1
error: a clause is written at a whole number, as y_0, at its index, as y_n, or ahead of it, as y_(n+1), not at y_(n+1/2)

>> y_(n+k) = 1
error: a clause is written at a whole number, as y_0, at its index, as y_n, or ahead of it, as y_(n+1), not at y_(n+k)

# The form whatever the name holds, as x_t is the general clause whatever
# t holds. Before, with t = 3, x_(t+1) = 5 was the base clause x_4. It
# holds at every index no base clause takes, 0 among them, as x_t = 5 does.
>> t = 3
t = 3

>> x_(t+1) = 5
x_(t+1) = 5

>> [x_0, x_7]
[5, 5]

# A computed index that reads only what is defined is a base clause, as
# before; the index less a number, plus 0 or after a number, is refused
# whatever the name holds, so that no spelling of x_(t+1) is a base clause.
>> w_(2*t) = 7
w_(2*t) = 7

>> w_6
7

>> w_(t-1) = 2
error: a clause is written at a whole number, as w_0, at its index, as w_t, or ahead of it, as w_(t+1), not at w_(t-1)

>> w_(1+t) = 2
error: a clause is written at a whole number, as w_0, at its index, as w_t, or ahead of it, as w_(t+1), not at w_(1+t)

>> w_(t+0) = 2
error: a clause is written at a whole number, as w_0, at its index, as w_t, or ahead of it, as w_(t+1), not at w_(t+0)

>> clear t
clear t

# Cases: a counter that wraps after 3. c_1 to c_3 count, c_4 wraps, c_5
# counts again.
>> c_0 = 0
c_0 = 0

>> c_(n+1) | c_n >= 3 = 0
c_(n+1) | c_n >= 3 = 0

>> c_(n+1) = c_n + 1
c_(n+1) = c_n + 1

>> [c_3, c_4, c_5]
[3, 0, 1]

>> ?c
c_0 = 0
c_(n+1) | c_n >= 3 = 0
c_(n+1) = c_n + 1

>> tex ?c
c_0 = 0
c_{n+1} = \begin{cases} 0 & \text{if } c_n \ge 3 \\ c_n + 1 & \text{otherwise} \end{cases}

# Written again, a guarded clause replaces itself where it stands, named by
# its left side: c_4 is now 1, and c_5 2.
>> c_(n+1) | c_n >= 3 = 1
c_(n+1) | c_n >= 3 = 1

>> [c_3, c_4, c_5]
[3, 1, 2]

# A guard reading the index reads it as its shift does: gd_(n+1) is 10 where
# n > 3, from gd_5 on, and counts up to gd_4.
>> gd_0 = 0
gd_0 = 0

>> gd_(n+1) | n > 3 = 10
gd_(n+1) | n > 3 = 10

>> gd_(n+1) = gd_n + 1
gd_(n+1) = gd_n + 1

>> [gd_4, gd_5, gd_6]
[4, 10, 10]

# A definition's clauses for every index are written at one offset, so
# that its cases have one left side.
>> c_n | n > 100 = 7
error: c is written c_(n+1), so a clause cannot be written c_n; write 'clear c' first

>> d_n = n
d_n = n

>> d_(n+1) | d_n > 2 = 0
error: d is written d_n, so a clause cannot be written d_(n+1); write 'clear d' first

>> clear d
clear d

>> d_(n+1) | d_n > 2 = 0
d_(n+1) | d_n > 2 = 0

>> ?d
d_(n+1) | d_n > 2 = 0

# The one general clause, written again at another offset, is replaced, as
# any clause written again is.
>> h_(n+1) = h_n + 1
h_(n+1) = h_n + 1

>> h_n = 2*n
h_n = 2*n

>> ?h
h_n = 2*n

>> h_3
6

# A clause reading its own left side is the loop its shift is, named as
# written; one reading its own term after it is a recursion from the tip,
# which a paper writes at its index.
>> r_0 = 1
r_0 = 1

>> r_(n+1) = r_(n+1)/2
error: a loop without a delay: r_(n+1) reads itself

>> r_(n+1) = r_(n+2)/2
error: r_(n+1) reads r_(n+2), a term after it

# Another sequence read at the left side's index is read at the same step:
# the loop is found through it, and a delay breaks it. lp_n = lq_n and
# lq_n = lp_(n-1) + 1, so lp_n = n and lq_3 = lp_2 + 1.
>> lp_(n+1) = lq_(n+1)
lp_(n+1) = lq_(n+1)

>> lq_(n+1) = lp_(n+1) + 1
error: a loop without a delay: lp_(n+1) reads lq_(n+1), which reads lp_(n+1)

>> lq_(n+1) = lp_n + 1
lq_(n+1) = lp_n + 1

>> lp_0 = 0
lp_0 = 0

>> lq_3
3

# Binding the index again would leave the clause reading the bound name a
# step back.
>> b_(n+1) = sum_(n=1)^3 n
error: n is the index of b_(n+1), so the clause cannot bind it again

>> b_(n+1) = grad_(n = 2) n^2
error: n is the index of b_(n+1), so the clause cannot bind it again

>> b_(n+1) = (n = 2) + n
error: n is the index of b_(n+1), so the clause cannot bind it again

# README section 4's Newton's method, x_(k+1) = x_k - F(x_k)/F'(x_k) as a
# text writes it, with parameters and lim. The root of x^3 - 2x + 2 near
# -2 is -1.7692923542386314 by Python's floats; from 0 the walk cycles and
# the fallback answers.
>> cubic(x) = x^3 - 2*x + 2
cubic(x) = x^3 - 2*x + 2

>> nw(x)_0 = x
nw(x)_0 = x

>> nw(x)_(k+1) = nw(x)_k - cubic(nw(x)_k)/(3*nw(x)_k^2 - 2)
nw(x)_(k+1) = nw(x)_k - cubic(nw(x)_k)/(3*nw(x)_k^2 - 2)

>> root(x) | abs(cubic(lim nw(x))) < 1/10^6 = lim nw(x)
root(x) | abs(cubic(lim nw(x))) < 1/10^6 = lim nw(x)

>> root(x) = x
root(x) = x

>> root(-2)
~-1.76929235

>> root(0)
~0

# README section 4's delay line by cells: tap_n[1] = n^2, and each cell
# below it the one above, a step back.
>> tap_0[j<=3] = 0
tap_0[j<=3] = 0

>> tap_(n+1)[j<=3] = tap_n[j-1]
tap_(n+1)[j<=3] = tap_n[j-1]

>> tap_(n+1)[j<=3] | j == 1 = (n+1)^2
tap_(n+1)[j<=3] | j == 1 = (n+1)^2

>> tap_4
[16;
  9;
  4]

>> tex ?tap
\mathit{tap}_{0,j} = 0, \quad 1 \le j \le 3
\mathit{tap}_{n+1,j} = \begin{cases} (n + 1)^2 & \text{if } j = 1 \\ \mathit{tap}_{n,j-1} & \text{otherwise} \end{cases}, \quad 1 \le j \le 3

# A cell's bounds read the index as its shift does: grow_(n+1) has n + 2
# cells, j + grow_n[1] each, so grow_1 is [2; 3] and grow_2 [3; 4; 5].
>> grow_0[j<=1] = 1
grow_0[j<=1] = 1

>> grow_(n+1)[j<=n+2] = j + grow_n[1]
grow_(n+1)[j<=n+2] = j + grow_n[1]

>> grow_2
[3;
 4;
 5]

# Smith's doubling for the Stein equation P = A P A' + I, as
# test/compile/blocks.ink quotes it, A_(k+1) = A_k^2 and P_(k+1) = P_k +
# A_k P_k A_k', at its A for x = 1. P_2 is the sum of A^j (A^j)' for j from
# 0 to 3, by Python's fractions both ways.
>> As_0 = [1/2, 1; 0, 1/4]
As_0 = [1/2, 1; 0, 1/4]

>> Ps_0 = [1, 0; 0, 1]
Ps_0 = [1, 0; 0, 1]

>> As_(k+1) = As_k^2
As_(k+1) = As_k^2

>> Ps_(k+1) = Ps_k + As_k*Ps_k*As_k'
Ps_(k+1) = Ps_k + As_k*Ps_k*As_k'

>> frac Ps_2
[ 789/256,  311/1024;
 311/1024, 4369/4096]

# Nesterov's momentum (Sutskever, Martens, Dahl and Hinton 2013, section
# 2, eqs. (3) and (4)) on f(w) = w'Qw/2 - q'w: the velocity at t + 1 from
# the gradient at the look-ahead point, and the weights at t + 1 reading the
# velocity at t + 1, the same step's term. With mu = 1/2 and eps = 1/4,
# v_1 = [1/4; 1/4], v_2 = [3/16; 9/32] and v_3 = [5/64; 57/256].
>> Q = [2, 0; 0, 1]
Q = [2, 0; 0, 1]

>> q = [1; 1]
q = [1; 1]

>> f(w) = w'*Q*w/2 - q'*w
f(w) = w'*Q*w/2 - q'*w

>> mu = 1/2
mu = 1/2

>> eps = 1/4
eps = 1/4

>> th_0 = [0; 0]
th_0 = [0; 0]

>> v_0 = [0; 0]
v_0 = [0; 0]

>> v_(t+1) = mu*v_t - eps*grad_(w = th_t + mu*v_t) f(w)
v_(t+1) = mu*v_t - eps*grad_(w = th_t + mu*v_t) f(w)

>> th_(t+1) = th_t + v_(t+1)
th_(t+1) = th_t + v_(t+1)

>> frac th_3
[  33/64;
 193/256]

# grad through a forward sequence: pw(a)_3 is a^3, its derivative 3a^2, 12
# at 2.
>> pw(a)_0 = 1
pw(a)_0 = 1

>> pw(a)_(n+1) = pw(a)_n*a
pw(a)_(n+1) = pw(a)_n*a

>> grad_(a = 2) pw(a)_3
12

# README section 5's loop, the plant's explicit Euler step written as the
# method writes it: the plant reads the controller's output at its own
# step, one behind its left side, which is the delay that lets the loop
# start. x_1 = 1, x_2 = 1/2, x_3 = 3/4, as README's.
>> controller(kp = 2, r = 1, y_n) = { u_n = kp*(r - y_n) }
controller(kp = 2, r = 1, y_n) = { ... }

>> plant(dt = 1/2, u_n) = {
..     x_0 = 0
..     x_(n+1) = x_n + dt*(u_n - x_n)
.. }
plant(dt = 1/2, u_n) = { ... }

>> ctl = controller(y_n = plt.x_n)
ctl = controller(y_n = plt.x_n)

>> plt = plant(u_n = ctl.u_n)
plt = plant(u_n = ctl.u_n)

>> frac plt.x_3
3/4

>> ?plant
plant(dt = 1/2, u_n) = {
    x_0 = 0
    x_(n+1) = x_n + dt*(u_n - x_n)
}

>> tex ?plant
\operatorname{plant}(\mathit{dt} = \frac{1}{2}, u_n):
    x_0 = 0
    x_{n+1} = x_n + \mathit{dt}\,(u_n - x_n)

# Several rates: a decimating accumulator, two samples a term. y_3 is the
# sum of x_0 to x_5, 15.
>> acc(x_n) = {
..     y_0 = 0
..     y_(m+1) = y_m + x_(2*m) + x_(2*m+1)
.. }
acc(x_n) = { ... }

>> acc(x_n = n).y_3
15

>> b_(n+1) = acc(x_n = n).y_3
error: n is the index of b_(n+1), so the clause cannot bind it again

# A history is written at the input's index; a forward clause reads it
# where its shift does, c_0 reading x_(-1).
>> held(x_n) = {
..     x_(n+1) | n < -1 = 0
..     c_n = x_(n-1)
.. }
error: x is an input of held, so its history is written at its index, as x_n

>> late(x_n) = {
..     x_(-1) = 0
..     c_(n+1) = x_n
.. }
late(x_n) = { ... }

>> late(x_n = n^2).c_0
0

>> late(x_n = n^2).c_3
4

# README section 5's optional input, the last value seen held: the guard
# and the value read z at the left side's index, the same term. x_4 is
# z_3 = 1, x_6 is z_6 = 2, and with no input x stays 0.
>> kept(z_n?) = {
..     x_0 = 0
..     x_(n+1) | z_(n+1) >= 0 = z_(n+1)
..     x_(n+1) = x_n
.. }
kept(z_n?) = { ... }

>> every3_n | mod(n, 3) == 0 = n/3
every3_n | mod(n, 3) == 0 = n/3

>> [kept(z_n = every3_n).x_4, kept(z_n = every3_n).x_6, kept().x_6]
[1, 2, 0]

# Its static check quotes the read as written, where the shift reads
# z_(n-1).
>> bad(z_n?) = {
..     y_(n+1) | z_(n+1) > 0 = z_n
..     y_(n+1) = 0
.. }
error: y reads z_n, an optional input of bad, in a value whose guard may hold without reading it

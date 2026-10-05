# A model's history of its inputs (DESIGN.md, next in line). A model that
# reads its input before the stream says what the input was there, by
# clauses of its own on the input: terms, and guarded clauses. The
# instance's argument is the input's default clause, so the clauses are
# tried in the order written and the argument after them, as in any
# definition.

>> delay(x_n) = {
..     x_n | n < 0 = 0
..     c_n = x_(n-1)
.. }
delay(x_n) = { ... }

>> ?delay
delay(x_n) = {
    x_n | n < 0 = 0
    c_n = x_(n-1)
}

# Typeset, a history is a clause of the input like any other.
>> tex ?delay
\operatorname{delay}(x_n):
    x_n = \begin{cases} 0 & \text{if } n < 0 \end{cases}
    c_n = x_{n-1}

# Where both apply, the history wins: x_(-1) is 0 where n^2 gives 1.
>> d = delay(x_n = n^2)
d = delay(x_n = n^2)

>> d.c_0
0

>> d.c_3
4

>> d.x_(-1)
0

>> d.x_2
4

# The workaround of an argument that starts at 0 is no longer needed, and
# still answers.
>> s_n | n >= 0 = n^2
s_n | n >= 0 = n^2

>> e = delay(x_n = s_n)
e = delay(x_n = s_n)

>> e.c_0
0

>> e.c_5
16

# With no argument, the history alone, and where it does not hold, nothing.
>> delay().c_0
0

>> delay().c_1
error: delay(...).x_0 is an input, and nothing defines it

# Where the history does not hold, the argument answers, before 0 too, or
# says why it cannot.
>> late(x_n) = {
..     x_n | n < -1 = 0
..     c_n = x_(n-1)
.. }
late(x_n) = { ... }

>> late(x_n = n^2).c_0
1

>> late(x_n = s_n).c_0
error: no clause of s applies

>> late(x_n = s_n).c_(-1)
0

# A term of the history written before its guarded clause beats it.
>> hold(x_n) = {
..     x_(-1) = 5
..     x_n | n < 0 = 0
..     c_n = x_(n-1) + x_(n-2)
.. }
hold(x_n) = { ... }

>> h = hold(x_n = n)
h = hold(x_n = n)

>> h.c_0
5

>> h.c_1
5

>> h.c_2
1

# Written after it, the term is never reached, as at the top level.
>> after(x_n) = {
..     x_n | n < 0 = 0
..     x_(-1) = 5
..     c_n = x_(n-1)
.. }
after(x_n) = { ... }

>> after(x_n = n).c_0
0

# A history of terms alone leaves the input its limit, the argument's (C93).
>> last(x_n) = {
..     x_(-1) = 0
..     l = lim x
.. }
last(x_n) = { ... }

>> last(x_n = 2).l
2

# A history may be any expression in its index.
>> ramp(x_n) = {
..     x_n | n < 0 = n/2
..     c_n = x_(n-2)
.. }
ramp(x_n) = { ... }

>> r = ramp(x_n = 7)
r = ramp(x_n = 7)

>> r.c_0
-1

>> r.c_1
-0.5

>> r.c_2
7

# And any guard: one that holds at 0 and 1 is a clause like any other, and
# beats the argument there. The compiler refuses it.
>> early(x_n) = {
..     x_n | n < 2 = 0
..     c_n = x_(n-1)
.. }
early(x_n) = { ... }

>> early(x_n = n^2).c_2
0

>> early(x_n = n^2).c_3
4

# A matrix input's history is a matrix.
>> swap(x_n) = {
..     x_n | n < 0 = [0; 0]
..     c_n = [0 1; 1 0]*x_(n-1)
.. }
swap(x_n) = { ... }

>> w = swap(x_n = [n; 1])
w = swap(x_n = [n; 1])

>> w.c_0
[0;
 0]

>> w.c_3
[1;
 2]

# At another rate, a slow sequence's term before its first tick reads the
# history through its samples: z_0 is y_(-1), f_(-1), (x_(-1) + x_(-2))/2.
>> warmed(x_n) = {
..     x_n | n < 0 = 1
..     f_n = (x_n + x_(n-1))/2
..     y_m = f_(2*m + 1)
..     z_n = y_(floor((n - 1)/2))
.. }
warmed(x_n) = { ... }

>> lit = warmed(x_n = n^2)
lit = warmed(x_n = n^2)

>> lit.f_0
0.5

>> lit.z_0
1

>> lit.z_3
6.5

# Each input has its own.
>> both(u_n, v_n) = {
..     u_n | n < 0 = 1
..     v_n | n < 0 = 0
..     c_n = u_(n-1) - v_(n-1)
.. }
both(u_n, v_n) = { ... }

>> b = both(u_n = n, v_n = 2*n)
b = both(u_n = n, v_n = 2*n)

>> b.c_0
1

>> b.c_2
-1

# A guard may read another input.
>> mute(u_n, x_n) = {
..     x_n | n < 0 and u_n > 0 = 0
..     c_n = x_(n-1)
.. }
mute(u_n, x_n) = { ... }

>> mute(u_n = 1, x_n = 5).c_0
0

>> mute(u_n = -1, x_n = 5).c_0
5

# Within another model, an input's history is its own; where it has none,
# its argument answers, and there the outer input's history.
>> bare(u_n) = {
..     c_n = u_(n-1)
.. }
bare(u_n) = { ... }

>> wrap(x_n) = {
..     x_n | n < 0 = 3
..     inner = bare(u_n = x_n)
..     c_n = inner.c_n
.. }
wrap(x_n) = { ... }

>> wrap(x_n = n).c_0
3

>> shift(x_n) = {
..     x_n | n < 0 = 5
..     inner = delay(x_n = x_n)
..     c_n = inner.c_n + x_(n-1)
.. }
shift(x_n) = { ... }

>> shift(x_n = n).c_0
5

>> shift(x_n = n).c_2
2

# A clause that always applies would leave the argument nothing; a
# parameter is still not the body's to define.
>> bad(x_n) = {
..     x_n = 0
.. }
error: x is an input of bad, so its body can give it only a history: a term or a guarded clause

>> bad(k = 1) = {
..     k = 2
.. }
error: k is a parameter of bad, so its body cannot define it

# Cases in a paper's order, and 'clear' (DESIGN.md, next in line). Written
# by hand from the rules, never recorded.

# A paper writes the cases first and "otherwise" last. The unguarded clause
# joins the definition as its fallback, tried after the guards, as it does
# written first; it used to start the definition over and drop the guard.
>> clamp(x) | x > 1 = 1
clamp(x) | x > 1 = 1

>> clamp(x) = x
clamp(x) = x

>> clamp(5)
1

>> clamp(1/2)
0.5

>> ?clamp
clamp(x) | x > 1 = 1
clamp(x) = x

>> tex ?clamp
\operatorname{clamp}(x) = \begin{cases} 1 & \text{if } x > 1 \\ x & \text{otherwise} \end{cases}

# Written again, the fallback replaces itself where it stands, as any clause
# does, and the guard stays.
>> clamp(x) = 2*x
clamp(x) = 2*x

>> clamp(5)
1

>> clamp(1/4)
0.5

>> ?clamp
clamp(x) | x > 1 = 1
clamp(x) = 2*x

# In a model's body too: a ReLU in a paper's order. Its slope where it is
# dead is the fallback's, 0, and where it is alive 1; started over, it was
# 0 everywhere and its slope 0 everywhere.
>> unit(w = 2) = {
..     relu(z) | z > 0 = z
..     relu(z) = 0
..     y_n = relu(w*n - 3)
..     d_n = grad_(z = w*n - 3) relu(z)
.. }
unit(w = 2) = { ... }

>> unit().y_1
0

>> unit().y_3
3

>> unit().d_1
0

>> unit().d_3
1

# And in a file used, which is read as the session is.
>> use cases
use cases

>> cases.clamp(5)
1

>> cases.clamp(1/2)
0.5

# A sequence and a definition by cells kept their guards already.
>> s_n | n > 1 = 1
s_n | n > 1 = 1

>> s_n = n
s_n = n

>> s_0
0

>> s_5
1

>> cap(v)[j] | v[j] > 1 = 1
cap(v)[j] | v[j] > 1 = 1

>> cap(v)[j] = v[j]
cap(v)[j] = v[j]

>> cap([3; 0])
[1;
 0]

# A matrix written whole joins its clauses by cells in either order: the
# cells no clause gives are its own.
>> T[j<=2, k<=2] | j == k = 1
T[j<=2, k<=2] | j == k = 1

>> T = [5 6; 7 8]
T = [5 6; 7 8]

>> T
[1, 6;
 7, 1]

# Nothing a name holds is dropped but by 'clear'. A clause that cannot join
# the definition is refused, and the refusal says how to start it anew.
>> m_n = 2*n
m_n = 2*n

>> m = 5
error: m is a sequence, so a clause of it has an index; write 'clear m' first

>> m_3
6

>> clear m
clear m

>> m = 5
m = 5

>> m
5

>> p = 9
p = 9

>> p_0 = 1
error: p is not a sequence, so a clause of it has no index; write 'clear p' first

>> p
9

# Guarded or not: a guarded clause beside one of the other kind made the name
# both, where C70 dropped only an unguarded value.
>> q(x) | x > 0 = 1
q(x) | x > 0 = 1

>> q(x)_n = n
error: q is not a sequence, so a clause of it has no index; write 'clear q' first

>> s | 1 = 5
error: s is a sequence, so a clause of it has an index; write 'clear s' first

# A definition that is one plain clause is replaced by another, parameters
# and all, since nothing else is dropped. Beside other clauses the
# parameters must agree (C51).
>> f(x) = x
f(x) = x

>> f(x, y) = x + y
f(x, y) = x + y

>> f(2, 3)
5

>> f(x, y) | x > y = x
f(x, y) | x > y = x

>> f(x) = x
error: f takes (x, y), so a clause cannot take (x); write 'clear f' first

>> f(3, 2)
3

# A default is part of the parameters, so the clauses agree on it too: one
# call binds it once, and a clause's own default was never read (C154).
>> g(x, k = 2) | x > 0 = k*x
g(x, k = 2) | x > 0 = k*x

>> g(x, k = 3) | x < 0 = k
error: g takes (x, k = 2), so a clause cannot take (x, k = 3); write 'clear g' first

>> g(x, k) | x < 0 = k
error: g takes (x, k = 2), so a clause cannot take (x, k); write 'clear g' first

>> g(x, k = 2) | x < 0 = k
g(x, k = 2) | x < 0 = k

>> g(-1)
2

>> g(-1, 5)
5

>> g(3)
6

>> gd(x, k) | x > 0 = k
gd(x, k) | x > 0 = k

>> gd(x, k = 3) | x < 0 = k
error: gd takes (x, k), so a clause cannot take (x, k = 3); write 'clear gd' first

# A model and a file are each one statement. Written again a model replaces
# itself, and its instances follow; a clause cannot join either, nor either
# replace a name defined otherwise.
>> lp(a = 1/2, u_n) = {
..     v_0 = 0
..     v_n = a*u_n + (1-a)*v_(n-1)
.. }
lp(a = 1/2, u_n) = { ... }

>> lo = lp(u_n = 1)
lo = lp(u_n = 1)

>> frac lo.v_2
3/4

>> lp = 3
error: lp is a model; write 'clear lp' first

>> cases = 2
error: cases is a file; write 'clear cases' first

>> f(k = 1) = { y_n = k }
error: f is already defined; write 'clear f' first

>> lp(a = 1/2, u_n) = { v_n = a*u_n }
lp(a = 1/2, u_n) = { ... }

>> lo.v_2
0.5

>> clear lp
clear lp

>> lo.v_2
error: lo is neither an instance nor a file

# 'use' brings a name in as the session's own, so not over one the session
# has. A clause written after joins the session's, not the file's.
>> use cases (clamp)
error: clamp is already defined; write 'clear clamp' first

>> clear clamp
clear clamp

>> use cases (clamp)
use cases (clamp)

>> clamp(1/2)
0.5

>> clamp(x) | x < 0 = 0
clamp(x) | x < 0 = 0

>> clamp(-3)
0

>> cases.clamp(-3)
-3

# A session's clause on a name of the prelude starts a definition of its own
# (trig), and the clauses after it join that one. Cleared, the name is the
# prelude's again; the prelude's and the built-ins cannot be cleared.
>> max(a, b) | a < b = b
max(a, b) | a < b = b

>> max(a, b) = a
max(a, b) = a

>> max(2, 7)
7

>> max(7, 2)
7

>> clear max
clear max

>> ?max
max(a, b) = a
max(a, b) | a < b = b

>> clear max
error: max comes with the language, so it cannot be cleared

>> clear pi
error: pi comes with the language, so it cannot be cleared

>> clear nothing
error: nothing is not defined

# Whatever reads a name reads what it is now: a memoised answer goes with
# a clause added, and with the name cleared.
>> cl(x) | x > 1 = 1
cl(x) | x > 1 = 1

>> inc(x) = cl(x) + 1
inc(x) = cl(x) + 1

>> inc(5)
2

>> inc(1/2)
error: no clause of cl applies

>> cl(x) = x
cl(x) = x

>> inc(1/2)
1.5

>> inc(5)
2

>> cl(x) | x > 1 = 3
cl(x) | x > 1 = 3

>> inc(5)
4

>> sq(x) = x^2
sq(x) = x^2

>> h(x) = sq(x) + 1
h(x) = sq(x) + 1

>> h(3)
10

>> clear sq
clear sq

>> h(3)
error: sq is not defined

>> ?sq
error: sq is not defined

>> sq(x) = x^3
sq(x) = x^3

>> h(3)
28

>> abs(x) = 7
abs(x) = 7

>> ab(x) = abs(x)
ab(x) = abs(x)

>> ab(-2)
7

>> clear abs
clear abs

>> ab(-2)
2

# 'clear' clears a whole definition: the language removes no single clause.
# It is a word at the start of a line, as 'use' is, and not reserved.
>> clear s_0
error: clear clears a whole definition, as 'clear s'

>> clear = 2
clear = 2

>> clear
2

>> clear clear
clear clear

>> clear
error: clear is not defined

# A model's body holds definitions only.
>> mm(k = 1) = {
..     y_n = k
..     clear y
.. }
error: a model's body holds definitions, not 'clear y'

# So a refusal there does not advise 'clear'. A model in it, as at the
# prompt, replaces a model only, and no clause joins one.
>> mb(k = 1) = {
..     y_n = k
..     y = 2
.. }
mb(k = 1) = { ... }

>> mb().y_1
error: y is a sequence, so a clause of it has an index

>> mn(k = 1) = {
..     y = k
..     y(a = 1) = { z = a }
.. }
mn(k = 1) = { ... }

>> mn().y
error: y is already defined

>> mo(k = 1) = {
..     y(a = 1) = { z = a }
..     y = k
.. }
mo(k = 1) = { ... }

>> mo().y
error: y is a model

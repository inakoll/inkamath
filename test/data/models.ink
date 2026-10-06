# Models, files and their scopes (DESIGN.md, phase 15). A model is
# defined as a function is, its value a group of definitions in braces, one a
# line; its signature is its interface: parameters with their defaults, and
# inputs, which have none. At the prompt an open brace continues the line, as
# an open bracket does, and the lines keep their breaks.

>> gain(k = 2, b = 1, x_n) = {
..     y_n = k*x_n + b
..     s_0 = 0
..     s_n = s_(n-1) + y_n
.. }
gain(k = 2, b = 1, x_n) = { ... }

>> ?gain
gain(k = 2, b = 1, x_n) = {
    y_n = k*x_n + b
    s_0 = 0
    s_n = s_(n-1) + y_n
}

# An instance is a definition, the model applied to arguments, and the
# session reads its names qualified: its terms, its parameters and its
# inputs alike.
>> g = gain(x_n = n)
g = gain(x_n = n)

>> g.y_3
7

>> g.s_3
15

>> g.k
2

>> g.x_2
2

>> g.z
error: g.z is not defined

>> g
error: g is an instance of gain; read one of its names (g.y_0)

>> gain
error: gain is a model; define an instance of it (g = gain(...))

# A second instance is a second definition, and neither sees the other.
>> h = gain(k = 3, x_n = n)
h = gain(k = 3, x_n = n)

>> h.y_3
10

>> g.y_3
7

# Arguments are given as a function's are, by position or by name; an input
# given by position is its general term.
>> gain(3, 0, n).y_3
9

>> gain(z = 1)
error: gain has no parameter z

>> gain(y_n = 1)
error: gain has no parameter y

>> gain(1, 2, n, 4)
error: gain expects 3 arguments, got 4

# An input left unsupplied is allowed -- compiled, it is an argument of the
# step -- and says so when read.
>> e = gain()
e = gain()

>> e.y_1
error: e.x_1 is an input, and nothing defines it

# A model's body reads its parameters, its inputs and its own names, then
# those of the scope it is written in: its file's, or, at the prompt, the
# session's. A parameter hides the session's name: the session's k is not
# the model's, while the z below is the session's.
>> k = 10
k = 10

>> g.y_3
7

>> leak(x) = { y = x + z }
leak(x) = { ... }

>> z = 1
z = 1

>> l = leak(1)
l = leak(1)

>> l.y
2

# An argument is a clause of the instance written in the session: it reads
# the session's names, and follows them, as any definition does.
>> step = 1/2
step = 1/2

>> w = gain(x_n = step*n)
w = gain(x_n = step*n)

>> w.y_2
3

>> step = 1
step = 1

>> w.y_2
5

# The session does not define an instance's names from outside: the instance
# is changed where it is defined.
>> g.k = 3
error: g is defined by gain(x_n = n); define g again to change it

# 'use' names a file by its stem, beside the file that names it, and makes
# its names reachable qualified. The file is a scope as a model is: its
# definitions read its own names, never the session's.
>> use filters
use filters

>> filters.a0
0.1

>> a0
error: a0 is not defined

>> lowpass
error: lowpass is not defined

>> fast = filters.lowpass(a = 1/2, u_n = 1)
fast = filters.lowpass(a = 1/2, u_n = 1)

>> slow = filters.lowpass(a = 1/4, u_n = fast.v_n)
slow = filters.lowpass(a = 1/4, u_n = fast.v_n)

>> frac fast.v_3
7/8

>> frac slow.v_3
55/128

# A limit is taken of a sequence read qualified, through an instance, a model
# applied or a file (C141).
>> lim slow.v
~1

>> lim filters.highpass(a = 1/2, u_n = 1).low.v
~1

>> a0 = 5
a0 = 5

>> frac filters.lowpass(u_n = 1).v_1
1/10

>> filters.a0 = 1/2
error: filters.a0 is defined in filters.ink, and only there

# A model instantiates another as the session does, named or not; a named one
# is read through the instance that holds it.
>> frac filters.highpass(a = 1/2, u_n = 1).v_3
1/8

>> frac filters.highpass(a = 1/2, u_n = 1).low.v_3
7/8

# Names brought in unqualified are those listed, and no others.
>> use filters (lowpass)
use filters (lowpass)

>> frac lowpass(a = 1/2, u_n = 1).v_3
7/8

>> highpass
error: highpass is not defined

# A file that cannot be read or parsed loads nothing, and says where.
>> use bad
error: bad.ink, line 3: missing ')' after '2'

>> bad.fine
error: bad is not defined

>> use nowhere
error: cannot read nowhere.ink

# A closed loop is two instances reading each other's terms. Neither can be
# written with its argument already defined, and neither need be: a
# definition evaluates nothing, so the order does not matter.
#   u_n = kp*(r - y_n), and the plant's x_n = x_(n-1) + dt*(u_(n-1) - x_(n-1))
#   x: 0, 1, 1/2, 3/4    u: 2, 0, 1, 1/2
>> controller(kp = 2, r = 1, y_n) = { u_n = kp*(r - y_n) }
controller(kp = 2, r = 1, y_n) = { ... }

>> plant(dt = 1/2, u_n) = {
..     x_0 = 0
..     x_n = x_(n-1) + dt*(u_(n-1) - x_(n-1))
.. }
plant(dt = 1/2, u_n) = { ... }

>> c = controller(y_n = p.x_n)
c = controller(y_n = p.x_n)

>> p = plant(u_n = c.u_n)
p = plant(u_n = c.u_n)

>> frac p.x_3
3/4

>> c.u_3
0.5

# Without a delay a loop has no term to start from, each reading the other
# at the same index. The definition that closes one is refused, naming the
# loop, rather than accepted and found only when a term is read.
>> left = gain(x_n = right.y_n)
left = gain(x_n = right.y_n)

>> right = gain(x_n = left.y_n)
error: a loop without a delay: left.y_n reads left.x_n, which reads right.y_n, which reads right.x_n, which reads left.y_n

>> right
error: right is not defined

# An instance reads its model when it is read, as a definition reads the
# functions it calls: redefined, the model changes its instances.
>> gain(k = 2, b = 1, x_n) = { y_n = k*x_n - b }
gain(k = 2, b = 1, x_n) = { ... }

>> g.y_3
5

>> g.s_3
error: g.s is not defined

# The prompt is to a model written there what a file is to its own: it reads
# the session's models and functions, itself included.
>> twice(t) = 2*t
twice(t) = 2*t

>> smooth(a = 1/2, u_n) = {
..     v_0 = 0
..     v_n = a*u_n + (1-a)*v_(n-1)
.. }
smooth(a = 1/2, u_n) = { ... }

>> sharp(a = 1/2, u_n) = { v_n = twice(u_n) - smooth(a, u_n).v_n }
sharp(a = 1/2, u_n) = { ... }

>> frac sharp(u_n = 1).v_2
5/4

>> tree(d) = {
..     v = 2*tree(d - 1).v + 1
..     v | d == 0 = 1
.. }
tree(d) = { ... }

>> tree(5).v
63

# An instance in a term's cells is one per cell, each with its memory: a
# bank of filters, one to a row.
>> bank_n[j<=3, k<=1] = smooth(a = j/4, u_n = 1).v_n
bank_n[j<=3, k<=1] = smooth(a = j/4, u_n = 1).v_n

>> frac bank_2
[ 7/16;
   3/4;
 15/16]

# The prelude, built into the interpreter, is included bare beneath the
# session, as the built-ins are: no scope and no qualified name, its names
# unqualified from every scope, its text read where it was written.
#   ceil(x) = -floor(-x)
#   mod(a, b) = a - b*floor(a/b)
>> ?ceil
ceil(x) = -floor(-x)

>> ceil(7/2)
4

>> ceil(-7/2)
-3

>> mod(7, 3)
1

>> mod(-7, 3)
2

>> mod(7, -3)
-2

>> prelude.ceil
error: prelude is not defined

>> wrap(m = 4, x_n) = { y_n = mod(x_n, m) }
wrap(m = 4, x_n) = { ... }

>> q = wrap(x_n = n)
q = wrap(x_n = n)

>> q.y_5
1

# Replaced in the session, a name of the prelude or a built-in is replaced for
# the session only: the prelude's own floor is the built-in. A model written
# at the prompt reads the session, and so its mod.
>> floor = 3
floor = 3

>> ceil(7/2)
4

>> mod(a, b) = 0
mod(a, b) = 0

>> mod(7, 3)
0

>> q.y_5
0

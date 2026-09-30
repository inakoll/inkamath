# Models, files and their scopes (MODERNIZATION.md, phase 15). A model is
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

# A model's body reads its parameters, its inputs and its own names, then its
# file's, never the session's: the session's k is not the model's, and the
# z below is sought inside l, where nothing defines it.
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
error: l.z is not defined

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

# An instance reads its model when it is read, as a definition reads the
# functions it calls: redefined, the model changes its instances.
>> gain(k = 2, b = 1, x_n) = { y_n = k*x_n - b }
gain(k = 2, b = 1, x_n) = { ... }

>> g.y_3
5

>> g.s_3
error: g.s is not defined

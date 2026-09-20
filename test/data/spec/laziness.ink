# SPECIFICATION -- see locals.ink.
#
# A parameter binds an expression, like every other name in the language. The
# 2014 README lists lazy evaluation first among the project's features;
# parameters have bound values since the beginning, which is the opposite.
#
# Whether an argument is evaluated once or on every read is NOT specified
# here, because it is not observable: nothing a body can do changes the
# caller's scope while the call is running, since a definition inside the
# body binds a local in the body's own frame. Call-by-name and call-by-need
# differ only in cost -- see MODERNIZATION.md, phase 8.

# An argument the body never reads is never evaluated.
>> f(x) = 1
f(x) = 1

>> f(undefined)
1

>> g(a, b) = a
g(a, b) = a

>> g(7, undefined + 1)
7

# One the body does read is evaluated, and its errors are reported.
>> h(x) = x + 1
h(x) = x + 1

>> h(undefined)
error: undefined is not defined

# The argument expression belongs to the caller: it resolves in the scope it
# was written in, not in the body that reads it.
>> x = 99
x = 99

>> inner(x) = x * 10
inner(x) = x * 10

>> outer(y) = inner(y + 1)
outer(y) = inner(y + 1)

>> outer(1)
20

# A default is an expression too, evaluated only when the call leaves its
# parameter empty, and in the definition's own scope.
>> k(a, b = 2*a) = b
k(a, b = 2*a) = b

>> k(5)
10

>> k(5, 1)
1

# A default for a parameter the body never reads is never evaluated either.
# C30 stopped evaluating an overridden default; this goes further.
>> m(a, b = undefined) = a
m(a, b = undefined) = a

>> m(1)
1

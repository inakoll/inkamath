# SPECIFICATION -- the language we are building, not the language we have.
# These fail until phase 4 lands. Do not record them from current behaviour.
#
# A definition is a statement. It binds a name and echoes what it bound.
# It evaluates nothing: no left-hand-side lookup, no right-hand-side
# evaluation, no convergence loop triggered by typing a definition.

>> a = 1
a = 1

>> a
1

# Names bind expressions, not values. This is the idea the project exists for
# and it does not change.
>> b = a+a
b = a+a

>> b
2

>> a = 2
a = 2

>> b
4

# Parameters are lexically scoped. A parameter name is bound by the
# definition and is not visible outside it.
>> f(x, y) = x^2+y
f(x, y) = x^2+y

>> f(2, 1)
5

>> f(x=3, y=2)
11

# An argument that was not supplied is an error, not a search of the
# enclosing scope for a name that happens to match.
>> x = 5
x = 5

>> f(1)
error: f expects 2 arguments, got 1

>> f
error: f expects 2 arguments, got 0

# ... and surplus arguments are an error too, rather than being dropped.
>> g = 1+2
g = 1+2

>> g(7)
error: g takes no arguments

# A later definition replaces the earlier one outright.
>> g = 10
g = 10

>> g
10

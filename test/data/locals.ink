# A definition written inside an expression binds a LOCAL: a name that lives
# to the end of the line and is invisible outside it. This is what the 2014
# design meant by "l'assignation etant une expression comme une autre", and
# what C29 found had never worked (MODERNIZATION.md, phase 8).

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

# ... and shadows a parameter the same way, because a call's frame is one
# scope and a local is bound in it.
>> h(x) = (x = 10) + x
h(x) = (x = 10) + x

>> h(1)
20

# A local in a sequence clause lives for the evaluation of that term.
>> s(x)_0 = 1
s(x)_0 = 1

>> s(x)_n = (step = x^n) + s(x)_(n-1) + step
s(x)_n = (step = x^n) + s(x)_(n-1) + step

>> s(2)_2
13

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


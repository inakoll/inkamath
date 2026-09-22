# '?name' prints a definition back as it was written, without evaluating it
# (MODERNIZATION.md, phase 4 item 6). This is what makes the core idea
# visible: a name is bound to an expression, not to the number that
# expression last produced.

>> a = 1
a = 1

>> b = a+a
b = a+a

>> a = 2
a = 2

>> ?b
b = a+a

>> b
4

# It quotes what was typed, spacing included, rather than a rendering of the
# parsed expression.
>> f(x,y)   =   x^2+y
f(x,y)   =   x^2+y

>> ?f
f(x,y)   =   x^2+y

# A sequence is one definition, so '?' shows all of its clauses at once --
# the thing three parallel slots used to make impossible (MODERNIZATION.md,
# C11). They print in the order they were written.
>> s_0 = 1
s_0 = 1

>> s_n = s_(n-1)/2
s_n = s_(n-1)/2

>> ?s
s_0 = 1
s_n = s_(n-1)/2

>> u_n = 2*n
u_n = 2*n

>> u_3 = 100
u_3 = 100

>> ?u
u_n = 2*n
u_3 = 100

# One clause on its own.
>> ?u_3
u_3 = 100

>> ?u_5
error: u has no clause for index 5

# '?' takes a name, because a definition is what it prints.
>> ?undefined
error: undefined is not defined

>> ?
error: expected a name after '?'

>> ?3
error: expected a name after '?', not '3'

>> ?f(2)
error: '?' takes a name, not a call


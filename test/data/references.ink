# References: identifiers name expressions, not values (README.md section 4.1).

>> a = 1
a = 1

>> b = a+a
b = a+a

>> a = 2
a = 2

>> b
4

# Parameters, positional and named (README.md section 4.1).
>> f(x, y)=x^2+y
f(x, y)=x^2+y

>> f(2, 1)
5

>> f(x=3, y=2)
11

# Surplus arguments and missing ones are both errors. Missing ones used to
# fall back to the calling scope and then to zero, reporting nothing.
>> g=1+2
g=1+2

>> g
3

>> g(1)
error: g takes no arguments

# Scoping (README.md section 6).
>> h(x)=x^2
h(x)=x^2

>> x
0

>> h(2)
4

>> h(x=4)
16

>> x
0

>> x=5
x=5

>> h
error: h expects 1 argument, got 0

# One definition per name. An indexed clause extends a sequence; a plain
# definition replaces whatever the name held. The two used to coexist, with
# an undocumented precedence that made a plain definition unreachable
# (MODERNIZATION.md, C11).
>> m_n=2*n
m_n=2*n

>> m_3
6

>> m=5
m=5

>> m
5

>> m_3
5

# The reverse: a clause turns a plain definition into a sequence.
>> p=9
p=9

>> p_0=1
p_0=1

>> p_0
1

>> p
error: p is a sequence; index it, as in p_0

>> p_5
error: p has no clause for index 5


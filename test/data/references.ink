# References: identifiers name expressions, not values (README.md section 3).

>> a = 1
a = 1

>> b = a+a
b = a+a

>> a = 2
a = 2

>> b
4

# Parameters, positional and named (README.md section 3).
>> f(x, y)=x^2+y
f(x, y)=x^2+y

>> f(2, 1)
5

>> f(x=3, y=2)
11

>> f(1)
error: f expects 2 arguments, got 1

# A keyword argument is checked against the parameter names, not just counted.
# An unknown one used to satisfy the count, leave the real parameter unbound,
# and let it fall through to a global (MODERNIZATION.md, C25).
>> f(z=1, y=2)
error: f has no parameter z

>> f(1, x=2)
error: f got two values for x

>> k(a, b=5)=a*100+b
k(a, b=5)=a*100+b

>> k(1)
105

>> k(1, 2)
102

# A keyword can fill the optional parameter and leave the required one with
# nothing, which the count alone does not catch.
>> k(b=2)
error: k has no value for a

# A default is evaluated only when the call leaves its parameter empty. Every
# default used to be evaluated on every call, so this reported 'zzz is not
# defined' for a value it never needed (MODERNIZATION.md, C30).
>> n(a, b=zzz)=a
n(a, b=zzz)=a

>> n(1, 2)
1

>> n(1)
error: zzz is not defined

# An argument is evaluated at the call site too, whether or not the body reads
# it. Lazy parameters would leave this one alone; they were specified,
# measured and deferred (MODERNIZATION.md, Deferred).
>> u(x)=1
u(x)=1

>> u(zzz)
error: zzz is not defined

# And it is evaluated in the definition's scope, so it can refer to the
# definition's other parameters. It used to be evaluated in the caller's, so
# this answered 100 -- the global x, not the argument.
>> outer=50
outer=50

>> o(outer, y=2*outer)=y
o(outer, y=2*outer)=y

>> o(5)
10

# Surplus arguments and missing ones are both errors. Missing ones used to
# fall back to the calling scope and then to zero, reporting nothing.
>> g=1+2
g=1+2

>> g
3

>> g(1)
error: g takes no arguments

# Parameters are bound by the definition and are not visible outside it. An
# undefined name is a diagnostic; it used to evaluate to zero in silence
# (MODERNIZATION.md, C13).
>> h(x)=x^2
h(x)=x^2

>> x
error: x is not defined

>> h(2)
4

>> h(x=4)
16

>> x
error: x is not defined

>> x=5
x=5

>> h
error: h expects 1 argument, got 0

# A definition sees the global scope and its own parameters, never the
# caller's. 'q' means the global y here, whoever is on the stack; it used to
# mean whatever the innermost active call happened to have bound
# (MODERNIZATION.md, phase 4 item 5).
>> q=y+1
q=y+1

>> r(y)=q
r(y)=q

>> r(2)
error: y is not defined

>> y=10
y=10

>> r(2)
11

# A memoised answer must not become a stale one: a definition the user
# changes drops what was remembered (MODERNIZATION.md, phase 9).
>> y=20
y=20

>> r(2)
21

# A definition written inside an expression is a local, and a local is not a
# global: the answer it gives must not be remembered as the global's. The
# memoised answer used to be keyed on the name alone, so 'cc_0' kept the
# local's 99 after the line that bound it had ended, and a local even reached
# a callee that lexical scoping keeps it out of (MODERNIZATION.md, C49).
>> cc_0 = 1
cc_0 = 1

>> (cc_0 = 99)+0
99

>> cc_0
1

>> cc_0
1

>> (cc_0 = 99)+0
99

# The other way round: a remembered global must not answer for a local that
# has just been bound.
>> dd_0 = 1
dd_0 = 1

>> dd_0
1

>> (dd_0 = 99)+0
99

# And a callee sees the global, whatever the caller bound on its own line.
>> ee(x) = x+1
ee(x) = x+1

>> ff(n) = ee(n)
ff(n) = ee(n)

>> (ee(x) = x+100)*0 + ff(2)
3

# Two arguments with the same cells and different shapes are two questions.
# The key held the cells and not the extent, so whichever shape was asked
# first answered for both (MODERNIZATION.md, C50).
>> gg(m) = m+m
gg(m) = m+m

>> gg([1 2;3 4])
[2, 4;
 6, 8]

>> gg([1 2 3 4])
[2, 4, 6, 8]

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

# Indexing what is not a sequence is an error. This used to answer 5 -- the
# plain definition was returned without the index ever being looked at
# (MODERNIZATION.md, C26).
>> m_3
error: m is not a sequence

>> ?m_3
error: m is not a sequence

# The reverse: a clause turns a plain definition into a sequence.
>> p=9
p=9

>> p_0=1
p_0=1

>> p_0
1

>> p
error: p is a sequence; index it (p_0)

>> p_5
error: p has no clause for index 5


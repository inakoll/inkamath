# Indexed references and general terms (README.md sections 4.2 and 4.3).

>> f_0=1+2
f_0=1+2

>> f_1=3+4
f_1=3+4

>> f_0
3

>> f_1
7

# A general term applies to every index that has no singular definition.
>> u_n=2*n
u_n=2*n

>> u_1
2

>> u_2
4

>> u_0=2
u_0=2

>> u_1=3
u_1=3

>> u_0
2

>> u_1
3

>> u_2
4

>> u_3
6

# Series: the interpreter iterates the general term until it converges.
>> exp(x)_n=exp(x)_(n-1)+x^n/!n
exp(x)_n=exp(x)_(n-1)+x^n/!n

>> exp(1)
2.71828183

>> exp(1)-e
-7.69606601e-13

>> cos(x)=(exp(i*x)+exp(-i*x))/2
cos(x)=(exp(i*x)+exp(-i*x))/2

>> cos(pi/3)
0.5

>> sin(x)=(exp(i*x)-exp(-i*x))/(2*i)
sin(x)=(exp(i*x)-exp(-i*x))/(2*i)

>> sin(pi/6)
0.5

# A recurrence written the textbook way -- an initial value plus a general
# term -- then asked for its limit (MODERNIZATION.md C12).
>> s_0=1
s_0=1

>> s_n=s_(n-1)/2
s_n=s_(n-1)/2

>> s_20
9.53674316e-07

>> s
9.31322575e-10

# Recursion is bounded: past the budget the interpreter says so rather than
# dying (MODERNIZATION.md, C1).
>> g_0=1
g_0=1

>> g_n=g_(n-1)+1
g_n=g_(n-1)+1

>> g_10
11

>> g_500
error: evaluation nests more than 256 references deep

# A base case at a negative index keeps its place in the ordering. These
# clauses used to be keyed by size_t, so k_(-1) sorted above k_0 and the
# iteration started from a wrapped index (MODERNIZATION.md, C17).
>> k_(-1)=0
k_(-1)=0

>> k_0=5
k_0=5

>> k_n=k_(n-1)+1
k_n=k_(n-1)+1

>> k_1
6

>> k
35


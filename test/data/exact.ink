# Exact numbers (MODERNIZATION.md, phase 13). This was the specification,
# written before they existed (CLAUDE.md, section 3), and every exact value was
# checked against Python's Fraction. It was written for a display of
# fractions; the outputs moved with the decimals that replaced it
# (decimals.ink), and where the display no longer shows the kind, 'frac' does.
#
# A literal is exact, and stays exact through + - * / and whole powers. What
# can only be approached -- pi, e, i, a root, a limit, a sum without an upper
# bound -- is inexact, as is anything written after '~', and anything an
# inexact number touches.

# The promise, and what it costs the README: a quotient is exact.
>> 1/3+1/3+1/3
1

>> 10/4
2.5

>> 6/3
2

>> 1/-2
-0.5

# A whole number prints in full, where a double printed nine digits of it.
>> 2^40
1099511627776

>> !20
2432902008176640000

# A whole power is exact whichever its sign.
>> 2^-3
0.125

>> (2/3)^-2
2.25

# A tenth is exact however it is written. Before this, both answered 0.
>> 1/10*3 == 3/10
1

>> 0.1*3 == 0.3
1

>> 1/2 == 0.5
1

# '~' makes a number inexact, and an inexact number makes everything it
# touches inexact.
>> 2.5
2.5

>> 1e3
1000

>> frac 5/2 + ~0.5
error: 3 is approximate, so it has no exact fraction

# What can only be approached.
>> pi
~3.14159265

>> 2^(1/2)
~1.41421356

>> 4^(1/2)
2

>> 2^(1/2)*2^(1/2)
~2

# A complex number is inexact, even when it happens to be real and whole.
>> frac i*i
error: -1 is approximate, so it has no exact fraction

# A trailing zero is not a digit the literal needs, however many there are:
# these went inexact once the digits passed 64 bits.
>> frac 1.00000000000000000000
1

>> frac 12345678901234567890e-10
1234567890123456789/1000000000

>> frac 0e99999999999999999999
0

# '~' gives the double nearest the exact value. Dividing the numerator by the
# denominator as doubles rounds twice once either has more than 53 bits, and
# this printed ~9.2339442047127047.
>> digits = 17
digits = 17

>> ~9.233944204712703
~9.2339442047127029

>> digits = 9
digits = 9

# An exact zero cannot be divided by. Today these answer a complex infinity
# and a NaN (C31).
>> 1/0
error: division by zero

>> 0/0
error: division by zero

>> 0^-1
error: division by zero

# Exact until it no longer fits in 64 bits, then inexact rather than wrong: a
# 64-bit fraction wrapped to a negative number here. A bignum moves the point
# where exactness ends from 2^63 to never (phase 13, step 2).
>> 2^62
4611686018427387904

>> 2^63
~9.22337204e+18

>> !21
~5.10909422e+19

>> 9223372036854775808
~9.22337204e+18

# A binomial coefficient through a factorial that does not fit: the right
# number, and inexact.
>> !52/(!5*!47)
2598960

# Sequences stay exact for as long as their terms fit.
>> s_0 = 1
s_0 = 1

>> s_n = s_(n-1)/3
s_n = s_(n-1)/3

>> s_5
~0.00411522634

>> s_(4/2)
~0.111111111

>> s_(3/2)
error: an index must be a whole number, not 1.5

>> fib_0 = 0
fib_0 = 0

>> fib_1 = 1
fib_1 = 1

>> fib_n = fib_(n-1) + fib_(n-2)
fib_n = fib_(n-1) + fib_(n-2)

>> fib_92
7540113804746346429

>> fib_93
~1.22001604e+19

>> h_1 = 1
h_1 = 1

>> h_n = h_(n-1) + 1/n
h_n = h_(n-1) + 1/n

>> h_30
~3.99498713

>> h_50
~4.49920534

# A finite sum of exact terms is exact; the limit of the same terms is not.
>> sum_(k=1)^10 1/k
~2.92896825

>> ex(x)_n = sum_(k=0)^n x^k/!k
ex(x)_n = sum_(k=0)^n x^k/!k

>> ex(1)_10
~2.7182818

>> lim ex(1)
~2.71828183

# A limit is inexact even when every partial sum is exact and the limit
# itself happens to be whole: lim approaches, it does not reach. The same
# holds for a sequence that stops moving.
>> sum_(k=0) 1/2^k
~2

>> lt_0 = 0
lt_0 = 0

>> lt_n | n > 2 = 5
lt_n | n > 2 = 5

>> lt_n = n
lt_n = n

>> frac lim lt
error: 5 is approximate, so it has no exact fraction

# Every cell has its own kind.
>> a = [1 2;3 4]
a = [1 2;3 4]

>> a/3
[~0.333333333, ~0.666666667;
            1,  ~1.33333333]

>> [1/10 ~0.1]*3
[0.3, ~0.3]

# An inverse with thirds in it multiplies back to the identity exactly. The
# commas are needed: '2/3 -1/3' is one cell, a subtraction (C52).
>> [2 1;1 2]*[2/3, -1/3; -1/3, 2/3]
[1, 0;
 0, 1]

# An exact argument and an inexact one are different arguments, however equal:
# a memo key that dropped the kind would answer the second from the first
# (as C50's dropped the shape).
>> dbl(x) = x*2
dbl(x) = x*2

>> frac dbl(1/2)
1

>> frac dbl(~0.5)
error: 1 is approximate, so it has no exact fraction


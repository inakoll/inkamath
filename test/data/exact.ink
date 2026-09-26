# Exact numbers (MODERNIZATION.md, phase 13). This was the specification,
# written before they existed (CLAUDE.md, section 3); every expected output is
# as it was specified, and every exact one was checked against Python's
# Fraction.
#
# A number written as a whole number is exact, and stays exact through + - * /
# and whole powers. What can only be approached -- pi, e, i, a root, a limit,
# a sum without an upper bound -- is inexact, as is a literal with a point or
# an exponent, and anything an inexact number touches.
#
# An exact number prints as the literal that makes it (C52): a whole number in
# full, anything else as a reduced fraction. An inexact one prints as it does
# today, with a trailing point wherever it would otherwise read as whole.

# The promise, and what it costs the README: a quotient is a fraction.
>> 1/3+1/3+1/3
1

>> 10/4
5/2

>> 6/3
2

>> 1/-2
-1/2

# A whole number prints in full, where a double printed nine digits of it.
>> 2^40
1099511627776

>> !20
2432902008176640000

# A whole power is exact whichever its sign.
>> 2^-3
1/8

>> (2/3)^-2
9/4

# A tenth is exact only when it is written as one. Today both answer 0.
>> 1/10*3 == 3/10
1

>> 0.1*3 == 0.3
0

>> 1/2 == 0.5
1

# A point or an exponent makes a literal inexact, and an inexact number makes
# everything it touches inexact. The trailing point is how a whole inexact
# number says so.
>> 2.5
2.5

>> 1e3
1000.

>> 5/2 + 0.5
3.

# What can only be approached.
>> pi
3.14159265

>> 2^(1/2)
1.41421356

>> 4^(1/2)
2.

>> 2^(1/2)*2^(1/2)
2.

# A complex number is inexact, and says so when it happens to be real and
# whole.
>> i*i
-1.

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
9.22337204e+18

>> !21
5.10909422e+19

>> 9223372036854775808
9.22337204e+18

# A binomial coefficient through a factorial that does not fit: the right
# number, visibly inexact.
>> !52/(!5*!47)
2598960.

# Sequences stay exact for as long as their terms fit.
>> s_0 = 1
s_0 = 1

>> s_n = s_(n-1)/3
s_n = s_(n-1)/3

>> s_5
1/243

>> s_(4/2)
1/9

>> s_(3/2)
error: an index must be a whole number, not 3/2

>> fib_0 = 0
fib_0 = 0

>> fib_1 = 1
fib_1 = 1

>> fib_n = fib_(n-1) + fib_(n-2)
fib_n = fib_(n-1) + fib_(n-2)

>> fib_92
7540113804746346429

>> fib_93
1.22001604e+19

>> h_1 = 1
h_1 = 1

>> h_n = h_(n-1) + 1/n
h_n = h_(n-1) + 1/n

>> h_30
9304682830147/2329089562800

>> h_50
4.49920534

# A finite sum of exact terms is exact; the limit of the same terms is not.
>> sum_(k=1)^10 1/k
7381/2520

>> ex(x)_n = sum_(k=0)^n x^k/!k
ex(x)_n = sum_(k=0)^n x^k/!k

>> ex(1)_10
9864101/3628800

>> lim ex(1)
2.71828183

# A limit is inexact even when every partial sum is exact and the limit
# itself happens to be whole: lim approaches, it does not reach. The same
# holds for a sequence that stops moving.
>> sum_(k=0) 1/2^k
2.

>> lt_0 = 0
lt_0 = 0

>> lt_n | n > 2 = 5
lt_n | n > 2 = 5

>> lt_n = n
lt_n = n

>> lim lt
5.

# Every cell has its own kind.
>> a = [1 2;3 4]
a = [1 2;3 4]

>> a/3
[1/3, 2/3;
   1, 4/3]

>> [1/2 0.5]*2
[1, 1.]

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

>> dbl(1/2)
1

>> dbl(0.5)
1.


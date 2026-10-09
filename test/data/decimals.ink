# Numbers shown as decimals (DESIGN.md, phase 13). This was the
# specification, written before they existed (CLAUDE.md, section 3); every
# expected output is as it was specified, computed from the exact value by a
# reference printer written apart from the interpreter -- but for the wording
# of 'frac''s refusal, "2 is approximate" having read oddly after a plain 2,
# and for the answers past 64 bits, which step 2 made exact (bignum.ink).
#
# Every number prints in decimal. An exact whole number prints in full;
# anything else is rounded to 'digits' significant digits -- 9 unless set, and
# 17 at most for an inexact number, which holds no more -- with an exponent
# below 1e-4 and from 10^digits up, and with '~' in front unless the number
# is exact and what is printed is all of it (C240).
#
# A literal is exact as written, point and exponent included: 0.1 is 1/10.
# '~' in front of anything makes it inexact, so every answer reads back as
# what it says it is. 'frac' at the start of a line shows the answer as its
# exact fraction, and 'digits = n' sets how many significant digits are shown.

# A quotient is a decimal. '~' says when the decimal is not all of it: rounded,
# because '~' says what follows is an approximation.
>> 10/4
2.5

>> 1/3
~0.333333333

>> 2/3
~0.666666667

>> -2/3
~-0.666666667

>> 1/3+1/3+1/3
1

>> 1/-2
-0.5

>> 2^-3
0.125

>> (2/3)^-2
2.25

# An exact whole number prints in full, as it did.
>> 2^40
1099511627776

>> !20
2432902008176640000

# Very small and very large numbers take an exponent.
>> 1/1048576
~9.53674316e-07

>> 1/10^18
1e-18

>> 10^12/7
~1.42857143e+11

# A literal is exact as written: the classic surprise is gone.
>> 0.1+0.2
0.3

>> 0.1*3 == 0.3
1

>> 1e3
1000

>> 1.5e-3
0.0015

>> frac 0.1
1/10

# '~' makes a number inexact, so what the printer writes reads back as the
# approximation it is. A literal too long for 64 bits is approximated too, as
# an overflow is.
>> ~3.14159265
~3.14159265

>> ~0.1
~0.1

>> ~(1/3)
~0.333333333

>> 3.14159265358979323846
~3.14159265

# What can only be approached, and i: a complex number is inexact. An inexact
# number is marked whatever its digits, as a bare one would read back exact,
# and a complex number once, before its parts (C240).
>> pi
~3.14159265

>> 2^(1/2)
~1.41421356

>> 2^(1/2)*2^(1/2)
~2

>> i*i
~-1

>> 2+3*i
~(2+i*3)

>> e^(i*pi)
~(-1+i*1.2246468e-16)

>> ~1
~1

# Exact past 64 bits, to a thousand digits (bignum.ink). These were specified
# as approximated where 64 bits ran out: 2^63 as ~9.22337204e+18.
>> 2^62
4611686018427387904

>> 2^63
9223372036854775808

>> !21
51090942171709440000

>> !52/(!5*!47)
2598960

# A limit approaches; a partial sum is exact. A limit that lands on a whole
# number prints as one.
>> ex(x)_n = sum_(k=0)^n x^k/!k
ex(x)_n = sum_(k=0)^n x^k/!k

>> ex(1)_10
~2.7182818

>> lim ex(1)
~2.71828183

>> sum_(k=0) 1/2^k
~2

>> lt_0 = 0
lt_0 = 0

>> lt_n | n > 2 = 5
lt_n | n > 2 = 5

>> lt_n = n
lt_n = n

>> lim lt
~5

# Sequences, exact while they fit.
>> s_0 = 1
s_0 = 1

>> s_n = s_(n-1)/3
s_n = s_(n-1)/3

>> s_5
~0.00411522634

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
12200160415121876738

>> h_1 = 1
h_1 = 1

>> h_n = h_(n-1) + 1/n
h_n = h_(n-1) + 1/n

>> h_30
~3.99498713

>> h_50
~4.49920534

# 'frac' shows the exact fraction behind a decimal. It is about the whole
# answer, so it begins a line, and it refuses what was only approximated.
>> frac 10/4
5/2

>> frac 2/3
2/3

>> frac 6/3
2

>> frac ex(1)_10
9864101/3628800

>> frac s_5
1/243

>> frac pi
error: ~3.14159265 was approximated, so it has no exact fraction

>> frac ~(1/3)
error: ~0.333333333 was approximated, so it has no exact fraction

>> frac !52/(!5*!47)
2598960

>> 1 + frac 2
error: frac can only begin a line

# The kind is still there, shown and by 'frac'. A memo key that dropped the
# kind would answer the fourth line from the third (C50).
>> half(x) = x/2
half(x) = x/2

>> half(1)
0.5

>> half(~1)
~0.5

>> frac half(1)
1/2

>> frac half(~1)
error: ~0.5 was approximated, so it has no exact fraction

# Newton's method doubles an exact fraction's digits at every step, so an
# iteration meant to be approximate starts from an approximation.
>> rt_0 = 1
rt_0 = 1

>> rt_n = (rt_(n-1) + 2/rt_(n-1))/2
rt_n = (rt_(n-1) + 2/rt_(n-1))/2

>> rt_4
~1.41421356

>> frac rt_4
665857/470832

>> root_0 = ~1
root_0 = ~1

>> root_n = (root_(n-1) + 2/root_(n-1))/2
root_n = (root_(n-1) + 2/root_(n-1))/2

>> root_30
~1.41421356

# How many digits is a setting, written into the session that uses it. An
# exact number has as many as are asked for; a double has 17 and no more,
# which is where a partial sum of e overtakes the built-in one.
>> digits
9

>> digits = 20
digits = 20

>> 1/7
~0.14285714285714285714

>> ex(1)_20
~2.7182818284590452353

>> e
~2.7182818284590451

>> ~0.1
~0.10000000000000001

>> 0.1
0.1

>> digits = 9
digits = 9

>> digits = 0
error: digits must be a whole number of at least 1, not 0

>> digits = 1/2
error: digits must be a whole number of at least 1, not 0.5

# Every digit costs time and memory in every cell -- some 25 ns and 20 bytes
# each -- so there is a ceiling: a thousand digits is a page.
>> digits = 1000
digits = 1000

>> digits = 1001
error: digits can be 1000 at most, not 1001

>> digits = 2^40
error: digits can be 1000 at most, not 1099511627776

>> digits
1000

>> digits = 9
digits = 9

# Every cell by its own rule.
>> a = [1 2;3 4]
a = [1 2;3 4]

>> a/3
[~0.333333333, ~0.666666667;
            1,  ~1.33333333]

>> frac a/3
[1/3, 2/3;
   1, 4/3]

>> [1/3 ~0.1]*3
[1, ~0.3]

# The ellipsis is not claimed: it stays free for the '...' of matrix notation,
# which C41 suspects the padding rule was an attempt at.
>> [1 2...]
error: unexpected character '.'

>> [1 2 ...]
error: unexpected character '.'

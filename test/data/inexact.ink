# A bare number is exact, and a truth read from a double is not (DESIGN.md,
# C240, C241). Every value was written from the rule, apart
# from the interpreter: by hand, and in Python's doubles where a double's
# rounding is the point.

# An exact number prints as before: bare where the decimal is all of it,
# '~' where digits are cut.
>> 1/2
0.5

>> 1/3
~0.333333333

>> 2^100
1267650600228229401496703205376

# An inexact one always has '~', whatever its digits, 0 and inf too.
>> ~0.5
~0.5

>> ~1/3
~0.333333333

>> ~0
~0

>> 0*~1
~0

>> 2*~1
~2

>> -~1
~-1

>> ~10^400
~inf

>> -~10^400
~-inf

# A double's 0 has a sign, which 1/x tells: -10^-400 is -0 as a double. It
# reads back as +0 still, ~ taking -0's exact 0 (C273). An exact 0 has none.
>> ~-10^-400
~-0

>> 1/~-10^-400
~-inf

>> [~-10^-400, ~0]
[~-0, ~0]

>> -0
0

>> floor(~2.5)
~2

>> exp(0)
~1

# NaN is no number a decimal approximates, and has none.
>> 0/~0
-nan

# At 17 digits as at 9.
>> digits = 17
digits = 17

>> ~0.1
~0.10000000000000001

>> ~0.5
~0.5

>> digits = 9
digits = 9

# The answers of the prelude that printed bare (README.md, section 2).
>> rho([0 1; 0 0])
~0

>> dhinf(1/2, 1, 1)
~2

# Approximated past a thousand digits: '~' says inexact, the comment why.
>> (1/3)^2100
~0  # approximated past a thousand digits

>> 10^4000
~inf  # approximated past a thousand digits

# smax's mark where scaling back does not give its root again, a subnormal.
>> smax(10^-310)
~1e-310  # approximated past a thousand digits

# A matrix cell by cell, each column aligned with its marks.
>> [1 ~2; 3 ~0.1]
[1,   ~2;
 3, ~0.1]

>> [1/2, ~0.5]
[0.5, ~0.5]

>> [0 1]*~1
[~0, ~1]

# A complex number is a pair of doubles, never exact, the unit too: one
# '~' before its parts in parentheses, which print bare. One whose
# imaginary part is 0 prints as a real.
>> i
~(i)

>> -i
~(-i)

>> 1-i
~(1-i)

>> 2*i
~(i*2)

>> i*0
~0

# One mark per cell of a matrix.
>> [1 i]
[1, ~(i)]

>> [1 i; 2*i 3]
[     1, ~(i);
 ~(i*2),    3]

# Two NaN parts are no value, and bare as NaN is.
>> (0/~0)*i
-nan+i*-nan

>> abs(3+4*i)
~5

# re of an exact number is that number, exact.
>> re(1/2)
0.5

# What is printed reads back as the kind it was: ~1 is the double 1.
>> ~1
~1

>> ~1 == 1
~1

>> frac ~1
error: ~1 was approximated, so it has no exact fraction

>> ~(1-i)
~(1-i)

>> ~(2+i*3) == 2+3*i
~1

# At 17 digits a complex number reads back as the pair of doubles it was.
>> digits = 17
digits = 17

>> z = (1+i)/3
z = (1+i)/3

>> z
~(0.33333333333333331+i*0.33333333333333331)

>> ~(0.33333333333333331+i*0.33333333333333331) == z
~1

>> digits = 9
digits = 9

>> [1, ~2; 3, ~0.1] == [1 ~2; 3 ~0.1]
~1

# A comparison of exact numbers is an exact truth.
>> 1/3+1/3+1/3 == 1
1

>> 0.1*3 > 3/10
0

>> [1 2] == [1 2]
1

# One that reads an inexact number answers an inexact one, however far
# from its threshold: no bound is carried.
>> ~0.1*3 > 3/10
~1

>> ~0.5 == 1/2
~1

>> ~2 > 1
~1

>> i == i
~1

# Whole matrices: inexact where a cell of either is.
>> [1 ~2] == [1 ~2]
~1

>> [1 ~2] == [3 ~2]
~0

# rho(A) is the double nearest 5^(1/2)/2, whose square in doubles is
# 1.2500000000000002: the answer is wrong, and says it may be.
>> A = [1/2 1; -1 1/2]
A = [1/2 1; -1 1/2]

>> rho(A)^2 == 5/4
~0

>> rho(A) > 1
~1

# An inexact truth makes inexact what it multiplies.
>> (~0.1*3 > 3/10)*5
~5

# and and or: inexact where a side read is; a side not read cannot be.
>> 0 and ~1
0

>> ~0 and 1
~0

>> 1 and ~1
~1

>> ~1 or 0
~1

>> 1 or ~0
1

>> 0 or ~0.5
~1

>> 1 > 0 and 2 > 1
1

# A guard chooses by an inexact truth as the double says, and the answer
# of the clause chosen is inexact, whether its own guard held or one
# tried before it failed.
>> g(x) | x > 1 = 1
g(x) | x > 1 = 1

>> g(x) = 0
g(x) = 0

>> g(2)
1

>> g(~2)
~1

>> g(~1/2)
~0

>> h(x) | x == 5/4 = 1
h(x) | x == 5/4 = 1

>> h(x) = 0
h(x) = 0

>> h(5/4)
1

>> h(rho(A)^2)
~0

# The prelude's guards alike.
>> max(2, 1)
2

>> max(2, ~1)
~2

>> min(1/3, ~0.5)
~0.333333333

>> frac min(1/3, 1/4)
1/4

>> frac min(1/3, ~0.5)
error: ~0.333333333 was approximated, so it has no exact fraction

>> abs(~-2)
~2

>> ilogb(8)
3

>> ilogb(~8)
~3

>> ilogb(~0.1)
~-4

>> frac ilogb(~3)
error: ~1 was approximated, so it has no exact fraction

# A refusal keeps its words: an error has no value to mark.
>> k(x) | x > 1 = 1
k(x) | x > 1 = 1

>> k(~1/2)
error: no clause of k applies

# By cells, each cell by the guards read for it.
>> p = [~0.5; 2]
p = [~0.5; 2]

>> w[j<=2] | p[j] > 1 = 1
w[j<=2] | p[j] > 1 = 1

>> w[j<=2] = 0
w[j<=2] = 0

>> w
[~0;
  1]

# A count of choices made by inexact truths is inexact, and so no index.
>> c_0 = 0
c_0 = 0

>> c_n | ~n/10 > 1/2 = c_(n-1) + 1
c_n | ~n/10 > 1/2 = c_(n-1) + 1

>> c_n = c_(n-1)
c_n = c_(n-1)

>> c_10
~5

>> x_n = n
x_n = n

>> x_(c_10)
error: an index must be exact, and ~5 was approximated

>> u = [5; 6]
u = [5; 6]

>> u[1 > 0]
5

>> u[~1 > 0]
error: an index must be exact, and ~1 was approximated

# A number quoted in an error is quoted so.
>> x_(~2)
error: an index must be exact, and ~2 was approximated

>> smax(~-10^310)
error: smax needs finite cells, not ~-inf

>> !~5.5
error: a factorial needs a whole number, not ~5.5

# frac refuses an inexact truth, and an approximated one as before.
>> frac (1/3 + 1/3 > 1/2)
1

>> frac (~0.1*3 > 3/10)
error: ~1 was approximated, so it has no exact fraction

>> 10^4000 > 1
~1  # approximated past a thousand digits

>> frac (10^4000 > 1)
error: ~1 was approximated past a thousand digits, so it has no exact fraction

# grad: a clause chosen by an inexact truth makes the whole answer
# inexact, as C165 marks it for an approximated one.
>> grad_(x = 2) g(x)
0

>> grad_(x = ~2) g(x)
~0

>> grad_(x = ~-2) abs(x)
~-1

>> grad_(x = ~25) tanh(~1*x)
~0

>> 1/grad_(x = ~25) tanh(~1*x)
~inf

# Where no guard reads one, a part is what arithmetic makes it.
>> grad_(x = ~2) 3*x
3

# The prelude's functions of a double, computed in C, answer as their
# walks do, whose guards read the double.
>> grad_(x = ~3) ilogb(x)
~0

>> grad_(x = ~3) [exp(x) 5]
[~20.0855369, ~0]

# Values the rule must not move. A matrix of doubles is bisected in
# doubles now, and stops where the bracket's ends are adjacent doubles,
# unmarked as before.
>> rho([~0.5 1; -1 0.5])
~1.11803399

>> eig([~2 1; 1 3])
[~1.38196601;
 ~3.61803399]

# log of a double scales by 2^ilogb, inexact now: an exact part past a
# double's range is still divided exactly (C242). The slope is 1/t.
>> grad_(t = ~(1.5e-300)) log(t*10^200*10^200)
~6.66666667e+299

# README.md's examples that move.
>> 2+3*i
~(2+i*3)

>> (1+i)*(1-i)
~2

>> im(2+3*i)
~3

>> ab(x) | x < 0 = 0-x
ab(x) | x < 0 = 0-x

>> ab(x) | x >= 0 = x
ab(x) | x >= 0 = x

>> ab(i)
error: a comparison needs real numbers, not ~(i)

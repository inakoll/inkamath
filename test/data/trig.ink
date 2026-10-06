# sin, cos, abs, max and min in the prelude (DESIGN.md, next in line),
# written in inkamath as exp, log and tanh are. sin and cos reduce by the
# whole number of pi/2 nearest, pi/2 taken in five parts so that every
# product is exact and every subtraction that cancels is too (Cody and
# Waite), round once where the reduction ends, then sum a polynomial of
# fixed degree; abs, max and min are a guard each. Expected values are
# mpmath's at nine digits, and at seventeen those of the design's own
# arithmetic, emulated apart from the interpreter and held to mpmath's.
>> sin(1)
~0.841470985

>> cos(1)
~0.540302306

>> sin(-1)
~-0.841470985

>> sin(1/2)
~0.479425539

>> cos(2)
~-0.416146837

>> sin(10)
~-0.544021111

>> cos(100)
~0.862318872

>> sin(1000000)
~-0.349993502

>> cos(1000000)
~0.936752128

# A double's 0 and 1, as exp(0) is a double's 1: a clause for 0 alone would
# make them exact, and grad refuses a clause that holds at a point alone.
>> sin(0)
0

>> frac sin(0)
error: 0 was approximated, so it has no exact fraction

>> cos(0)
1

>> frac cos(0)
error: 1 was approximated, so it has no exact fraction

# pi is the double nearest it, so these are the sine and cosine of that
# double, as C gives them.
>> sin(pi)
~1.2246468e-16

>> cos(pi/2)
~6.123234e-17

# 355 is 3.0e-5 from 113 pi: an exact argument is reduced exactly, however
# much cancels.
>> sin(355)
~-3.01443534e-05

# Up to 2^20 either way, where every product of the reduction is exact in a
# double, and refused past it, where they round: the hardest doubles below
# 2^22 would be 2^40 units off.
>> sin(1048576)
~0.33049314

>> cos(-1048576)
~0.943808394

>> sin(1048577)
error: division by zero

>> cos(~(-2000000))
error: division by zero

>> sin(10^400)
error: division by zero

>> sin(~1/0)
error: division by zero

# Not a real number, refused as exp refuses it.
>> sin(0/~0)
error: a comparison needs a number, not -nan

>> cos(i)
error: a comparison needs real numbers, not i

>> sin(~1 + i)
error: a comparison needs real numbers, not 1+i

>> sin([1 2])
error: sin needs single values, not a 1x2 matrix; write it by its cells

# abs is README's own, two guards; its value is exact of an exact number.
>> ?abs
abs(x) | x < 0 = -x
abs(x) | x >= 0 = x

>> abs(-3)
3

>> frac abs(-1/3)
1/3

>> frac abs(0)
0

>> abs(~(-2.5))
2.5

>> abs(-1/~0)
inf

# A modulus needs the real and imaginary parts and a root, which the
# prelude has not: a complex number is refused, as README's abs refuses it.
>> abs(i)
error: a comparison needs real numbers, not i

>> abs(0/~0)
error: a comparison needs a number, not -nan

>> abs([1 -2])
error: abs needs single values, not a 1x2 matrix; write it by its cells

# max and min take two arguments and answer the one chosen, exact or not.
>> max(3, 7)
7

>> min(3, 7)
3

>> frac max(1/3, 1/4)
1/3

>> frac min(1/3, 1/4)
1/4

>> frac min(1/3, ~0.5)
1/3

>> max(1/3, ~0.5)
0.5

>> max(2, 2)
2

>> max(~1/0, 1)
inf

>> min(-1/~0, 1)
-inf

# NaN is no number to compare, on either side: C's fmax would answer the
# other argument, which hides it.
>> max(0/~0, 1)
error: a comparison needs a number, not -nan

>> min(1, 0/~0)
error: a comparison needs a number, not -nan

>> max(i, 1)
error: a comparison needs real numbers, not i

# The greatest cell of a matrix is a reduction over its cells, not this.
>> max([1 2], [2 1])
error: max needs single values, not a 1x2 matrix; write it by its cells

>> max(1, 2, 3)
error: max expects 2 arguments, got 3

>> digits = 17
digits = 17

>> sin(1)
~0.8414709848078965

>> cos(1)
~0.54030230586813977

# Not correctly rounded: mpmath's, rounded, is -0.54402111088936977, a unit
# away; this is 0.65 units from the sine, within the 2.4 of any double.
>> sin(10)
~-0.54402111088936989

>> sin(pi)
~1.2246467991473532e-16

>> cos(pi/2)
~6.123233995736766e-17

# The doubles nearest a multiple of pi/2, where the reduction cancels most:
# below 2^20 the nearest in absolute terms is 45.553093477052, 6.2e-19 from
# 29 pi/2, then 91.106186954104, 1.2e-18 from 29 pi; and two past 2^18,
# where 73 bits cancel, the most. Each is mpmath's.
>> cos(~45.553093477052)
~-6.1898063658835771e-19

>> sin(~91.106186954104)
~-1.2379612731767154e-18

>> sin(~642615.9188844458)
~8.8592016691922586e-17

>> cos(~321307.9594422229)
~-4.4296008345961293e-17

# Exact, and 2.3e-6 from 265381 pi.
>> sin(833719)
~2.3129194164527015e-06

# 2^20 is a double as well as an exact number, reduced as each is: exactly
# and rounded once, or by a double's operations. The double's is mpmath's
# here, and the exact one's a unit below it.
>> sin(1048576)
~0.33049314002173463

>> sin(~1048576)
~0.33049314002173469

>> sin(~1048575.75)
~0.086716975228377249

>> sin(1/10^9)
~1.0000000000000001e-09

# Odd to the bit: -x is reduced as x is, but where x*2/pi + 1/2 is whole,
# as at the double nearest 3 pi/4, and floor takes k one way on each side.
>> sin(~(-2.5)) + sin(~2.5)
0

>> sin(~2.356194490192345) + sin(~(-2.356194490192345))
~-1.1102230246251565e-16

# A 0 has the definition's sign: sin's subtraction from 0 makes sin(-0) +0,
# where C's is -0; abs answers its argument, so abs(-0) is -0, where C's
# fabs is +0.
>> 1/sin(~0*(-1))
inf

>> 1/abs(~0*(-1))
-inf

>> digits = 9
digits = 9

# grad differentiates the definitions: the polynomials' own derivatives, and
# floor's 0.
>> grad_(x = 0) sin(x)
1

>> grad_(x = 0) cos(x)
0

>> grad_(x = 1) sin(x)
~0.540302306

>> grad_(x = 1) cos(x)
~-0.841470985

>> grad_(x = 3) sin(x)
~-0.989992497

>> grad_(x = 10) sin(x)*cos(x)
~0.408082062

>> grad_(x = 1000000) sin(x)
~0.936752128

>> grad_(x = 2^21) sin(x)
error: division by zero

# Where x*2/pi + 1/2 is whole, floor jumps, as in exp.
>> grad_(x = 1/2/0.6366197723675814) sin(x)
error: floor jumps at x = ~0.785398163

# abs, max and min take the slope of the clause that holds, one side of the
# threshold as a ReLU does: abs's x >= 0 at 0, and at a tie the first
# argument's, as TensorFlow's maximum and minimum, so max(0, x) has a
# ReLU's slope 0 at 0, as PyTorch's has.
>> grad_(x = 0) abs(x)
1

>> grad_(x = -2) abs(x)
-1

>> grad_(x = 0) max(0, x)
0

>> grad_(x = 0) max(x, 0)
1

>> grad_(x = 0) min(0, x)
0

>> grad_(x = 0) min(x, 0)
1

>> grad_(x = 3) max(1, x)
1

>> grad_(x = -3) max(1, x)
0

>> grad_(x = 2) max(x, x)
1

>> grad_(x = 2) max(x*x, 4)
4

# sin and cos of a real double within 2^20 are called compiled, as exp is:
# one step and one reference deep. Through the definition each is some 40
# steps and nests 5 references deeper, so these ran out of steps and of
# depth; the sums are mpmath's.
>> sum_(k=1)^40000 sin(~k/40000)
~18388.3285

>> sum_(k=1)^40000 cos(~k/40000)
~33658.6095

>> dive(k) = dive(k - 1)
dive(k) = dive(k - 1)

>> dive(k) | k < 1 = sin(~3)
dive(k) | k < 1 = sin(~3)

>> dive(254)
~0.141120008

>> dive(k) | k < 1 = cos(~3)
dive(k) | k < 1 = cos(~3)

>> dive(254)
~-0.989992497

# An exact argument walks the definition, and so do abs, max and min.
>> dive(k) | k < 1 = sin(3)
dive(k) | k < 1 = sin(3)

>> dive(254)
error: evaluation nests more than 256 references deep

>> dive(k) | k < 1 = abs(~3)
dive(k) | k < 1 = abs(~3)

>> dive(254)
error: evaluation nests more than 256 references deep

>> a = (10^600 + 1)/10^600
a = (10^600 + 1)/10^600

>> sin(a*a)
~0.841470985  # approximated past a thousand digits

# A session's clause starts a definition of its own, as a model's or a
# file's does, so this abs has one clause; a plain definition starts over.
>> abs(y) | y < 0 = -y
abs(y) | y < 0 = -y

>> abs(-3)
3

>> abs(3)
error: no clause of abs applies

>> max(x, y) = (x + y + abs(x - y))/2
max(x, y) = (x + y + abs(x - y))/2

>> max(3, 7)
7

# A clause on a built-in starts a definition too: this floor holds above
# 10 alone.
>> floor(x) | x > 10 = 0
floor(x) | x > 10 = 0

>> floor(2.5)
error: no clause of floor applies

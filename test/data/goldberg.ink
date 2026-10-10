# D. Goldberg, "What Every Computer Scientist Should Know About
# Floating-Point Arithmetic", ACM Computing Surveys 23(1), 1991. Every
# expected value is the paper's, typed by hand, never recorded. Most of its
# examples are in base 10 with three digits; that arithmetic is a function
# here, exact numbers rounded to p digits of base b, ties to even.

>> ex(b, x) | x >= b = ex(b, x/b) + 1
ex(b, x) | x >= b = ex(b, x/b) + 1

>> ex(b, x) | x < 1 = ex(b, x*b) - 1
ex(b, x) | x < 1 = ex(b, x*b) - 1

>> ex(b, x) = 0
ex(b, x) = 0

>> ulp(b, p, x) = b^(ex(b, x) - p + 1)
ulp(b, p, x) = b^(ex(b, x) - p + 1)

>> even(q) = floor(q + 1/2) - (q - floor(q) == 1/2 and mod(floor(q), 2) == 0)
even(q) = floor(q + 1/2) - (q - floor(q) == 1/2 and mod(floor(q), 2) == 0)

>> fl(b, p, x) | x == 0 = 0
fl(b, p, x) | x == 0 = 0

>> fl(b, p, x) | x < 0 = 0 - fl(b, p, 0 - x)
fl(b, p, x) | x < 0 = 0 - fl(b, p, 0 - x)

>> fl(b, p, x) = even(x/ulp(b, p, x))*ulp(b, p, x)
fl(b, p, x) = even(x/ulp(b, p, x))*ulp(b, p, x)

>> d(x) = fl(10, 3, x)
d(x) = fl(10, 3, x)

# Relative Error and Ulps: 12.35 is 1.24e1, 0.5 ulps and 0.8 eps; 8 times
# it is 9.92e1 for 98.8, 4 ulps and still 0.8 eps; .0314159 as 3.14e-2 is
# .159 ulps and 0.1 eps, eps being 5*10^-3.
>> eps = 10/2*10^-3
eps = 10/2*10^-3

>> eps
0.005

>> d(12.35)
12.4

>> (d(12.35) - 12.35)/ulp(10, 3, 12.35)
0.5

>> fl(10, 1, (d(12.35) - 12.35)/12.35/eps)
0.8

>> d(8*d(12.35))
99.2

>> (d(8*d(12.35)) - 8*12.35)/ulp(10, 3, 98.8)
4

>> fl(10, 1, (d(8*d(12.35)) - 8*12.35)/(8*12.35)/eps)
0.8

>> (0.0314159 - d(0.0314159))/ulp(10, 3, 0.0314159)
0.159

>> fl(10, 1, (0.0314159 - d(0.0314159))/0.0314159/eps)
0.1

# Guard Digits: x - y with p digits and g guard digits, y shifted to x's
# exponent and the digits past p + g dropped.
>> chop(y, u) = floor(y/u)*u
chop(y, u) = floor(y/u)*u

>> sub(p, g, x, y) = fl(10, p, x - chop(y, 10^(ex(10, x) - p + 1 - g)))
sub(p, g, x, y) = fl(10, p, x - chop(y, 10^(ex(10, x) - p + 1 - g)))

>> sub(3, 0, 2.15*10^12, 1.25*10^-5) == d(2.15*10^12 - 1.25*10^-5)
1

>> d(2.15*10^12 - 1.25*10^-5)
2150000000000

>> sub(3, 0, 10.1, 9.93)
0.2

>> (sub(3, 0, 10.1, 9.93) - (10.1 - 9.93))/ulp(10, 3, 0.17)
30

>> sub(3, 1, 10.1, 9.93)
0.17

>> sub(3, 1, 110, 8.59)
102

>> 110 - 8.59
101.41

>> fl(10, 1, (sub(3, 1, 110, 8.59) - 101.41)/101.41)
0.006

# Theorem 1: without a guard digit the relative error reaches b - 1, at
# x = 1.00 and y = .999.
>> (sub(3, 0, 1, 0.999) - (1 - 0.999))/(1 - 0.999)
9

# Cancellation: b^2 - 4ac at b = 3.34, a = 1.22, c = 2.28 is .0292, and
# rounded products give .1. The text's 70 ulps is an error in the paper:
# 708 in ulps of .0292, which the editor's note 6 gives as 700.
>> 3.34^2 - 4*1.22*2.28
0.0292

>> d(3.34^2)
11.2

>> d(4*1.22*2.28)
11.1

>> d(d(3.34^2) - d(4*1.22*2.28))
0.1

>> (0.1 - 0.0292)/ulp(10, 3, 0.0292)
708

# The area of a thin triangle, a = 9, b = c = 4.53: s is 9.03 and the area
# 2.342..., 2.34216 to six digits. Heron's formula (6) computes s as 9.05,
# 2 ulps, and the area as 3.04, 70 ulps; Kahan's (7) gives 2.35, 0.7 eps.
>> a = 9
a = 9

>> b = 4.53
b = 4.53

>> c = 4.53
c = 4.53

>> s = (a + b + c)/2
s = (a + b + c)/2

>> s
9.03

>> fl(10, 6, (s*(s - a)*(s - b)*(s - c))^(1/2))
~2.34216

>> 16*s*(s - a)*(s - b)*(s - c) == (a + (b + c))*(c - (a - b))*(c + (a - b))*(a + (b - c))
1

>> sh = d(d(d(b + c) + a)/2)
sh = d(d(d(b + c) + a)/2)

>> sh
9.05

>> (sh - s)/ulp(10, 3, s)
2

>> heron = d(d(d(d(sh*d(sh - a))*d(sh - b))*d(sh - c))^(1/2))
heron = d(d(d(d(sh*d(sh - a))*d(sh - b))*d(sh - c))^(1/2))

>> heron
~3.04

>> fl(10, 1, (heron - (s*(s - a)*(s - b)*(s - c))^(1/2))/0.01)
~70

>> k = [d(a + d(b + c)); d(c - d(a - b)); d(c + d(a - b)); d(a + d(b - c))]
k = [d(a + d(b + c)); d(c - d(a - b)); d(c + d(a - b)); d(a + d(b - c))]

>> kahan = d(d(d(d(d(k[1]*k[2])*k[3])*k[4])^(1/2))/4)
kahan = d(d(d(d(d(k[1]*k[2])*k[3])*k[4])^(1/2))/4)

>> kahan
~2.35

>> fl(10, 1, (kahan - 2.34216)/2.34216/eps)
~0.7

# A deposit of $100 a day at 6% compounded daily for a year,
# 100((1 + i/n)^n - 1)/(i/n): $37614.05 exactly, and in base 2 with 24
# bits $37615.45 as written, $37617.26 with ln(1 + x) taken for x, and
# $37614.07 by Theorem 4's ln(1 + x). The exact value outgrows a thousand
# digits, and its digits stay right.
>> x = 6/100/365
x = 6/100/365

>> digits = 7
digits = 7

>> 100*((1 + x)^365 - 1)/x
~37614.05  # approximated past a thousand digits

>> 100*(exp(365*log(1 + x)) - 1)/x
~37614.05

>> f(y) = fl(2, 24, y)
f(y) = fl(2, 24, y)

>> xf = f(f(6/100)/365)
xf = f(f(6/100)/365)

>> pw_0 = 1
pw_0 = 1

>> pw_k = f(pw_(k-1)*f(1 + xf))
pw_k = f(pw_(k-1)*f(1 + xf))

>> pay(z) = f(100*f(f(z - 1)/xf))
pay(z) = f(100*f(f(z - 1)/xf))

>> pay(pw_365)
~37615.45

# The paper's $37617.26 is an error, cut where .27 is rounded: every order
# of these operations gives 37617.2656, and a unit of exp moves it by .07.
>> pay(f(exp(f(365*xf))))
~37617.27

>> L(y) | f(1 + y) == 1 = y
L(y) | f(1 + y) == 1 = y

>> L(y) = f(f(y*f(log(f(1 + y))))/f(f(1 + y) - 1))
L(y) = f(f(y*f(log(f(1 + y))))/f(f(1 + y) - 1))

>> pay(f(exp(f(365*L(xf)))))
~37614.07

>> digits = 9
digits = 9

# Exactly Rounded Operations: 12.5 to even is 12, VAX's way 13.
>> fl(10, 2, 12.5)
12

>> floor(12.5 + 1/2)
13

# Theorem 5: x_n = (x_(n-1) - y) + y at y = -.555 from 1.00. Rounding
# halves up, x_1 is 1.01, each term .01 more, until 9.45 at n = 845 and
# from there on (note 9); to even, every term is 1.00.
>> up(b, p, x) = floor(x/ulp(b, p, x) + 1/2)*ulp(b, p, x)
up(b, p, x) = floor(x/ulp(b, p, x) + 1/2)*ulp(b, p, x)

>> u_0 = 1
u_0 = 1

>> u_n = up(10, 3, up(10, 3, u_(n-1) + 0.555) - 0.555)
u_n = up(10, 3, up(10, 3, u_(n-1) + 0.555) - 0.555)

>> up(10, 3, u_0 + 0.555)
1.56

>> u_1
1.01

>> up(10, 3, u_1 + 0.555)
1.57

>> u_844
9.44

>> u_845
9.45

>> u_900
9.45

>> v_0 = 1
v_0 = 1

>> v_n = d(d(v_(n-1) + 0.555) - 0.555)
v_n = d(d(v_(n-1) + 0.555) - 0.555)

>> v_900
1

# Theorem 6, Dekker's split, in base 10 with 4 digits: b = 3.476,
# a = 3.463, c = 3.479. b^2 - ac rounds to .03480, b*b to 12.08 and a*c to
# 12.05, so .03, 480 ulps. Split around 3.5, each part exact: b^2 is
# 12.25 - .168 + .000576, ac is 12.25 - .2030 + .000777, and their
# difference .03480, exactly rounded.
>> q(x) = fl(10, 4, x)
q(x) = fl(10, 4, x)

>> q(3.476^2 - 3.463*3.479)
0.0348

>> q(3.476^2)
12.08

>> q(3.463*3.479)
12.05

>> q(q(3.476^2) - q(3.463*3.479))
0.03

>> (q(3.476^2 - 3.463*3.479) - 0.03)/ulp(10, 4, 0.0348)
480

>> hi(x) = q(q(101*x) - q(q(101*x) - x))
hi(x) = q(q(101*x) - q(q(101*x) - x))

>> [hi(3.476), 3.476 - hi(3.476); hi(3.463), 3.463 - hi(3.463); hi(3.479), 3.479 - hi(3.479)]
[3.5, -0.024;
 3.5, -0.037;
 3.5, -0.021]

>> [3.5^2, 2*3.5*(-0.024), 0.024^2]
[12.25, -0.168, 0.000576]

>> [3.5^2, 0 - (3.5*0.037 + 3.5*0.021), 0.037*0.021]
[12.25, -0.203, 0.000777]

>> q((0 - 0.168 + 0.203) + (0.000576 - 0.000777))
0.0348

# And not without exact rounding: base 2, 3 digits, x = 7, m = 5. m*x is
# 32, and with one guard digit 32 - 7 is 28, so xh = 4 and xl = 3, two
# bits; exactly rounded, xh = 8 and xl = -1.
>> fl(2, 3, 5*7)
32

>> g2(x, y) = fl(2, 3, x - chop(y, 2^(ex(2, x) - 3)))
g2(x, y) = fl(2, 3, x - chop(y, 2^(ex(2, x) - 3)))

>> g2(32, 7)
28

>> [32 - g2(32, 7), 7 - (32 - g2(32, 7))]
[4, 3]

>> [32 - fl(2, 3, 32 - 7), 7 - (32 - fl(2, 3, 32 - 7))]
[8, -1]

# Theorem 7 in double: (3.0/10.0)*10.0 is 3.
>> (~3/~10)*~10 == 3
~1

# Dekker's exact sum, x + y = (x + y rounded) + ((x - that) + y), holds in
# base 2 only: base 10 with 5 digits, x = .99998, y = .99997.
>> t(x) = fl(10, 5, x)
t(x) = fl(10, 5, x)

>> t(0.99998 + 0.99997) + t(t(0.99998 - t(0.99998 + 0.99997)) + 0.99997) == 0.99998 + 0.99997
0

# The IEEE Standard, Operations: ((2*10^-30 + 10^30) - 10^30) - 10^-30 is
# 10^-30, and -10^-30 in IEEE arithmetic.
>> ((2*10^-30 + 10^30) - 10^30) - 10^-30
1e-30

>> ((~2*~10^-30 + ~10^30) - ~10^30) - ~10^-30
~-1e-30

# Ambiguity: (x + y) + z is 1 and x + (y + z) is 0 at x = 10^30,
# y = -10^30, z = 1.
>> (~10^30 + ~(-10^30)) + ~1
~1

>> ~10^30 + (~(-10^30) + ~1)
~0

# Infinity: 3/inf is 0, 4 - inf is -inf, sqrt(inf) is inf, inf/inf NaN.
>> inf = ~1/~0
inf = ~1/~0

>> 3/inf
~0

>> 4 - inf
~-inf

>> inf^(1/2)
~inf

>> inf/inf
-nan

# x/(x^2 + 1) is 0 past the square root of the largest double, and
# 1/(x + 1/x) right there and, through 1/0, at 0.
>> X = ~(10^200)
X = ~(10^200)

>> X/(X^2 + 1)
~0

>> 1/(X + 1/X)
~1e-200

>> 1/(~0 + 1/~0)
~0

# The hypotenuse at 3*10^200 and 4*10^200 is inf in IEEE.
>> ((~3*~10^200)^2 + (~4*~10^200)^2)^(1/2)
~inf

# Exact, the squares are far inside a thousand digits, and the root of
# their sum and 1, which is not a square, about 5*10^200, a double. So
# below the range: 2*10^-400's root is about 1.41*10^-200.
>> ((3*10^200)^2 + (4*10^200)^2 + 1)^(1/2)
~5e+200

>> (2*10^400)^(1/2)
~1.41421356e+200

>> (2*10^-400)^(1/2)
~1.41421356e-200

>> (2^1101)^(1/4)
~7.21948646e+82

# A complex exponent too: 10^200 at an angle of 400 ln 10, and 10^-200 at
# minus that, by mpmath.
>> (10^400)^(1/2+i)
~(-8.53885989e+199-i*5.20460102e+199)

>> (10^-400)^(1/2+i)
~(-8.53885989e-201+i*5.20460102e-201)

# Signed Zero: the principal root of -1 + i0 is i, as C's csqrt has it.
>> (-1)^(1/2)
~(i)

>> (-1)^(1/2) == i
~1

>> (-4)^(1/2)
~(i*2)

# Denormalized Numbers: base 10, 3 digits, emin = -98. 6.87e-97 minus
# 6.81e-97 flushes to 0, and with gradual underflow is .6e-98; Smith's
# formula for (2e-98 + i 1e-98)/(4e-98 + i 2e-98) gives 0.5 with gradual
# underflow and 0.4 flushing, 100 ulps.
>> gu(x) | x == 0 = 0
gu(x) | x == 0 = 0

>> gu(x) | x < 0 = 0 - gu(0 - x)
gu(x) | x < 0 = 0 - gu(0 - x)

>> gu(x) | ex(10, x) < -98 = even(x/10^-100)*10^-100
gu(x) | ex(10, x) < -98 = even(x/10^-100)*10^-100

>> gu(x) = d(x)
gu(x) = d(x)

>> fz(x) | x == 0 = 0
fz(x) | x == 0 = 0

>> fz(x) | x < 0 = 0 - fz(0 - x)
fz(x) | x < 0 = 0 - fz(0 - x)

>> fz(x) | ex(10, x) < -98 = 0
fz(x) | ex(10, x) < -98 = 0

>> fz(x) = d(x)
fz(x) = d(x)

>> fz(6.87*10^-97 - 6.81*10^-97)
0

>> gu(6.87*10^-97 - 6.81*10^-97) == 0.6*10^-98
1

>> sg(a, b, c, w) = gu(gu(a + gu(b*gu(w/c)))/gu(c + gu(w*gu(w/c))))
sg(a, b, c, w) = gu(gu(a + gu(b*gu(w/c)))/gu(c + gu(w*gu(w/c))))

>> sz(a, b, c, w) = fz(fz(a + fz(b*fz(w/c)))/fz(c + fz(w*fz(w/c))))
sz(a, b, c, w) = fz(fz(a + fz(b*fz(w/c)))/fz(c + fz(w*fz(w/c))))

>> sg(2*10^-98, 10^-98, 4*10^-98, 2*10^-98)
0.5

>> sz(2*10^-98, 10^-98, 4*10^-98, 2*10^-98)
0.4

# Binary to Decimal Conversion: a product of 12.51 rounded to 3 digits,
# then to 2, is 12, where rounded once it is 13.
>> fl(10, 2, fl(10, 3, 12.51))
12

>> fl(10, 2, 12.51)
13

# Theorem 15: 17 digits recover a double, and 16 need not.
>> digits = 16
digits = 16

>> ~0.1 + ~0.2
~0.3

>> ~0.3 == ~0.1 + ~0.2
~0

>> digits = 17
digits = 17

>> ~0.1 + ~0.2
~0.30000000000000004

>> ~0.30000000000000004 == ~0.1 + ~0.2
~1

>> digits = 9
digits = 9

# Its proof counts 393,216 floats in [1000, 1024) and 240,000 decimals of
# eight digits.
>> [(2^10 - 10^3)*2^14, (2^10 - 10^3)*10^4]
[393216, 240000]

# Base: of the 105 pairs of 1 to 15, 70 need a shift in base 2, 4 bits.
>> sum_(j=1)^15 sum_(k=j+1)^15 1
105

>> sum_(j=1)^15 sum_(k=j+1)^15 (ex(2, j) <> ex(2, k))
70

# Optimizers: halving eps while eps + 1 > 1 stops at 2^-53 in double; in
# exact arithmetic it never stops.
>> me(e) | ~1 + e/2 > 1 = me(e/2)
me(e) | ~1 + e/2 > 1 = me(e/2)

>> me(e) = e/2
me(e) = e/2

>> me(~1) == 2^-53
~1

>> mx(e) | 1 + e/2 > 1 = mx(e/2)
mx(e) | 1 + e/2 > 1 = mx(e/2)

>> mx(e) = e/2
mx(e) = e/2

>> mx(1)
error: evaluation nests more than 256 references deep

# Theorem 9's proof: in base 2 with one guard digit, x = 1 + 2^(2-p) and
# y = 2^(1-p) - 2^(1-2p) bring the relative error to 2 eps as p grows.
>> sub2(p, x, y) = fl(2, p, x - chop(y, 2^(ex(2, x) - p)))
sub2(p, x, y) = fl(2, p, x - chop(y, 2^(ex(2, x) - p)))

>> w(p) = (sub2(p, 1 + 2^(2-p), 2^(1-p) - 2^(1-2*p)) - (1 + 2^(2-p) - (2^(1-p) - 2^(1-2*p))))/(1 + 2^(2-p) - (2^(1-p) - 2^(1-2*p)))/2^-p
w(p) = (sub2(p, 1 + 2^(2-p), 2^(1-p) - 2^(1-2*p)) - (1 + 2^(2-p) - (2^(1-p) - 2^(1-2*p))))/(1 + 2^(2-p) - (2^(1-p) - 2^(1-2*p)))/2^-p

>> fl(10, 3, w(53))
2

# Exactly Rounded Operations: base 2, 5 digits, .10111 splits as .11 and
# -.00001.
>> hi5(x) = fl(2, 5, fl(2, 5, 9*x) - fl(2, 5, fl(2, 5, 9*x) - x))
hi5(x) = fl(2, 5, fl(2, 5, 9*x) - fl(2, 5, fl(2, 5, 9*x) - x))

>> [hi5(23/32), 23/32 - hi5(23/32)] == [3/4, -1/32]
1

# Precision: 10^13 = 2^13 5^13 with 5^13 < 2^32, and 10^9 < 2^32.
>> [10^13 == 2^13*5^13, 5^13 < 2^32, 10^9 < 2^32]
[1, 1, 1]

# Base: in base 16 with one digit, 15/8 is 1, chopped as System/370 does
# (rounded it is 2).
>> chop(15/8, ulp(16, 1, 15/8))
1

# Systems Aspects: 3/7 in single is not 3/7 in double.
>> fl(2, 24, 3/7) == fl(2, 53, 3/7)
0

# Operations, the table maker's dilemma: exp(1.626) to eight digits is
# 5.0835000.
>> digits = 8
digits = 8

>> exp(1.626)
~5.0835

>> digits = 9
digits = 9

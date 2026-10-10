# atan, atan2, asin, acos and acosh in the prelude (DESIGN.md, next in
# line), written in inkamath as exp, log, sin and cos are. atan reduces |x|
# by one of 0, 1/2, 1, 2 and infinity, chosen by thresholds, atan(c) +
# atan((x - c)/(1 + cx)), to |u| <= 4/15, rounds once where the reduction
# ends, then sums Taylor's series to u^29; atan2 is atan(y/x) moved by pi,
# or pi/2 less atan(x/y) on the axis; asin and acos are atan2 of x and the
# root of (1 - x)(1 + x); acosh is log(x + root(x^2 - 1)), its rounding
# added back, and near 1 the series log takes. Expected values are mpmath's
# at nine digits, and at seventeen mpmath's correctly rounded double, or
# where the design rounds otherwise the design's own, emulated apart from
# the interpreter in C and in Python with exact fractions, and said so.
>> atan(1)
~0.785398163

>> atan(-1)
~-0.785398163

>> atan(1/2)
~0.463647609

>> atan(2)
~1.10714872

>> atan(1/10)
~0.0996686525

>> atan(10)
~1.47112767

>> atan(-1000000)
~-1.57079533

# Machin's formula, pi/4 = 4 atan(1/5) - atan(1/239).
>> 16*atan(1/5) - 4*atan(1/239)
~3.14159265

# A double's 0, as sin(0) is: a clause for 0 alone would make it exact, and
# grad refuses a clause that holds at a point alone.
>> atan(0)
~0

>> frac atan(0)
error: ~0 was approximated, so it has no exact fraction

# atan(1) is pi/4 to the double, and so a quarter of pi, the built-in.
>> 4*atan(1) == pi
~1

>> atan(10^400)
~1.57079633

>> atan(~1e300)
~1.57079633

>> atan(~1/0)
~1.57079633

>> atan(-1/~0)
~-1.57079633

# Below the least normal double atan(x) is x.
>> atan(~(2^-1074)) == ~(2^-1074)
~1

# Odd by its guard, exactly; and a 0 has the definition's sign, as sin's:
# atan(-0) is +0, where C's is -0.
>> atan(~(-2.5)) + atan(~2.5)
~0

>> 1/atan(~0*(-1))
~inf

>> atan(0/~0)
error: a comparison needs a number, not -nan

>> atan(i)
error: atan needs real numbers, not ~(i)

>> atan([1 2])
error: atan needs single values, not a 1x2 matrix; write it by its cells

# atan2(y, x) as C and the papers write it, y first: the angle of the point
# (x, y), in (-pi, pi], every quadrant.
>> atan2(1, 1)
~0.785398163

>> atan2(1, -1)
~2.35619449

>> atan2(-1, -1)
~-2.35619449

>> atan2(-1, 1)
~-0.785398163

>> atan2(4, 3)
~0.927295218

>> atan2(3, -4)
~2.49809154

>> atan2(-3, -4)
~-2.49809154

>> atan2(-4, 3)
~-0.927295218

# The axes, each the double nearest its angle; the negative x axis is pi,
# the principal value, from above.
>> atan2(0, 1)
~0

>> atan2(1, 0) == pi/2
~1

>> atan2(-1, 0) == -pi/2
~1

>> atan2(0, -1) == pi
~1

# The origin has no angle: refused, where C answers 0.
>> atan2(0, 0)
error: atan2 needs y or x other than 0

# A double's -0 is 0, as everywhere in the language: atan2(-0, -1) is pi,
# where C's is -pi, atan2(-0, 1) is +0, and (0, -0) is the origin, where
# C's is pi.
>> atan2(~0*(-1), -1) == pi
~1

>> 1/atan2(~0*(-1), 1)
~inf

>> atan2(0, ~0*(-1))
error: atan2 needs y or x other than 0

>> atan2(1, ~0*(-1)) == pi/2
~1

# Exact arguments are divided exactly, and the quotient reduced exactly.
>> atan2(2, 6) == atan(1/3)
~1

# Far from 1 the quotient leaves the doubles: its angle is the limit's.
>> atan2(~1e300, ~1e-300) == pi/2
~1

>> atan2(~1e-300, ~1e300)
~0

>> atan2(~1/0, 1) == pi/2
~1

>> atan2(1, -1/~0) == pi
~1

# Two infinities give no direction, refused as NaN is (DESIGN.md).
>> atan2(~1/0, ~1/0)
error: a comparison needs a number, not -nan

>> atan2(1, i)
error: a comparison needs real numbers, not ~(i)

# Either argument complex, refused by a comparison before any atan, as the
# prelude's other functions of two arguments are (C290).
>> atan2(1 + i, 1)
error: a comparison needs real numbers, not ~(1+i)

>> atan2(i, 0)
error: a comparison needs real numbers, not ~(i)

>> atan2(1, [1 2])
error: atan2 needs single values, not a 1x2 matrix; write it by its cells

>> atan2(1)
error: atan2 expects 2 arguments, got 1

# asin and acos, principal values in [-pi/2, pi/2] and [0, pi].
>> asin(1/2)
~0.523598776

>> asin(-1/2)
~-0.523598776

>> acos(1/2)
~1.04719755

>> acos(-1/2)
~2.0943951

>> asin(1/3)
~0.339836909

>> acos(2/3)
~0.841068671

>> asin(0)
~0

>> acos(1)
~0

>> asin(1) == pi/2
~1

>> asin(-1) == -pi/2
~1

>> acos(0) == pi/2
~1

>> acos(-1) == pi
~1

# Near 1 and -1, where acos is the root of 2(1 - x) and pi/2 - asin(x)
# would keep none of its digits: 1 - x is exact there.
>> acos(1 - 10^-10)
~1.41421356e-05

>> asin(1 - 10^-10)
~1.57078218

>> acos(-1 + 10^-10)
~3.14157851

>> acos(~(1 - 2^-53))
~1.49011612e-08

>> asin(~(1 - 2^-53))
~1.57079631

>> acos(~(-1 + 2^-53))
~3.14159264

>> asin(~(2^-1074)) == ~(2^-1074)
~1

>> acos(~(2^-1074)) == pi/2
~1

# Past 1 either way, refused in plain words as log refuses 0: no complex
# angle, as the prelude's functions are real. An exact number just past 1
# is refused, its double 1 is not; a number is shown to nine digits in a
# message, as everywhere.
>> asin(2)
error: asin needs a number between -1 and 1, not 2

>> acos(~(-1.5))
error: acos needs a number between -1 and 1, not ~-1.5

>> asin(1 + 10^-30)
error: asin needs a number between -1 and 1, not ~1

>> asin(~(1 + 10^-30)) == pi/2
~1

>> asin(i)
error: asin needs real numbers, not ~(i)

>> acos(0/~0)
error: a comparison needs a number, not -nan

# acosh, from 1, the hyperbolic anomaly Apollo 11's coast asked for.
>> acosh(2)
~1.3169579

>> acosh(3/2)
~0.96242365

>> acosh(10)
~2.99322285

>> acosh(1)
~0

>> acosh(1 + 10^-10)
~1.41421356e-05

>> acosh(17/16)
~0.35173739

>> acosh(10^10)
~23.7189981

>> acosh(10^400)
~921.727184

>> acosh(~1e300)
~691.468675

>> acosh(~(2^26))
~18.7149739

>> acosh(~1/0)
~inf

>> acosh(1/2)
error: acosh needs a number at least 1, not 0.5

>> acosh(1 - 10^-30)
error: acosh needs a number at least 1, not ~1

>> acosh(-1/~0)
error: acosh needs a number at least 1, not ~-inf

>> acosh(i)
error: acosh needs real numbers, not ~(i)

>> digits = 17
digits = 17

>> atan(1)
~0.78539816339744828

>> atan(~0.5)
~0.46364760900080609

>> atan(~2.5)
~1.1902899496825317

>> atan(1/10^10)
~1e-10

# Each threshold of the reduction, 17/64, 3/4, 11/8 and 15/4, is taken by
# the branch below it.
>> atan(~0.265625)
~0.25962962940825751

>> atan(~0.75)
~0.64350110879328437

>> atan(~3.75)
~1.3101939350475555

>> atan(~1e16)
~1.5707963267948966

# Not correctly rounded: mpmath's are 0.94200004037946361 and
# 0.32175055439664219. 1/3 is reduced exactly to -1/7, and rounded once,
# there; the error is the series' and the sum's, 0.51 and 0.86 units.
>> atan(~1.375)
~0.94200004037946372

>> atan(1/3)
~0.32175055439664224

# The farthest of 10^8 doubles a range from mpmath's -0.26275820120790777:
# 1.42 units, just above 17/64 where atan(1/2) less atan(u) cancels most.
>> atan(~(-0.2689771099460534))
~-0.26275820120790783

>> atan2(1, -1)
~2.3561944901923448

>> atan2(-1, -1)
~-2.3561944901923448

>> atan2(4, 3)
~0.92729521800161219

>> atan2(3, -4)
~2.4980915447965089

>> atan2(-3, -4)
~-2.4980915447965089

>> asin(1/2)
~0.52359877559829893

>> asin(~(1 - 2^-53))
~1.5707963118937354

>> acos(~(-1 + 2^-53))
~3.1415926386886319

# Not correctly rounded: mpmath's are 1.0471975511965979,
# 1.4142135623848802e-05 and 1.4901161193847656e-08, 2^-26: 0.52, 0.64 and
# 0.96 units. And asin's farthest of 10^8 doubles a range, 2.44 units from
# mpmath's 0.24546399978245742, its root and quotient each rounded.
>> acos(1/2)
~1.0471975511965976

>> acos(1 - 10^-10)
~1.4142135623848801e-05

>> acos(~(1 - 2^-53))
~1.490116119384766e-08

>> asin(~0.24300644216948256)
~0.24546399978245736

# acosh either side of 17/16, the series below it; then, not correctly
# rounded, mpmath's being 0.3517373900432606, 1.3169578969248168,
# 1.41421356236131e-05 and 2.1073424255447014e-08, each 0.55 to 0.63 units
# off; and the farthest of 10^8 doubles a range, 3.19 units from mpmath's
# 0.36819443047396117, log's own error near its fold at the root of 2.
>> acosh(~1.0624999999999998)
~0.35173739004325993

>> acosh(~1.0625)
~0.35173739004326054

>> acosh(~2)
~1.3169578969248166

>> acosh(1 + 10^-10)
~1.4142135623613098e-05

>> acosh(~(1 + 2^-52))
~2.1073424255447017e-08

>> acosh(~1.0685528068531271)
~0.368194430473961

>> digits = 9
digits = 9

# grad differentiates the definitions, whose parts are 1/(1 + x^2),
# 1/root(1 - x^2) and 1/root(x^2 - 1) to a few units; at a threshold of
# atan's reduction the branch below it, continuous there.
>> grad_(x = 0) atan(x)
~1

>> grad_(x = 1) atan(x)
~0.5

>> grad_(x = 10) atan(x)
~0.0099009901

>> grad_(x = 3/4) atan(x)
~0.64

>> grad_(x = -1/2) atan(x)
~0.8

>> grad_(x = 1) grad_(y = x) atan(y)
~-0.5

# atan2's partials, x/(x^2 + y^2) and -y/(x^2 + y^2), across the axes:
# pi/2 less atan(x/y) on the y axis, and on the negative x axis the clause
# from above.
>> grad_(y = 1) atan2(y, 1)
~0.5

>> grad_(x = 1) atan2(1, x)
~-0.5

>> grad_(x = 0) atan2(1, x)
~-1

>> grad_(y = 0) atan2(y, -1)
~-1

>> grad_(x = 0) atan2(0, x)
error: atan2 needs y or x other than 0

# Far from the axes' diagonals the quotient is of the smaller coordinate by
# the larger, so its partial does not overflow: -1 here, as at x = 0, where
# atan(1/x) had 0 times inf (C290).
>> grad_(x = ~1e-300) atan2(1, x)
~-1

>> grad_(x = ~(-1e-300)) atan2(1, x)
~-1

>> grad_(x = ~1e-300) atan2(-1, x)
~1

>> grad_(x = ~(-1e-300)) atan2(-1, x)
~1

>> grad_(y = ~1) atan2(y, ~1e-300)
~1e-300

>> grad_(y = ~1e-300) atan2(y, 1)
~1

>> grad_(y = ~1e-300) atan2(y, -1)
~-1

>> grad_(x = ~1) atan2(~1e-300, x)
~-1e-300

>> grad_(x = ~(-1)) atan2(~(-1e-300), x)
~1e-300

>> grad_(x = 0) asin(x)
~1

>> grad_(x = 1/2) asin(x)
~1.15470054

>> grad_(x = 1/2) acos(x)
~-1.15470054

>> grad_(x = 2) acosh(x)
~0.577350269

>> grad_(x = 1 + 10^-10) acosh(x)
~70710.6781

# Where the part is infinite, refused as a root's is.
>> grad_(x = 1) asin(x)
error: a power's derivative is infinite at x = 1

>> grad_(x = -1) acos(x)
error: a power's derivative is infinite at x = -1

>> grad_(x = 1) acosh(x)
error: a power's derivative is infinite at x = 1

>> grad_(x = 3) asin(x)
error: asin needs a number between -1 and 1, not 3

# atan, asin, acos and acosh of a real double are called compiled, as sin
# is: one step and one reference deep. Through their definitions each nests
# two to six references deeper; an exact argument walks them. The sums are
# mpmath's.
>> sum_(k=1)^40000 atan(~k/40000)
~17553.3756

>> sum_(k=1)^40000 asin(~k/40000)
~22832.6399

>> dive(k) = dive(k - 1)
dive(k) = dive(k - 1)

>> dive(k) | k < 1 = atan(~3)
dive(k) | k < 1 = atan(~3)

>> dive(254)
~1.24904577

>> dive(k) | k < 1 = asin(~0.5)
dive(k) | k < 1 = asin(~0.5)

>> dive(254)
~0.523598776

>> dive(k) | k < 1 = acosh(~2)
dive(k) | k < 1 = acosh(~2)

>> dive(254)
~1.3169579

>> dive(k) | k < 1 = atan(3)
dive(k) | k < 1 = atan(3)

>> dive(254)
error: evaluation nests more than 256 references deep

# Doyle (1978), the regulator alone at q = 0: its loop gain L = fs/(s - 1)^2
# crosses |L| = 1 at w = (f - root(f^2 - 4))/2, where Re L = -2/f, so the
# phase margin is acos(2/f), 60 degrees at f = 4 and toward 90 as f grows;
# and from L itself, 180 less its angle. Worked out by hand and with mpmath.
>> A = [1 1; 0 1]
A = [1 1; 0 1]

>> B = [0; 1]
B = [0; 1]

>> G = [1; 1]
G = [1; 1]

>> L(f, s) = f*G'*(s*A^0 - A)^-1*B
L(f, s) = f*G'*(s*A^0 - A)^-1*B

>> wc(f) = (f - (f^2 - 4)^(1/2))/2
wc(f) = (f - (f^2 - 4)^(1/2))/2

>> pm(f) = acos(2/f)*180/pi
pm(f) = acos(2/f)*180/pi

>> pm(4)
~60

>> 180 - atan2(im(L(4, i*wc(4))), re(L(4, i*wc(4))))*180/pi
~60

>> pm(5)
~66.4218215

>> 180 - atan2(im(L(5, i*wc(5))), re(L(5, i*wc(5))))*180/pi
~66.4218215

>> pm(10^6)
~89.9998854

>> grad_(f = 4) pm(f)
~8.26993343

# Apollo 11's state at the first midcourse correction's cutoff, table 7-II
# of the Mission Report as apollo11.ink builds it, read back: its
# longitude, its flight path angle, asin of the radial part of the
# velocity, as the coast reads it about the Moon, and its heading, the
# angle of the velocity from north toward east. Each is the table's.
>> use apollo11 (ecef, vecef, north, east, nrm, deg, nmi, ft)
use apollo11 (ecef, vecef, north, east, nrm, deg, nmi, ft)

>> r = ecef(~6.00, ~-11.17, ~109477.2*nmi)
r = ecef(~6.00, ~-11.17, ~109477.2*nmi)

>> v = vecef(~6.00, ~-11.17, ~109477.2*nmi, ~5010.0*ft, ~76.88, ~120.87)
v = vecef(~6.00, ~-11.17, ~109477.2*nmi, ~5010.0*ft, ~76.88, ~120.87)

>> atan2(r[2], r[1])/deg
~-11.17

>> asin(r'*v/(nrm(r)*nrm(v)))/deg
~76.88

>> atan2(east(~-11.17)'*v, north(~6.00, ~-11.17, ~109477.2*nmi)'*v)/deg
~120.87

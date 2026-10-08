# The H-infinity norm of a stable state-space system, in the prelude
# (DESIGN.md). Every value was worked out apart from the
# interpreter, as its specification: the norm exactly with sympy for one
# input and one output, from the critical points of |G(iw)|^2 or of
# |G(e^(iw))|^2, and with mpmath at 60 digits for more, a sweep then a
# golden section; python-control's linfnorm agrees within 1e-14 but at a
# time scale of 10^-200 and past a double's range, where it fails. The
# bisection was transcribed in Python's fractions, every test decided from
# that norm, and its double printed by a transcription of Number::Shown; an
# inexact or marked answer is the norm to the digits shown.

# ||G||_inf of G(s) = C(sI - A)^-1 B + D, the largest singular value of
# G(iw) over every frequency. D may be left out. 3/(s + 2) peaks at w = 0,
# 3/2, reached exactly by a halving and so itself.
>> hinf(-2, 1, 3)
1.5

# 1/(s + 3): 1/3, which no double holds. Normalised by its input, its
# norm is 1, which a halving reaches; the third scaling it back is rounded.
>> hinf(-3, 1, 1)
~0.333333333

# A resonance, 1/(s^2 + s/2 + 1), damping 1/4: 8/sqrt(15) at w = sqrt(7/8),
# which the magnitude the paper writes, |T(iw)|, gives there too.
>> hinf([0 1; -1 -1/2], [0; 1], [1 0])
~2.06559112

>> w = (7/8)^(1/2)
w = (7/8)^(1/2)

>> abs(1/((i*w)^2 + i*w/2 + 1))
~2.06559112

>> digits = 15
digits = 15

>> hinf([0 1; -1 -1/2], [0; 1], [1 0])
~2.06559111797729

>> digits = 9
digits = 9

# s/(s + 1) rises to 1 as w grows and never reaches it: the norm is D's.
>> hinf(-1, -1, 1, 1)
1

# 1 + 1/(s + 2) falls from 3/2 at w = 0.
>> hinf(-2, 1, 1, 1)
1.5

# 22 + 3/(s + 1): |G(iw)|^2 = (625 + 484w^2)/(1 + w^2) falls from 25 at
# w = 0. It was an ulp above, its root rounded before 22 scaled it back
# (DESIGN.md, C216).
>> hinf(-1, 1, 3, 22)
25

# Two inputs and two outputs: (sI - A)^-1 of a normal A, whose largest
# singular value is 1 over the distance from iw to the nearest eigenvalue,
# -1 + 2i, so 1 at w = 2.
>> hinf([-1 2; -2 -1], [1 0; 0 1], [1 0; 0 1])
1

# With D, peaking at w = 2.32459288 (mpmath: 3.723454759512796; linfnorm
# 3.723454759512783), and its first output alone, at w = 2.39330777.
>> hinf([-2 -3; 2 -1], [-1 1; 0 2], [1 2; 0 -1], [-1 1; 0 -1])
~3.72345476

>> hinf([-2 -3; 2 -1], [-1 1; 0 2], [1 2], [-1 1])
~3.24470738

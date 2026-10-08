# The H-infinity norm of a stable state-space system, in the prelude
# (DESIGN.md, next in line). Every value was worked out apart from the
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

# The norm is the transfer function's: a slow mode B cannot reach adds
# nothing, and G = 0 is certified 0 without a bisection, as is a system
# whose one mode B reaches C cannot see. G = D is D's largest singular
# value.
>> hinf([-1 0; 0 -1/1000000], [1; 0], [1 1])
1

>> hinf(-1, 0, 1)
0

>> hinf([-1 0; 0 -2], [1; 0], [0 1])
0

>> hinf(-1, 1, 0, -3)
3

# Normalised by its largest cells, exactly, so a scale is no cost: an
# output in other units, a time scale, an input.
>> hinf([0 1; -1 -1/2], [0; 1], [1 0]*10^200)
~2.06559112e+200

>> hinf([0 1; -1 -1/2]/10^200, [0; 1]/10^200, [1 0])
~2.06559112

>> hinf([0 1; -1 -1/2], [0; 1]/10^200, [1 0])
~2.06559112e-200

>> hinf(-1, 1, 1, 10^200)
~1e+200

# Past a double's range, the double ~ makes of it, inf or 0, marked though
# every test is exact, as eig's is.
>> hinf([0 1; -1 -1/2], [0; 1], [1 0]*10^400)
inf  # approximated past a thousand digits

>> hinf([0 1; -1 -1/2], [0; 1], [1 0]/10^400)
0  # approximated past a thousand digits

# Tests whose numbers pass a thousand digits are approximated, and the
# answer marked: a damping of 1/4 - 7^-600/2, whose polynomial's
# coefficients have a thousand digits from the first test.
>> hinf([0 1; -1 -1/2 + 1/7^600], [0; 1], [1 0])
~2.06559112  # approximated past a thousand digits

# Time scales 10^200 apart, 1/(s + 1) + 1/(s + 10^-200): 1 + 10^200 at
# w = 0, its tests past a thousand digits.
>> hinf([-1 0; 0 -10^-200], [1; 1], [1 1])
~1e+200  # approximated past a thousand digits

# The discrete-time norm, over the unit circle: 1/(z - 1/2) peaks at
# z = 1, 1/(z + 1/2) at z = -1, both 2, and 1/(z^2 - z + 1/2) at 2 sqrt(2).
>> dhinf(1/2, 1, 1)
2

>> dhinf(-1/2, 1, 1)
2

>> dhinf([0 1; -1/2 1], [0; 1], [1 0])
~2.82842712

# Unstable, refused in words: the norm of G is that of a stable A. A
# pole on the axis, an integrator or an undamped pair, makes it infinite,
# and is refused alike rather than answered inf, as the pole may cancel; so
# is an unstable mode that cancels, though G = 1/(s + 1).
>> hinf(1, 1, 1)
error: hinf needs every eigenvalue of A left of the imaginary axis

>> hinf(0, 1, 1)
error: hinf needs every eigenvalue of A left of the imaginary axis

>> hinf([0 1; -1 0], [0; 1], [1 0])
error: hinf needs every eigenvalue of A left of the imaginary axis

>> hinf([-1 0; 0 1], [1; 0], [1 1])
error: hinf needs every eigenvalue of A left of the imaginary axis

>> dhinf(2, 1, 1)
error: dhinf needs every eigenvalue of A inside the unit circle

>> dhinf(1, 1, 1)
error: dhinf needs every eigenvalue of A inside the unit circle

>> dhinf(-1, 1, 1)
error: dhinf needs every eigenvalue of A inside the unit circle

# An inexact system is bisected by rounded tests, as rho's matrix is, and
# is good to many more digits than shown at this size: 8/sqrt(15) again,
# 1/sqrt(2), and a discretised lag, e^(-1/2) the pole of 1/(s + 1) sampled
# every 1/2, 1/(1 - e^(-1/2)) at z = 1.
>> hinf([0 1; -1 ~-0.5], [0; 1], [1 0])
~2.06559112

>> hinf(-2^(1/2), 1, 1)
~0.707106781

>> dhinf(exp(-1/2), 1, 1)
~2.54149408

# A cell that is itself inf has no value to scale, as rho's has not (C206).
>> hinf(-1, ~10^310, 1)
error: hinf needs finite cells, not inf

# A complex cell in max's words, as rho refuses one.
>> hinf(-1, i, 1)
error: a comparison needs real numbers, not i

# Sizes by the signature. A 0 for a D of two columns is a single value.
>> hinf([1 2 3], 1, 1)
error: hinf takes A[j<=n, k<=n], not a 1x3 matrix

>> hinf([-1 0; 0 -2], [1; 1; 1], [1 1])
error: hinf takes A[j<=n, k<=n] and B[j<=n, k<=m], not a 2x2 matrix and a 3x1 matrix

>> hinf(-1, [1 1], 1, 0)
error: hinf takes B[j<=n, k<=m] and D[j<=p, k<=m], not a 1x2 matrix and a single value

# A bisection is a staircase in the system: refused where it moves, as
# rho is.
>> grad_(a = 1) hinf(-a, 1, 1)
error: grad cannot differentiate hinf yet

>> grad_(a = 1/2) dhinf(a, 1, 1)
error: grad cannot differentiate dhinf yet

>> grad_(a = 2) a*hinf(-1, 1, 1)
1

# The names are the prelude's, which a session may take for itself.
>> hinf = 7
hinf = 7

>> hinf*2
14

>> clear hinf
clear hinf

>> hinf(-1, 1, 1)
1

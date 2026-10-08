# What the H-infinity norm reads and refuses: units far apart, a double's
# range, a thousand digits, inexact and infinite cells, unstable systems,
# sizes, grad and the names. Worked out apart from the interpreter, as
# hinf.ink says.

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

# Time scales 10^100 apart, 1/(s + 1) + 1/(s + 10^-100): 1 + 10^100 at
# w = 0, its tests past a thousand digits. At 10^200 apart their doubles
# could not hold them, and it is refused rather than answered from them.
>> hinf([-1 0; 0 -10^-100], [1; 1], [1 1])
~1e+100  # approximated past a thousand digits

>> hinf([-1 0; 0 -10^-200], [1; 1], [1 1])
error: hinf needs tests within a double's range, by a guard approximated past a thousand digits

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

# An inexact cell is read as the exact rational its double is, so the
# answer is certified for the data as stored, or marked: 8/sqrt(15) again,
# -0.5 being a double exactly; 1 over sqrt(2)'s double; and a discretised
# lag, a = e^(-1/2) the pole of 1/(s + 1) sampled every 1/2, peaking at
# z = 1, 1/(1 - a) for a's double to its last digit, where rounded tests
# answer ...7989.
>> hinf([0 1; -1 ~-0.5], [0; 1], [1 0])
~2.06559112

>> hinf(-2^(1/2), 1, 1)
~0.707106781

>> digits = 17
digits = 17

>> dhinf(exp(-1/2), 1, 1)
~2.5414940825367984

>> digits = 9
digits = 9

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

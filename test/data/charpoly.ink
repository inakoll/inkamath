# The characteristic polynomial, Routh's and the Schur-Cohn tests, and the
# spectral radius and abscissa, in the prelude, written in inkamath
# (DESIGN.md, next in line). Every value was worked out apart from the
# interpreter, as its specification: polynomials and roots exactly with
# sympy, eigenvalues with mpmath at 60 digits, and each bracket by bisecting
# in Python's fractions with every test decided from the exact eigenvalue.

# Faddeev and LeVerrier's characteristic polynomial, det(lambda*I - A), as a
# column, highest power first, exactly.
>> charpoly([1 2; 3 4])
[ 1;
 -5;
 -2]

>> charpoly([2 0 0; 0 3 4; 0 4 9])
[  1;
 -14;
  35;
 -22]

# A companion matrix gives back the polynomial it was built from, (s + 1)^4.
>> charpoly([-4 -6 -4 -1; 1 0 0 0; 0 1 0 0; 0 0 1 0])
[1;
 4;
 6;
 4;
 1]

>> frac charpoly([1 1/2 1/3; 1/2 1/3 1/4; 1/3 1/4 1/5])
[      1;
  -23/15;
 127/720;
 -1/2160]

# A single value is a 1x1 matrix.
>> charpoly(5)
[ 1;
 -5]

# Complex entries are taken, the polynomial being exact over them.
>> charpoly([1 i; -i 1])
[ 1;
 -2;
  0]

>> charpoly([i 0; 0 2])
[   1;
 -2-i;
  i*2]

>> charpoly([1 2 3])
error: charpoly takes A[j<=n, k<=n], not a 1x3 matrix

# Its last coefficient is det(A) for n even, whose gradient is the cofactor
# matrix.
>> grad_(A = [1 2; 3 4]) charpoly(A)[3]
[ 4, -3;
 -2,  1]

# Routh's test: every root in Re s < 0, strictly, so a root on the axis
# fails it. (s + 1)^3, then (s + 1)(s^2 + 1).
>> hurwitz([1; 3; 3; 1])
1

>> hurwitz([1; 1; 1; 1])
0

# Third order: stable where a1*a2 > a0*a3.
>> hurwitz([1; 2; 1; 1])
1

>> hurwitz([1; 1; 1; 2])
0

# Every fifth root of unity but 1: positive coefficients, two roots to the
# right, and a zero in Routh's first column.
>> hurwitz([1; 1; 1; 1; 1])
0

>> hurwitz([1; 5; 10; 10; 5; 1])
1

# A polynomial's sign does not move its roots.
>> hurwitz([-1; -3; -3; -1])
1

# A leading 0 is a root at infinity, which no half-plane holds; the zero
# polynomial vanishes everywhere; a nonzero constant has no root to fail.
>> hurwitz([0; 1; 2])
0

>> hurwitz([0])
0

>> hurwitz([5])
1

# Every root in Re s < s0: s^2 + s + 1 has its roots on Re s = -1/2.
>> hurwitz([1; 1; 1], -1/2)
0

>> hurwitz([1; 1; 1], -49/100)
1

# A damped oscillator, its rate of decay 1/10: stable, decaying faster than
# 1/20, not faster than 1/10; and undamped.
>> hurwitz(charpoly([0 1; -1 -1/5]))
1

>> hurwitz(charpoly([0 1; -1 -1/5]), -1/20)
1

>> hurwitz(charpoly([0 1; -1 -1/5]), -1/10)
0

>> hurwitz(charpoly([0 1; -1 0]))
0

# A test is flat where it does not jump.
>> grad_(a = 2) hurwitz([1; a; 1])
0

>> hurwitz([1 2 3])
error: hurwitz takes p[j<=m], not a 1x3 matrix

# Refused in the words max refuses it with.
>> hurwitz([1; i])
error: a comparison needs real numbers, not i

# A complex s is named, not a cell of the shifted polynomial, 1/2 + i.
>> hurwitz([1; 1/2], i)
error: a comparison needs real numbers, not i

# The Schur-Cohn test: every root in |z| < 1, strictly, or in |z| < r.
>> schurcohn([1; -1/2])
1

>> schurcohn([1; -1])
0

>> schurcohn([1; 0; 1/4])
1

>> schurcohn([1; 0; 1])
0

>> schurcohn([1; 0; 1], 2)
1

# Roots -1 and -1/2.
>> schurcohn([2; 3; 1])
0

>> schurcohn([1; -1/2], 1/2)
0

>> schurcohn([0; 1; 1/2])
0

# No root is in a disc of radius 0, and no disc has a negative one.
>> schurcohn([1; 1/2], 0)
0

>> schurcohn([1; 1/2], -1)
error: no clause of schurcohn applies

# A discrete-time system is stable where every eigenvalue is in the unit
# disc: 1/2 and -3/4; then 1/2 + i and 1/2 - i, of modulus sqrt(5)/2 =
# 1.1180339887.
>> schurcohn(charpoly([1/2 1/4; 0 -3/4]))
1

>> schurcohn(charpoly([1/2 1; -1 1/2]))
0

>> schurcohn(charpoly([1/2 1; -1 1/2]), 1118/1000)
0

>> schurcohn(charpoly([1/2 1; -1 1/2]), 1119/1000)
1

# Refused on the polynomial written, not on a cell of its image.
>> schurcohn([1; i])
error: a comparison needs real numbers, not i

# The spectral radius by bisection on the Schur-Cohn test, from [0, B), B
# the power of two above the sum of |A[j,k]|: rhob(A)_m is the bracket after
# m halvings, its lower end at most rho and its upper end above it, exact,
# and the answer its lower end, inexact, once the bracket is within 2^-53 of
# its end nearer 0: after 64 halvings at least and 256 at most.
>> rho([1/2 1; -1 1/2])
~1.11803399

>> frac rhob([1/2 1; -1 1/2])_64
[5156021714044493573/4611686018427387904;
 2578010857022246787/2305843009213693952]

# Each halving halves it, B = 4 here; past the 64th, only while it is
# wider than 2^-53 of its end nearer 0, which this one no longer is.
>> frac rhob([1/2 1; -1 1/2])_40[2] - rhob([1/2 1; -1 1/2])_40[1]
1/274877906944

>> frac rhob([1/2 1; -1 1/2])_70[2] - rhob([1/2 1; -1 1/2])_70[1]
1/4611686018427387904

# The spectral abscissa by Routh's test, from [-B, B): that of the damped
# oscillator, -1/10, below.
>> abscissa([0 1; -1 -1/5])
~-0.1

>> frac abscissab([0 1; -1 -1/5])_64
[  -57646075230342349/576460752303423488;
 -230584300921369395/2305843009213693952]

>> digits = 17
digits = 17

>> rho([1/2 1; -1 1/2])
~1.1180339887498949

>> abscissa([0 1; -1 -1/5])
~-0.10000000000000001

>> digits = 9
digits = 9

# A bound on the boundary fails its strict test and becomes the lower end,
# so a value a halving reaches is the answer, every digit of it.
>> rho([2 0; 0 -3])
3

>> abscissa([2 0; 0 -3])
2

>> abscissa([1/2 1; -1 1/2])
0.5

# A value far below B takes more halvings: unstable by 10^-30, an abscissa
# is not 0, and a radius of 10^-15 has every digit.
>> abscissa([1/10^30 0; 0 -1])
~1e-30

>> rho([0 1; 1/10^30 0])
~1e-15

>> rho(-1/3)
~0.333333333

>> abscissa(-1/3)
~-0.333333333

# Scaled by a power of two, every bracket is.
>> rho(2*[1/2 1; -1 1/2]) == 2*rho([1/2 1; -1 1/2])
1

# The tests are of A/B, exactly, and the bracket scaled back by B, so a
# small matrix or a large one tests numbers a double holds: rounded, the
# coefficients of [1 2; 3 4]/10^100's polynomial would underflow.
>> rho(10^-300)
~1e-300

>> rho([~1 2; 3 4]/10^100)*10^100
~5.37228132

# Exact, unscaled, their tests would pass a thousand digits and be rounded
# to doubles that underflow, or overflow to a NaN.
>> rho([1 2; 3 4]/10^150)
~5.37228132e-150

>> rho([1 2; 3 4]*10^250)
~5.37228132e+250

# B need not be a double: the bracket is of A/B inside, and A is scaled to
# it and back by B's power in two halves, each a double (C190).
>> rho(~1e308)
~1e+308

>> abscissa(~-1e308)
~-1e+308

>> rho([~1 2; 3 4]*10^307)
~5.37228132e+307

# Nor need the sum of |A[j,k]|: past a double, it is summed a power of 4
# per row smaller, 2e308 here, and rho is (5 + 33^(1/2))/2 times 2e307,
# 1.07e308, as is abscissa (C196).
>> rho([~1 2; 3 4]*2*10^307)
~1.07445626e+308

>> abscissa([~1 2; 3 4]*2*10^307)
~1.07445626e+308

# Past a double's range, the answer is the double ~ makes of it, marked, as
# A/B's tests are past a thousand digits (C191).
>> rho([1 2; 3 4]*10^400)
inf  # approximated past a thousand digits

>> rho([1 2; 3 4]/10^400)
0  # approximated past a thousand digits

# So is it where the tests stay exact: the bracket is certified, its double,
# subnormal or none, is not, and a stable system's abscissa of -10^-400 is
# no marginal 0 (C197).
>> rho(10^400)
inf  # approximated past a thousand digits

>> rho(10^-400)
0  # approximated past a thousand digits

>> rho(10^-320)
~9.99988867e-321  # approximated past a thousand digits

>> abscissa(-10^-400)
0  # approximated past a thousand digits

>> abscissa(-10^-400) < 0
0  # approximated past a thousand digits

# An end that is itself 0, certified, is not: its double is exact.
>> rhoa([0; 0], [0; 0])
0

# A cell that is itself inf is a double whose value is lost, and no radius
# is certified of it, inf or other: [1 x; 0 1]'s is 1 and [x x; -x -x]'s 0
# whatever x is (C206).
>> rho([1 ~10^310; 0 1])
error: rho needs finite cells, not inf

>> abscissa([~1 1; 1 1]*10^310)
error: abscissa needs finite cells, not inf

# Five by five, the tests exact; ten by ten, in bisection_digits.ink, they
# are not.
>> rho([6 -8 -6 -5 -6; 6 7 2 -9 -8; -3 -1 2 0 -4; -6 4 4 -9 -7; -1 -2 7 0 -2]/10)
~1.13016494

>> abscissa([6 -8 -6 -5 -6; 6 7 2 -9 -8; -3 -1 2 0 -4; -6 4 4 -9 -7; -1 -2 7 0 -2]/10)
~0.850451511

# An inexact matrix is bisected by rounded tests, so its bracket is inexact
# too and certifies nothing.
>> rho([~1/2 1; -1 1/2])
~1.11803399

>> frac rhob([~1/2 1; -1 1/2])_64
error: ~1.11803399 was approximated, so it has no exact fraction

# One approximated past a thousand digits marks the answer: rt_20 is within
# a double of sqrt(2).
>> rt_0 = 1
rt_0 = 1

>> rt_n = (rt_(n-1) + 2/rt_(n-1))/2
rt_n = (rt_(n-1) + 2/rt_(n-1))/2

>> rho([rt_20 0; 0 1/2])
~1.41421356  # approximated past a thousand digits

>> rho([1 2; 3 4; 5 6])
error: rho takes A[j<=n, k<=n], not a 3x2 matrix

>> abscissa([1 2 3])
error: abscissa takes A[j<=n, k<=n], not a 1x3 matrix

# In max's words, as hurwitz is.
>> rho([1 i; -i 1])
error: a comparison needs real numbers, not i

# The bisection is a staircase in A, flat between its steps, so its
# derivative is not rho's: refused, for now. A rho that does not move is
# a constant.
>> grad_(A = [1/2 1; -1 1/2]) rho(A)
error: grad cannot differentiate rho yet

>> grad_(a = 1/2) abscissa([a 1; -1 1/2])
error: grad cannot differentiate abscissa yet

>> grad_(a = 2) a*rho([1/2 1; -1 1/2])
~1.11803399

# The names are the prelude's: a session may take one for itself, as a
# density, and clear gives it back.
>> rho = 1000
rho = 1000

>> rho*2
2000

>> clear rho
clear rho

>> rho([2])
2

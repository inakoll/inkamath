# An exact root of a perfect power (DESIGN.md, next in line): x^(p/q), x
# and p/q exact, is exact where x is not negative and its reduced numerator
# and denominator are perfect q-th powers, whatever q; then it is that root
# to the whole power p, by the whole power's rules. Anything else is as
# before. Every value was worked out apart from the interpreter: each
# exact root in sympy, the inexact ones as the double C's sqrt or pow
# gives, checked against mpmath to the nine digits shown, and printed as
# Number::Shown prints, transcribed in Python.

# Exact, so a bare number, and a truth read from it exact (C241).
>> 25^(1/2)
5

>> 25^(1/2) == 5
1

>> (9/4)^(1/2)
1.5

# The queued example: 2/3 shows the same nine digits as before, but is
# exact.
>> (4/9)^(1/2)
~0.666666667

>> frac (4/9)^(1/2)
2/3

>> 8^(2/3)
4

>> (9/4)^(3/2)
3.375

# A negative exponent is the reciprocal: 27^(-1/3) is 1/3.
>> 27^(-1/3)
~0.333333333

>> frac 27^(-1/3)
1/3

>> 27^(-1/3) == 1/3
1

>> 25^(-1/2)
0.2

>> (4/9)^(-3/2)
3.375

# An exponent written as a decimal is the exact fraction it says.
>> 25^0.5
5

>> 16^0.25
2

>> 16^0.75
8

>> 8^0.5
~2.82842712

# Not a perfect power: approached, as before, and the root of 2 squared is
# not 2.
>> 2^(1/2)
~1.41421356

>> frac 2^(1/2)
error: ~1.41421356 was approximated, so it has no exact fraction

>> 2^(1/2)*2^(1/2) == 2
~0

# Both parts must be: 4 is a square and 3 is not.
>> (4/3)^(1/2)
~1.15470054

>> (3/4)^(1/2)
~0.866025404

# Any q: a whole number from 2 up has a q-th root only below its bit
# length, so a larger q is decided at once.
>> 1^(1/3)
1

>> 1024^(1/10)
2

>> 128^(3/7)
8

>> (2^1000)^(1/1000)
2

>> 1^(1/10^30)
1

>> 2^(1/10^30)
~1

# Zero: 0 to a positive power is 0, and to a negative one a division by an
# exact zero, as 0^-1 is.
>> 0^(1/2)
0

>> 0^(2/3)
0

>> 0^(-1/2)
error: division by zero

>> 0^-1
error: division by zero

# A negative base keeps its principal root (C33, C174): never real for a
# power that is not whole, so never exact. The principal cube root of -8
# is 2 e^(i pi/3), 1 + i sqrt(3).
>> (-8)^(1/3)
~(1+i*1.73205081)

>> (-4)^(1/2)
~(i*2)

>> (-1/4)^(1/2)
~(i*0.5)

# The real cube root a paper means is a definition in cases, exact where
# the cube is perfect.
>> cbrt(x) | x < 0 = -(-x)^(1/3)
cbrt(x) | x < 0 = -(-x)^(1/3)

>> cbrt(x) = x^(1/3)
cbrt(x) = x^(1/3)

>> cbrt(-8)
-2

>> cbrt(27/8)
1.5

>> cbrt(2)
~1.25992105

# Past 64 bits: 10^300 and 3^50 in full, and 10^-200 all of it.
>> (10^600)^(1/2)
1000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

>> (3^100)^(1/2)
717897987691852588770249

>> (10^-400)^(1/2)
1e-200

>> (10^600 + 1)^(1/2)
~1e+300

>> ((3*10^200)^2 + (4*10^200)^2)^(1/2) == 5*10^200
1

>> (2^1100)^(1/4) == 2^275
1

>> 4^(1001/2) == 2^1001
1

# C175's path, for bases past a double's range that are not perfect
# powers, as before.
>> (2*10^400)^(1/2)
~1.41421356e+200

>> (2*10^-400)^(1/2)
~1.41421356e-200

>> (2^1101)^(1/4)
~7.21948646e+82

>> ((3*10^200)^2 + (4*10^200)^2 + 1)^(1/2)
~5e+200

# The root is exact and its power past a thousand digits: (7/6)^1185,
# whose numerator has 1002, approximated as a whole power is.
>> (49/36)^(1185/2)
~2.14756201e+79  # approximated past a thousand digits

# A base approximated past a thousand digits is a double, and so is its
# root: (7/6)^600.
>> ((7/6)^1200)^(1/2)
~1.47256264e+40  # approximated past a thousand digits

# An inexact base or exponent: inexact, as before.
>> (~25)^(1/2)
~5

>> 25^(~1/2)
~5

# A matrix to a power that is not whole is refused, as before; cell by
# cell, each root is exact.
>> D = [4 0; 0 9]
D = [4 0; 0 9]

>> D^(1/2)
error: a matrix power must be a whole number, not 0.5

>> R[j<=2, k<=2] = D[j,k]^(1/2)
R[j<=2, k<=2] = D[j,k]^(1/2)

>> R
[2, 0;
 0, 3]

# A derivative at an exact point is exact where its power is: c x^(c-1)
# is 1/2 4^(-1/2), 2/3 8^(-1/3), and the second -1/4 4^(-3/2). A bar's
# length (x^2 + 16)^(1/2) moves by x/L, 3/5 at x = 3.
>> grad_(x = 4) x^(1/2)
0.25

>> frac grad_(x = 8) x^(2/3)
1/3

>> grad_(y = 4) grad_(x = y) x^(1/2)
-0.03125

>> grad_(x = 3) (x^2 + 4^2)^(1/2)
0.6

>> grad_(x = 2) x^(1/2)
~0.353553391

>> grad_(x = 0) x^(1/2)
error: a power's derivative is infinite at x = 0

# A truss's lengths from its coordinates. The 3-4-5 bar first, then
# truss_calfem.ink's three bars with their lengths computed where that
# golden types them; its displacements and forces are unchanged, worked
# again in sympy from the computed lengths.
>> (3^2 + 4^2)^(1/2)
5

>> X = [0 0; 0 6/5; 8/5 0; 8/5 6/5]
X = [0 0; 0 6/5; 8/5 0; 8/5 6/5]

>> C = [1 3; 3 4; 2 3]
C = [1 3; 3 4; 2 3]

>> dx(e) = X[C[e,2],1] - X[C[e,1],1]
dx(e) = X[C[e,2],1] - X[C[e,1],1]

>> dy(e) = X[C[e,2],2] - X[C[e,1],2]
dy(e) = X[C[e,2],2] - X[C[e,1],2]

>> L(e) = (dx(e)^2 + dy(e)^2)^(1/2)
L(e) = (dx(e)^2 + dy(e)^2)^(1/2)

>> [L(1); L(2); L(3)]
[1.6;
 1.2;
   2]

>> Ar = [6; 3; 10]/10^4
Ar = [6; 3; 10]/10^4

>> Em = 2*10^11
Em = 2*10^11

>> t(e) = [-dx(e) -dy(e) dx(e) dy(e)]
t(e) = [-dx(e) -dy(e) dx(e) dy(e)]

>> ke(e) = Em*Ar[e]/L(e)^3*t(e)'*t(e)
ke(e) = Em*Ar[e]/L(e)^3*t(e)'*t(e)

>> G(e)[a<=4, p<=8] = p == 2*C[e, ceil(a/2)] - mod(a, 2)
G(e)[a<=4, p<=8] = p == 2*C[e, ceil(a/2)] - mod(a, 2)

>> K = sum_(e=1)^3 G(e)'*ke(e)*G(e)
K = sum_(e=1)^3 G(e)'*ke(e)*G(e)

>> S[f<=2, p<=8] = p == f + 4
S[f<=2, p<=8] = p == f + 4

>> F[p<=8] | p == 6 = -80000
F[p<=8] | p == 6 = -80000

>> u = S'*(S*K*S')^-1*S*F
u = S'*(S*K*S')^-1*S*F

>> Nf(e) = Em*Ar[e]/L(e)^2*t(e)*G(e)*u
Nf(e) = Em*Ar[e]/L(e)^2*t(e)*G(e)*u

>> frac u[5]
-48/120625

>> frac u[6]
-139/120625

>> frac [Nf(1); Nf(2); Nf(3)]
[-5760000/193;
 11120000/193;
  7200000/193]

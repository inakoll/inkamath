# The parts and modulus of a complex number: re, im and abs extended
# (DESIGN.md, next in line). Every value was worked out apart from the
# interpreter, as its specification: exactly with sympy, and in Python's
# doubles where a double's range or a mark is the point.

# The paper's Re and Im, lowercase as every name here.
>> re(3+4*i)
3

>> im(3+4*i)
4

>> im(3-4*i)
-4

# Exact of an exact number: a real one is its real part, and its
# imaginary part is an exact 0.
>> frac re(-1/3)
-1/3

>> frac im(-1/3)
0

# A complex number is a pair of doubles, so its parts are inexact, as is
# anything an inexact number touches.
>> frac re(3+4*i)
error: 3 was approximated, so it has no exact fraction

>> frac im(~1/3)
error: 0 was approximated, so it has no exact fraction

# Cell by cell of a matrix, as floor is.
>> re([1+2*i, 3; -i, 1/2])
[1,   3;
 0, 0.5]

>> im([1+2*i, 3; -i, 1/2])
[ 2, 0;
 -1, 0]

# |z|, by abs: the double 5, all of it, so printed without '~'; inexact
# all the same.
>> abs(3+4*i)
5

>> frac abs(3+4*i)
error: 5 was approximated, so it has no exact fraction

>> abs(-2*i)
2

# The root of 2 correctly rounded, the same double as 2^(1/2) (C153).
>> abs(1+i)
~1.41421356

>> abs(1+i) == 2^(1/2)
1

# A real number's stays exact.
>> frac abs(-1/3)
1/3

# Far from 1: 5*10^200 and 5*10^-200, where the root of re^2 + im^2 in
# doubles is inf and 0.
>> abs(3*10^200 + 4*10^200*i)
~5e+200

>> abs(3*10^-200 + 4*10^-200*i)
~5e-200

# A part of an approximated number is marked, and so is its modulus,
# sqrt(13).
>> x = 2 + 0*10^-1000
x = 2 + 0*10^-1000

>> re(x + 3*i)
2  # approximated past a thousand digits

>> im(x + 3*i)
3  # approximated past a thousand digits

>> abs(x + 3*i)
~3.60555128  # approximated past a thousand digits

# abs is the prelude's, so a matrix is written by its cells.
>> abs([3+4*i, 1])
error: abs needs single values, not a 1x2 matrix; write it by its cells

# The frequency response of T(z) = 1/(z - 1/2) on the unit circle, whose
# magnitude is 2/sqrt(5 - 4 cos w): 2, 2/sqrt(3), 2/sqrt(5) and 2/3.
>> T(z) = 1/(z - 1/2)
T(z) = 1/(z - 1/2)

>> H(w) = abs(T(e^(i*w)))
H(w) = abs(T(e^(i*w)))

>> H(0)
2

>> H(pi/3)
~1.15470054

>> H(pi/2)
~0.894427191

>> H(pi)
~0.666666667

# In decibels: 20 log10(2/sqrt(5)).
>> 20*log(H(pi/2))/log(10)
~-0.96910013

# Sampled from 0 to pi by quarters; the second and fourth are
# sqrt((20 +- 8 sqrt(2))/17).
>> M[k<=5] = H(pi*(k-1)/4)
M[k<=5] = H(pi*(k-1)/4)

>> M
[           2;
  ~1.35719669;
 ~0.894427191;
 ~0.714813489;
 ~0.666666667]

# Its parts at pi/2: -2/5 and -4/5.
>> re(T(e^(i*pi/2)))
~-0.4

>> im(T(e^(i*pi/2)))
~-0.8

# Of a real variable, a part's derivative is the derivative's part: -12/25
# and 16/25.
>> grad_(w = pi/2) re(T(e^(i*w)))
~-0.48

>> grad_(w = pi/2) im(T(e^(i*w)))
~0.64

# The magnitude's slope, -4 sqrt(5)/25 at pi/2, and 0 at its peak, where
# T is real and T' = -4i is not: abs's real clauses take the real part
# (C201).
>> grad_(w = pi/2) H(w)
~-0.357770876

>> grad_(w = 0) H(w)
0

# A part has no derivative in a complex variable, nor has the modulus,
# refused in its own words rather than its guard's im's.
>> grad_(z = 1+i) re(z)
error: re has no complex derivative at z = 1+i

>> grad_(z = i) abs(z)
error: abs has no complex derivative at z = i

# A session's re is its own; the prelude's abs keeps the built-in.
>> re = 3000
re = 3000

>> abs(3+4*i)
5

>> clear re
clear re

>> re(2+i)
2

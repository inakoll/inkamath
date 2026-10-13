# conv and polyval in the prelude: a polynomial's product and its value,
# each polynomial a column, highest power first, as charpoly writes one
# (DESIGN.md, next in line). Every value was worked out apart from the
# interpreter, as its specification: the exact ones in Python's fractions,
# each product a second way by collecting powers and each value by a sum
# of powers, the inexact ones in mpmath at 30 digits.

# Columns of unequal length, in either order: c_r = sum of p_j q_(r+1-j).
>> conv([1; 2; 3], [1; -1])
[ 1;
  1;
  1;
 -3]

>> conv([1; 2; 3], [1; -1]) == conv([1; -1], [1; 2; 3])
1

# A single value is a column of one, a constant polynomial.
>> conv(2, 3)
6

>> conv(2, [1; 2; 3])
[2;
 4;
 6]

# The length is always a + b - 1: a leading zero is kept, and so is the
# zero polynomial's every cell.
>> conv([0; 1], [1; 1])
[0;
 1;
 1]

>> conv(0, [1; 2; 3])
[0;
 0;
 0]

# A row is MATLAB's convention, not this one's, and is refused rather than
# read as a column; so is anything else not a column.
>> conv([1; 2], [1 2])
error: conv takes q[k<=b], not a 1x2 matrix

>> conv([1 2 3], 1)
error: conv takes p[j<=a], not a 1x3 matrix

>> conv([1 2; 3 4], 1)
error: conv takes p[j<=a], not a 2x2 matrix

>> conv([1 2; 3 4;; 5 6; 7 8], 1)
error: conv takes p[j<=a], not a 2x2x2 tensor

# The value by Horner's rule, exactly of exact numbers. x^0 is 1, at 0
# too, so p(0) is the last coefficient.
>> polyval([1; -3; 2], 1)
0

>> polyval([1; -3; 2], 3)
2

>> polyval(5, 3)
5

>> polyval([1; 2; 3], 0)
3

>> frac polyval([1/2; 1/3; 1/4], 2/3)
25/36

>> polyval([1 2 3], 2)
error: polyval takes p[j<=m], not a 1x3 matrix

# Of a square matrix, the matrix polynomial, as a paper's p(A): A^2 and not
# each cell squared, the constant 2A + I and not 2A + 1 in every cell.
>> polyval([1; 0; 0], [1 2; 3 4])
[ 7, 10;
 15, 22]

>> polyval([2; 1], [1 2; 3 4])
[3, 4;
 6, 9]

# Cayley and Hamilton: a matrix is a root of its characteristic polynomial.
>> polyval(charpoly([1 2; 3 4]), [1 2; 3 4])
[0, 0;
 0, 0]

>> polyval([1; 2], [1 2 3])
error: polyval takes x[r<=n, c<=n], not a 1x3 matrix

# grad follows the definition: products and sums, x^0 included. At 2 and
# at 0 the derivative 3x^2 - 12x + 11 of (x - 1)(x - 2)(x - 3).
>> grad_(x = 2) polyval([1; -6; 11; -6], x)
-1

>> grad_(x = 0) polyval([1; -6; 11; -6], x)
11

# With respect to the coefficients, through conv: q(3)^2 moves by 2 q(3)
# [3; 1], q(3) = 5.
>> grad_(q = [1; 2]) polyval(conv(q, q), 3)
[30;
 10]

# The Control Tutorials for MATLAB (CTM), as the University of Alberta
# mirrors them under https://www.ece.ualberta.ca/~tchen/ctm/examples/, and
# test/data/ctms.ink on branch ctms. pend/invpen.html: the pendulum's
# angle over the force, num/den, exact.
>> M = 1/2
M = 1/2

>> m = 1/5
m = 1/5

>> b = 1/10
b = 1/10

>> In = 6/1000
In = 6/1000

>> g = 98/10
g = 98/10

>> l = 3/10
l = 3/10

>> q = (M+m)*(In+m*l^2) - (m*l)^2
q = (M+m)*(In+m*l^2) - (m*l)^2

>> num = [m*l/q; 0]
num = [m*l/q; 0]

>> den = [1; b*(In+m*l^2)/q; -(M+m)*m*g*l/q; -b*m*g*l/q]
den = [1; b*(In+m*l^2)/q; -(M+m)*m*g*l/q; -b*m*g*l/q]

>> p = In*(M+m) + M*m*l^2
p = In*(M+m) + M*m*l^2

>> A = [0, 1, 0, 0; 0, -(In+m*l^2)*b/p, m^2*g*l^2/p, 0; 0, 0, 0, 1; 0, -m*l*b/p, m*g*l*(M+m)/p, 0]
A = [0, 1, 0, 0; 0, -(In+m*l^2)*b/p, m^2*g*l^2/p, 0; 0, 0, 0, 1; 0, -m*l*b/p, m*g*l*(M+m)/p, 0]

# The state space's det(sI - A) is s den, the pole at the origin the page
# cancels put back; ctms.ink writes s den as [den; 0].
>> charpoly(A) == conv([1; 0], den)
1

>> conv([1; 0], den) == [den; 0]
1

# pend/invPID.html: the PID (kd s^2 + k s + ki)/s closes the loop on
# s den + (kd s^2 + k s + ki) num, at kd = k = ki = 1, the page's numc and
# denc. ctms.ink's pd(1, 1, 1).
>> frac conv(num, [1; 0])
[50/11;
     0;
     0]

>> frac conv([1; 0], den) + [0; conv([1; 1; 1], num)]
[      1;
   52/11;
 -293/11;
    1/11;
       0]

# The same denominator's value at s = 2 moves with the gain k by
# (50/11) s^2.
>> frac grad_(k = 1) polyval(conv([1; 0], den) + [0; conv([1; k; 1], num)], 2)
200/11

# pend/invRL.html: the lead (s + 4)/(s + 50) and the lag (s + 3)/s, the
# page's num3 = conv(conv(num, numlead), numlag) and den3 likewise.
>> frac conv(conv(num, [1; 4]), [1; 3])
[ 50/11;
 350/11;
 600/11;
      0]

>> frac conv(conv(den, [1; 50]), [1; 0])
[        1;
    552/11;
   -243/11;
 -17199/11;
  -2450/11;
         0]

# motor/freq2.html: P(s) = K/((Js + b)(Ls + R) + K^2), and |P(10i)|,
# which the page's bode reads as 0.0139: 0.0138685716265052918595600060095
# in mpmath, K/|(1001/10000 - 1/2) + 6i/10|.
>> J = 1/100
J = 1/100

>> b = 1/10
b = 1/10

>> K = 1/100
K = 1/100

>> R = 1
R = 1

>> L = 1/2
L = 1/2

>> den = [J*L; J*R + L*b; b*R + K^2]
den = [J*L; J*R + L*b; b*R + K^2]

>> conv([J; b], [L; R]) + [0; 0; K^2] == den
1

>> den
[ 0.005;
   0.06;
 0.1001]

>> polyval(den, 10*i)
~(-0.3999+i*0.6)

>> abs(polyval(K, 10*i)/polyval(den, 10*i))
~0.0138685716

>> abs(abs(polyval(K, 10*i)/polyval(den, 10*i)) - 0.0139) < 5/10^5
~1

# The page's lag (s + 1)/(s + 0.1) at the gain 50: numb = conv(num, numa)
# of a single value, and denb = conv(den, dena).
>> conv(50*K, [1; 1])
[0.5;
 0.5]

>> conv(den, [1; 1/10])
[  0.005;
  0.0605;
  0.1061;
 0.01001]

# motor/ss2.html: the poles at -5 +- i, (s + 5 - i)(s + 5 + i), complex
# coefficients multiplied as they are, without conjugating; and K by
# Ackermann's formula, [0 1] [B, AB]^-1 phi(A), sympy's and scipy's [12.99,
# -1] as ctms.ink has it from phi(A) written out.
>> conv([1; 5 - i], [1; 5 + i])
[  1;
 ~10;
 ~26]

>> A = [-b/J, K/J; -K/L, -R/L]
A = [-b/J, K/J; -K/L, -R/L]

>> B = [0; 1/L]
B = [0; 1/L]

>> [0, 1]*[B, A*B]^(0-1)*polyval([1; 10; 26], A)
[12.99, -1]

>> polyval(charpoly(A), A)
[0, 0;
 0, 0]

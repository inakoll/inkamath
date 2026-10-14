# roots in the prelude: a polynomial's roots, the polynomial a column,
# highest power first, as charpoly writes one (DESIGN.md, next in line).
# Every value was worked out apart from the interpreter, as its
# specification: the exact ones in sympy, the inexact ones in mpmath at 50
# digits, the pendulum's as its Hamiltonian's stable eigenvalues, and
# printed to nine digits as Number::Shown prints them.

# motor/rlocus2.html: the closed loop at the page's gain, -6 +- 2.0510i,
# which ctms.ink takes by the quadratic formula. In order of real part,
# then of imaginary part.
>> J = 1/100
J = 1/100

>> Km = 1/100
Km = 1/100

>> R = 1
R = 1

>> L = 1/2
L = 1/2

>> b = 1/10
b = 1/10

>> den = [J*L; J*R + L*b; b*R + Km^2]
den = [J*L; J*R + L*b; b*R + Km^2]

>> roots(den + [0; 0; 10.0934*Km])
[~(-6-i*2.05104851);
 ~(-6+i*2.05104851)]

# pend/invSS.html, R = 1 and Q = diag(5000, 0, 100, 0): the poles of the
# loop the LQR gain closes, -8.4910 +- 7.9283i and -4.7592 +- 0.8309i,
# which eig refuses, its count holding only where every eigenvalue is real.
# K2 is approximated past a thousand digits, and so is every root of it.
>> M = 1/2
M = 1/2

>> m = 1/5
m = 1/5

>> In = 6/1000
In = 6/1000

>> g = 98/10
g = 98/10

>> l = 3/10
l = 3/10

>> p = In*(M+m) + M*m*l^2
p = In*(M+m) + M*m*l^2

>> A = [0, 1, 0, 0; 0, -(In+m*l^2)*b/p, m^2*g*l^2/p, 0; 0, 0, 0, 1; 0, -m*l*b/p, m*g*l*(M+m)/p, 0]
A = [0, 1, 0, 0; 0, -(In+m*l^2)*b/p, m^2*g*l^2/p, 0; 0, 0, 0, 1; 0, -m*l*b/p, m*g*l*(M+m)/p, 0]

>> B = [0; (In+m*l^2)/p; 0; m*l/p]
B = [0; (In+m*l^2)/p; 0; m*l/p]

>> sgn(Z)_0 = Z
sgn(Z)_0 = Z

>> sgn(Z)_k = (sgn(Z)_(k-1) + sgn(Z)_(k-1)^(0-1))/2
sgn(Z)_k = (sgn(Z)_(k-1) + sgn(Z)_(k-1)^(0-1))/2

>> blk(W, o)[r<=4, c<=4] = W[r, c+o] + (r == c)*(o == 0)
blk(W, o)[r<=4, c<=4] = W[r, c+o] + (r == c)*(o == 0)

>> kq(W) = -B'*blk(W, 4)^(0-1)*blk(W, 0)
kq(W) = -B'*blk(W, 4)^(0-1)*blk(W, 0)

>> K2 = kq(lim sgn([A, -B*B'; -[5000, 0, 0, 0; 0, 0, 0, 0; 0, 0, 100, 0; 0, 0, 0, 0], -A']))
K2 = kq(lim sgn([A, -B*B'; -[5000, 0, 0, 0; 0, 0, 0, 0; 0, 0, 100, 0; 0, 0, 0, 0], -A']))

>> roots(charpoly(A - B*K2))
[ ~(-8.49098024-i*7.92827769);
  ~(-8.49098024+i*7.92827769);
 ~(-4.75916101-i*0.830918045);
 ~(-4.75916101+i*0.830918045)]  # approximated past a thousand digits

# pend/invPID.html: the loop the PID closes at kd = k = ki = 1, its
# denominator ctms.ink's pd(1, 1, 1). A root right of the axis, so
# unstable, as the page finds; the 0 is the pole and zero at the origin
# the page cancels, exact as a trailing 0 is, and printed inexact because
# its place in the order was found by comparing doubles.
>> roots([1; 52/11; -293/11; 1/11; 0])
[  ~-8.04117509;
             ~0;
 ~0.00341504057;
    ~3.31048732]

# README's PI loop, s(s + 1)(s + 2) + 2s + 1, its product by conv.
>> roots(conv(conv([1; 1], [1; 2]), [1; 0]) + [0; 0; 2; 1])
[~(-1.3411639-i*1.1615414);
 ~(-1.3411639+i*1.1615414);
             ~-0.317672196]

# A real polynomial's real root is real: the iteration starts off the real
# line, and each root is averaged with the conjugate of the root nearest
# its own conjugate, itself where it is real. So a real root compares.
>> roots([1; 0; -2])
[~-1.41421356;
  ~1.41421356]

>> roots([1; -6; 11; -6])
[~1;
 ~2;
 ~3]

>> roots([1; 3; 2])[1] < 0
~1

# Wilkinson's (s - 1)(s - 2)...(s - 10): its roots spread to 10, which the
# start reaches scaled by |p_m/p_1|^(1/(m-1)).
>> roots([1; -55; 1320; -18150; 157773; -902055; 3416930; -8409500; 12753576; -10628640; 3628800])
[ ~1;
  ~2;
  ~3;
  ~4;
  ~5;
  ~6;
  ~7;
  ~8;
  ~9;
 ~10]

# Small roots: the iteration runs on p scaled by a power of two near the
# roots' geometric mean, so lim's tolerance, absolute below 1, is relative
# to them. (s - 10^-10)(s - 2*10^-10), exact in sympy:
>> roots([1; -3/10^10; 2/10^20])
[~1e-10;
 ~2e-10]

# README's PI loop scaled by 10^-12, s^3 + 3*10^-12 s^2 + 4*10^-24 s +
# 10^-36, in mpmath: its roots times 10^-12.
>> roots([1; 3/10^12; 4/10^24; 1/10^36])
[~(-1.3411639e-12-i*1.1615414e-12);
 ~(-1.3411639e-12+i*1.1615414e-12);
                  ~-3.17672196e-13]

# Roots far from their geometric mean, 1 here, where lim's tolerance is
# absolute: (s - 10^-23)(s + 1)(s - 10^23), its roots by construction. The
# small one answered ~-3.46262328e-22, its sign wrong.
>> roots(conv(conv([1; -1/10^23], [1; 1]), [1; -10^23]))
[   ~-1;
 ~1e-23;
 ~1e+23]

# A repeated root: the roots of p/gcd(p, p'), each simple, then the gcd's,
# whose roots are p's repeated ones, Euclid's gcd exact in exact numbers.
# Each of these comes to factors of degree 1, so it is exact.
>> roots([1; 2; 1])
[-1;
 -1]

>> roots([1; 3; 3; 1])
[-1;
 -1;
 -1]

# A deadbeat controller's poles, z^3.
>> roots([1; 0; 0; 0])
[0;
 0;
 0]

>> roots([1; 2; 0])
[-2;
  0]

# Beside an inexact root, an exact one is inexact: the order compares them.
>> roots(conv(conv([1; 1], [1; 1]), [1; 2]))
[~-2;
 ~-1;
 ~-1]

# A repeated pair, ((s + 1)^2 + 4)^2, equal ones in the order found.
>> roots(conv([1; 2; 5], [1; 2; 5]))
[~(-1-i*2);
 ~(-1-i*2);
 ~(-1+i*2);
 ~(-1+i*2)]

# A near-double root is not repeated: (s + 1)^2 - 10^-12, -1 +- 10^-6.
>> roots([1; 2; 1 - 1/10^12])
[~-1.000001;
 ~-0.999999]

# Doubles are read as the rationals they are, as by eig, so a repeated
# root of the data as stored is found, and the answer is inexact.
>> roots(~[1; 2; 1])
[~-1;
 ~-1]

# A complex polynomial's roots are not paired: (s - 2i)^2 - 1/100.
>> roots([1; -4*i; -4 - 1/100])
[~(-0.1+i*2);
  ~(0.1+i*2)]

# A degree 1 is -p_2/p_1, exact.
>> roots([2; 3])
-1.5

>> frac roots([3; 1])
-1/3

# The degree is the polynomial's, not the column's: leading zeros are
# dropped, as conv keeps one.
>> roots(conv([0; 1], [1; 2]))
-2

>> roots([0; 0; 1; 3; 2])
[~-2;
 ~-1]

# A constant has no root and the zero polynomial every number; there is
# no empty column. A single value is a constant.
>> roots(5)
error: roots needs a polynomial of degree 1 or more

>> roots([0; 0; 0])
error: roots needs a polynomial of degree 1 or more

# A row is MATLAB's convention, refused as conv refuses one.
>> roots([1, 3, 2])
error: roots takes p[j<=m], not a 1x3 matrix

# grad refuses it, as eig: a root's derivative, -p_theta(r)/p'(r), has
# none where the root is repeated.
>> grad_(k = 2) roots([1; 3; k])
error: grad cannot differentiate roots yet

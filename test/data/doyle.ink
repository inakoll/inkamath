# J. C. Doyle, "Guaranteed Margins for LQG Regulators", IEEE Trans.
# Automat. Contr. AC-23(4), 1978, its Example. Every expected value is the
# paper's or worked out by hand from its formulas, and checked with sympy
# and python-control; none was recorded.
#
# The plant, its noise entering by G, Q = q G G', R = 1, the noise
# intensities sigma and 1: the gains are g = f G and k = d G, where
# f = 2 + sqrt(4 + q) and d is the same of sigma.
>> A = [1 1; 0 1]
A = [1 1; 0 1]

>> B = [0; 1]
B = [0; 1]

>> C = [1 0]
C = [1 0]

>> G = [1; 1]
G = [1; 1]

>> f(q) = 2 + (4 + q)^(1/2)
f(q) = 2 + (4 + q)^(1/2)

# The control Riccati equation is solved by [2f f; f f], each cell of its
# residual being 4f - f^2 + q, which f's root makes 0: at q = 5, f = 5,
# and B'P is the paper's g'. The filter's is the dual, [d d; d 2d]; at
# sigma = 21, d = 7, and SC' is k.
>> P(f) = [2*f f; f f]
P(f) = [2*f f; f f]

>> A'*P(f(5)) + P(f(5))*A - P(f(5))*B*B'*P(f(5)) + 5*G*G'
[0, 0;
 0, 0]

>> B'*P(f(5))
[5, 5]

>> S(d) = [d d; d 2*d]
S(d) = [d d; d 2*d]

>> A*S(f(21)) + S(f(21))*A' - S(f(21))*C'*C*S(f(21)) + 21*G*G'
[0, 0;
 0, 0]

>> S(f(21))*C'
[7;
 7]

# The full system matrix, m the gain at the plant's input that only the
# plant knows. Its characteristic polynomial moves with m in its last two
# coefficients alone, the paper's linear term d + f - 4 + 2(m - 1)df and
# constant term 1 + (1 - m)df: 43 and -33/2 at m = 3/2, f = 5, d = 7. The
# first three are 1, f + d - 4 and fd - 2f - 2d + 6.
>> Acl(m, f, d) = [A, -m*B*f*G'; d*G*C, A - B*f*G' - d*G*C]
Acl(m, f, d) = [A, -m*B*f*G'; d*G*C, A - B*f*G' - d*G*C]

>> charpoly(Acl(3/2, 5, 7))
[    1;
     8;
    17;
    43;
 -16.5]

>> grad_(m = 1) charpoly(Acl(m, 5, 7))
[  0;
   0;
   0;
  70;
 -35]

# The constant term vanishes at m = 1 + 1/(df), 36/35, the upper end of
# the stable range, an eigenvalue at 0.
>> hurwitz(charpoly(Acl(36/35, 5, 7)))
0

>> hurwitz(charpoly(Acl(36/35 - 10^-30, 5, 7)))
1

>> abscissa(Acl(36/35, 5, 7))
0

# The lower end is where the third Hurwitz determinant vanishes, at
# d = f = 5 43/25 - 9 sqrt(5)/25, 0.915015528 (sympy), bisected here on
# the test. At q = sigma = 10^6 the range is 0.999004986 to
# 1 + 1/f^2 = 1.000000996 (sympy): the margins close on 1 as q and sigma
# grow.
>> low(f, d)_0 = [0; 1]
low(f, d)_0 = [0; 1]

>> low(f, d)_n = (b = low(f, d)_(n-1)) + (b[2] - b[1])/2*([1; 0] - hurwitz(charpoly(Acl((b[1] + b[2])/2, f, d)))*[1; 1])
low(f, d)_n = (b = low(f, d)_(n-1)) + (b[2] - b[1])/2*([1; 0] - hurwitz(charpoly(Acl((b[1] + b[2])/2, f, d)))*[1; 1])

>> lim low(5, 5)
[~0.915015528;
 ~0.915015528]

>> lim low(f(10^6), f(10^6))
[~0.999004986;
 ~0.999004986]

>> 1 + 1/f(10^6)^2
~1.000001

# With m = 1 the filter's error and the regulator separate: the
# eigenvalues are those of A - Bg' and A - kC, each s^2 + (f - 2)s + 1,
# (-3 -+ sqrt(5))/2 twice at f = d = 5.
>> eig(Acl(1, 5, 5))
[ ~-2.61803399;
  ~-2.61803399;
 ~-0.381966011;
 ~-0.381966011]

# The loop broken at the plant's input has the gain
# L = fd(2s - 1)/((s - 1)^2 (s^2 + (f + d - 2)s + 1 + fd)), -fd/(1 + fd)
# at s = 0, where L/(1 + L) is -fd, so that the margin above 1, 1/(fd),
# is one over it; |L/(1 + L)| peaks there (python-control).
>> [0 0 5 5]*(0*Acl(1, 5, 7) - Acl(1, 5, 7))^-1*[B; 0; 0]
-35

# The regulator alone, for contrast, closes s^2 + (mf - 2)s + 1, stable
# for every m above 2/f: at q = 0, f = 4, the 6 dB the introduction
# recalls, and no upper end.
>> hurwitz(charpoly(A - 1/2*B*4*G'))
0

>> hurwitz(charpoly(A - (1/2 + 10^-20)*B*4*G'))
1

>> hurwitz(charpoly(A - 10^20*B*4*G'))
1

# Its loop gain L = fs/(s - 1)^2 keeps |1 + L(iw)| >= 1 (Kalman), so its
# sensitivity's norm is 1, at q = 0 everywhere, an all-pass
# ((s - 1)/(s + 1))^2. There |L(iw)| = 4w/(1 + w^2) is 1 at w = 2 - sqrt(3),
# where Re L = -8w^2/(1 + w^2)^2 = -1/2: the 60 degrees of phase.
>> hinf(A - B*5*G', B, -5*G', 1)
1

>> hinf(A - B*4*G', B, -4*G', 1)
1

>> L(f, s) = f*G'*(s*A^0 - A)^-1*B
L(f, s) = f*G'*(s*A^0 - A)^-1*B

>> re(L(4, i*(2 - 3^(1/2))))
-0.5

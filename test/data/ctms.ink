# The Control Tutorials for MATLAB (CTM), as the University of Alberta mirrors
# them under https://www.ece.ualberta.ca/~tchen/ctm/examples/. Every expected
# value is the page's or worked out apart from the interpreter, with Python's
# fractions, sympy, scipy and mpmath; none was recorded.

# pend/invpen.html, "Matlab representation and the open-loop response": the
# pendulum's angle over the force, and the state space, exact. The page's
# table says m = 0.5 kg; its code, which all its figures follow, 0.2.
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

>> frac num
[50/11;
     0]

>> frac den
[      1;
    2/11;
 -343/11;
  -49/11]

>> p = In*(M+m) + M*m*l^2
p = In*(M+m) + M*m*l^2

>> A = [0, 1, 0, 0; 0, -(In+m*l^2)*b/p, m^2*g*l^2/p, 0; 0, 0, 0, 1; 0, -m*l*b/p, m*g*l*(M+m)/p, 0]
A = [0, 1, 0, 0; 0, -(In+m*l^2)*b/p, m^2*g*l^2/p, 0; 0, 0, 0, 1; 0, -m*l*b/p, m*g*l*(M+m)/p, 0]

>> B = [0; (In+m*l^2)/p; 0; m*l/p]
B = [0; (In+m*l^2)/p; 0; m*l/p]

# The two models agree: det(sI - A) is s times the denominator, the pole and
# zero at the origin the page cancels put back.
>> charpoly(A) - [den; 0]
[0;
 0;
 0;
 0;
 0]

# pend/invPID.html, "Adding the PID controller": the closed loop's
# denominator s den + (kd s^2 + k s + ki) num at kd = k = ki = 1. The page's
# 4.7273, -26.6363 and 0.0910 are sums of its own 4-place den and num; the
# exact -293/11 and 1/11 are -26.6364 and 0.0909.
>> pd(kd, k, ki) = [den; 0] + num[1]*[0; kd; k; ki; 0]
pd(kd, k, ki) = [den; 0] + num[1]*[0; kd; k; ki; 0]

>> frac pd(1, 1, 1)
[      1;
   52/11;
 -293/11;
    1/11;
       0]

# pend/invSS.html, "LQR design", R = 1: the Riccati solution from the sign of
# the Hamiltonian, the limit of Z_k = (Z + Z^-1)/2, whose top rows
# [W11 + I, W12] give P = -W12^-1 (W11 + I) and K = B'P. Against scipy's
# solve_continuous_are and mpmath's 50 digits, and the page's 4 places.
# K2's last cell is 20.92384374921, 8e-10 from a ninth digit of 8.
>> sgn(Z)_0 = Z
sgn(Z)_0 = Z

>> sgn(Z)_k = (sgn(Z)_(k-1) + sgn(Z)_(k-1)^(0-1))/2
sgn(Z)_k = (sgn(Z)_(k-1) + sgn(Z)_(k-1)^(0-1))/2

>> blk(W, o)[r<=4, c<=4] = W[r, c+o] + (r == c)*(o == 0)
blk(W, o)[r<=4, c<=4] = W[r, c+o] + (r == c)*(o == 0)

>> kq(W) = -B'*blk(W, 4)^(0-1)*blk(W, 0)
kq(W) = -B'*blk(W, 4)^(0-1)*blk(W, 0)

>> lqr(Q) = kq(lim sgn([A, -B*B'; -Q, -A']))
lqr(Q) = kq(lim sgn([A, -B*B'; -Q, -A']))

>> K1 = lqr([1, 0, 0, 0; 0, 0, 0, 0; 0, 0, 1, 0; 0, 0, 0, 0])
K1 = lqr([1, 0, 0, 0; 0, 0, 0, 0; 0, 0, 1, 0; 0, 0, 0, 0])

>> K1
[~-1, ~-1.65671003, ~18.6853959, ~3.45943818]  # approximated past a thousand digits

>> K2 = lqr([5000, 0, 0, 0; 0, 0, 0, 0; 0, 0, 100, 0; 0, 0, 0, 0])
K2 = lqr([5000, 0, 0, 0; 0, 0, 0, 0; 0, 0, 100, 0; 0, 0, 0, 0])

>> K2
[~-70.7106781, ~-37.834454, ~105.529782, ~20.9238437]  # approximated past a thousand digits

>> max_(j=1)^4 abs((K1 - [-1.0000, -1.6567, 18.6854, 3.4594])[1, j]) < 5/10^5
~1  # approximated past a thousand digits

>> max_(j=1)^4 abs((K2 - [-70.7107, -37.8345, 105.5298, 20.9238])[1, j]) < 5/10^5
~1  # approximated past a thousand digits

# "Adding the reference input": Nbar = -1/(C (A - BK)^-1 B) with C = [1 0 0
# 0], which is K's first cell exactly: the cart's position is an integrator,
# so at rest the angle is 0 and K x = K[1] r. The page's -70.7107.
>> nbar(K) = -1/([1, 0, 0, 0]*(A - B*K)^(0-1)*B)
nbar(K) = -1/([1, 0, 0, 0]*(A - B*K)^(0-1)*B)

>> abs(nbar(K1) - K1[1, 1]) < 1/10^12
~1  # approximated past a thousand digits

>> abs(nbar(K2) - K2[1, 1]) < 1/10^12
~1  # approximated past a thousand digits

# motor/ss2.html, "Designing the full-state feedback controller" and
# "Adding a reference input": the poles at -5 +- i, (s + 5)^2 + 1, by
# Ackermann's formula; the page prints neither K nor Nbar, which sympy and
# scipy's place_poles give as [12.99, -1] and 13.
>> J = 1/100
J = 1/100

>> Km = 1/100
Km = 1/100

>> R = 1
R = 1

>> L = 1/2
L = 1/2

>> A = [-b/J, Km/J; -Km/L, -R/L]
A = [-b/J, Km/J; -Km/L, -R/L]

>> B = [0; 1/L]
B = [0; 1/L]

>> C = [1, 0]
C = [1, 0]

>> K = [0, 1]*[B, A*B]^(0-1)*(A^2 + 10*A + 26*A^0)
K = [0, 1]*[B, A*B]^(0-1)*(A^2 + 10*A + 26*A^0)

>> K
[12.99, -1]

>> charpoly(A - B*K)
[ 1;
 10;
 26]

>> -1/(C*(A - B*K)^(0-1)*B)
13

# motor/rlocus2.html, "Finding the gain using the rlocfind command": the
# closed loop at the page's gain, s^2 + 12s + 40.2068 once monic, so the
# poles are -6 +- i sqrt(4.2068), -6 +- 2.0510i. The page's 2.0511 is from
# rlocfind's gain unrounded, at least 10.093403.
>> den = [J*L; J*R + L*b; b*R + Km^2]
den = [J*L; J*R + L*b; b*R + Km^2]

>> p = den + [0; 0; 10.0934*Km]
p = den + [0; 0; 10.0934*Km]

>> frac p/p[1]
[          1;
          12;
 100517/2500]

>> -p[2]/(2*p[1]) + i*(p[3]/p[1] - (p[2]/(2*p[1]))^2)^(1/2)
~(-6+i*2.05104851)

# An explicit step on Robertson's stiff kinetics, robertson.ink (Hairer
# and Wanner II, IV.1): its stability limit, set by the fast eigenvalue,
# and the step just inside and just outside it. Every expected value was
# worked out apart from the interpreter and none was recorded: the Euler
# and Runge-Kutta steps again in numpy, f's products as robertson.ink
# writes them; each eigenvalue of the Jacobian's doubles in mpmath; and
# y40, the state at t = 40, scipy's Radau at rtol 1e-13, with which BDF and
# LSODA agree to 11 digits.
>> use robertson (f, J, y0)
use robertson (f, J, y0)

# The Jacobian by cells of grad is the one written by hand.
>> Jg(y)[j<=3, k<=3] = (grad_(v = y) f(v)[j])[k]
Jg(y)[j<=3, k<=3] = (grad_(v = y) f(v)[j])[k]

>> Jg([1/2; 1/1000; 1/3]) == J([1/2; 1/1000; 1/3])
1

# Stiffness: at rest the fast eigenvalue is -(k3 + k1), the slow one 0.
>> eig(J([0; 0; 1]))
[~-10000.04;
         ~0;
         ~0]

>> y40 = [~0.7158270687194010; ~9.185534764557751e-06; ~0.2841637457458290]
y40 = [~0.7158270687194010; ~9.185534764557751e-06; ~0.2841637457458290]

>> lam = eig(J(y40))[1]
lam = eig(J(y40))[1]

>> lam
~-3392.78812

# Forward Euler is stable for |1 + h*lam| <= 1.
>> hfe = -2/lam
hfe = -2/lam

>> hfe
~0.000589485676

>> fe(h, y)_0 = y
fe(h, y)_0 = y

>> fe(h, y)_n = fe(h, y)_(n-1) + h*f(fe(h, y)_(n-1))
fe(h, y)_n = fe(h, y)_(n-1) + h*f(fe(h, y)_(n-1))

# Just inside, it follows the solution, which Radau gives as 0.712909078,
# 9.07298289e-06 and 0.287081849 there; just outside, y2 is twice the
# solution's 9.07075476e-06.
>> fe(99/100*hfe, y40)_2000
[    ~0.71290906;
 ~9.07298221e-06;
    ~0.287081867]

>> fe(101/100*hfe, y40)_2000
[   ~0.711365644;
 ~1.91737024e-05;
    ~0.288615182]

# RK4's real stability boundary, where 1 + z + z^2/2 + z^3/6 + z^4/24 is 1
# again: z^3/24 + z^2/6 + z/2 + 1 = 0.
>> q_0 = -3
q_0 = -3

>> q_k = q_(k-1) - (q_(k-1)^3/24 + q_(k-1)^2/6 + q_(k-1)/2 + 1)/(q_(k-1)^2/8 + q_(k-1)/3 + 1/2)
q_k = q_(k-1) - (q_(k-1)^3/24 + q_(k-1)^2/6 + q_(k-1)/2 + 1)/(q_(k-1)^2/8 + q_(k-1)/3 + 1/2)

>> lim q
~-2.78529356

>> rk(h, y)_0 = y
rk(h, y)_0 = y

>> rk(h, y)_n = (u = rk(h, y)_(n-1)) + h/6*((a = f(u)) + 2*(b = f(u + h/2*a)) + 2*(c = f(u + h/2*b)) + f(u + h*c))
rk(h, y)_n = (u = rk(h, y)_(n-1)) + h/6*((a = f(u)) + 2*(b = f(u + h/2*a)) + 2*(c = f(u + h/2*b)) + f(u + h*c))

# Just outside, y2 is a quarter above the solution's 9.02693547e-06.
>> rk(101/100*lim q/lam, y40)_2000
[   ~0.711281932;
 ~1.13263016e-05;
    ~0.288706741]

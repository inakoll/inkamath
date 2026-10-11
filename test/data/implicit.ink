# Implicit steps on Robertson's stiff kinetics, robertson.ink (Hairer and
# Wanner II, IV.10): backward Euler, the trapezoidal rule and two-stage
# Radau IIA, each step's equation solved by Newton's method. Every expected
# value was worked out apart from the interpreter and none was recorded:
# each method again in mpmath at 50 digits, Newton run until its step is
# below 1e-45, and Newton's first iterates in exact fractions.
>> use robertson (f, J, I, y0)
use robertson (f, J, I, y0)

# Backward Euler, each step's Newton iteration under lim.
>> nw(yp, h)_0 = yp
nw(yp, h)_0 = yp

>> nw(yp, h)_k = (z = nw(yp, h)_(k-1)) - (I - h*J(z))^-1*(z - yp - h*f(z))
nw(yp, h)_k = (z = nw(yp, h)_(k-1)) - (I - h*J(z))^-1*(z - yp - h*f(z))

>> be(h)_0 = ~y0
be(h)_0 = ~y0

>> be(h)_n = lim nw(be(h)_(n-1), h)
be(h)_n = lim nw(be(h)_(n-1), h)

>> be(1/10)_400
[   ~0.716174955;
 ~9.19906765e-06;
    ~0.283815846]

>> be(4)_10
[   ~0.728237195;
 ~9.68389091e-06;
    ~0.271753121]

# The trapezoidal rule is A-stable, not L-stable: y2 goes negative.
>> tw(yp, h)_0 = yp
tw(yp, h)_0 = yp

>> tw(yp, h)_k = (z = tw(yp, h)_(k-1)) - (I - h/2*J(z))^-1*(z - yp - h/2*(f(yp) + f(z)))
tw(yp, h)_k = (z = tw(yp, h)_(k-1)) - (I - h/2*J(z))^-1*(z - yp - h/2*(f(yp) + f(z)))

>> tr(h)_0 = ~y0
tr(h)_0 = ~y0

>> tr(h)_n = lim tw(tr(h)_(n-1), h)
tr(h)_n = lim tw(tr(h)_(n-1), h)

>> tr(1)_40
[    ~0.631609409;
 ~-3.51851741e-06;
     ~0.368394109]

# Radau IIA, two stages: a 6x6 Newton system written in blocks. Of order
# 3, it is within 1e-4 of the solution at t = 40 in ten steps of 4.
>> O = 0*I
O = 0*I

>> P1 = [I, O]
P1 = [I, O]

>> P2 = [O, I]
P2 = [O, I]

>> G(Z, yp, h) = Z - [yp; yp] - h*[5/12*f(P1*Z) - 1/12*f(P2*Z); 3/4*f(P1*Z) + 1/4*f(P2*Z)]
G(Z, yp, h) = Z - [yp; yp] - h*[5/12*f(P1*Z) - 1/12*f(P2*Z); 3/4*f(P1*Z) + 1/4*f(P2*Z)]

>> DG(Z, h) = [I, O; O, I] - h*[5/12*J(P1*Z), -1/12*J(P2*Z); 3/4*J(P1*Z), 1/4*J(P2*Z)]
DG(Z, h) = [I, O; O, I] - h*[5/12*J(P1*Z), -1/12*J(P2*Z); 3/4*J(P1*Z), 1/4*J(P2*Z)]

>> rw(yp, h)_0 = [yp; yp]
rw(yp, h)_0 = [yp; yp]

>> rw(yp, h)_k = (z = rw(yp, h)_(k-1)) - DG(z, h)^-1*G(z, yp, h)
rw(yp, h)_k = (z = rw(yp, h)_(k-1)) - DG(z, h)^-1*G(z, yp, h)

>> ra(h)_0 = ~y0
ra(h)_0 = ~y0

>> ra(h)_n = P2*lim rw(ra(h)_(n-1), h)
ra(h)_n = P2*lim rw(ra(h)_(n-1), h)

>> ra(4)_10
[   ~0.71572989;
 ~9.1817588e-06;
   ~0.284260928]

# To t = 10^11, the test set's end, in 50 steps doubling, the time exact.
# Newton is read at 40 iterations rather than under lim, whose 1e-10 is
# absolute where y2 is 1.7e-13.
>> hv_n = 10^11/(2^51 - 2)*2^n
hv_n = 10^11/(2^51 - 2)*2^n

>> tv_0 = 0
tv_0 = 0

>> tv_n = tv_(n-1) + hv_n
tv_n = tv_(n-1) + hv_n

>> bv_0 = ~y0
bv_0 = ~y0

>> bv_n = nw(bv_(n-1), hv_n)_40
bv_n = nw(bv_(n-1), hv_n)_40

>> tv_50
100000000000

>> bv_50
[~4.16603775e-08;
 ~1.66641517e-13;
    ~0.999999958]

# Exact data: Newton's iterates conserve y1 + y2 + y3 exactly, and each
# triples the digits, the sixth passing a thousand.
>> [1 1 1]*nw(y0, 1/10)_5
1

>> nw(y0, 1/10)_6
[   ~0.996507916;
 ~0.000127315119;
  ~0.00336476853]  # approximated past a thousand digits

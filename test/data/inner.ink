# An inner instance's optional input (DESIGN.md), the interpreter's answers
# that test/compile/inner.ink's compiled step is held to. The interpreter
# gives them already; every value was worked out apart from it, the small
# models' by hand and in Python's fractions, the robot's by a simulation of
# its loop in doubles.

# Forwarded: e's z is fw's own optional input, k's the plain input w. Before
# the stream an optional input is absent, whatever its argument gives there:
# w_-3 is -11/4, k's z_-3 is absent, and k's rise is 0 until 3.
>> last(z_n?) = {
..     x_0 = 0
..     x_n | z_n >= 0 = z_n
..     x_n = x_(n-1)
..     d_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)
..     d_n = 0
.. }
last(z_n?) = { ... }

>> fw(z_n?, w_n) = {
..     e = last(z_n = z_n)
..     k = last(z_n = w_n)
..     y_n = e.x_n + k.d_n
.. }
fw(z_n?, w_n) = { ... }

>> zd_n | mod(n, 3) == 0 and (n < 9 or n > 14) = n + 1/4
zd_n | mod(n, 3) == 0 and (n < 9 or n > 14) = n + 1/4

>> fwd = fw(z_n = zd_n, w_n = n + 1/4)
fwd = fw(z_n = zd_n, w_n = n + 1/4)

>> [fwd.e.x_3, fwd.e.x_9, fwd.e.x_14, fwd.e.x_15]
[3.25, 6.25, 6.25, 15.25]

>> [fwd.e.d_0, fwd.e.d_3, fwd.e.d_9, fwd.e.d_15, fwd.e.d_18]
[0, 3, 0, 0, 3]

>> fwd.w_(-3)
-2.75

>> fwd.k.z_(-3)
error: fwd.k.z_-3 is absent

>> [fwd.k.d_0, fwd.k.d_2, fwd.k.d_3, fwd.y_3, fwd.y_99]
[0, 0, 3, 6.25, 102.25]

# A sibling's chain: e's z is s's term, absent where no clause of s's chain
# gives it, k's that term doubled. A clause gives NaN at 20, which is no
# absence: x refuses it, where taking it for absence would give x_19,
# 18.25, and d, which reads z_17 too, absent, falls.
>> src() = {
..     z_n | n == 20 = 0/~0
..     z_n | mod(n, 3) == 0 and (n < 9 or n > 14) = n + 1/4
.. }
src() = { ... }

>> pair() = {
..     s = src()
..     e = last(z_n = s.z_n)
..     k = last(z_n = 2*s.z_n)
.. }
pair() = { ... }

>> sib = pair()
sib = pair()

>> [sib.e.x_19, sib.e.x_21, sib.e.x_22, sib.k.x_21]
[18.25, 21.25, 21.25, 42.5]

>> sib.e.x_20
error: a comparison needs a number, not -nan

>> [sib.e.d_20, sib.e.d_21, sib.k.d_20, sib.k.d_21]
[0, 3, 0, 6]

# Before the stream s's term is there, e's z is not.
>> sib.s.z_(-3)
-2.75

>> sib.e.d_0
0

# robot_loop.ink's loop as one instance, its fix the GPS instance's z,
# present at every tenth step but from 300 to 490, and no ok flag. The
# simulation gives robot_loop.ink's flags and u_300 = -0.0848377303296.
>> use robot (dt, F, G, gain, imu)
use robot (dt, F, G, gain, imu)

>> cart(u_n) = {
..     p_0 = 0
..     v_0 = 0
..     a_0 = 0
..     a_n = u_(n-1) - v_n/10
..     p_n = p_(n-1) + dt*v_(n-1) + dt^2/2*a_(n-1)
..     v_n = v_(n-1) + dt*a_(n-1)
.. }
cart(u_n) = { ... }

>> gps(p_n) = {
..     r_0 = 4242
..     r_n = mod(16807*r_(n-1), 2^31 - 1)
..     z_n | mod(n, 10) == 0 and (n < 300 or n >= 500) = p_n + (r_n/(2^31 - 1) - 1/2)/5
.. }
gps(p_n) = { ... }

>> nav(qp = 1, rz = 1/300, gate = 4, pmax = 1/400, umax = 3/2, r_n, am_n, z_n?) = {
..     W = [~1.0087082446036357; ~-0.49608088328325778; ~0.23801367405832982; ~2.0196709407791769]
..     cb = [~-0.0017651237166237193; ~0.24287475294768127; ~-0.24650153472845093; ~-0.019883014028842676]
..     V = [~0.0038140777920965263, ~-0.0014083530162575402, ~0.016730466244441917, ~0.19218841286451346]
..     h(s)[j<=4] = tanh(s[j])
..     corr(v) = V*h(W*v + cb) + ~0.054968825254486729
..     kg(q) | gain([q 0; 0 1/10], ~1/10)[1,1] > 0 = gain([q 0; 0 1/10], ~1/10)
..     kg(q) = [3 5/2]
..     Kc = kg(qp)
..     x_0 = [0; 0]
..     P_0 = [1/100 0; 0 1/100]
..     xp_0 = [0; 0]
..     Pp_0 = [1/100 0; 0 1/100]
..     ac_0 = 0
..     ac_n = am_n - corr(x_(n-1)[2])
..     xp_n = F*x_(n-1) + G*ac_n
..     Pp_n = F*P_(n-1)*F' + G*G'/25
..     S_n = [1 0]*Pp_n*[1; 0] + rz
..     fix_n | (z_n - [1 0]*xp_n)^2 < gate^2*S_n = 1
..     fix_n = 0
..     x_n | (z_n - [1 0]*xp_n)^2 < gate^2*S_n = xp_n + Pp_n*[1; 0]*(z_n - [1 0]*xp_n)/S_n
..     x_n = xp_n
..     P_n | fix_n == 1 = Pp_n - Pp_n*[1; 0]*[1 0]*Pp_n/S_n
..     P_n = Pp_n
..     lost_n = P_n[1,1] > pmax
..     top_n | lost_n == 1 = umax/4
..     top_n = umax
..     uc_n = Kc*([r_n; 0] - x_n)
..     u_n | uc_n > top_n = top_n
..     u_n | uc_n < -top_n = -top_n
..     u_n = uc_n
.. }
nav(qp = 1, rz = 1/300, gate = 4, pmax = 1/400, umax = 3/2, r_n, am_n, z_n?) = { ... }

>> loop() = {
..     w = cart(u_n = f.u_n)
..     s = imu(a_n = w.a_n, v_n = w.v_n)
..     g = gps(p_n = w.p_n)
..     f = nav(r_n = 1 - 3/2*(n >= 350), am_n = s.am_n, z_n = g.z_n)
.. }
loop() = { ... }

>> bot = loop()
bot = loop()

>> [bot.f.fix_290, bot.f.fix_300, bot.f.fix_490, bot.f.fix_500]
[~1, 0, 0, ~1]

>> [bot.f.lost_414, bot.f.lost_415, bot.f.lost_499, bot.f.lost_500]
[~0, ~1, ~1, ~0]

>> bot.f.u_300
~-0.0848377303

# Where the fix is absent, the term is not there, and the filter's z with it.
>> bot.g.z_301
error: no clause of z applies

>> bot.f.z_301
error: bot.f.z_301 is absent

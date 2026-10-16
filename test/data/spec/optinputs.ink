# Optional inputs (DESIGN.md, next in line): an input present on some steps
# only. Specified before it is built, and never recorded: the small models'
# values by hand and in Python's fractions, the robot's by a simulation of
# its loop in doubles apart from inkamath.

# The signature marks an input optional, 'z_n?'. A guard that reads it where
# it is absent does not hold, and the clause after is tried, as for a limit
# that runs out (README section 4); absence is data, not a rounding, so the
# clause after is exact. Here the last value seen, held: an idiom the user
# writes, its guard the measurement's own acceptance test.
>> est(z_n?) = {
..     x_0 = 0
..     x_n | z_n >= 0 = z_n
..     x_n = x_(n-1)
.. }
est(z_n?) = { ... }

>> ?est
est(z_n?) = {
    x_0 = 0
    x_n | z_n >= 0 = z_n
    x_n = x_(n-1)
}

# It is absent where its argument reads a term that is not there: a
# sequence defined on some steps only, here every third, which no clause
# gives elsewhere.
>> zd_n | mod(n, 3) == 0 = n/3
zd_n | mod(n, 3) == 0 = n/3

>> e = est(z_n = zd_n)
e = est(z_n = zd_n)

>> [e.x_1, e.x_2, e.x_3, e.x_4, e.x_6, e.x_7]
[0, 0, 1, 1, 2, 2]

# Read from outside the model, an absent term is refused, saying so, and a
# guard reading it does not hold.
>> e.z_3
1

>> e.z_4
error: e.z_4 is absent

>> g(k) | e.z_k > 0 = 1
g(k) | e.z_k > 0 = 1

>> g(k) = 0
g(k) = 0

>> [g(3), g(4)]
[1, 0]

# Where no clause is left to fall to, the refusal names the absence.
>> h(k) | e.z_k > 0 = 1
h(k) | e.z_k > 0 = 1

>> h(4)
error: e.z_4 is absent

# Not marked, the same input is refused where its argument has no term, as
# today.
>> est2(z_n) = {
..     x_0 = 0
..     x_n | z_n >= 0 = z_n
..     x_n = x_(n-1)
.. }
est2(z_n) = { ... }

>> est2(z_n = zd_n).x_1
error: no clause of zd applies

# Left out of the instance, it is absent at every step.
>> est().x_5
0

# Absent only where the argument itself reads a term that is not there:
# read through another term, it is that term's refusal, as a limit that
# runs out is.
>> est(z_n = zd_n + 1).x_4
2

>> zp_n = zd_n + 1
zp_n = zd_n + 1

>> est(z_n = zp_n).x_4
error: no clause of zd applies

# Below a sequence's base clauses is a term that is not there too: z_1 reads
# wb_(-1), absent, and z_3 reads wb_1.
>> wb_0 = 1
wb_0 = 1

>> wb_n = 2*wb_(n-1)
wb_n = 2*wb_(n-1)

>> [est(z_n = wb_(n-2)).x_1, est(z_n = wb_(n-2)).x_3]
[0, 2]

# NaN is not absence. Given NaN, the input is present, and a guard comparing
# it is refused, marked or not, as any comparison of NaN is; compiled, the
# step is NaN there (C189), where an absent input takes the clause after.
>> zn_n | n == 2 = 0/~0
zn_n | n == 2 = 0/~0

>> zn_n | n >= 3 = n
zn_n | n >= 3 = n

>> f = est(z_n = zn_n)
f = est(z_n = zn_n)

>> [f.x_1, f.x_3]
[0, 3]

>> f.x_2
error: a comparison needs a number, not -nan

>> est2(z_n = zn_n).x_2
error: a comparison needs a number, not -nan

# What reads an optional input is checked where the model is defined: each
# read sits in a guard with a clause after it, or in the value of a clause
# whose guard reads it at the same index wherever the guard holds, or is the
# argument of an inner instance's optional input. Any other is refused,
# naming it, so that no term of the model is ever absent. The clauses after
# end in an unguarded one: a base clause or another guard may fall too.
>> bad(z_n?) = {
..     y_n = 2*z_n
.. }
error: y reads z_n, an optional input of bad, outside a guard with a clause after it

>> bad(z_n?) = {
..     y_n | z_n > 0 = 1
.. }
error: y reads z_n, an optional input of bad, outside a guard with a clause after it

>> bad(z_n?) = {
..     y_n | z_n > 0 = 1
..     y_n | n > 3 = 2
..     y_0 = 0
.. }
error: y reads z_n, an optional input of bad, outside a guard with a clause after it

>> bad(z_n?) = {
..     y_n | z_n > 0 = z_(n-1)
..     y_n = 0
.. }
error: y reads z_(n-1), an optional input of bad, in a value whose guard may hold without reading it

>> bad(z_n?) = {
..     y_n | n > 5 or z_n > 0 = z_n
..     y_n = 0
.. }
error: y reads z_n, an optional input of bad, in a value whose guard may hold without reading it

# Wherever the guard holds is the guard's reading, not its text: an 'and'
# compared may hold with its right unread, a sum's body is read only where
# it has terms, and z_n under a sum's own n is at another index.
>> bad(z_n?) = {
..     y_n | (n > 5 and z_n > 0) == 0 = z_n
..     y_n = 0
.. }
error: y reads z_n, an optional input of bad, in a value whose guard may hold without reading it

>> bad(z_n?) = {
..     y_n | sum_(k=1)^(n-5) z_n >= 0 = z_n
..     y_n = 0
.. }
error: y reads z_n, an optional input of bad, in a value whose guard may hold without reading it

>> bad(z_n?) = {
..     y_n | z_n > 0 = sum_(n=0)^2 z_n
..     y_n = 0
.. }
error: y reads z_n, an optional input of bad, in a value whose guard may hold without reading it

>> dbl(x_n) = { y_n = 2*x_n }
dbl(x_n) = { ... }

>> bad(z_n?) = {
..     d = dbl(x_n = z_n)
.. }
error: d reads z_n, an optional input of bad, outside a guard with a clause after it

# Read on the left of 'or', it is read wherever the guard is, and where it
# is absent the guard does not hold, whatever its right says: a guard that
# cannot be read whole has no truth. On the right of 'and', it was read
# wherever the guard holds.
>> lr(z_n?) = {
..     y_n | z_n > 0 or n > 5 = z_n
..     y_n = 0
..     w_n | n > 5 and z_n > 0 = z_n
..     w_n = 0
.. }
lr(z_n?) = { ... }

>> l = lr(z_n = zd_n)
l = lr(z_n = zd_n)

>> [l.y_6, l.y_7, l.y_9, l.w_3, l.w_7, l.w_9]
[2, 0, 3, 0, 0, 3]

# The clause after is inexact only where an inexact truth decided that the
# input be read, the left of an 'and' or an 'or' (C241).
>> ix(z_n?) = {
..     y_n | ~1/2 < 1 and z_n > 0 = 1
..     y_n = 0
..     w_n | 1/2 < 1 and z_n > 0 = 1
..     w_n = 0
.. }
ix(z_n?) = { ... }

>> [ix(z_n = zd_n).y_1, ix(z_n = zd_n).y_3, ix(z_n = zd_n).w_1, ix(z_n = zd_n).w_3]
[~0, ~1, 0, 1]

# An instance the model writes takes it as an optional input of its own,
# absent where it is.
>> fw(z_n?) = {
..     e = est(z_n = z_n)
..     y_n = e.x_n
.. }
fw(z_n?) = { ... }

>> fw(z_n = zd_n).y_4
1

# Before the stream nothing has arrived: below 0, where no clause of its
# history gives it, it is absent. A history gives what it says.
>> rise(z_n?) = {
..     d_n | z_n - z_(n-1) > 0 = 1
..     d_n = 0
.. }
rise(z_n?) = { ... }

>> [rise(z_n = n).d_0, rise(z_n = n).d_1]
[0, 1]

>> rise2(z_n?) = {
..     z_n | n < 0 = -1
..     d_n | z_n - z_(n-1) > 0 = 1
..     d_n = 0
.. }
rise2(z_n?) = { ... }

>> rise2(z_n = n).d_0
1

# Sampled at a slower rate under a guard, and held at the input's, as any
# input is: z is present at 0, 12 and 24 of the multiples of 4.
>> slow(z_n?) = {
..     s_0 = 0
..     s_m | z_(4*m) >= 0 = z_(4*m)
..     s_m = s_(m-1)
..     h_n = s_(floor(n/4))
.. }
slow(z_n?) = { ... }

>> sl = slow(z_n = zd_n)
sl = slow(z_n = zd_n)

>> [sl.h_11, sl.h_12, sl.h_23, sl.h_24]
[0, 4, 4, 8]

# grad through a guard that falls because an input is absent takes the
# clause after's slope: which clause applies is the data's, not the
# point's, where grad refuses a guard whose limit runs out. 2a at 3 where z
# is absent or 0, z where it is positive.
>> gr(z_n?) = {
..     f(a, k) | z_k > 0 = a*z_k
..     f(a, k) = a^2
..     g_n = grad_(a = 3) f(a, n)
.. }
gr(z_n?) = { ... }

>> [gr(z_n = zd_n).g_0, gr(z_n = zd_n).g_1, gr(z_n = zd_n).g_3, gr(z_n = zd_n).g_6]
[6, 6, 1, 2]

# An input with a size is marked after its bounds, and is absent or present
# whole. Out of range at 2, absent at 3, the last value seen holds.
>> pos(z_n[j<=2]?) = {
..     y_0 = [0; 0]
..     y_n | z_n[1]^2 + z_n[2]^2 <= 25 = z_n
..     y_n = y_(n-1)
.. }
pos(z_n[j<=2]?) = { ... }

>> vd_n | n == 1 = [3; 4]
vd_n | n == 1 = [3; 4]

>> vd_n | n == 2 = [6; 8]
vd_n | n == 2 = [6; 8]

>> po = pos(z_n = vd_n)
po = pos(z_n = vd_n)

>> [po.y_1, po.y_2, po.y_3]
[3, 3, 3;
 4, 4, 4]

# A measurement arriving at irregular times is an optional input at the
# step's rate, its data the times it arrived and its values.
>> ts = [2; 3; 7; 11]
ts = [2; 3; 7; 11]

>> zs = [5; 6; 4; 8]
zs = [5; 6; 4; 8]

>> at_n | sum_(k=1)^4 (ts[k] == n) == 1 = zs[argmax_(k=1)^4 (ts[k] == n)]
at_n | sum_(k=1)^4 (ts[k] == n) == 1 = zs[argmax_(k=1)^4 (ts[k] == n)]

>> ir = est(z_n = at_n)
ir = est(z_n = at_n)

>> [ir.x_1, ir.x_2, ir.x_3, ir.x_6, ir.x_7, ir.x_10, ir.x_11]
[0, 5, 6, 6, 4, 4, 8]

# The robot's loop (robot.ink, robot_loop.ink) with its fix optional and no
# ok flag: the fix is present at every tenth step but from 300 to 490, where
# it fails, and the filter's update reads it in its guard and value, its
# innovation written in both. Where it is absent the update falls to its
# clause after, x_n = xp_n: the filter coasts, is lost from 415, when its
# position's variance passes 1/400, as flagged, and the fix at 500 recovers
# it. A simulation of the loop in doubles, numpy with the doubling's gain,
# gives these flags and u_300 = -0.084837730329553; with scipy's gain, the
# same to 1e-14.
>> use robot (F, G, gain, cart, imu)
use robot (F, G, gain, cart, imu)

>> gps2(mode = 1, p_n) = {
..     r_0 = 4242
..     r_n = mod(16807*r_(n-1), 2^31 - 1)
..     down(m) = 30 <= m and m < 50
..     z_n | mod(n, 10) == 0 and down(n/10) == 0 = p_n + (r_n/(2^31 - 1) - 1/2)/5
..     z_n | mod(n, 10) == 0 and mode == 3 = 0/~0
.. }
gps2(mode = 1, p_n) = { ... }

>> nav2(qp = 1, rz = 1/300, gate = 4, pmax = 1/400, umax = 3/2, r_n, am_n, z_n?) = {
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
nav2(qp = 1, rz = 1/300, gate = 4, pmax = 1/400, umax = 3/2, r_n, am_n, z_n?) = { ... }

>> fo = nav2(r_n = 1 - 3/2*(n >= 350), am_n = so.am_n, z_n = xo.z_n)
fo = nav2(r_n = 1 - 3/2*(n >= 350), am_n = so.am_n, z_n = xo.z_n)

>> wo = cart(u_n = fo.u_n)
wo = cart(u_n = fo.u_n)

>> so = imu(a_n = wo.a_n, v_n = wo.v_n)
so = imu(a_n = wo.a_n, v_n = wo.v_n)

>> xo = gps2(p_n = wo.p_n)
xo = gps2(p_n = wo.p_n)

# Absent, the fix is not taken, exactly: the guard reads no inexact truth
# before it.
>> [fo.fix_290, fo.fix_300, fo.fix_490, fo.fix_500]
[~1, 0, 0, ~1]

>> [fo.lost_414, fo.lost_415, fo.lost_499, fo.lost_500]
[~0, ~1, ~1, ~0]

# The failure stops at the update's clause after, and the command goes on.
>> fo.u_300
~-0.0848377303

# The fix failing NaN and present instead, as robot.ink's third mode did:
# NaN is no absence, so the update's comparison is refused, and with it the
# command, which the step writes NaN.
>> f3 = nav2(r_n = 1 - 3/2*(n >= 350), am_n = s3.am_n, z_n = x3.z_n)
f3 = nav2(r_n = 1 - 3/2*(n >= 350), am_n = s3.am_n, z_n = x3.z_n)

>> w3 = cart(u_n = f3.u_n)
w3 = cart(u_n = f3.u_n)

>> s3 = imu(a_n = w3.a_n, v_n = w3.v_n)
s3 = imu(a_n = w3.a_n, v_n = w3.v_n)

>> x3 = gps2(mode = 3, p_n = w3.p_n)
x3 = gps2(mode = 3, p_n = w3.p_n)

>> f3.u_300
error: a comparison needs a number, not -nan

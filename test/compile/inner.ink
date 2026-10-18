# An inner instance's optional input, compiled (DESIGN.md): its flag is
# the outer model's where the argument is the outer model's own optional
# input, 1 from 0 where it is any other input, and, where it is a sibling's
# sequence, whether a clause of that sequence's chain gave the term. Before
# the stream it is absent, whatever the argument gives there.
#
# 'robot', robot_loop.ink's loop as one instance, without an ok flag: the
# fix is the GPS instance's z, present at every tenth step but from 300 to
# 490, given to the filter's optional input. A simulation of the loop in
# doubles apart from inkamath, scipy's gain, gives the flags robot_loop.ink
# records, fix 1 at 290 and 500 and 0 at 300 and 490, lost from 415 to 499,
# u_300 = -0.0848377303296, and every fix present taken, the gate's test 67%
# of its threshold from it at the closest, lost's 0.25% and the command's
# limit 0.3%, so that no disturbed run takes another clause. Checked over
# 600 steps:
#
#     robot: 600 steps from 0, against exact values until 0 and inexact ones from there
#     robot.<s>: within 0
#
# for s.r, g.r, f.r, f.fix, f.lost and f.top, and within about 1e-15 for
# the others, g.z and f.z among them, NaN where the interpreter gives none;
# the guards last:
#
#     robot.f.fix: where z is absent, 'fix_n | (z_n - [1 0]*xp_n)^2 < gate^2*S_n = 1' falls to 'fix_n = 0'
#     robot.f.kg: where lim Z runs out, ...
#     robot.f.x: where z is absent, 'x_n | (z_n - [1 0]*xp_n)^2 < gate^2*S_n = xp_n + Pp_n*[1; 0]*(z_n - [1 0]*xp_n)/S_n' falls to 'x_n = xp_n'
#
# The step takes no input: loop_step(loop* m_).
dt = 1/100
F = [1 dt; 0 1]
G = [dt^2/2; dt]
blk(Z, i)[j<=2, k<=2] = Z[2*i - 2 + j, k]
dbl(a, v, h) = [a*(v*h + F^0)^-1*a; v + a*(v*h + F^0)^-1*v*a'; h + a'*h*(v*h + F^0)^-1*a]
Z(Q, R)_0 = [F; G*G'/R; Q]
Z(Q, R)_k = dbl(blk(Z(Q, R)_(k-1), 1), blk(Z(Q, R)_(k-1), 2), blk(Z(Q, R)_(k-1), 3))
gain(Q, R) = (R + G'*blk(lim Z(Q, R), 3)*G)^-1*G'*blk(lim Z(Q, R), 3)*F
cart(u_n) = {
    p_0 = 0
    v_0 = 0
    a_0 = 0
    a_n = u_(n-1) - v_n/10
    p_n = p_(n-1) + dt*v_(n-1) + dt^2/2*a_(n-1)
    v_n = v_(n-1) + dt*a_(n-1)
}
imu(a_n, v_n) = {
    r_0 = 777
    r_n = mod(16807*r_(n-1), 2^31 - 1)
    am_n = a_n + tanh(2*v_n)/5 + 1/20 + (r_n/(2^31 - 1) - 1/2)/10
}
gps(p_n) = {
    r_0 = 4242
    r_n = mod(16807*r_(n-1), 2^31 - 1)
    z_n | mod(n, 10) == 0 and (n < 300 or n >= 500) = p_n + (r_n/(2^31 - 1) - 1/2)/5
}
nav(qp = 1, rz = 1/300, gate = 4, pmax = 1/400, umax = 3/2, r_n, am_n, z_n?) = {
    W = [~1.0087082446036357; ~-0.49608088328325778; ~0.23801367405832982; ~2.0196709407791769]
    cb = [~-0.0017651237166237193; ~0.24287475294768127; ~-0.24650153472845093; ~-0.019883014028842676]
    V = [~0.0038140777920965263, ~-0.0014083530162575402, ~0.016730466244441917, ~0.19218841286451346]
    h(s)[j<=4] = tanh(s[j])
    corr(v) = V*h(W*v + cb) + ~0.054968825254486729
    kg(q) | gain([q 0; 0 1/10], ~1/10)[1,1] > 0 = gain([q 0; 0 1/10], ~1/10)
    kg(q) = [3 5/2]
    Kc = kg(qp)
    x_0 = [0; 0]
    P_0 = [1/100 0; 0 1/100]
    xp_0 = [0; 0]
    Pp_0 = [1/100 0; 0 1/100]
    ac_0 = 0
    ac_n = am_n - corr(x_(n-1)[2])
    xp_n = F*x_(n-1) + G*ac_n
    Pp_n = F*P_(n-1)*F' + G*G'/25
    S_n = [1 0]*Pp_n*[1; 0] + rz
    fix_n | (z_n - [1 0]*xp_n)^2 < gate^2*S_n = 1
    fix_n = 0
    x_n | (z_n - [1 0]*xp_n)^2 < gate^2*S_n = xp_n + Pp_n*[1; 0]*(z_n - [1 0]*xp_n)/S_n
    x_n = xp_n
    P_n | fix_n == 1 = Pp_n - Pp_n*[1; 0]*[1 0]*Pp_n/S_n
    P_n = Pp_n
    lost_n = P_n[1,1] > pmax
    top_n | lost_n == 1 = umax/4
    top_n = umax
    uc_n = Kc*([r_n; 0] - x_n)
    u_n | uc_n > top_n = top_n
    u_n | uc_n < -top_n = -top_n
    u_n = uc_n
}
loop() = {
    w = cart(u_n = f.u_n)
    s = imu(a_n = w.a_n, v_n = w.v_n)
    g = gps(p_n = w.p_n)
    f = nav(r_n = 1 - 3/2*(n >= 350), am_n = s.am_n, z_n = g.z_n)
}
robot = loop()

# 'fwd', forwarded: e's z is fw's own optional input, k's z the plain input
# w. z is n + 1/4 at the multiples of 3 but 9 to 14, w at every n, and
# d is the rise from three steps before. Before the stream k's z is absent
# though w_-3 is -11/4, so k.d is 0 at 0, 1 and 2 and 3 from 3; e.d is 0
# at 0, 9, 12 and 15, where one of the two is absent, and 3 at the other
# multiples of 3; e.x the last value seen, 25/4 from 6 to 14. Every term a
# dyadic, by hand and in Python's fractions. Its step is fw_step(fw* m_,
# double z_in_, int z_present_in_, double w_in_), and checked:
#
#     fwd: 100 steps from 0, against exact values
#     fwd.<s>: within 0
#
# for e.z, e.x, e.d, k.z, k.x, k.d and y, then the guards, e's and k's alike:
#
#     fwd.e.d: where z is absent, 'd_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)' falls to 'd_n = 0'
#     fwd.e.x: where z is absent, 'x_n | z_n >= 0 = z_n' falls to 'x_n = x_(n-1)'
last(z_n?) = {
    x_0 = 0
    x_n | z_n >= 0 = z_n
    x_n = x_(n-1)
    d_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)
    d_n = 0
}
fw(z_n?, w_n) = {
    e = last(z_n = z_n)
    k = last(z_n = w_n)
    y_n = e.x_n + k.d_n
}
zd_n | mod(n, 3) == 0 and (n < 9 or n > 14) = n + 1/4
fwd = fw(z_n = zd_n, w_n = n + 1/4)
# A file's definition reads it as the interpreter does, as any term: NaN
# where the interpreter refuses fwd.e.z as absent (C86). The file compiles
# whole with nothing to list.
r_n = fwd.e.z_n

# 'sib', a sibling's chain, zd's but NaN at 20, given to e as it is and to
# k doubled. NaN is no absence: a clause gave it, so the guard of x refuses
# it, as the interpreter does, and x is NaN at 20 where the step taking
# NaN for absence would hold 73/4; d is 0 there, z_17 being absent. Before
# the stream s.z_-3 is -11/4 and e's z absent, so e.d_0 is 0. The terms
# holding the NaN are reported, as any NaN of the interpreter's is:
#
#     sib: 100 steps from 0, against exact values until 20 and inexact ones from there
#     sib.<s>: -nan at 20, where the interpreter's term is not a finite number
#
# for s.z, e.z and k.z, then e.x, e.d, k.x and k.d within 0, and the guards
# as fwd's.
src() = {
    z_n | n == 20 = 0/~0
    z_n | mod(n, 3) == 0 and (n < 9 or n > 14) = n + 1/4
}
pair() = {
    s = src()
    e = last(z_n = s.z_n)
    k = last(z_n = 2*s.z_n)
}
sib = pair()

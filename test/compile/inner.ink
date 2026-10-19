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

# 'dp', two levels: a's z is deep's own optional input, a.e's a's, so
# a.e's flag is a's, forwarded twice. Every term is fwd's, a.e.x_9 6.25,
# a.e.d_18 3, a.k.d_2 0, a.y_99 102.25, and a.e.z_9 absent. Checked:
#
#     dp: 100 steps from 0, against exact values
#     dp.<s>: within 0
#
# for a.z, a.w, a.e.z, a.e.x, a.e.d, a.k.z, a.k.x, a.k.d and a.y, then
# fwd's guards under dp.a.
deep(z_n?) = {
    a = fw(z_n = z_n, w_n = n + 1/4)
}
dp = deep(z_n = zd_n)

# 'ow', the model's own guarded closed form as the argument: e's z is
# absent where no clause of q gives it, at e.z_1, though q reads no
# optional input. e.x_1 is 0, e.x_3 and e.x_5 13/4, e.d_0 0, e.d_3 3 and
# e.d_4 0. Checked, q, e.z, e.x and e.d within 0, the guards as fwd's e's.
own(w_n) = {
    q_n | mod(n, 3) == 0 = w_n
    e = last(z_n = q_n)
}
ow = own(w_n = n + 1/4)

# 'gp', a sibling's chain whose guard is NaN at 21: the interpreter
# refuses s.z_21, so e's z is there and refused, not absent: e.x is
# refused from 21 to 23, where taking it for absence would hold 73/4, and
# e.d at 21 and 24, where it would be 0, each reading z_21 beside a term
# present, so C380 is not reached; e.d_20 is 0, z_20 absent. Checked, s.z,
# e.z, e.x and e.d within 0 against the interpreter's refusals, the
# guards as fwd's e's.
gsrc() = {
    z_n | n == 21 and 0/(n - 21 + ~0) > 0 = 1
    z_n | mod(n, 3) == 0 = n + 1/4
}
gpair() = {
    s = gsrc()
    e = last(z_n = s.z_n)
}
gp = gpair()

# 'hs', a history on the inner input: before the stream e's z is 0, not
# absent, so e.d_0 is 1/4 where clearing every flag would give -1; e.d_1
# is -1, e.d_3 3. Checked, q, e.z and e.d within 0, and
#
#     hs.e.d: where z is absent, 'd_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)' falls to 'd_n = -1'
lh(z_n?) = {
    z_n | n < 0 = 0
    d_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)
    d_n = -1
}
hist(w_n) = {
    q_n | mod(n, 3) == 0 = w_n
    e = lh(z_n = q_n)
}
hs = hist(w_n = n + 1/4)

# 'gb', a sibling's chain whose guard reads y_-1 at 1, before the stream:
# the interpreter refuses s.z_1, so e's z is there and refused, not absent,
# and e.x is refused at 1 and 2, where taking it for absence would give 0;
# z_2 is absent, y_0 being 5, and e.x_3 is 3. Checked, s.y, s.z, e.z and
# e.x within 0 against the interpreter's refusals, the guard as fwd's e.x.
kept(z_n?) = {
    x_0 = 0
    x_n | z_n >= 0 = z_n
    x_n = x_(n-1)
}
bsrc() = {
    y_0 = 5
    y_n = y_(n-1) + 1
    z_0 = 1
    z_n | y_(n-2) > 5 = n
}
bpair() = {
    s = bsrc()
    e = kept(z_n = s.z_n)
}
gb = bpair()

# 'lt', a sibling's chain that starts late (C391): s.z's guard reads
# y_(n-1), so the interpreter refuses s.z_0, reading y_-1, and no clause
# gives s.z_1 to s.z_3. e's z starts with the stream all the same: there
# and refused at 0, absent from 1 to 3, and n from 4. So e.x is refused
# at 0, -1 from 1 to 3 and n from 4, and e.d, which reads z_(n-3) first,
# is refused at 3, 0 below and to 6, and 3 from 7, where starting e's z
# with s's own would leave e.d NaN from 1 to 3. b's z, s.y_(n-1), is
# absent at 0, below y's base clause, and n - 1 from 1: b.x is -1 at 0,
# and b.d 0 to 3 and 3 from 4. Checked, s.y, e.z, e.x, e.d, b.z, b.x and
# b.d within 0 against the interpreter's refusals, s.z from 1, where the
# step computes it, then the guards, b's and e's alike:
#
#     lt.b.d: where z is absent, 'd_n | z_(n-3) >= 0 and z_n > z_(n-3) = z_n - z_(n-3)' falls to 'd_n = 0'
#     lt.b.x: where z is absent, 'x_n | z_n >= 0 = z_n' falls to 'x_n = -1'
early(z_n?) = {
    x_n | z_n >= 0 = z_n
    x_n = -1
    d_n | z_(n-3) >= 0 and z_n > z_(n-3) = z_n - z_(n-3)
    d_n = 0
}
lsrc() = {
    y_0 = 0
    y_n = y_(n-1) + 1
    z_n | y_(n-1) > 2 = y_n
}
lpair() = {
    s = lsrc()
    e = early(z_n = s.z_n)
    b = early(z_n = s.y_(n-1))
}
lt = lpair()

# 'lo', inner optional inputs left out (C399): absent at every step, as the
# interpreter has them, and before the stream but where a history gives
# them. f's z is left out and forwarded to f.e, whose x holds its base, 0,
# and d is 0, so f.y is f.k.d, 0 to 2 and 3 from 3; p's z, a pair, is
# [n; 1] below 0, so p.c is -3 at 0, -2 at 1 and -1 at 2, and 1 from 3, z_0
# being absent: y is -3, -2 and -1, then 4. Checked, f.w, f.e.x, f.e.d, f.k.z,
# f.k.x, f.k.d, f.y, p.c and y within 0, f.z, f.e.z and p.z with no value
# to compare,
#
#     lo.p.z: no value compared, the step NaN where the interpreter gives none, as at 0: lo.p.z_0 is absent
#
# then f's guards as fwd's and p's:
#
#     lo.p.c: where z is absent, 'c_n | [1 0]*z_(n-3) < 0 = [1 0]*z_(n-3)' falls to 'c_n = 1'
pre(z_n[j<=2]?) = {
    z_n | n < 0 = [n; 1]
    c_n | [1 0]*z_(n-3) < 0 = [1 0]*z_(n-3)
    c_n = 1
}
alone(w_n) = {
    f = fw(w_n = w_n)
    p = pre()
    y_n = f.y_n + p.c_n
}
lo = alone(w_n = n)
# A file's instance leaving it out compiles with the file.
el = last()

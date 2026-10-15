# A robot's whole loop, checked as one instance: a cart on a rail, an IMU
# every step whose bias grows with speed, a position fix every tenth step
# that fails from step 20 to 179, a small network subtracting the bias, a
# Kalman filter, and an LQR whose gain falls to a fixed one where its limit
# runs out. The fix's outage leaves the filter lost from step 170, its
# command limited, and the first fix after it, at 180, recovers both.
#
#     robot: 200 steps from 0, against exact values until 0 and inexact ones from there
#     robot.f.lost: within 0
#     robot.f.kg: where lim Z runs out, 'kg(q) | ...' falls to 'kg(q) = [3 5/2]'
#
# In float its LCGs part at step 3, their products past 2^24 (lcg.ink), and
# the noise after is another, so it is not checked in float.
dt = 1/100
F = [1 dt; 0 1]
G = [dt^2/2; dt]
# The Riccati equation by doubling, its three iterates stacked in one term.
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
    y_m = p_(10*m) + (r_(10*m)/(2^31 - 1) - 1/2)/5
    z_n = y_(floor(n/10))
    ok_n = floor(n/10) < 2 or floor(n/10) >= 18
}
nav(qp = 1, rz = 1/300, gate = 4, pmax = 1/400, umax = 3/2, am_n, z_n, ok_n) = {
    W = [~1.0087082446036357; ~-0.49608088328325778; ~0.23801367405832982; ~2.0196709407791769]
    cb = [~-0.0017651237166237193; ~0.24287475294768127; ~-0.24650153472845093; ~-0.019883014028842676]
    V = [~0.0038140777920965263, ~-0.0014083530162575402, ~0.016730466244441917, ~0.19218841286451346]
    h(s)[j<=4] = tanh(s[j])
    corr(v) = V*h(W*v + cb) + ~0.054968825254486729
    kg(q) | gain([q 0; 0 1/10], ~1/10)[1,1] > 0 = gain([q 0; 0 1/10], ~1/10)
    kg(q) = [3 5/2]
    Kc = kg(qp)
    # x and P are guarded by fix, another sequence, which C376 does not fold
    # below the stream, so xp, Pp and ac, which read them at -1, take bases.
    x_0 = [0; 0]
    P_0 = [1/2000 0; 0 1/2000]
    xp_0 = [0; 0]
    Pp_0 = [1/2000 0; 0 1/2000]
    ac_0 = 0
    ac_n = am_n - corr(x_(n-1)[2])
    xp_n = F*x_(n-1) + G*ac_n
    Pp_n = F*P_(n-1)*F' + G*G'/25
    S_n = [1 0]*Pp_n*[1; 0] + rz
    nu_n = z_n - [1 0]*xp_n
    fix_n = mod(n, 10) == 0 and ok_n == 1 and nu_n^2 < gate^2*S_n
    x_n | fix_n == 1 = xp_n + Pp_n*[1; 0]*nu_n/S_n
    x_n = xp_n
    P_n | fix_n == 1 = Pp_n - Pp_n*[1; 0]*[1 0]*Pp_n/S_n
    P_n = Pp_n
    lost_n = P_n[1,1] > pmax
    top_n | lost_n == 1 = umax/4
    top_n = umax
    uc_n = Kc*([1; 0] - x_n)
    u_n | uc_n > top_n = top_n
    u_n | uc_n < -top_n = -top_n
    u_n = uc_n
}
loop() = {
    w = cart(u_n = f.u_n)
    s = imu(a_n = w.a_n, v_n = w.v_n)
    g = gps(p_n = w.p_n)
    f = nav(am_n = s.am_n, z_n = g.z_n, ok_n = g.ok_n)
}
robot = loop()

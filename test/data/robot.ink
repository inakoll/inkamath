# A robot's loop, for robot_loop.ink. Its LQR gain for a cart, a double
# integrator, by doubling the Riccati equation, its three iterates stacked in
# one term so that lim walks them together.
dt = 1/100
F = [1 dt; 0 1]
G = [dt^2/2; dt]
blk(Z, i)[j<=2, k<=2] = Z[2*i - 2 + j, k]
dbl(a, v, h) = [a*(v*h + F^0)^-1*a; v + a*(v*h + F^0)^-1*v*a'; h + a'*h*(v*h + F^0)^-1*a]
Z(Q, R)_0 = [F; G*G'/R; Q]
Z(Q, R)_k = dbl(blk(Z(Q, R)_(k-1), 1), blk(Z(Q, R)_(k-1), 2), blk(Z(Q, R)_(k-1), 3))
gain(Q, R) = (R + G'*blk(lim Z(Q, R), 3)*G)^-1*G'*blk(lim Z(Q, R), 3)*F

# The world: a cart with drag, pushed by the command, its motor limited to
# 2 m/s^2 whatever it is asked.
cart(c = 1/10, u_n) = {
    sat(x) | x > 2 = 2
    sat(x) | x < -2 = -2
    sat(x) = x
    p_0 = 0
    v_0 = 0
    a_0 = 0
    a_n = sat(u_(n-1)) - c*v_n
    p_n = p_(n-1) + dt*v_(n-1) + dt^2/2*a_(n-1)
    v_n = v_(n-1) + dt*a_(n-1)
}

# The IMU: true acceleration, a bias tanh(2v)/5 + 1/20, and uniform noise
# from Park and Miller's LCG.
imu(a_n, v_n) = {
    r_0 = 777
    r_n = mod(16807*r_(n-1), 2^31 - 1)
    am_n = a_n + tanh(2*v_n)/5 + 1/20 + (r_n/(2^31 - 1) - 1/2)/10
}

# The position fix, every tenth step, with noise; from fix 30 to 49 it fails:
# mode 1 says so (ok = 0), mode 2 freezes at its last value and says ok.
gps(mode = 1, p_n) = {
    r_0 = 4242
    r_n = mod(16807*r_(n-1), 2^31 - 1)
    down(m) = 30 <= m and m < 50
    y_0 = p_0 + (r_0/(2^31 - 1) - 1/2)/5
    y_m | down(m) and mode == 2 = y_(m-1)
    y_m = p_(10*m) + (r_(10*m)/(2^31 - 1) - 1/2)/5
    z_n = y_(floor(n/10))
    okf_n | down(floor(n/10)) and mode == 1 = 0
    okf_n = 1
}

# What runs on the robot: inputs the IMU, the fix and its flag, at 100 Hz.
# Its correction's weights were fitted offline to the bias.
nav(qp = 1, qa = 1/5, rz = 1/300, gate = 4, pmax = 1/400, umax = 3/2,
    W = [~1.0087082446036357; ~-0.49608088328325778; ~0.23801367405832982; ~2.0196709407791769],
    cb = [~-0.0017651237166237193; ~0.24287475294768127; ~-0.24650153472845093; ~-0.019883014028842676],
    V = [~0.0038140777920965263, ~-0.0014083530162575402, ~0.016730466244441917, ~0.19218841286451346],
    d = ~0.054968825254486729,
    r_n, am_n, z_n, ok_n) = {
    h(s)[j<=4] = tanh(s[j])
    corr(v) = V*h(W*v + cb) + d
    kg(q) | gain([q 0; 0 1/10], ~1/10)[1,1] > 0 = gain([q 0; 0 1/10], ~1/10)
    kg(q) = [3 5/2]
    Kc = kg(qp)
    Qn = G*G'*qa^2
    x_0 = [0; 0]
    P_0 = [1/100 0; 0 1/100]
    xp_0 = [0; 0]
    Pp_0 = [1/100 0; 0 1/100]
    ac_0 = 0
    ac_n = am_n - corr(x_(n-1)[2])
    xp_n = F*x_(n-1) + G*ac_n
    Pp_n = F*P_(n-1)*F' + Qn
    S_n = [1 0]*Pp_n*[1; 0] + rz
    nu_n = z_n - [1 0]*xp_n
    frozen_n = n >= 20 and z_n == z_(n-10) and z_n == z_(n-20)
    fix_n = mod(n, 10) == 0 and ok_n == 1 and frozen_n == 0 and nu_n^2 < gate^2*S_n
    x_n | fix_n == 1 = xp_n + Pp_n*[1; 0]*nu_n/S_n
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

world = cart(u_n = fc.u_n)
sens = imu(a_n = world.a_n, v_n = world.v_n)
fix = gps(p_n = world.p_n)
fc = nav(r_n = 1 - 3/2*(n >= 350), am_n = sens.am_n, z_n = fix.z_n, ok_n = fix.okf_n)

# The same loop, the fix frozen instead of flagged.

world2 = cart(u_n = fc2.u_n)
sens2 = imu(a_n = world2.a_n, v_n = world2.v_n)
fix2 = gps(mode = 2, p_n = world2.p_n)
fc2 = nav(r_n = 1 - 3/2*(n >= 350), am_n = sens2.am_n, z_n = fix2.z_n, ok_n = fix2.okf_n)

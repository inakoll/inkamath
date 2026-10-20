# The forward form compiled (DESIGN.md): each model written as
# its source writes it, x_(t+1) = f(x_t, u_t), beside its shift written by
# hand, x_t = f(x_(t-1), u_(t-1)), named 'shifted' and its own name. The
# forward form is read as its shift where it is defined, so --compile writes
# the same header for both but for the name and where its comments wrap,
# and --check the same report for an instance of each, every term against
# exact values; cli.cmake holds both. The shifts by hand are the oracle,
# each compiled and checked before the form was built.

# README section 5's plant, the explicit Euler step as the method writes it,
# driven by a ramp.
plant(dt = 1/2, u_n) = {
    x_0 = 0
    x_(n+1) = x_n + dt*(u_n - x_n)
}
shiftedplant(dt = 1/2, u_n) = {
    x_0 = 0
    x_n = x_(n-1) + dt*(u_(n-1) - x_(n-1))
}
plt = plant(u_n = 1 - n/8)
shiftedplt = shiftedplant(u_n = 1 - n/8)

# Nesterov's momentum (Sutskever, Martens, Dahl and Hinton 2013, section 2,
# eqs. (3) and (4)) on a quadratic: the weights at t + 1 read the velocity
# at t + 1, the term the same step computes.
nesterov(mu = 1/2, eps = 1/4) = {
    A = [2, 0; 0, 1]
    b = [1; 1]
    f(w) = w'*A*w/2 - b'*w
    th_0 = [0; 0]
    v_0 = [0; 0]
    v_(t+1) = mu*v_t - eps*grad_(w = th_t + mu*v_t) f(w)
    th_(t+1) = th_t + v_(t+1)
}
shiftednesterov(mu = 1/2, eps = 1/4) = {
    A = [2, 0; 0, 1]
    b = [1; 1]
    f(w) = w'*A*w/2 - b'*w
    th_0 = [0; 0]
    v_0 = [0; 0]
    v_t = mu*v_(t-1) - eps*grad_(w = th_(t-1) + mu*v_(t-1)) f(w)
    th_t = th_(t-1) + v_t
}
ball = nesterov()
shiftedball = shiftednesterov()

# k = 2: a second-order difference equation, its double pole at 3/4, the
# input two steps behind the left side.
second(a1 = 3/2, a2 = -9/16, u_n) = {
    y_0 = 0
    y_1 = 0
    y_(n+2) = a1*y_(n+1) + a2*y_n + u_n
}
shiftedsecond(a1 = 3/2, a2 = -9/16, u_n) = {
    y_0 = 0
    y_1 = 0
    y_n = a1*y_(n-1) + a2*y_(n-2) + u_(n-2)
}
pair = second(u_n = 1)
shiftedpair = shiftedsecond(u_n = 1)

# Several rates: a decimating accumulator, its samples a whole multiple of
# the index plus a constant once shifted, x_(2*m - 2) and x_(2*m - 1), as
# the compiler reads them.
acc(x_n) = {
    y_0 = 0
    y_(m+1) = y_m + x_(2*m) + x_(2*m+1)
}
shiftedacc(x_n) = {
    y_0 = 0
    y_m = y_(m-1) + x_(2*m - 2) + x_(2*m - 1)
}
total = acc(x_n = n)
shiftedtotal = shiftedacc(x_n = n)

# A hold in a forward clause: floor(n/2) shifted is floor((n - 1)/2), a
# hold as the compiler reads one.
held(x_n) = {
    y_m = x_(2*m)
    z_0 = 0
    z_(n+1) = y_(floor(n/2))
}
shiftedheld(x_n) = {
    y_m = x_(2*m)
    z_0 = 0
    z_n = y_(floor((n - 1)/2))
}
hold = held(x_n = n^2)
shiftedhold = shiftedheld(x_n = n^2)

# kalman.ink's tracker, predicted and corrected at k + 1 from the estimate
# at k, as a Kalman filter is written: the correction reads the prediction,
# the gain and the measurement at k + 1, the same step's terms.
tracker(z_n) = {
    F = [1 1; 0 1]
    H = [1 0]
    Q = [1/100 0; 0 1/100]
    R = 1
    I[j<=2, k<=2] = j == k
    x_0 = [0; 0]
    P_0 = [10 0; 0 10]
    xp_(k+1) = F*x_k
    Pp_(k+1) = F*P_k*F' + Q
    K_(k+1) = Pp_(k+1)*H'*(H*Pp_(k+1)*H' + R)^-1
    x_(k+1) = xp_(k+1) + K_(k+1)*(z_(k+1) - H*xp_(k+1))
    P_(k+1) = (I - K_(k+1)*H)*Pp_(k+1)
}
shiftedtracker(z_n) = {
    F = [1 1; 0 1]
    H = [1 0]
    Q = [1/100 0; 0 1/100]
    R = 1
    I[j<=2, k<=2] = j == k
    x_0 = [0; 0]
    P_0 = [10 0; 0 10]
    xp_k = F*x_(k-1)
    Pp_k = F*P_(k-1)*F' + Q
    K_k = Pp_k*H'*(H*Pp_k*H' + R)^-1
    x_k = xp_k + K_k*(z_k - H*xp_k)
    P_k = (I - K_k*H)*Pp_k
}
track = tracker(z_n = n/2 + (-1)^n/4)
shiftedtrack = shiftedtracker(z_n = n/2 + (-1)^n/4)

# The accumulator's samples written m*2, folded as 2*m is: x_(m*2 - 2)
# and x_(m*2 - 1) once shifted, as the compiler reads them.
sampled(x_n) = {
    y_0 = 0
    y_(m+1) = y_m + x_(m*2) + x_(m*2+1)
}
shiftedsampled(x_n) = {
    y_0 = 0
    y_m = y_(m-1) + x_(m*2 - 2) + x_(m*2 - 1)
}
tally = sampled(x_n = n)
shiftedtally = shiftedsampled(x_n = n)

# At every index, as its shift: with no base clause, c_0 reads x_(-1),
# refused as the shift is until a history or a base clause says what it is.
delay(x_n) = {
    c_(n+1) = x_n
}

# A cell of every term beside a base term by its cells is named as written.
tapped(x_n) = {
    v_0[j<=3] = 0
    v_(n+1)[2] = x_n
    v_(n+1)[j<=3] = v_n[j]
}
ranked(x_n) = {
    v_(n+1)[2, 1] = x_n
    v_(n+1) = [1, 2; 3, 4;; 5, 6; 7, 8]
}

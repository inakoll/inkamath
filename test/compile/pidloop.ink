# README's getting started: a PID controller, one step every dt, closing a
# loop on a first-order plant, x' = u - x, discretised by hand. The plant
# reads the controller a step late, which lets the loop start.
pid(dt = 1/10, r = 1, kp = 2, ki = 1, kd = 1/10, y_n) = {
    e_n = r - y_n
    s_0 = 0
    s_n = s_(n-1) + e_n*dt
    d_0 = 0
    d_n = (e_n - e_(n-1))/dt
    u_n = kp*e_n + ki*s_n + kd*d_n
}
plant(dt = 1/10, u_n) = {
    x_0 = 0
    x_n = x_(n-1) + dt*(u_(n-1) - x_(n-1))
}
ctl = pid(y_n = plt.x_n)
plt = plant(u_n = ctl.u_n)

# The PID with its output limited to what the actuator can give, and its
# integral held while the output is at a limit, so that it does not wind up.
dt = 1/10
r = 1
kp = 2
ki = 1
kd = 1/10
umax = 3/2
e_n = r - y_n
s_0 = 0
s_n | u_(n-1) >= umax = s_(n-1)
s_n | u_(n-1) <= -umax = s_(n-1)
s_n = s_(n-1) + e_n*dt
d_0 = 0
d_n = (e_n - e_(n-1))/dt
v_n = kp*e_n + ki*s_n + kd*d_n
u_n | v_n > umax = umax
u_n | v_n < -umax = -umax
u_n = v_n

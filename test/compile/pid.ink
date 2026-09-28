# A PID controller, one step every dt: the host measures y and applies u.
dt = 1/10
r = 1
kp = 2
ki = 1
kd = 1/10
e_n = r - y_n
s_0 = 0
s_n = s_(n-1) + e_n*dt
d_0 = 0
d_n = (e_n - e_(n-1))/dt
u_n = kp*e_n + ki*s_n + kd*d_n

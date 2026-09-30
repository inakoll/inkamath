# Two instances closing a loop, compiled to one step: the plant's state, then
# the controller reading it, then the plant's next input (README.md, section
# 5).
controller(kp = 2, r = 1, y_n) = { u_n = kp*(r - y_n) }
plant(dt = 1/2, u_n) = {
    x_0 = 0
    x_n = x_(n-1) + dt*(u_(n-1) - x_(n-1))
}
ctl = controller(y_n = plt.x_n)
plt = plant(u_n = ctl.u_n)

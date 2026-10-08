# J. C. Doyle, "Guaranteed Margins for LQG Regulators" (1978), its Example
# held to the step: the plant and the regulator with its filter, f = d = 5,
# by forward Euler with dt = 1/10, test/data/doyle.ink holding the exact
# values. The plant is unstable, A's eigenvalue 1 twice, so where it is
# replayed alone from the loop's input a step's rounding grows as
# (1 + dt)^n, summed over 100 steps (1.1^100 - 1)/0.1, about 1.4e5 times
# a rounding; closed, it is damped as (1 + dt lambda)^n, lambda =
# (-3 -+ sqrt(5))/2 twice, by 0.96 a step at the least.
A = [1 1; 0 1]
B = [0; 1]
C = [1 0]
G = [1; 1]

plant(dt = 1/10, u_n) = {
    x_0 = [1; 0]
    x_n = x_(n-1) + dt*(A*x_(n-1) + B*u_(n-1))
    y_n = C*x_n
}

lqg(dt = 1/10, f = 5, d = 5, y_n) = {
    xh_0 = [0; 0]
    xh_n = xh_(n-1) + dt*(A*xh_(n-1) + B*u_(n-1) + d*G*(y_(n-1) - C*xh_(n-1)))
    u_n = -f*G'*xh_n
}

closed() = {
    ctl = lqg(y_n = plt.y_n)
    plt = plant(u_n = ctl.u_n)
}

regulated = closed()
replayed = plant(u_n = regulated.ctl.u_n)

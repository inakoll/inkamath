# lim compiled (DESIGN.md, next in line): backward Euler on x' = u - x^3,
# whose step is the root of x - xp - dt*(u - x^3), found by Newton's method
# inside every step, as a loop with the interpreter's own stopping rule. A
# limit of constants is folded instead.
euler(dt = 1/10, u_n) = {
    nw(xp, v)_0 = xp
    nw(xp, v)_k = nw(xp, v)_(k-1) - (nw(xp, v)_(k-1) - xp - dt*(v - nw(xp, v)_(k-1)^3))/(1 + 3*dt*nw(xp, v)_(k-1)^2)
    h_0 = 1
    h_k = h_(k-1)/2 + 1
    x_0 = 1
    x_n = lim nw(x_(n-1), u_n)
    y_n = x_n/lim h
}
stiff = euler(u_n = 1/2)

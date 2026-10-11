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

# A repeated term is not a limit (DESIGN.md, C279): the binary digits of
# 1/(n + 2), where each 0 repeats the term before.
binary(u_n) = {
    dig(x)_0 = 0
    dig(x)_k = dig(x)_(k-1) + (dig(x)_(k-1) + 2^-k <= x)*2^-k
    y_n = lim dig(u_n)
}
halves = binary(u_n = 1/(n + 2))

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

# A step of 0 in exact terms is one of an ulp or two in doubles, which is
# not a limit either: odd terms that cancel exactly.
cancel(u_n) = {
    s(x)_0 = 0
    s(x)_k = s(x)_(k-1) + x^k/(k^2*2^k) + (-x)^k/(k^2*2^k)
    y_n = lim s(u_n)
}
evens = cancel(u_n = n/100 - 0.121)
celled(u_n) = {
    s(x)_0 = [0, 1]
    s(x)_k = [s(x)_(k-1)[1,1] + x^k/(k^2*2^k) + (-x)^k/(k^2*2^k), s(x)_(k-1)[1,2]]
    y_n = lim s(u_n)
}
evened = celled(u_n = n/100 - 0.121)

# Clauses that name their index differently, the limit's own and a sequence
# walked beside it.
ns(x)_0 = 1
ns(x)_k | k < 3 = ns(x)_(k-1) + x/k
ns(x)_j = ns(x)_(j-1) + x^j/2^j
nt(x)_0 = 1
nt(x)_k | k < 3 = nt(x)_(k-1)*x/k
nt(x)_j = nt(x)_(j-1)*x/j/2
nu(x)_0 = 0
nu(x)_k = nu(x)_(k-1) + nt(x)_k
named(u_n) = {
    a_n = lim ns(u_n)
    b_n = lim nu(u_n)
}
twonames = named(u_n = n/10 - 1)

# Wrong on purpose, a departure (C353): past about 225,000 a double cannot
# show a step within the tolerance, and once 0.8^k no longer moves 10^14 the
# walk stops on steps of 0, where the exact ones never settle.
heavy(u_n) = {
    s(x)_0 = x*10^14
    s(x)_k = s(x)_(k-1) + 0.8^k
    y_n = lim s(u_n)
}
weighed = heavy(u_n = n + 1)

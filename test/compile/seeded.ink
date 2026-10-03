# A base clause reading a term (DESIGN.md, next in line): an exponential
# average whose state starts at its first sample, y_0 = x_0, and a running sum
# seeded by another sequence's first term. A base term is computed at its own
# index, so a term it reads is a constant step back from there.
ema(a = 1/4, x_n) = {
    y_0 = x_0
    y_n = a*x_n + (1 - a)*y_(n-1)
    c_n = 3*n + 1
    s_0 = c_0
    s_n = s_(n-1) + c_n
}
warm = ema(x_n = n^2/10)

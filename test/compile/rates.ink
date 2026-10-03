# Two rates (MANIFESTO.md, Time: several rates, and events), as index
# arithmetic: a decimator keeping the mean of each pair of samples, its index
# m reading the input's at 2*m, and two holds reading its terms back at the
# input's rate, the latest at floor(n/2) and the one before at floor(n/2) - 1.
# The step is the input's, and a term of y is computed on every second one,
# the first at which the samples it reads exist: m at step 2*m.
#
# What the compiler refuses, by name, is in test/cli.cmake.
halve(x_n) = {
    y_0 = x_0
    y_m = (x_(2*m - 1) + x_(2*m))/2
    z_n = y_(floor(n/2))
    w_n = y_(floor(n/2) - 1)
}
pairs = halve(x_n = n^2)

# A decimator ticking on the odd steps, its first sample at 2*m + 1: the hold
# that reads it at the input's rate reads the term before, a step or two
# back in its window by the step's parity.
offset(x_n) = {
    y_0 = 0
    y_m = x_(2*m + 1)
    z_n = y_(floor(n/2) - 1)
}
odd = offset(x_n = n^2)

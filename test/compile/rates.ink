# Two rates (MANIFESTO.md, Time: several rates, and events), as index
# arithmetic: a decimator keeping the mean of each pair of samples, its index
# m reading the input's at 2*m, and two holds reading its terms back at the
# input's rate, the latest at floor(n/2) and the one before at floor(n/2) - 1.
# The step is the input's, and a term of y is computed on every second one,
# the first at which the samples it reads exist: m at step 2*m.
#
# What the compiler refuses, by name, written as a file of its own:
#
#     y_0 = 0
#     y_m = x_(2*m + 1)
#     z_n = y_(floor(n/2))
#     v_n = y_(floor(n/3))
#     u_m = x_(m*m)
#
#     cannot compile u: x_(...): an index other than a whole multiple of m plus a constant
#     cannot compile v: y_(...): read every 3 steps, and y is computed every 2
#     cannot compile z: y_(...): read before it is computed; read the term before it
#
# And what 'inkamath --check rates.ink pairs' reports, every term of each
# sequence held, a term of y to the latest computed at the step:
#
#     pairs: 100 steps from 0, against exact values
#     pairs.<name>: within 0, for each of x, y, z and w
halve(x_n) = {
    y_0 = x_0
    y_m = (x_(2*m - 1) + x_(2*m))/2
    z_n = y_(floor(n/2))
    w_n = y_(floor(n/2) - 1)
}
pairs = halve(x_n = n^2)

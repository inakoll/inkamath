# Decimators and interpolators (DESIGN.md, next in line: what several rates
# left refused). 'down' is a decimator as a paper draws one: an anti-aliasing
# filter at the input's rate, a closed form with no base clause, then every
# second of its terms, by a sequence with no base clause either, each term
# computed at the step of its latest sample: m at step 2*m + 1. Back at the
# input's rate, a hold, and a linear interpolator between the two latest
# terms; and a second stage, every second term of y, sampled through the
# hold, as a slow sequence cannot read another. The stream starts at 0, as a
# host's does, so that no term is answered from before it.
#
# 'conv' is a strided convolution: two channels, each a ReLU of a pair of
# samples, computed every second step cell by cell.
#
# What the compiler refuses, by name, written as files of their own. A slow
# sequence read back at the input's rate, refused by its rate rather than as
# a sample computed again; and a hold before the first tick of a slow
# sequence whose samples could give the term there:
#
#     k_n = n/8
#     y_m = k_(2*m + 1)
#     c_n = y_(n-1)
#     z_n = y_(floor((n - 1)/2))
#
#     cannot compile c: y_(...): read every step, and y is computed every 2
#     cannot compile z: z_0 reads y_-1, before y's first tick, where its samples could give a term
#
# A ratio of rates that is not whole, and a slow sequence read by another,
# by a sample or a hold:
#
#     u_0 = 0
#     u_m = x_(2*m)
#     t_m = x_(3*m)
#     q_n = t_(floor(2*n/3))
#     v_k = u_(2*k)
#     w_k = x_(4*k) - u_(floor(k/2))
#
#     cannot compile q: t_(...): an index other than a whole multiple of n plus a constant
#     cannot compile v: u_(...): one sequence at another rate read by another; hold u at the input's rate and sample the hold
#     cannot compile w: u_(...): one sequence at another rate read by another; hold u at the input's rate and sample the hold
#
# And what 'inkamath --check decimate.ink boxcar' and 'stride' report:
#
#     boxcar: 100 steps from 0, against exact values
#     boxcar.<name>: within 0, for each of f, y, z, t, w and q
#
#     stride: 100 steps from 0, against exact values
#     stride.h: within 0
down(x_n) = {
    f_n = (x_n + x_(n-1))/2
    y_m = f_(2*m + 1)
    z_n = y_(floor((n - 1)/2))
    t_n = n/2 - floor((n - 1)/2)
    w_n = y_(floor((n - 1)/2) - 1) + t_n*(y_(floor((n - 1)/2)) - y_(floor((n - 1)/2) - 1))
    q_k = z_(4*k + 3)
}
s_n | n >= 0 = n^2
boxcar = down(x_n = s_n)

conv(W = [1, 1; 1, -1], x_n) = {
    h_m[j<=2] | W[j,1]*x_(2*m) + W[j,2]*x_(2*m + 1) < 0 = 0
    h_m[j<=2] = W[j,1]*x_(2*m) + W[j,2]*x_(2*m + 1)
}
stride = conv(x_n = 3 - n/2 + (-1)^n)

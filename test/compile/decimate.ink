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
# samples, computed every second step cell by cell, on an input that takes
# each channel below 0 in turn.
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
# A ratio of rates that is not whole; a slow sequence read by another, by a
# sample or a hold, which a hold at the input's rate sampled says exactly;
# and a hold at another period than the sequence it holds, whose term falls
# further behind at each tick, which no hold says (C74):
#
#     u_0 = 0
#     u_m = x_(2*m)
#     t_m = x_(3*m)
#     p_k = x_(4*k)
#     q_n = t_(floor(2*n/3))
#     v_k = u_(2*k)
#     r_m = x_(2*m) - p_(floor(m/2))
#     w_k = x_(4*k) - u_(floor(k/2))
#
#     cannot compile q: t_(...): an index other than a whole multiple of n plus a constant
#     cannot compile r: p_(...): one sequence at another rate read by another; hold p at the input's rate and sample the hold
#     cannot compile v: u_(...): one sequence at another rate read by another; hold u at the input's rate and sample the hold
#     cannot compile w: u_(...): read every 8 steps, and u is computed every 2
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
stride = conv(x_n = 3 - n/2 + (-1)^n*(1 - n/8))

# A frame, each term the pair of samples it covers, cell j sampling
# x_(2*m + j): a cell's place is a constant there, so each cell's sample
# is of the form a whole term's is, and the phase is the latest of them.
#
#     frame: 100 steps from 0, against exact values
#     frame.p: within 0
pair(x_n) = {
    p_m[j<=2] = x_(2*m + j)
}
frame = pair(x_n = n^2)

# A decimator whose base clause has a negative index, -2 at step 0: the
# latest term at step 1 is y_-2, floor((1 - 4)/2), not y_-1.
#
#     lead: 100 steps from 0, against exact values
#     lead.y: within 0
ahead(x_n) = {
    y_(-2) = 7
    y_m = x_(2*m + 4)
}
lead = ahead(x_n = s_n)

# Holds of it, each index's numerator negative at the first steps: z_1 is
# y_-2, a step before y_-1, and w_1 and w_2 are y_-2, held from step 1.
#
#     trail: 100 steps from 0, against exact values
#     trail.<name>: within 0, for each of y, z and w
behind(x_n) = {
    y_(-2) = 7
    y_m = x_(2*m + 4)
    z_n = y_(floor(n/2) - 2)
    w_n = y_(floor((n - 3)/2) - 1)
}
trail = behind(x_n = s_n)

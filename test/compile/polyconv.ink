# conv and polyval compiled (DESIGN.md, next in line): a PI controller
# (k s + 1)/s around 1/((s + 1)(s + 2)), its closed loop's denominator
# d = s (s + 1)(s + 2) + k s + 1 by conv, d(-1/2) = 5/8 - k/2 and its
# derivative in the gain, -1/2, and d of its companion matrix, 0 by
# Cayley and Hamilton, a matrix polynomial by Horner's rule. Each number
# the step computes is a multiple of 1/8 below 64, so a double and a
# float hold it exactly, and every term is the exact one:
#
#     tune: 100 steps from 0, against exact values
#     tune.<s>: within 0
#
# for d, Z, g and y; in float, "100 steps from 0 in float" and each
# within 0, Z's line alone without its 0 units of a float, its terms 0.
closed(k_n) = {
    d_n = conv(conv([1; 1], [1; 2]), [1; 0]) + [0; 0; k_n; 1]
    y_n = polyval(d_n, -1/2)
    g_n = grad_(k = k_n) polyval(conv(conv([1; 1], [1; 2]), [1; 0]) + [0; 0; k; 1], -1/2)
    Z_n = polyval(d_n, [0, 1, 0; 0, 0, 1; -1, -2 - k_n, -3])
}
tune = closed(k_n = n/4)

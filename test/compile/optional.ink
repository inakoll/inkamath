# Optional inputs, compiled (DESIGN.md): a position measured
# where it arrives, every third step but none from 9 to 14, and 40 at 30, an
# outlier. The filter takes it where it is present and within its gate, and
# coasts elsewhere, x_n = xp_n; age counts the steps since it last took one,
# d is the rise between fixes three steps apart, and the command stops at
# its own clause, u_n = 0, from four steps without a fix. Every term is a
# dyadic of under 40 bits, so exact in a double, by Python's fractions:
# x_3 = 25/8, x_15 = 487/32, d_30 = 51/4, age_14 = 8, u_10 to u_14 and u_31
# and u_32 are 0. Before the stream nothing has arrived, so d_0 is 0.
#
# The step takes z beside a flag, 0 where z is absent:
#
#     static inline void track_step(track* m_, double z_in_, int z_present_in_) {
#
# and checked, 'seen' reports, the guards last:
#
#     seen: 100 steps from 0, against exact values
#     seen.<s>: within 0
#     seen.age: where z is absent, 'age_n | (z_n - xp_n)^2 <= gate^2 = 0' falls to 'age_n = age_(n-1) + 1'
#     seen.d: where z is absent, 'd_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)' falls to 'd_n = 0'
#     seen.x: where z is absent, 'x_n | (z_n - xp_n)^2 <= gate^2 = xp_n + a*(z_n - xp_n)' falls to 'x_n = xp_n'
#
# for xp, x, age, d and u.
track(a = 1/2, gate = 2, z_n?) = {
    xp_0 = 0
    x_0 = 0
    age_0 = 0
    xp_n = x_(n-1) + 1
    x_n | (z_n - xp_n)^2 <= gate^2 = xp_n + a*(z_n - xp_n)
    x_n = xp_n
    age_n | (z_n - xp_n)^2 <= gate^2 = 0
    age_n = age_(n-1) + 1
    d_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)
    d_n = 0
    u_n | age_n < 4 = (n - x_n)/2
    u_n = 0
}
zd_n | n == 30 = 40
zd_n | mod(n, 3) == 0 and (n < 9 or n > 14) = n + 1/4
seen = track(z_n = zd_n)

# Under grad, a guard that falls for an absent input takes the clause after,
# and only one whose limit runs out is NaN, so the audit says so of a guard
# reading both. lean calls f outside grad and under it, steep under it alone.
# Checked, each reports
#
#     leaned.f: where lim qp runs out or z is absent, 'f(a, k) | z_k > 0 and lim qp(k + 1) > 0 = a*z_k' falls to 'f(a, k) = a^2', and to NaN under grad where lim qp runs out
#
# and steeped.f the same.
lean(z_n?) = {
    qp(s)_0 = 0
    qp(s)_k = s/2 + qp(s)_(k-1)/2
    f(a, k) | z_k > 0 and lim qp(k + 1) > 0 = a*z_k
    f(a, k) = a^2
    g_n = grad_(a = 3) f(a, n)
    h_n = f(3, n)
}
steep(z_n?) = {
    qp(s)_0 = 0
    qp(s)_k = s/2 + qp(s)_(k-1)/2
    f(a, k) | z_k > 0 and lim qp(k + 1) > 0 = a*z_k
    f(a, k) = a^2
    g_n = grad_(a = 3) f(a, n)
}
leaned = lean(z_n = zd_n)
steeped = steep(z_n = zd_n)

# C380: a guard stops at the first operand that stops it, as the
# interpreter reads it, an absence or a limit run out falling to the
# clause after and NaN refused. z is n at the odd n but 3, where it is
# NaN, and absent at the even, so y is refused at 3, z_3 read first, and
# 0 at 4, z_4 absent read first; x is NaN at 3 and 1 elsewhere, and
# lim qp runs out wherever it is read, so w is refused at 3 and 0
# elsewhere. Checked, y and w within 0 against the interpreter's
# refusals, the guards last:
#
#     first.w: where lim qp runs out, 'w_n | x_n > 0 and lim qp(n) > 0 = 1' falls to 'w_n = 0'
#     first.y: where z is absent, 'y_n | z_n > 0 and z_(n-1) > 0 = 1' falls to 'y_n = 0'
stops(x_n, z_n?) = {
    qp(s)_0 = 0
    qp(s)_k = s + mod(k, 2)
    y_n | z_n > 0 and z_(n-1) > 0 = 1
    y_n = 0
    w_n | x_n > 0 and lim qp(n) > 0 = 1
    w_n = 0
}
zn_n | n == 3 = 0/~0
zn_n | mod(n, 2) == 1 = n
first = stops(x_n = 0/(n - 3 + ~0) + 1, z_n = zn_n)

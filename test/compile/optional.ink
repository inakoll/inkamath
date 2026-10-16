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

# A control cascade (MANIFESTO.md, Time: several rates, and events): an
# outer loop every fourth step sets a velocity from the position, resting
# within an eighth of the target and clamped to 2, and an inner loop at every
# step drives the plant's velocity to the setpoint it holds, with a
# derivative of its error; the plant integrates. The outer loop's guards read
# its samples, the right of an 'and' among them, and the derivative reads the
# error back, which the compiler may compute again a step earlier, the hold
# it reads with it.
#
# What the compiler refuses, by name, written as a file of its own: a hold
# below a guarded slow sequence's base clauses, where its guards could give
# a term, as a read at the input's rate is.
#
#     u_0 = 0
#     u_m | x_(2*m) > 1 = 1
#     u_m = x_(2*m)
#     w_n = u_(floor(n/2) - 1)
#
#     cannot compile w: w_0 reads u_-1, below u's base clauses, where only its guards could give a term
#
# And what 'inkamath --check cascade.ink servo' reports, a term of u held at
# every step to the latest computed:
#
#     servo: 100 steps from 0, against exact values
#     servo.<name>: within <x>, for each of p, v, u, e and f
cascade(kp = 1/2, kv = 1/2, kd = 1/4, r = 10, b = 1/8) = {
    p_0 = 0
    p_n = p_(n-1) + v_(n-1)/4
    v_0 = 0
    v_n = v_(n-1) + f_(n-1)/2
    u_0 = 0
    u_m | p_(4*m) > r - b and p_(4*m) < r + b = 0
    u_m | kp*(r - p_(4*m)) > 2 = 2
    u_m | kp*(r - p_(4*m)) < -2 = -2
    u_m = kp*(r - p_(4*m))
    e_n = u_(floor(n/4)) - v_n
    f_0 = kv*e_0
    f_n = kv*e_n + kd*(e_n - e_(n-1))
}
servo = cascade()

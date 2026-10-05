# A float target (DESIGN.md, next in line): '--float' beside '--compile' or
# '--check' writes floats where the header wrote doubles, and the check holds
# that step to the interpreter's terms, exact where they can be, as before.
# Its numbers are worked out by hand, in C with float and in numpy's float32,
# which agree to the bit, against exact terms; none is recorded.
#
# A float target is far from one part in a billion. A term parts where it is
# more than a thousandth of one plus the interpreter's term from it, and each
# line says how far its sequence came, as now, and that distance in units of
# a float: the spacing of floats at the interpreter's term, or at 1 where the
# term is smaller. The step's term, where it parts, is shown to nine digits.
#
# 'calm', 'wild' and 'ledge' of drift.ink:
#
#     calm: 100 steps from 0 in float, against exact values
#     calm.v: within 1.1e-06, 1.4 units of a float
#
#     wild: 100 steps from 0 in float, against exact values
#     wild.d: 0.096026361 at 6, where the interpreter gives 0.10000000000000001
#
#     ledge: 100 steps from 0 in float, against exact values
#     ledge.g: at 1 the compiled step takes 'g_n | d_n < c - w = 1' and the interpreter 'g_n = 0'; the guard of the first is 1e-12 from its threshold
#     ledge.d: 0.096026361 at 6, where the interpreter gives 0.10000000000000001
#     ledge.g: 1 at 1, where the interpreter gives 0
#
# 'ledge', whose margin a double holds out to the sixth step, flips at the
# first, as 'brink' does: a trillionth is below a float's spacing at a tenth.
#
# 'gate' of logistic.ink, its limit walked in float, and 'ball' of
# momentum.ink, its exp the prelude's. Each estimate is the seeds', and
# ball's digits are its powf's or, with the prelude written in inkamath, its
# exp's; either way under 6 units:
#
#     gate: 100 steps from 0 in float, against exact values until 0 and inexact ones from there
#     gate.b: within 1.2e-06, 2.5 units of a float; the interpreter's terms about <e> from the exact ones
#     gate.w: within 9.4e-07, 3.9 units of a float; the interpreter's terms about <e> from the exact ones
#     gate.p: within 2.1e-07, 1.8 units of a float; the interpreter's terms about <e> from the exact ones
#
#     ball: 100 steps from 0 in float, against exact values until 0 and inexact ones from there
#     ball.<s>: within <d>, <u> units of a float; the interpreter's terms about <e> from the exact ones
#
# 'stiff' of newton.ink, Newton's method inside every step. Its iterates
# close on a pair of floats a unit apart, a step a float cannot make smaller,
# and the interpreter's 1e-10 is below a float's spacing past 8.4e-4: held to
# it alone, the limit is NaN from step 25 on. A step within FLT_EPSILON
# times its term ends the walk too, after 2 or 3 terms:
#
#     stiff: 100 steps from 0 in float, against exact values until 0 and inexact ones from there
#     stiff.h: within 6e-08, 0.5 units of a float
#     stiff.x: within <d>, <u> units of a float; the interpreter's terms about <e> from the exact ones
#     stiff.y: within <d>, <u> units of a float; the interpreter's terms about <e> from the exact ones
#
# with x 1.6e-07 and 1.4 units and y 8.1e-08 and 0.68, by glibc's powf.
#
# 'fan' of inputs.ink, its input a pointer to floats:
#
#     fan: 100 steps from 0 in float, against exact values
#     fan.y: within 0, 0 units of a float
#
# Two in this file. A resonator, a second-order filter whose poles are at
# radius r: its coefficients are derived in float, and the closer the poles
# to the unit circle the more of their rounding its terms carry. 'hum' at 0.99
# and 'drone' at 0.999, from rest, on a step:
#
#     hum: 100 steps from 0 in float, against exact values
#     hum.y: within 2.7e-06, 22 units of a float
#
#     drone: 100 steps from 0 in float, against exact values
#     drone.y: within 5.6e-06, 47 units of a float
#
# And large coordinates: a position a million from its origin, moved a
# hundredth at each step. A float's spacing there is a sixteenth, so the
# position never moves: 0.99 from the interpreter's at 99, which is 16 units
# of a float and well within the tolerance, while the motion, which has lost
# every digit, parts at once:
#
#     far: 100 steps from 0 in float, against exact values
#     far.x: within 0.99, 16 units of a float
#     far.d: 0 at 1, where the interpreter gives 0.01
#
# In double each of the three is within the billionth.
#
# The header changes only its words for numbers. 'dot' of inputs.ink, by
# name:
#
#     typedef struct dot {
#         long long index_;
#         float x[1][2][1];
#         float y[1];
#     } dot;
#     ...
#     static inline void dot_step(dot* m_, const float x[2]) {
#
# 'gate''s limit, whose header includes <float.h> before <math.h>:
#
#     static inline float gate_lim0(const gate* m_, float arg_z) {
#     ...
#             const float t_ = t1_ + (t1_ - t2_) * arg_z / (float)k_;
#             if (started_) {
#                 step_ = fabsf(t_ - t1_);
#                 if (step_ <= FLT_EPSILON * fabsf(t_) && stepped_) return t_;
#                 if (step_ <= 1e-10f && stepped_ &&
#     ...
#         m_->p[0][0][0] = 1.0f / (1.0f + gate_lim0(m_, 0.0f - (0.0f * m_->w[0][0][0] + 0.0f * m_->w[0][1][0] + m_->b[0])));
#
# And in the program checking 'calm', the inputs and the step's terms as
# floats, each input the float nearest the interpreter's:
#
#     static const float in_0[100] = {
#         0.0f, 0.1f, 0.2f, 0.3f,
#     ...
#     static float got_0[100];
#     ...
#             calm_step(&m, in_0[n]);
#             memcpy(&got_0[n * 1], &m.v[0], sizeof(float) * 1);
#
# Whole headers, for pid.ink and adc.ink, are in compile/expected/float.
resonator(r = 99/100, c = 3/5, x_n) = {
    a1 = -2*r*c
    a2 = r^2
    b0 = 1 + a1 + a2
    y_0 = 0
    y_1 = 0
    y_n = b0*x_n - a1*y_(n-1) - a2*y_(n-2)
}
hum = resonator(x_n = 1)
drone = resonator(r = 999/1000, x_n = 1)

glide(x0 = 10^6, v = 1/100) = {
    x_0 = x0
    x_n = x_(n-1) + v
    d_0 = v
    d_n = x_n - x_(n-1)
}
far = glide()

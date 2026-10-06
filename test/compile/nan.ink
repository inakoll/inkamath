# A NaN reaches every term that reads it (DESIGN.md, next in line). NaN is
# the step's word for what the interpreter refuses, but a comparison with NaN
# is false, so the guard reading one took the clause after it and the
# failure left as a plausible value. In a header that writes NaN, a guard or
# a comparison whose operand is NaN is to answer NaN.
#
# 'run' reads a limit that does not converge from step 8, where r is -1;
# 'sign' a function no clause of which applies from step 4, through a
# comparison and through a power, pow(1, NaN) being 1 in C; 'shy' a clause
# the interpreter refuses wherever it is taken (C86), from step 3; 'slope'
# the same as 'sign', cell by cell, and through a healthy cell of a term the
# interpreter refuses whole; 'pair' the same as 'sign' through a bare
# truth from step 2, and an 'and' whose right side reads it from 3 and, at 0,
# a term before its start; 'lap' a guard inside a limit's walk reading the
# same as 'sign' from step 2; 'trim' a guard computed in update reading it at
# the default of its parameter, so at every step; 'any' an 'or' whose right
# side reads it from step 4. Today each parts where the failure was made a
# value:
#
#     run.high: 0 at 8, where the interpreter gives none: p did not converge within 100 terms (last term 1)
#     sign.on: 0 at 4, where the interpreter gives none: no clause of r applies
#     sign.p: 1 at 4, where the interpreter gives none: no clause of r applies
#     shy.z: 0 at 3, where the interpreter gives none: division by zero
#     slope.y[1,1]: 1 at 2, where the interpreter gives none: no clause of r applies
#     slope.g[1,1]: 0 at 0, where the interpreter gives none: no clause of r applies
#     slope.h: 0 at 0, where the interpreter gives none: no clause of r applies
#     pair.w: 0 at 3, where the interpreter gives none: no clause of r applies
#     pair.z: 1 at 2, where the interpreter gives none: no clause of r applies
#     lap.y: 5.8207660913467407e-11 at 2, where the interpreter gives none: no clause of r applies
#     trim.y: 0 at 0, where the interpreter gives none: no clause of r applies
#     any.o: 0 at 4, where the interpreter gives none: no clause of r applies
#
# 'pair's program, built with -fsanitize=float-cast-overflow, stops at 0,
# where its 'and' is NaN and the clause kept is an int (C90). Each is to
# report:
#
#     run: 100 steps from 0, against exact values until 0 and inexact ones from there
#     run.y: within <x>
#     run.high: within 0
#
#     sign: 100 steps from 0, against exact values
#     sign.y: within 0
#     sign.on: within 0
#     sign.p: within 0
#
#     shy: 100 steps from 0, against exact values
#     shy.y: within 0
#     shy.z: within 0
#
#     slope: 100 steps from 0, against exact values
#     slope.y: within 0
#     slope.g: within 0
#     slope.h: within 0
#
#     pair: 100 steps from 0, against exact values
#     pair.w: within 0
#     pair.y: within 0
#     pair.z: within 0
#
#     lap: 100 steps from 0, against exact values until 0 and inexact ones from there
#     lap.y: within <x>
#
#     trim: 100 steps from 0, against exact values
#     trim.y: within 0
#
#     any: 100 steps from 0, against exact values
#     any.y: within 0
#     any.o: within 0
#
# 'slope' because a matrix term with a NaN cell is NaN in every cell: at 2,
# y_2[2] has no clause, so y_2[1], which is 1, is NaN with it, and so is h_2,
# which reads it and would otherwise be 1. So is a cell of a matrix that is
# no sequence's term: 'cell' reads one of a matrix written inline, c from 2,
# of one given to a function, p from 2, and of a term computed again, b from
# 3; 'held' of a value from the parameters, at every step. Each answered 1
# where the interpreter gives none, and is to report within 0:
#
#     cell: 100 steps from 0, against exact values
#     cell.b: within 0
#     cell.c: within 0
#     cell.m: within 0
#     cell.p: within 0
#
#     held: 100 steps from 0, against exact values
#     held.d: within 0
#
# Each model compiles as now, nothing on stderr, and its step decides so,
# 'inkamath --compile nan.ink <model> -o <model>.h' writing, for level,
# pick, refuse, ramp and both:
#
#     m_->y[0] = level_lim0(m_, m_->x[0]);
#     m_->high[0] = isnan(m_->y[0]) ? NAN : m_->y[0] > 0.5 ? 1.0 : 0.0;
#
#     m_->y[0] = isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN;
#     m_->on[0] = isnan(m_->y[0]) ? NAN : m_->y[0] > 2.0 ? 1.0 : 0.0;
#     m_->p[0] = (isnan(m_->y[0]) ? NAN : pow(1.0, m_->y[0]));
#
#     m_->y[0] = isnan(m_->x[0]) ? NAN : m_->x[0] <= 0.0 ? NAN : m_->x[0];
#     m_->z[0] = isnan(m_->y[0]) ? NAN : m_->y[0] > 1.0 ? 1.0 : 0.0;
#
#     m_->y[0][0][0] = isnan(m_->x[0] - 1.0) ? NAN : m_->x[0] - 1.0 > 0.0 ? m_->x[0] - 1.0 : NAN;
#     m_->y[0][1][0] = isnan(m_->x[0] - 2.0) ? NAN : m_->x[0] - 2.0 > 0.0 ? m_->x[0] - 2.0 : NAN;
#     if (isnan(m_->y[0][0][0]) || isnan(m_->y[0][1][0]))
#         for (int i_ = 0; i_ < 2; ++i_)
#             for (int j_ = 0; j_ < 1; ++j_) m_->y[0][i_][j_] = NAN;
#     m_->g[0][0][0] = isnan(m_->y[0][0][0]) ? NAN : m_->y[0][0][0] > 1.0 ? 1.0 : 0.0;
#     m_->g[0][1][0] = isnan(m_->y[0][1][0]) ? NAN : m_->y[0][1][0] > 1.0 ? 1.0 : 0.0;
#     if (isnan(m_->g[0][0][0]) || isnan(m_->g[0][1][0]))
#         for (int i_ = 0; i_ < 2; ++i_)
#             for (int j_ = 0; j_ < 1; ++j_) m_->g[0][i_][j_] = NAN;
#     m_->h[0] = isnan(m_->y[0][0][0]) ? NAN : m_->y[0][0][0] > 0.5 ? 1.0 : 0.0;
#
#     const double t0_ = (isnan(m_->x[0]) ? NAN : m_->x[0] < 3.0 ? 1.0 : 0.0);
#     const double t1_ = (t0_ == 0.0 ? 0.0 : t0_ != t0_ ? NAN : (m_->index_ < 1 ? NAN : (isnan(m_->y[1]) ? NAN : m_->y[1] > 0.5 ? 1.0 : 0.0)));
#     m_->w[0] = isnan(t1_) ? NAN : t1_ != 0.0 ? 1.0 : 0.0;
#     m_->y[0] = m_->index_ == 0 ? 2.0 : isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN;
#     m_->z[0] = isnan(m_->y[0]) ? NAN : m_->y[0] != 0.0 ? 1.0 : 0.0;
#
# for walked, its limit's term and its step,
#
#         const double t_ = isnan(arg_a) ? NAN : arg_a > 1.0 ? 0.0 : t1_ / 2.0;
#
#     m_->y[0] = walked_lim0(m_, isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN);
#
# for tuned, its update,
#
#     m_->a = isnan(m_->g) ? NAN : m_->g > 0.0 ? m_->g : NAN;
#     m_->k = isnan(m_->a) ? NAN : m_->a > 1.0 ? 1.0 : 0.0;
#
# and for either,
#
#     m_->y[0] = isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN;
#     const double t0_ = (isnan(m_->x[0]) ? NAN : m_->x[0] > 3.0 ? 1.0 : 0.0);
#     const double t1_ = (t0_ == 0.0 ? ((isnan(m_->y[0]) ? NAN : m_->y[0] > 0.5 ? 1.0 : 0.0)) : t0_ != t0_ ? NAN : 1.0);
#     m_->o[0] = isnan(t1_) ? NAN : t1_ != 0.0 ? 1.0 : 0.0;
#
# with the clause kept for --check 0 where the guard reads NaN:
#
#     m_->high_clause_ = isnan(m_->y[0]) ? 0 : m_->y[0] > 0.5 ? 1 : 2;
#     m_->w_clause_ = isnan(t1_) ? 0 : t1_ != 0.0 ? 1 : 2;
#     m_->o_clause_ = isnan(t1_) ? 0 : t1_ != 0.0 ? 1 : 2;
#
# and the Interface comment saying so, level's paragraph ending:
#
#     * name_(n-k) for each sequence: x, y and high. A term the interpreter would
#     * refuse is NaN, and so is every term that reads one, through a guard or a
#     * comparison as through arithmetic. Built with -ffinite-math-only, which
#     * -ffast-math implies, GCC removes the tests that make it so, and Clang warns
#     * of each NaN.
#
# A header that writes no NaN is byte for byte as now: kernel.h, net.h and
# pid_clamped.h keep their clamps and ReLUs untested, and bank, chain, fir,
# kalman, loop, mix and pid stay as they are. back.h gains the sentence
# alone, wrapped with its paragraph. heat.h and kalman2.h, whose inverses
# write NaN, gain it and a test after each matrix term, heat.h's step ending
#
#     m_->u[0][2][0] = m_->index_ == 0 ? 0.0 : m_->A[2][0] * t1_ + m_->A[2][1] * t2_ + m_->A[2][2] * t3_;
#     if (isnan(m_->u[0][0][0]) || isnan(m_->u[0][1][0]) || isnan(m_->u[0][2][0]))
#         for (int i_ = 0; i_ < 3; ++i_)
#             for (int j_ = 0; j_ < 1; ++j_) m_->u[0][i_][j_] = NAN;
#
# and kalman2.h testing Pp, K and xp inside the blocks that compute them, at
# eight spaces, and P, z and x after their cells. adc.h, whose rising
# edge writes NaN before its first term, ends its paragraph
#
#     * hi = 1.0, lo = -1.0 and q = 0.25. After assigning one, call adc_update. A
#     * term the interpreter would refuse is NaN, and so is every term that reads
#     * one, through a guard or a comparison as through arithmetic. Built with
#     * -ffinite-math-only, which -ffast-math implies, GCC removes the tests that
#     * make it so, and Clang warns of each NaN.
#
# and steps, from v on, its 'and' and 'or' as truths of 1, 0 or NaN, its
# temporaries numbered in the order the sequences are compiled, by name, so
# alarm's first:
#
#     m_->v[0] = m_->q * floor(m_->x[0] / m_->q);
#     const double t1_ = (isnan(m_->held[1] + (0.0 - m_->v[0])) || isnan(m_->q) ? NAN : m_->held[1] + (0.0 - m_->v[0]) < m_->q ? 1.0 : 0.0);
#     const double t2_ = (t1_ == 0.0 ? 0.0 : t1_ != t1_ ? NAN : (isnan(m_->v[0] + (0.0 - m_->held[1])) || isnan(m_->q) ? NAN : m_->v[0] + (0.0 - m_->held[1]) < m_->q ? 1.0 : 0.0));
#     m_->held[0] = m_->index_ == 0 ? 0.0 : isnan(t2_) ? NAN : t2_ != 0.0 ? m_->held[1] : m_->v[0];
#     const double t0_ = (isnan(m_->held[0]) || isnan(m_->lo) ? NAN : m_->held[0] < m_->lo ? 1.0 : 0.0);
#     m_->alarm[0] = (t0_ == 0.0 ? (isnan(m_->held[0]) || isnan(m_->hi) ? NAN : m_->held[0] > m_->hi ? 1.0 : 0.0) : t0_ != t0_ ? NAN : 1.0);
#     const double t3_ = ((double)m_->index_ > 0.0 ? 1.0 : 0.0);
#     m_->rising[0] = (t3_ == 0.0 ? 0.0 : t3_ != t3_ ? NAN : (m_->index_ < 1 ? NAN : (isnan(m_->held[0]) || isnan(m_->held[1]) ? NAN : m_->held[0] > m_->held[1] ? 1.0 : 0.0)));
#
# adc_test.c passes against it unchanged. compile_c86 and compile_log_refused
# in test/cli.cmake do not move.
#
# Of the instances checked in test/CMakeLists.txt, the programs of those
# whose step writes NaN move and their reports do not: ajar and rise test
# their 'and's right side, x_(n-3) and x_n against x_(n-1); cls, gate, mark
# and rnn each matrix term; stiff, whose powers have constant exponents,
# gains the sentence alone. Every other program is byte for byte as now.
level(x_n) = {
    p(r)_0 = 1
    p(r)_k = r*p(r)_(k-1)
    y_n = lim p(x_n)
    high_n | y_n > 1/2 = 1
    high_n = 0
}
run = level(x_n = 1 - n/4)

pick(x_n) = {
    r(v) | v > 0 = v
    y_n = r(x_n)
    on_n | y_n > 2 = 1
    on_n = 0
    p_n = 1^y_n
}
sign = pick(x_n = 4 - n)

refuse(x_n) = {
    h(v) = v
    h(v) | v <= 0 = 1/0
    y_n = h(x_n)
    z_n | y_n > 1 = 1
    z_n = 0
}
shy = refuse(x_n = 3 - n)

ramp(x_n) = {
    r(v) | v > 0 = v
    y_n[j<=2] = r(x_n - j)
    g_n[j<=2] | y_n[j] > 1 = 1
    g_n[j<=2] = 0
    h_n | y_n[1] > 1/2 = 1
    h_n = 0
}
slope = ramp(x_n = n)

both(x_n) = {
    r(v) | v > 0 = v
    y_0 = 2
    y_n = r(x_n)
    z_n | y_n = 1
    z_n = 0
    w_n | x_n < 3 and y_(n-1) > 1/2 = 1
    w_n = 0
}
pair = both(x_n = 2 - n)

walked(x_n) = {
    r(v) | v > 0 = v
    p(a)_0 = 1
    p(a)_k = p(a)_(k-1)/2
    p(a)_k | a > 1 = 0
    y_n = lim p(r(x_n))
}
lap = walked(x_n = 2 - n)

tuned(x_n, g = 0) = {
    r(v) | v > 0 = v
    s(v) = 0
    s(v) | v > 1 = 1
    a = r(g)
    k = s(a)
    y_n = k + x_n
}
trim = tuned(x_n = n)

either(x_n) = {
    r(v) | v > 0 = v
    y_n = r(x_n)
    o_n | x_n > 3 or y_n > 1/2 = 1
    o_n = 0
}
any = either(x_n = 4 - n)

cut(x_n) = {
    r(v) | v > 0 = v
    f(m) = m[2]
    m_n = [r(2 - n); 1]
    c_n = ([r(x_n); 1])[2]
    p_n | f([r(x_n); 1]) > 1/2 = 1
    p_n = 0
    b_n | m_(n-1)[2] > 1/2 = 1
    b_n = 0
}
cell = cut(x_n = 2 - n)

kept(x_n, a = -1) = {
    r(v) | v > 0 = v
    M = [r(a); 1]
    d_n | M[2] > x_n = 1
    d_n = 0
}
held = kept(x_n = n)

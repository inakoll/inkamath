# Locals compiled (DESIGN.md): each model written with locals, as phase 8
# has them, beside its twin written without, named 'staged' and its own
# name. A local is the value of its right side, a temporary of the step
# where it is bound, read after it in the evaluator's order and gone at the
# end of its clause, so --check gives the same report for an instance of
# each but for the twin's own sequences; cli.cmake holds it, and rk's
# header. The twins are the oracle, each compiled and checked before
# locals were; every value below is worked by hand or with exact fractions,
# none recorded. Before, each model with locals was refused, 'cannot compile
# x: a local definition'.

# Runge-Kutta 4 on x' = -x, its stages as locals, each read by the next:
# r.x_1 is 1 - h + h^2/2 - h^3/6 + h^4/24 = 72387/80000 = 0.9048375, and
# r.x_n its nth power. Its step is a temporary per stage, the twin's stage
# assignments read as their temporaries,
#
#     const double t0_ = 0.0 - m_->x[1];
#     const double t1_ = 0.0 - (m_->x[1] + m_->h / 2.0 * t0_);
#     const double t2_ = 0.0 - (m_->x[1] + m_->h / 2.0 * t1_);
#     const double t3_ = 0.0 - (m_->x[1] + m_->h * t2_);
#     m_->x[0] = m_->index_ == 0 ? 1.0 : m_->x[1] + m_->h / 6.0 * (t0_ + 2.0 * t1_ + 2.0 * t2_ + t3_);
#
# where the twin keeps each stage in the struct, two terms of it, k1[2] and
# the rest, shifted at every step for nothing to read.
f(x) = -x
rk(h = 1/10) = {
    x_0 = 1
    x_(n+1) = x_n + h/6*((k1 = f(x_n)) + 2*(k2 = f(x_n + h/2*k1)) + 2*(k3 = f(x_n + h/2*k2)) + (k4 = f(x_n + h*k3)))
}
stagedrk(h = 1/10) = {
    x_0 = 1
    k1_n = f(x_n)
    k2_n = f(x_n + h/2*k1_n)
    k3_n = f(x_n + h/2*k2_n)
    k4_n = f(x_n + h*k3_n)
    x_(n+1) = x_n + h/6*(k1_n + 2*k2_n + 2*k3_n + k4_n)
}
r = rk()
stagedr = stagedrk()

# The same on a state, x'' = -x as x' = A*x: each stage a column, a local
# whose value is a matrix. A^2 = -I, so o.x_1 is ((1 - h^2/2 + h^4/24)*I +
# (h - h^3/6)*A)*[1; 0] = [238801/240000; -599/6000].
osc(h = 1/10) = {
    A = [0 1; -1 0]
    g(v) = A*v
    x_0 = [1; 0]
    x_(n+1) = x_n + h/6*((k1 = g(x_n)) + 2*(k2 = g(x_n + h/2*k1)) + 2*(k3 = g(x_n + h/2*k2)) + (k4 = g(x_n + h*k3)))
}
stagedosc(h = 1/10) = {
    A = [0 1; -1 0]
    g(v) = A*v
    x_0 = [1; 0]
    k1_n = g(x_n)
    k2_n = g(x_n + h/2*k1_n)
    k3_n = g(x_n + h/2*k2_n)
    k4_n = g(x_n + h*k3_n)
    x_(n+1) = x_n + h/6*(k1_n + 2*k2_n + 2*k3_n + k4_n)
}
o = osc()
stagedo = stagedosc()

# Read twice, docs/language.md's sq: y is (u + 1)^2 + c, c (p + 1)^2 = 16
# computed where p is set, its s its own, where the model's would make it 400; z
# calls a local's function twice, each call binding its own s, (u + 1)^2 +
# (2u + 1)^2; w's callee reads the model's s, 100, not its caller's local,
# so w is u + 100; v reads the model's s before its local, 100 + u^2; a's
# argument binds its local once, (u + 102)^2 + 100 + u (C408). With u_n =
# n/4 every value is a double exactly: each sequence within 0.
twice(p = 3, s = 100, u_n) = {
    sq(x) = (s = x + 1)*s
    q(x) = s + 0*x
    c = (s = p + 1)*s
    y_n = (s = u_n + 1)*s + c
    z_n = sq(u_n) + sq(2*u_n)
    w_n = (s = u_n) + q(u_n)
    v_n = s + (s = u_n)*s
    a_n = sq((s = s + n/4) + 1) + s
}
stagedtwice(p = 3, s = 100, u_n) = {
    sq(x) = (x + 1)*(x + 1)
    q(x) = s + 0*x
    c = (p + 1)*(p + 1)
    y_n = (u_n + 1)*(u_n + 1) + c
    z_n = sq(u_n) + sq(2*u_n)
    w_n = u_n + q(u_n)
    v_n = s + u_n*u_n
    a_n = (s + u_n + 2)*(s + u_n + 2) + s + u_n
}
tw = twice(u_n = n/4)
stagedtw = stagedtwice(u_n = n/4)

# A sum unrolled term by term shares its line's locals, as the interpreter's
# frame does: each term reads the t the one before bound, and the line the
# last term's after it, 0 + (0 + u) + (u + 2u) + (2u + 3u) + 3u = 12u. An
# empty sum binds nothing, so e's t after it is still u, and e is 2u. A
# guard's local is read by its clause, and taken back where the guard fails
# (C300), so g's last clause reads the parameter d, 10: g is (u - 2)^2
# from u > 2, n > 8, and 10 up to it. Within 0 each.
across(d = 10, u_n) = {
    y_n = (t = 0) + (sum_(k=1)^3 (t + (t = k*u_n))) + t
    e_n = (t = u_n) + (sum_(k=1)^0 (t = 2*k)) + t
    g_n | (d = u_n - 2) > 0 = d*d
    g_n = d
}
stagedacross(d = 10, u_n) = {
    y_n = 0 + ((0 + u_n) + (u_n + 2*u_n) + (2*u_n + 3*u_n)) + 3*u_n
    e_n = u_n + 0 + u_n
    g_n | u_n - 2 > 0 = (u_n - 2)*(u_n - 2)
    g_n = d
}
ac = across(u_n = n/4)
stagedac = stagedacross(u_n = n/4)

# Each term a limit walks is evaluated in a frame of its own (C66), so a
# local in its iterate is its term's, a temporary of the walk's loop: r is
# sqrt(u + 1) by Newton's iteration, inexact as any limit, and its report
# the twin's. A local a term of a sum without an upper bound binds and reads
# itself, carried to no other term, is a temporary of the walk's loop as
# well: q is u^2/3, the sum of (u/2^k)^2.
root(u_n) = {
    nw(a)_0 = a
    nw(a)_k = (y = nw(a)_(k-1))/2 + a/(2*y)
    r_n = lim nw(u_n + 1)
    q_n = sum_(k=1) ((t = u_n/2^k)*t)
}
stagedroot(u_n) = {
    nw(a)_0 = a
    nw(a)_k = nw(a)_(k-1)/2 + a/(2*nw(a)_(k-1))
    r_n = lim nw(u_n + 1)
    q_n = sum_(k=1) (u_n/2^k*(u_n/2^k))
}
rt = root(u_n = n/4)
stagedrt = stagedroot(u_n = n/4)

# Under grad a local carries its part beside it. d is the sensitivity of
# the logistic's Runge-Kutta step to where it starts, gradlocal.ink's rk:
# at 1/2, 2510301413390836485599/2516582400000000000000, and x_1 is
# 281846054463086586882133/536870912000000000000000. e reads a local
# bound in grad's body after it, at the point, its part dropped: 8u + 2u.
# m is a local of a matrix's gradient, reverse mode, z = a + 2b at
# [a b] = [u 1]: 2z*[1 2], [2u + 4, 4u + 8]. In a nest, s's local t is
# read inside the inner grad with its outer part, so s is the derivative
# of x^2 + x^2, 4u; and v's local is bound inside the inner grad and read
# after it, at the inner point, moving with x: the derivative of x + x^2,
# 1 + 2u. Leaving a grad, a local keeps only the parts of the grads around
# it (C322): p's a is 3x, by x alone, so the grad by z beside the one that
# bound it reads it as a constant, and p is the derivative of x*(3 + 3x),
# 3 + 6u, where a's part by y read as one by z would give 6 + 6u; q's a is
# bound by reverse mode in grad's point and read in its body, 3u + 1, no
# node of the tape gone read. j's local holds a comparison that jumps at u
# = 1, n = 4, where the step is NaN as the twin's, though nothing reads it
# (C410). o's t, read in a nest, is still 4 after it, a constant that
# bounds a sum and places a cell: 8 + 10 + 4; r's second d, bound in a nest
# as the first was, moves with y as the first did, 3 + 3. With u_n = n/4,
# all but x and d within 0.
sens(h = 1/10, u_n) = {
    lg(y) = y - y^2
    x_0 = 1/2
    x_n = x_(n-1) + h/6*((a = lg(x_(n-1))) + 2*(b = lg(x_(n-1) + h/2*a)) + 2*(c = lg(x_(n-1) + h/2*b)) + lg(x_(n-1) + h*c))
    d_n = grad_(y = x_(n-1)) (y + h/6*((a = lg(y)) + 2*(b = lg(y + h/2*a)) + 2*(c = lg(y + h/2*b)) + lg(y + h*c)))
    e_n = (grad_(v = u_n) ((t = 2*v)*t)) + t
    m_n = grad_(w = [u_n, 1]) ((z = w*[1; 2])*z)
    s_n = grad_(x = u_n) ((t = x*x) + grad_(y = x) (t*y))
    v_n = grad_(x = u_n) (grad_(y = x) ((t = x*y)*1) + t)
    p_n = grad_(x = u_n) x*(grad_(y = x) ((a = 3*y) + 0) + grad_(z = 1) z*a)
    q_n = grad_(x = (grad_(v = [u_n, 1]) ((a = 3*v[1,1] + v[1,2]) + 0))[1,1]) x*a
    j_n = grad_(y = 1) ((c = (y*(u_n - 1) > 0)) + y*y)
    o_n = grad_(x = 2) ((t = x*x) + grad_(y = x) (t*y) + 0) + sum_(k=1)^t k + [1, 2, 3, 4, 5][1, t]
    r_n = grad_(y = u_n) ((grad_(w = [y, 1]) ((d = w*[1; 2])*d))[1,1] + d) + grad_(y = u_n) ((grad_(w = [y, 1]) ((d = w*[1; 2])*d))[1,1] + d)
}
stagedsens(h = 1/10, u_n) = {
    lg(y) = y - y^2
    x_0 = 1/2
    x_n = x_(n-1) + h/6*(lg(x_(n-1)) + 2*lg(x_(n-1) + h/2*lg(x_(n-1))) + 2*lg(x_(n-1) + h/2*lg(x_(n-1) + h/2*lg(x_(n-1)))) + lg(x_(n-1) + h*lg(x_(n-1) + h/2*lg(x_(n-1) + h/2*lg(x_(n-1))))))
    d_n = grad_(y = x_(n-1)) (y + h/6*(lg(y) + 2*lg(y + h/2*lg(y)) + 2*lg(y + h/2*lg(y + h/2*lg(y))) + lg(y + h*lg(y + h/2*lg(y + h/2*lg(y))))))
    e_n = grad_(v = u_n) (2*v*(2*v)) + 2*u_n
    m_n = grad_(w = [u_n, 1]) ((w*[1; 2])*(w*[1; 2]))
    s_n = grad_(x = u_n) (x*x + grad_(y = x) (x*x*y))
    v_n = grad_(x = u_n) (grad_(y = x) (x*y*1) + x*x)
    p_n = grad_(x = u_n) x*(grad_(y = x) (3*y + 0) + grad_(z = 1) z*(3*x))
    q_n = grad_(x = (grad_(v = [u_n, 1]) (3*v[1,1] + v[1,2] + 0))[1,1]) x*(3*u_n + 1)
    j_n = grad_(y = 1) ((y*(u_n - 1) > 0) + y*y)
    o_n = grad_(x = 2) (x*x + grad_(y = x) (x*x*y) + 0) + sum_(k=1)^4 k + [1, 2, 3, 4, 5][1, 4]
    r_n = grad_(y = u_n) ((grad_(w = [y, 1]) ((w*[1; 2])*(w*[1; 2])))[1,1] + y + 2) + grad_(y = u_n) ((grad_(w = [y, 1]) ((w*[1; 2])*(w*[1; 2])))[1,1] + y + 2)
}
se = sens(u_n = n/4)
stagedse = stagedsens(u_n = n/4)

# Refused, in the interpreter's words where it refuses too and for now
# where it answers: a local by cells (a); one bound on the right of 'and'
# or 'or' and read after it, which the interpreter reads only where the
# left did not decide (b); one a sum without an upper bound carries past a
# term, its terms sharing the line's frame, to the next term, so that c is
# u + u/2 + 1/2 (c), or to after the sum, where f reads the t of the term
# the walk stopped at (f);
# one a later cell reads, each cell being its own frame (C406), so that t
# is not defined there (d); a sum's index (g) and grad's variable (h),
# which a local cannot define (C299); a local with parameters (i) or an
# index (l) (C303, C309); a local named e, which is no longer the
# built-in e grad differentiates a power of (k); and one bound in a term's
# index, which the compiler asks more than once (j, C409), where u_(n - 1)
# was read as u_(n - 2) and t was 2. A local of the index's,
# a cell's place's or a parameter's name is refused where its clause is
# defined (C405), so not here.
refused(u_n) = {
    a_n = (M[j<=2] = j*u_n) + M
    b_n = ((u_n > 1) and ((t = u_n) > 2)) + t
    c_n = (t = u_n) + sum_(k=1) (t/2^k + 0*(t = 1))
    f_n = sum_(k=1) ((t = u_n/2^k)*t) + t
    d_n[j<=2] | j == 1 = (t = 7*u_n)
    d_n[j<=2] = t
    g_n = sum_(k=1)^3 ((k = 2) + k*u_n)
    h_n = grad_(x = u_n) x*((x = 5) + x)
    i_n = (lf(y) = y + 1) + lf(u_n)
    j_n = (t = 0) + u_(n - (t = t + 1)) + t
    k_n = grad_(x = u_n) ((e = 2) + e^x)
    l_n = (s_m = m*u_n) + s_3
}

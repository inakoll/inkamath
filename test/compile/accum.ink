# A sum up to the index compiled as an accumulator (DESIGN.md): each model
# written with the paper's sum, sum_(t=a)^(n-c) f(t), beside its running
# total written by hand as a recurrence, named 'rec' and its own name. The
# step keeps the total as a sequence of its own, s0_, adding f(n - c) once a
# step, so --check writes the same report for an instance of each but for
# the name and the lines of the totals by hand, named run; cli.cmake holds
# both. The recurrences by hand are the oracle, each compiled and checked
# before the accumulator was built, and each instance's terms its twin's
# in the interpreter, which cli.cmake holds now.

# The model that asked for it: the interpreter answers m.R_3 = 0.375, and
# --compile refused the sum as one whose bounds are not constants.
reg(x_n) = {
    e_n = (x_n - 1/2)^2
    R_n = sum_(t=0)^n e_t
}
recreg(x_n) = {
    e_n = (x_n - 1/2)^2
    R_0 = e_0
    R_n = R_(n-1) + e_n
}
m = reg(x_n = n/4)
recm = recreg(x_n = n/4)

# A regret as Reddi, Kale and Kumar 2018 define it (section 2), from t = 1,
# of online gradient descent on f_t(x) = (x - z_t)^2/2 against the best
# fixed point, the mean of z, 1/3. Exact for 300 steps, its denominators
# 361 digits at the last; in double the total parts from the exact one by
# 2.2e-15 at 100 steps and 3.6e-14 at 300.
ogd(eta = 1/4, xs = 1/3, z_n) = {
    x_0 = 0
    x_(t+1) = x_t - eta*(x_t - z_t)
    f_t = (x_t - z_t)^2/2
    fs_t = (xs - z_t)^2/2
    R_T = sum_(t=1)^T (f_t - fs_t)
}
recogd(eta = 1/4, xs = 1/3, z_n) = {
    x_0 = 0
    x_(t+1) = x_t - eta*(x_t - z_t)
    f_t = (x_t - z_t)^2/2
    fs_t = (xs - z_t)^2/2
    R_0 = 0
    R_T = R_(T-1) + (f_T - fs_T)
}
ol = ogd(z_n = 1/3 + (-1)^n/2)
recol = recogd(z_n = 1/3 + (-1)^n/2)

# AdaGrad's second moment as the same paper writes it (section 2), the sum
# inside a larger expression, divided by t outside it.
ada(g_n) = {
    v_0 = 0
    v_t = (sum_(i=1)^t g_i^2)/t
}
recada(g_n) = {
    run_0 = 0
    run_t = run_(t-1) + g_t^2
    v_0 = 0
    v_t = run_t/t
}
ag = ada(g_n = 1/(n+1) - 1/3)
recag = recada(g_n = 1/(n+1) - 1/3)

# A product: a discrete survival function from its hazard, 3/(n + 3) for
# this one, empty and so 1 at 0.
surv(h_n) = {
    S_n = prod_(t=1)^n (1 - h_t)
}
recsurv(h_n) = {
    S_0 = 1
    S_n = S_(n-1)*(1 - h_n)
}
sv = surv(h_n = 1/(n+3))
recsv = recsurv(h_n = 1/(n+3))

# Matrices: a least-squares fit's normal equations accumulated, two totals,
# one of them 2x2.
gram(x_n, y_n) = {
    u_n = [x_n; 1]
    G_n = sum_(t=0)^n u_t*u_t'
    b_n = sum_(t=0)^n u_t*y_t
}
recgram(x_n, y_n) = {
    u_n = [x_n; 1]
    G_0 = u_0*u_0'
    G_n = G_(n-1) + u_n*u_n'
    b_0 = u_0*y_0
    b_n = b_(n-1) + u_n*y_n
}
ls = gram(x_n = n/10, y_n = 1/(n+1))
recls = recgram(x_n = n/10, y_n = 1/(n+1))

# Read every other step, to the step before: the total advances at every
# step, read or not, from its first term at 1.
every(x_n) = {
    y_n | floor(n/2)*2 == n = sum_(t=0)^(n-1) x_t
    y_n = -1
}
recevery(x_n) = {
    run_0 = 0
    run_n = run_(n-1) + x_(n-1)
    y_n | floor(n/2)*2 == n = run_n
    y_n = -1
}
ev = every(x_n = 1/(n+1))
recev = recevery(x_n = 1/(n+1))

# By cells: one total for each cell the sum is compiled at, the first two
# moments of x.
moments(x_n) = {
    mo_n[j<=2, k<=1] = sum_(t=0)^n x_t^j
}
recmoments(x_n) = {
    run_0[j<=2, k<=1] = x_0^j
    run_n[j<=2, k<=1] = run_(n-1)[j, 1] + x_n^j
    mo_n = run_n
}
mm = moments(x_n = 1/(n+1))
recmm = recmoments(x_n = 1/(n+1))

# From before the stream: at 0 the sum holds three terms, two of them the
# history the model gives its input, unrolled as a sum with constant bounds.
early(x_n) = {
    x_n | n < 0 = 1/2
    y_n = sum_(t=-2)^n x_t
}
recearly(x_n) = {
    x_n | n < 0 = 1/2
    run_0 = x_(-2) + x_(-1) + x_0
    run_n = run_(n-1) + x_n
    y_n = run_n
}
ey = early(x_n = 1/(n+1))
recey = recearly(x_n = 1/(n+1))

# In an instance another model reads: the total is the instance's, a mean
# square.
inner(x_n) = {
    S_n = sum_(t=0)^n x_t
}
outer(u_n) = {
    k = inner(x_n = u_n^2)
    y_n = k.S_n/(n + 1)
}
recinner(x_n) = {
    S_0 = x_0
    S_n = S_(n-1) + x_n
}
recouter(u_n) = {
    k = recinner(x_n = u_n^2)
    y_n = k.S_n/(n + 1)
}
ms = outer(u_n = 1/(n+1))
recms = recouter(u_n = 1/(n+1))

# Refused. Adam's second moment as the same paper writes it (section 2):
# its terms read t, so no total kept from step to step is the sum at t.
adam(b2 = 999/1000, g_n) = {
    v_t = (1 - b2)*sum_(i=1)^t b2^(t-i)*g_i^2
}

# So too where they read it as an inner sum's last index: each outer term
# is the inner total at n, (sum_(t=0)^n x_t)^2 in all.
square(x_n) = {
    y_n = sum_(t=0)^n sum_(s=0)^n x_s*x_t
}

# A running maximum and its index, for now.
peak(x_n) = {
    M_n = max_(t=0)^n x_t
}
peakat(x_n) = {
    A_n = argmax_(t=0)^n x_t
}

# Last indexes other than the index less a constant, and a first one that
# is not a constant: a window is written from a constant, sum_(k=0)^2
# x_(n-k), as fir.ink's is.
ahead(x_n) = {
    y_n = sum_(t=0)^(n+1) x_t
}
twice(x_n) = {
    y_n = sum_(t=0)^(2*n) x_t
}
nested(x_n) = {
    y_n = sum_(t=0)^n sum_(s=0)^t x_s
}
window(x_n) = {
    y_n = sum_(t=n-2)^n x_t
}

# A first term read before the stream, refused as any such read is.
late(x_n) = {
    R_n = sum_(t=0)^n x_(t-1)
}

# Under grad and at a slower rate, for now.
slope(x_n) = {
    g_n = grad_(w = 1) sum_(t=0)^n w*x_t
}
slow(x_n) = {
    y_m = x_(2*m) + sum_(t=0)^m x_(2*t)
}

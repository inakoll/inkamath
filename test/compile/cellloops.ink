# Definitions by cells as loops (DESIGN.md, next in line): a definition by
# cells of more than 1000 cells, and a sum or a product whose terms, with
# those of the sums written in its body, number more than 1000, compile to
# a loop nest whose body is compiled once, its places bound to the loop's
# indices. Every number below is worked out apart from the interpreter and
# the compiler, with Python's fractions, doubles and floats rounded as C's,
# and none is recorded.

# The tank, trimmed from 64x64 to 32x32 cells, so that each definition is
# still past 1000: inkamath-cfd-tutorial benchmarks/wave2d_64x64.ink, the
# owner's, its models as written there and below at full size. step reads
# its neighbours through the periodic wraps lt and rt; misfit sums its
# 1024 squares by rows, two sums, one in the other's body; flatmisfit
# sums them in one, its row and column floor((k - 1)/32) + 1 and
# mod(k - 1, 32) + 1. 'inkamath --compile cellloops.ink forward32 -o
# forward32.h' writes the step's cells as
#
#     double t#_[32][32];
#     for (int i1_ = 0; i1_ < 32; ++i1_)
#         for (int j1_ = 0; j1_ < 32; ++j1_) t#_[i1_][j1_] = 2.0 * m_->u[0][i1_][j1_] + ...
#
# its neighbours read through two tables, lt's [31, 0, 1, ..., 30] and rt's
# [1, 2, ..., 31, 0], each read at i1_ and at j1_, and its default c set by
# one loop in init: under 8,000 bytes, where written out it is 250,819.
lt(i) = mod(i - 2, 32) + 1
rt(i) = mod(i, 32) + 1
step32(u, p, c)[i<=32, j<=32] = 2*u[i, j] - p[i, j] + c[i, j]^2*(u[lt(i), j] + u[rt(i), j] + u[i, lt(j)] + u[i, rt(j)] - 4*u[i, j])
misfit32(v, y) = sum_(i=1)^32 sum_(j=1)^32 (v[i, j] - y[i, j])^2/2
flatmisfit32(v, y) = sum_(k=1)^1024 (v[floor((k - 1)/32) + 1, mod(k - 1, 32) + 1] - y[floor((k - 1)/32) + 1, mod(k - 1, 32) + 1])^2/2
speed32[i<=32, j<=32] = 1/4
zero32[i<=32, j<=32] = 0
forward32(c = speed32, u_n[i<=32, j<=32], p_n[i<=32, j<=32]) = {
    field_n = step32(u_n, p_n, c)
}
nested32(c = speed32, y = zero32, u_n[i<=32, j<=32], p_n[i<=32, j<=32]) = {
    objective_n = misfit32(step32(u_n, p_n, c), y)
}
flattened32(c = speed32, y = zero32, u_n[i<=32, j<=32], p_n[i<=32, j<=32]) = {
    objective_n = flatmisfit32(step32(u_n, p_n, c), y)
}

# 'exact' and 'held': at U, whose cells are eighths, every double of the
# step is exact; exact's objective is 217 at every n, and held's
# field_n[1, 1] is 5/8, 11/16, 13/16 and 7/8 at n = 0 to 3, field_n[32, 32]
# -3/4, 5/16, 7/16 and 1/2. Checked for 6 steps, as are the others:
#
#     exact: 6 steps from 0, against exact values
#     exact.objective: within 0
#
# and held's field alike.
# 'wave', 'seen' and 'flat' at V, sevenths in doubles, where the order of
# each addition shows: summed by rows, two sums one in the other, the
# objective at n = 0 is 241.35873724489798, and in one sum 241.35873724489957,
# 1.6e-12 apart, as the interpreter's are; the step's are the
# interpreter's, the same operations in the same order, each sum from the
# first term:
#
#     wave: 6 steps from 0, against exact values until 0 and inexact ones from there
#     wave.field: within 0
#
# and seen's and flat's alike, their objectives within 0. In float, Python's
# floats in the step's order are 1.7, 2.2 and 96 units of a float from the
# interpreter's doubles at most, wave's field, seen's and flat's objective,
# over these 6 steps and 2 more: one sum of 1024 floats drifts where 32 sums
# of 32 do not.
U(t)[i<=32, j<=32] = mod(i + 3*j + t, 8)/8
V(t)[i<=32, j<=32] = ~(mod(i*j + t, 7)/7)
exact = nested32(u_n = U(n), p_n = U(n - 1))
held = forward32(u_n = U(n), p_n = U(n - 1))
wave = forward32(u_n = V(n), p_n = V(n - 1))
seen = nested32(u_n = V(n), p_n = V(n - 1))
flat = flattened32(u_n = V(n), p_n = V(n - 1))

# 'rod' and 'rods': a model's own sequence by cells, the clamped stencil of
# a rod with reflecting ends, max(i - 1, 1) and min(i + 1, 1024) read
# through two tables, [0, 0, 1, ..., 1022] and [1, 2, ..., 1023, 1023];
# and s, a sum of 1024 terms over the looped term. Its sum telescopes, so
# that at W, quarters, s_n is 384 at every n, the sum of y_n, and x_n[1] is
# 5/16, 9/16 and 9/16 at n = 0 to 2, x_n[1024] 3/16, 3/16 and 7/16. At Z,
# ninths in doubles:
#
#     rods: 6 steps from 0, against exact values until 0 and inexact ones from there
#     rods.x: within 0
#     rods.s: within 0
W(t)[i<=1024] = mod(i + t, 4)/4
Z(t)[i<=1024] = ~(mod(i*(t + 2), 9)/9)
clamped(y_n[i<=1024]) = {
    x_n[i<=1024] = y_n[i] + (y_n[max(i - 1, 1)] - 2*y_n[i] + y_n[min(i + 1, 1024)])/4
    s_n = sum_(i=1)^1024 x_n[i]
}
rod = clamped(y_n = W(n))
rods = clamped(y_n = Z(n))

# 'ends' and 'endsx': past the threshold, checked. t, of 1001 terms,
# loops, its index through the table [0, 1, ..., 999, 0]; and w, 7 sums of
# 143, 1001 terms in all, loops as a nest, the inner sum, reading i, a loop
# in the outer's body. At E, quarters, t_n is 1501/4, 751/2 and 1503/4 at
# n = 0 to 2, and w_n 1512, 1505 and 1498. At Ez, ninths in doubles:
#
#     endsx: 6 steps from 0, against exact values until 0 and inexact ones from there
#     endsx.t: within 0
#     endsx.w: within 0
E(t)[i<=1000] = mod(i + t, 4)/4
Ez(t)[i<=1000] = ~(mod(i*(t + 2), 9)/9)
edge(y_n[i<=1000]) = {
    t_n = sum_(k=1)^1001 y_n[mod(k - 1, 1000) + 1]
    w_n = sum_(i=1)^7 sum_(j=1)^143 i*y_n[j]
}
ends = edge(y_n = E(n))
endsx = edge(y_n = Ez(n))

# 'edgeout': at the threshold and the places of two indices, compiled
# once and held by its lines alone, its values by the interpreter's at
# 'outs'. x, of 1000 cells, and s, a sum of 1000 terms, are written out as
# today. v, 2 sums of 501, 1002 terms, loops, its place i + j - 1 affine in
# both, written k1_ + k2_; u, 11 sums of 91, 1001 terms, its place
# (i - 1)*90 + j written 90 * k1_ + k2_; and z, 2 sums of 501 again,
# would loop, but its place mod(i*j, 1000) + 1 is not affine in i and j,
# so it gives way and is written out as today. At E, s_n is 750, v_n
# 1503/4, 1505/4 and 1503/4 at n = 0 to 2, u_n 377, 1497/4 and 753/2,
# and z_n 1755/4, 1253/4 and 1751/4.
edgeout(y_n[i<=1000]) = {
    x_n[i<=1000] = y_n[max(i - 1, 1)] + y_n[min(i + 1, 1000)]
    s_n = sum_(i=1)^1000 x_n[i]
    v_n = sum_(i=1)^2 sum_(j=1)^501 y_n[i + j - 1]
    u_n = sum_(i=1)^11 sum_(j=1)^91 y_n[(i - 1)*90 + j]
    z_n = sum_(i=1)^2 sum_(j=1)^501 y_n[mod(i*j, 1000) + 1]
}
outs = edgeout(y_n = E(n))

# 'gate' and 'gatex': a guard reading what moves, the same chain at every
# cell, compiled once in the loop's body, as each cell writes it out,
#
#     for (int i1_ = 0; i1_ < 1024; ++i1_) t#_[i1_][0] = m_->y[0][i1_][0] > 0.5 ? m_->y[0][i1_][0] : 0.0;
#
# and d's placed subtrahend i - 1 is added negated, as written out:
#
#     for (int i1_ = 0; i1_ < 1024; ++i1_) t#_[i1_][0] = m_->y[0][i1_][0] + (double)(-i1_);
#
# so that at i = 1 the cell is y + 0.0, today's 'm_->y[0][0][0] + 0.0',
# and -0.0 in y gives +0.0, where 'y - 0.0' would keep -0.0. At W, q_n is
# 192 at every n, d_0[1] is 1/4 and d_0[1024] -1023; at Z, within 0.
relu(z)[i] | z[i] > 1/2 = z[i]
relu(z)[i] = 0
act(y_n[i<=1024]) = {
    r_n = relu(y_n)
    q_n = sum_(i=1)^1024 r_n[i]
    d_n[i<=1024] = y_n[i] - (i - 1)
}
gate = act(y_n = W(n))
gatex = act(y_n = Z(n))

# Written out as today: 'guarded', whose guard reads its place, i == 1, so
# that its chain is not the same at every cell, and 'pull', whose sq of
# 1024 cells is under grad, which stays unrolled until the sweep runs
# through loops. Neither header holds a loop's index; pull's d_n is 3/4,
# ws's cell 999, at every n.
guarded(y_n[i<=1024]) = {
    g_n[i<=1024] | i == 1 = 0
    g_n[i<=1024] = y_n[i - 1]
}
ws[i<=1024] = mod(i, 4)/4
sq(a, w)[i<=1024] = a*w[i]
pull(x_n) = {
    d_n = grad_(a = x_n) sq(a, ws)[999]
}
pulled = pull(x_n = ~(n/7))

# The tank at full size, the owner's models as written there. forward,
# nested and flattened are each checked for 4 steps at V64, within 0, as
# above, the objective at n = 0 976.5610650510209 summed by rows and
# 976.5610650510386 in one sum; their headers are under 8,000, 10,000 and 45,000 bytes, where
# forward was 1,006,007 and nested 2,106,456, and flattened was refused, "a
# sum or a product of more than 1000 terms". sensitivity stays unrolled,
# its step as today's, its defaults set by loops in init.
left(i) = mod(i-2,64)+1
right(i) = mod(i,64)+1
step(u,p,c)[i<=64,j<=64] = 2*u[i,j]-p[i,j]+c[i,j]^2*(u[left(i),j]+u[right(i),j]+u[i,left(j)]+u[i,right(j)]-4*u[i,j])
misfit(v,y) = sum_(i=1)^64 sum_(j=1)^64 (v[i,j]-y[i,j])^2/2
flatmisfit(v,y) = sum_(k=1)^4096 (v[floor((k-1)/64)+1,mod(k-1,64)+1]-y[floor((k-1)/64)+1,mod(k-1,64)+1])^2/2
speed[i<=64,j<=64] = 1/4
zero[i<=64,j<=64] = 0
forward(c=speed,u_n[i<=64,j<=64],p_n[i<=64,j<=64]) = {
    field_n = step(u_n,p_n,c)
}
nested(c=speed,y=zero,u_n[i<=64,j<=64],p_n[i<=64,j<=64]) = {
    objective_n = misfit(step(u_n,p_n,c),y)
}
sensitivity(c=speed,y=zero,u_n[i<=64,j<=64],p_n[i<=64,j<=64]) = {
    gradient_n = grad_(v=c) misfit(step(u_n,p_n,v),y)
}
flattened(c=speed,y=zero,u_n[i<=64,j<=64],p_n[i<=64,j<=64]) = {
    objective_n = flatmisfit(step(u_n,p_n,c),y)
}
V64(t)[i<=64, j<=64] = ~(mod(i*j + t, 7)/7)
tank = forward(u_n = V64(n), p_n = V64(n - 1))
tanked = nested(u_n = V64(n), p_n = V64(n - 1))
tankflat = flattened(u_n = V64(n), p_n = V64(n - 1))

# What stays refused past 1000 terms, each named by 'inkamath --compile' of
# a file of
#
#     a_n = grad_(t = x_n) sum_(k=1)^1001 t/k
#     b_n = sum_(i=1)^1001 sum_(j=1)^2 y_n[mod(i*j, 1600) + 1]
#     c_n = sum_(k=1)^1001 x_n^k
#     d_n = sum_(k=1)^1001 max(x_n, k)
#     e_n = sum_(k=1)^1001 y_n[k + 600]
#     f_n = sum_(i=1)^1001 sum_(j=1)^i x_n/j
#     g_n = max_(k=1)^1001 k*x_n
#     h_n = sum_(k=1)^1001 lim r(x_n/k)
#     m_n = sum_(k=1)^1001 x_n*[1; 2]/k
#     r(x)_0 = x
#     r(x)_k = r(x)_(k-1)/2 + 1
#
# with y_n of 1600 cells:
#
#     cannot compile a: a sum or a product of more than 1000 terms under grad, for now
#     cannot compile b: a sum or a product of more than 1000 terms whose term reads y_n at a place that is no sum of whole multiples of i and j and a constant, for now
#     cannot compile c: a sum or a product of more than 1000 terms whose term reads k in an exponent, for now
#     cannot compile d: a sum or a product of more than 1000 terms whose term reads k in a call of max, for now
#     cannot compile e: row 1601 is outside a 1600x1 matrix
#     cannot compile f: a sum or a product of more than 1000 terms whose inner sum's bounds read i, for now
#     cannot compile g: the maximum has more than 1000 terms
#     cannot compile h: a sum or a product of more than 1000 terms whose term reads a limit, for now
#     cannot compile m: a sum or a product of more than 1000 terms whose terms are 2x1 matrices, for now

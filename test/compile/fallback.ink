# A clause for a limit that does not converge, compiled (DESIGN.md): a guard
# reading a limit whose walk runs out does not hold, in the step as in the
# interpreter, and the clause after it is taken; where the walk is refused,
# or no clause comes after, the step is NaN as today. fallback_test.c holds
# each model to values computed apart from the interpreter, in Python's
# fractions and by README section 4's rule.
#
# Where a limit runs out the clause after is taken, which is safe only where
# the guarded clause is the risky one. --check lists, after the per-term
# lines, every guard the step compiles that reads a limit, and the clause
# after it, so that a reviewer can audit the direction (DESIGN.md, next in
# line; specified, not yet built). rootless, coupled, braked and buffered
# give theirs; every other report below then ends with its own, as
# test/CMakeLists.txt gives them.

# A small MPC with a fallback clause: the plan minimises 2(x + v)^2 + v^2/2
# over |v| <= 1 by projected gradient with a step of 1/2, too long for the
# curvature 5. It settles where the bound holds it, at -1 from 5/2 and 3/2,
# and from 1/2 on cycles: the plant moves by -x/2 instead, so x_n is
# 2^-(n-1) and u_n -2^-n from n = 2, and solved_n is 1 at 0 and 1, 0 after.
# Every term is a double exactly, and every walk that settles stops on a
# step of 0, so a disturbed run moves none. Checked, it reports
#
#     planned: 100 steps from 0, against exact values until 0 and inexact ones from there
#     planned.<s>: within 0
#
# for x, u and solved, the step taking the interpreter's clause at each.
mpc(x0 = 5/2) = {
    clip(v) | v > 1 = 1
    clip(v) | v < -1 = -1
    clip(v) = v
    qp(s)_0 = 0
    qp(s)_k = clip(qp(s)_(k-1) - (5*qp(s)_(k-1) + 4*s)/2)
    u_n | abs(x_n + lim qp(x_n)) <= abs(x_n) = lim qp(x_n)
    u_n = -x_n/2
    solved_n | abs(x_n + lim qp(x_n)) <= abs(x_n) = 1
    solved_n = 0
    x_0 = x0
    x_n = x_(n-1) + u_(n-1)
}
planned = mpc()

# Newton's method within a step, its fallback the term before: from 0, on
# x^3 - 2x + c, it cycles 0, 1, 0, ... at c = 2, and at c = 1 converges at
# its sixth term, within 7e-24 of (sqrt(5) - 1)/2 (mpmath), whose double is
# 0.6180339887498949. So x_n is 1/2 at 0 and 1, and the root from 2 on.
# Checked, it reports
#
#     held: 100 steps from 0, against exact values until 1 and inexact ones from there
#     held.x: within <e>; the interpreter's terms about <e> from the exact ones
newton(c_n) = {
    G(x, c) = x^3 - 2*x + c
    dG(x) = 3*x^2 - 2
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - G(nv(c)_(k-1), c)/dG(nv(c)_(k-1))
    x_0 = 1/2
    x_n | abs(G(lim nv(c_n), c_n)) < 1/10^6 = lim nv(c_n)
    x_n = x_(n-1)
}
held = newton(c_n = 1 + mod(n, 2))

# A walk the interpreter refuses is not one that ran out: Newton's root
# from an exact 0 divides by 0, and in doubles its terms are inf and then
# NaN. The step is NaN, not the clause after.
rooted(c_n) = {
    hs(c)_0 = 0
    hs(c)_k = hs(c)_(k-1) - (hs(c)_(k-1)^2 - c)/(2*hs(c)_(k-1))
    y_n | lim hs(c_n) > 0 = lim hs(c_n)
    y_n = 0
}
# The list is the code's, not the run's: no walk runs out here, and the
# guard is listed. Checked:
#
#     rootless: 100 steps from 0, against exact values
#     rootless.y: no value compared, the step NaN where the interpreter gives none, as at 0: division by zero
#     rootless.y: where lim hs runs out, 'y_n | lim hs(c_n) > 0 = lim hs(c_n)' falls to 'y_n = 0'
rootless = rooted(c_n = 2)

# A limit in a sum's terms within a guard: a sum's bounds are constants, so
# it is written out, and each term's limit is read as the guard's own. From
# 0, on x^3 - 2x + jc, Newton converges for j = 1 and 2 at c = 1/2, to
# 0.2586520225 and 0.6180339887 (mpmath), and at c = 1 cycles for j = 2:
# y_n is 1 at even n, 0 at odd.
paired(c_n) = {
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - (nv(c)_(k-1)^3 - 2*nv(c)_(k-1) + c)/(3*nv(c)_(k-1)^2 - 2)
    y_n | sum_(j=1)^2 lim nv(j*c_n) < 2 = 1
    y_n = 0
}
# Checked:
#
#     coupled: 100 steps from 0, against exact values until 0 and inexact ones from there
#     coupled.y: within 0
#     coupled.y: where lim nv runs out, 'y_n | sum_(j=1)^2 lim nv(j*c_n) < 2 = 1' falls to 'y_n = 0'
coupled = paired(c_n = (1 + mod(n, 2))/2)

# A cell whose guard reads a limit that runs out, with no clause after for
# it, refuses the matrix in the limit's words, though a cell no clause
# gives is otherwise 0, in a sequence's cells as in a function's: at odd n,
# Newton from 0 on x^3 - 2x + 2 cycles, and the step is NaN where the
# interpreter refuses. At even n it settles within 7e-24 of (sqrt(5) - 1)/2
# (mpmath), and y_n and z_n are [5; 2]. Checked:
#
#     ranout: 100 steps from 0, against exact values until 0 and inexact ones from there
#     ranout.y: within 0
#     ranout.z: within 0
cellwise(c_n) = {
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - (nv(c)_(k-1)^3 - 2*nv(c)_(k-1) + c)/(3*nv(c)_(k-1)^2 - 2)
    y_n[j<=2] | j == 1 = 5
    y_n[j<=2] | lim nv(j*c_n/2) > 0 = j
    f(v)[j] | j == 1 = 5
    f(v)[j] | lim nv(v[j]) > 0 = j
    z_n = f([c_n/2; c_n])
}
ranout = cellwise(c_n = 1 + mod(n, 2))

# grad of a definition whose guard reads a limit that runs out is refused,
# as interpreted: no clause is chosen there that grad can follow. At even n
# r's guard holds and y_n is the slope of x^2 at 1, 2; at odd n, where
# Newton cycles, the step is NaN. Checked:
#
#     tipped: 100 steps from 0, against exact values until 0 and inexact ones from there
#     tipped.y: within 0
tangent(c_n) = {
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - (nv(c)_(k-1)^3 - 2*nv(c)_(k-1) + c)/(3*nv(c)_(k-1)^2 - 2)
    r(x, c) | lim nv(c) > 0 = x^2
    r(x, c) = 3*x
    y_n = grad_(x = c_n/c_n) r(x, c_n)
}
tipped = tangent(c_n = 1 + mod(n, 2))

# A matrix limit read by a guard, its status asked as its return: Newton's
# iterate beside a cell that settles at 2, the matrix running out where
# Newton cycles, at odd n. y_n is 1 at even n and 0 at odd. Checked:
#
#     columned: 100 steps from 0, against exact values until 0 and inexact ones from there
#     columned.y: within 0
columns(c_n) = {
    N(x, c) = x - (x^3 - 2*x + c)/(3*x^2 - 2)
    M(c)_0 = [0; 0]
    M(c)_k = [N(M(c)_(k-1)[1,1], c); M(c)_(k-1)[2,1]/2 + 1]
    y_n | [1, 1]*lim M(c_n) > 0 = 1
    y_n = 0
}
columned = columns(c_n = 1 + mod(n, 2))

# A function whose own guard reads a limit that runs out, and whose clause
# after then answers, has answered: its caller's guard reads that answer,
# and where the guard is NaN because a term it reads is refused, the step
# is NaN, as the interpreter refuses. At even n Newton settles, inner is 1
# and y_n 0; at odd n inner is 5, r_n is refused, and so is y_n. Checked:
#
#     answered: 100 steps from 0, against exact values until 0 and inexact ones from there
#     answered.r: within 0
#     answered.y: within 0
inward(c_n) = {
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - (nv(c)_(k-1)^3 - 2*nv(c)_(k-1) + c)/(3*nv(c)_(k-1)^2 - 2)
    inner(c) | lim nv(c) > 0 = 1
    inner(c) = 5
    r_n | c_n < 2 = 1
    y_n | inner(c_n) == 5 and r_n == 1 = 1
    y_n = 0
}
answered = inward(c_n = 1 + mod(n, 2))

# A brake whose guard reads a solver's limit, the risky clause first: where
# Newton runs out, 'brake' falls to 0 and does not brake. 'careful' asks the
# opposite question, and falls to braking. Newton from 0 on x^3 - 2x + c
# settles at c = 1/2 and 1, near 0.2586520225 and 0.6180339887, below 1,
# and cycles at 2. So brake_n is 1 at even n and 0 at odd, careful_n 1
# throughout, and wheels_n, a clause per cell at c_n/2 and c_n, [1; 1] at
# even n and [1; 0] at odd. One line a clause, not a cell. Checked:
#
#     braked: 100 steps from 0, against exact values until 0 and inexact ones from there
#     braked.brake: within 0
#     braked.careful: within 0
#     braked.wheels: within 0
#     braked.brake: where lim nv runs out, 'brake_n | lim nv(c_n) < 1 = 1' falls to 'brake_n = 0'
#     braked.careful: where lim nv runs out, 'careful_n | lim nv(c_n) >= 1 = 0' falls to 'careful_n = 1'
#     braked.wheels: where lim nv runs out, 'wheels_n[j<=2] | lim nv(j*c_n/2) < 1 = 1' falls to 'wheels_n[j<=2] = 0'
braking(c_n) = {
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - (nv(c)_(k-1)^3 - 2*nv(c)_(k-1) + c)/(3*nv(c)_(k-1)^2 - 2)
    brake_n | lim nv(c_n) < 1 = 1
    brake_n = 0
    careful_n | lim nv(c_n) >= 1 = 0
    careful_n = 1
    wheels_n[j<=2] | lim nv(j*c_n/2) < 1 = 1
    wheels_n[j<=2] = 0
}
braked = braking(c_n = 1 + mod(n, 2))

# Through a function: 'gap' has a clause after its guard, so it answers
# where Newton runs out, 0, and the guard of 'brake' that reads it is not
# listed; 'near' has none, so the guard of 'alarm' falls too. brake_n
# is 1 throughout, alarm_n 1 at even n and 0 at odd. Checked:
#
#     buffered: 100 steps from 0, against exact values until 0 and inexact ones from there
#     buffered.alarm: within 0
#     buffered.brake: within 0
#     buffered.alarm: where lim nv runs out, 'alarm_n | near(c_n) == 1 = 1' falls to 'alarm_n = 0'
#     buffered.gap: where lim nv runs out, 'gap(c) | lim nv(c) > 0 = lim nv(c)' falls to 'gap(c) = 0'
#     buffered.near: where lim nv runs out, 'near(c) | lim nv(c) < 1 = 1' falls to NaN
relaying(c_n) = {
    nv(c)_0 = 0
    nv(c)_k = nv(c)_(k-1) - (nv(c)_(k-1)^3 - 2*nv(c)_(k-1) + c)/(3*nv(c)_(k-1)^2 - 2)
    gap(c) | lim nv(c) > 0 = lim nv(c)
    gap(c) = 0
    brake_n | gap(c_n) < 1 = 1
    brake_n = 0
    near(c) | lim nv(c) < 1 = 1
    alarm_n | near(c_n) == 1 = 1
    alarm_n = 0
}
buffered = relaying(c_n = 1 + mod(n, 2))

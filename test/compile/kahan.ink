# W. Kahan, "How Futile are Mindless Assessments of Roundoff in
# Floating-Point Computation?" (2006), held to the step: the digits each
# report shows are the paper's, and test/data/kahan.ink and muller.ink
# hold the exact values.

# §3: a series summed with compensation, and as an optimiser that finds
# comp_k algebraically 0 would leave it. In float the compensated total
# stays within a unit of the exact one, the plain sum near three:
#
#     sums.plain: within 0.0028, 2.8 units of a float
#     sums.total: within 0.00051, 0.52 units of a float
Term(k) = 3465/(k^2 - 1/16) + 3465/((k + 1/2)^2 - 1/16)
Tail(k) = 3465/(k + 1/2) + 3465/(k + 1)
summed() = {
    s_0 = 0
    comp_0 = 0
    c_k = comp_(k-1) + Term(k)
    s_k = c_k + s_(k-1)
    comp_k = (s_(k-1) - s_k) + c_k
    total_k = s_k + (Tail(k) + comp_k)
    o_0 = 0
    o_k = Term(k) + o_(k-1)
    plain_k = o_k + Tail(k)
}
sums = summed()

# §5, J.-M. Muller's recurrence, whose exact terms tend to 5 and rounded
# ones to 100. The table's 53-bit column is IEEE double, x_7 4.9455373955305
# where the exact one is 4.9455374041239; x_6 is within the billionth:
#
#     muller.x: 4.9455373955305078 at 7, where the interpreter gives 4.9455374041239164
#
# Started inexact, the interpreter's terms are the step's, and its estimate
# says they leave the exact ones from 7 on.
recurrence(x0 = 4, x1 = 17/4) = {
    E(y, z) = 108 - (815 - 1500/z)/y
    x_0 = x0
    x_1 = x1
    x_n = E(x_(n-1), x_(n-2))
}
muller = recurrence()
rounded = recurrence(x0 = ~4, x1 = ~(17/4))

# §6, a smooth surprise: G(x) = T(Q(x)^2) is 1, Q(x) being 0, yet 0 at
# every n to 9999 in double, where float gives 1 at n = 1. The estimate
# exposes T, and Kahan's Th, through log, holds. A disturbed run that
# rounds t off 1 takes Th's other clause, which meets the first there:
#
#     surprise.k: at 0 a disturbed run takes 'Th(t, z) = (t - 1)/log(t)' and the interpreter 'Th(t, z) | t == 1 = t'; the guard of the second is exactly on its threshold
#     surprise.g: within 0; the interpreter's terms about 5.4e+16 from the exact ones, past the tolerance from 0
#     surprise.k: within 0; the interpreter's terms about 8.9e-16 from the exact ones
#   in float:
#     surprise.g: 1 at 0, where the interpreter gives 0; ...
smooth(x_n) = {
    T(z) = (exp(z) - 1)/z
    T(z) | z == 0 = 1
    Th(t, z) = (t - 1)/log(t)
    Th(t, z) | t == 1 = t
    Th(t, z) | t == 0 = -1/z
    Q(y) = abs(y - (y^2 + 1)^(1/2)) - 1/(y + (y^2 + 1)^(1/2))
    g_n = T(Q(x_n)^2)
    k_n = Th(exp(Q(x_n)^2), Q(x_n)^2)
}
surprise = smooth(x_n = n + 1)

# §9, ProSolveur's system: in double nothing rounds, B/A and B*(B/A)
# being exact in 53 bits; in float the matrix is singular:
#
#     solveur.d: 0 at 0, where the interpreter gives -1
#     solveur.z[1,1]: nan at 0, where the interpreter gives 12582909
system(A = 4194304, B = 4194303, C = 4194302, p_n) = {
    z_n = [A B; B C]^-1*[0; p_n]
    d_n = A*C - B*B
}
solveur = system(p_n = 3)

# A comparison of matrices compiled (DESIGN.md): two matrices of one shape
# equal where every cell is. 'rational': spin.ink's rotation, of the unit
# quaternion (1, 2, 2, 4)/5, composed with itself, so R_n is exactly
# orthogonal and the interpreter's R_n'*R_n == I3 is 1 at every step. The
# step computes Rq(q0) from the doubles of q0, and its R_1'*R_1 is 1 +
# 4.4e-16 on the diagonal: it leaves SO(3) at its first step, where the
# interpreter's guard is exactly on its threshold. 'inexact': the same
# from ~q0, the step's doubles the interpreter's own, whose R_n'*R_n is I3
# at no step from 1, so k_n = n and o_n = 0. Each is to report:
#
#     rational: 100 steps from 0, against exact values
#     rational.k: at 1 the compiled step takes 'k_n | R_n'*R_n <> I3 = k_(n-1) + 1' and the interpreter 'k_n = k_(n-1)'; the guard of the first is exactly on its threshold
#     rational.R: within 1.9e-14
#     rational.k: 1 at 1, where the interpreter gives 0
#     rational.o: 0 at 1, where the interpreter gives 1
#
#     inexact: 100 steps from 0, against exact values until 1 and inexact ones from there
#     inexact.R: within 0; the interpreter's terms about <e> from the exact ones
#     inexact.k: within 0
#     inexact.o: within 0
#
# and 'rational' with --float, whose R_1'*R_1 is 1 + 2^-23 at [2,2] and [3,3]:
#
#     rational: 100 steps from 0 in float, against exact values
#     rational.k: at 1 the compiled step takes 'k_n | R_n'*R_n <> I3 = k_(n-1) + 1' and the interpreter 'k_n = k_(n-1)'; the guard of the first is exactly on its threshold
#     rational.R: within 6.5e-06, 1e+04 units of a float
#     rational.k: 1 at 1, where the interpreter gives 0
#     rational.o: 0 at 1, where the interpreter gives 1
Rq(q) = [q[1]^2+q[2]^2-q[3]^2-q[4]^2, 2*(q[2]*q[3]-q[1]*q[4]), 2*(q[2]*q[4]+q[1]*q[3]); 2*(q[2]*q[3]+q[1]*q[4]), q[1]^2-q[2]^2+q[3]^2-q[4]^2, 2*(q[3]*q[4]-q[1]*q[2]); 2*(q[2]*q[4]-q[1]*q[3]), 2*(q[3]*q[4]+q[1]*q[2]), q[1]^2-q[2]^2-q[3]^2+q[4]^2]
I3[j<=3, k<=3] = j == k
kept(q0) = {
    R_0 = I3
    R_n = R_(n-1)*Rq(q0)
    k_0 = 0
    k_n | R_n'*R_n <> I3 = k_(n-1) + 1
    k_n = k_(n-1)
    o_n = R_n'*R_n == I3
}
rational = kept(q0 = [1; 2; 2; 4]/5)
inexact = kept(q0 = ~[1; 2; 2; 4]/5)

# 'probe', at x_n = n/2 - 1: e NaN where r refuses, to 2, as a comparison
# reading a NaN cell is; g NaN at 4, t = 1, where the sides meet and one moves
# and the interpreter says "a comparison jumps at t = 1"; h NaN there too,
# "f takes a clause at t = 1 that holds only there". Elsewhere e is 1 but
# 0 at 4, g and h 1. It is to report:
#
#     probe: 100 steps from 0, against exact values
#     probe.e: within 0
#     probe.g: within 0
#     probe.h: within 0
r(v) | v > 0 = v
f(t) | [t, 1] == [1, 1] = 0
f(t) = t
probed(x_n) = {
    e_n = [r(x_n), 1] <> [1, 1]
    g_n = grad_(t = x_n) (([t, 2*t] == [1, 2]) + t)
    h_n = grad_(t = x_n) f(t)
}
probe = probed(x_n = n/2 - 1)

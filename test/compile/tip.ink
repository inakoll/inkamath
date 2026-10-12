# A planar arm of three uniform rods, m = l = 1, its inverse dynamics by the
# RNEA (Featherstone 2008, sec. 5.3 and table 5.1) along a trajectory whose
# joints are given by t = tan(q/2), so each cosine and sine is rational. The
# backward pass is written as the book writes a chain's: the force on link i
# read ahead from link i+1's, ended by a guard at the tip. Its compiled step
# holds y to the interpreter's exact one, and d holds it to the same pass
# written as a function, which compiles to the same doubles. Its values at 1
# and 5 are Euler-Lagrange's, by sympy (cli.cmake's compile_tip_values).
N = 3
l = [1; 1; 1]
m = [1; 1; 1]
g = 981/100
crm(v) = [0, 0, 0; v[3], 0, -v[1]; -v[2], v[1], 0]
crf(v) = -crm(v)'
plnr(c, s, r) = [1, 0, 0; s*r[1] - c*r[2], c, s; c*r[1] + s*r[2], -s, c]
mcI(mass, cx, cy, Ic) = [Ic + mass*(cx^2 + cy^2), -mass*cy, mass*cx; -mass*cy, mass, 0; mass*cx, 0, mass]
Sj = [1; 0; 0]
In(i) = mcI(m[i], l[i]/2, 0, m[i]*l[i]^2/12)
lp(i) | i == 1 = 0
lp(i) = l[i-1]
X(c, s, i) = plnr(c[i], s[i], [lp(i); 0])
v(c, s, qd)_0 = [0; 0; 0]
v(c, s, qd)_i = X(c, s, i)*v(c, s, qd)_(i-1) + Sj*qd[i]
a(c, s, qd, qdd)_0 = [0; 0; g]
a(c, s, qd, qdd)_i = X(c, s, i)*a(c, s, qd, qdd)_(i-1) + Sj*qdd[i] + crm(v(c, s, qd)_i)*Sj*qd[i]
fb(c, s, qd, qdd)_i = In(i)*a(c, s, qd, qdd)_i + crf(v(c, s, qd)_i)*In(i)*v(c, s, qd)_i
f(c, s, qd, qdd)_i | i == N = fb(c, s, qd, qdd)_i
f(c, s, qd, qdd)_i = fb(c, s, qd, qdd)_i + X(c, s, i+1)'*f(c, s, qd, qdd)_(i+1)
tau(c, s, qd, qdd)[j<=N] = (Sj'*f(c, s, qd, qdd)_j)[1,1]
ff(c, s, qd, qdd, i) | i == N = fb(c, s, qd, qdd)_i
ff(c, s, qd, qdd, i) = fb(c, s, qd, qdd)_i + X(c, s, i+1)'*ff(c, s, qd, qdd, i+1)
tauf(c, s, qd, qdd)[j<=N] = (Sj'*ff(c, s, qd, qdd, j))[1,1]
cq(t)[j<=N] = (1 - t[j]^2)/(1 + t[j]^2)
sq(t)[j<=N] = 2*t[j]/(1 + t[j]^2)

arm(t_n[j<=N], qd_n[j<=N], qdd_n[j<=N]) = {
    y_n = tau(cq(t_n), sq(t_n), qd_n, qdd_n)
    d_n = y_n - tauf(cq(t_n), sq(t_n), qd_n, qdd_n)
}
reach = arm(t_n = [n/7; 1 - n/5; n/3 - 1], qd_n = [1/2; -n/4; 1/3], qdd_n = [2 - n/6; 1/3; -1])

# A sequence with a base at 0 and a guard at its tip, read ahead from 1: a
# term read ahead is computed where it is read, not filled from the base.
h(x)_0 = x
h(x)_k | k == N = x
h(x)_k = h(x)_(k+1)/2 + x
rest(x_n) = {
    z_n = h(x_n)_1 - h(x_n)_0
}
rested = rest(x_n = n/3)

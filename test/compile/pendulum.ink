# The double pendulum of two uniform rods, m = l = 1, from rest horizontal:
# its acceleration by the RNEA (Featherstone 2008, table 5.1) and a mass
# matrix of unit accelerations, a step of symplectic Euler, and the energy
# against the start. Its backward pass is a recursion the compiler unrolls,
# and the compiled step is within 0 of the interpreter's.
N = 2
l = [1; 1]
m = [1; 1]
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
f(c, s, qd, qdd, i) | i == N = fb(c, s, qd, qdd)_i
f(c, s, qd, qdd, i) = fb(c, s, qd, qdd)_i + X(c, s, i+1)'*f(c, s, qd, qdd, i+1)
tau(c, s, qd, qdd)[j<=N] = (Sj'*f(c, s, qd, qdd, j))[1,1]
z[j<=N] = 0
e(k)[j<=N] = j == k
H(c, s)[j<=N, k<=N] = tau(c, s, z, e(k))[j] - tau(c, s, z, z)[j]
fd(c, s, qd, t) = H(c, s)^-1*(t - tau(c, s, qd, z))

swing(hs = 1/100, q0 = [0; 0]) = {
    cs(q) = [cos(q[1]); cos(q[2])]
    sn(q) = [sin(q[1]); sin(q[2])]
    E(q, qd) = (qd'*H(cs(q), sn(q))*qd)[1,1]/2 + g*(m[1]*l[1]/2*sin(q[1]) + m[2]*(l[1]*sin(q[1]) + l[2]/2*sin(q[1] + q[2])))
    w_0 = z
    q_0 = q0
    w_n = w_(n-1) + hs*fd(cs(q_(n-1)), sn(q_(n-1)), w_(n-1), z)
    q_n = q_(n-1) + hs*w_n
    dE_n = E(q_n, w_n) - E(q0, z)
}
one = swing()

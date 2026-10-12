# A planar arm of three uniform rods, m = l = 1, its joints given by cosine
# and sine. rotation.ink's library. Planar spatial vectors [w; vx; vy] and
# plnr, crm, crf, mcI: Featherstone 2008, sec. 2.16 and appendix A.
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
# the RNEA, table 5.1; the backward pass a function, as a sequence read
# ahead is refused compiled (DESIGN.md, next in line)
v(c, s, qd)_0 = [0; 0; 0]
v(c, s, qd)_i = X(c, s, i)*v(c, s, qd)_(i-1) + Sj*qd[i]
a(c, s, qd, qdd)_0 = [0; 0; g]
a(c, s, qd, qdd)_i = X(c, s, i)*a(c, s, qd, qdd)_(i-1) + Sj*qdd[i] + crm(v(c, s, qd)_i)*Sj*qd[i]
fb(c, s, qd, qdd)_i = In(i)*a(c, s, qd, qdd)_i + crf(v(c, s, qd)_i)*In(i)*v(c, s, qd)_i
f(c, s, qd, qdd, i) | i == N = fb(c, s, qd, qdd)_i
f(c, s, qd, qdd, i) = fb(c, s, qd, qdd)_i + X(c, s, i+1)'*f(c, s, qd, qdd, i+1)
tau(c, s, qd, qdd)[j<=N] = (Sj'*f(c, s, qd, qdd, j))[1,1]
# the mass matrix by unit accelerations, by grad, and by the CRBA, table 6.2
z[j<=N] = 0
e(k)[j<=N] = j == k
H(c, s)[j<=N, k<=N] = tau(c, s, z, e(k))[j] - tau(c, s, z, z)[j]
Hg(c, s)[j<=N, k<=N] = (grad_(u = z) tau(c, s, z, u)[j])[k]
I3[j<=3, k<=3] = j == k
Ic(c, s, i) | i == N = In(i)
Ic(c, s, i) = In(i) + X(c, s, i+1)'*Ic(c, s, i+1)*X(c, s, i+1)
Xc(c, s, j, k) | j == k = I3
Xc(c, s, j, k) = X(c, s, k)*Xc(c, s, j, k-1)
Hc(c, s)[j<=N, k<=N] = (Sj'*Xc(c, s, min(j, k), max(j, k))'*Ic(c, s, max(j, k))*Sj)[1,1]

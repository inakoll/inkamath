# Cook's membrane (Cook 1974): the quadrilateral (0,0) (48,44) (48,60)
# (0,44), clamped at x = 0, a unit shear spread evenly over x = 48; E = 1,
# nu = 1/3, plane stress. Constant-strain triangles on a q by q grid, each
# cell cut from its lower left corner. The value asked is the vertical
# displacement at the loaded edge's midpoint, (48,52), the point whose
# converged value is 23.9642 (Bergan and Felippa 1985), not the corner
# (48,60); triangles this coarse are far below it. Expected: the same
# meshes solved in Python's fractions and sympy, exactly.
>> q = 2
q = 2

>> nn = (q+1)^2
nn = (q+1)^2

>> ix(p) = mod(p-1, q+1)
ix(p) = mod(p-1, q+1)

>> iy(p) = floor((p-1)/(q+1))
iy(p) = floor((p-1)/(q+1))

>> X(p) = 48*ix(p)/q
X(p) = 48*ix(p)/q

>> Y(p) = 44*ix(p)/q + iy(p)/q*(44 - 28*ix(p)/q)
Y(p) = 44*ix(p)/q + iy(p)/q*(44 - 28*ix(p)/q)

>> node(a, b) = b*(q+1) + a + 1
node(a, b) = b*(q+1) + a + 1

>> qx(e) = mod(floor((e-1)/2), q)
qx(e) = mod(floor((e-1)/2), q)

>> qy(e) = floor(floor((e-1)/2)/q)
qy(e) = floor(floor((e-1)/2)/q)

>> tri(e, a) | mod(e, 2) == 1 = [node(qx(e), qy(e)); node(qx(e)+1, qy(e)); node(qx(e)+1, qy(e)+1)][a]
tri(e, a) | mod(e, 2) == 1 = [node(qx(e), qy(e)); node(qx(e)+1, qy(e)); node(qx(e)+1, qy(e)+1)][a]

>> tri(e, a) = [node(qx(e), qy(e)); node(qx(e)+1, qy(e)+1); node(qx(e), qy(e)+1)][a]
tri(e, a) = [node(qx(e), qy(e)); node(qx(e)+1, qy(e)+1); node(qx(e), qy(e)+1)][a]

>> xe(e)[a<=3] = X(tri(e, a))
xe(e)[a<=3] = X(tri(e, a))

>> ye(e)[a<=3] = Y(tri(e, a))
ye(e)[a<=3] = Y(tri(e, a))

>> A2(e) = (xe(e)[2]-xe(e)[1])*(ye(e)[3]-ye(e)[1]) - (xe(e)[3]-xe(e)[1])*(ye(e)[2]-ye(e)[1])
A2(e) = (xe(e)[2]-xe(e)[1])*(ye(e)[3]-ye(e)[1]) - (xe(e)[3]-xe(e)[1])*(ye(e)[2]-ye(e)[1])

>> B(e) = 1/A2(e)*[ye(e)[2]-ye(e)[3], 0, ye(e)[3]-ye(e)[1], 0, ye(e)[1]-ye(e)[2], 0; 0, xe(e)[3]-xe(e)[2], 0, xe(e)[1]-xe(e)[3], 0, xe(e)[2]-xe(e)[1]; xe(e)[3]-xe(e)[2], ye(e)[2]-ye(e)[3], xe(e)[1]-xe(e)[3], ye(e)[3]-ye(e)[1], xe(e)[2]-xe(e)[1], ye(e)[1]-ye(e)[2]]
B(e) = 1/A2(e)*[ye(e)[2]-ye(e)[3], 0, ye(e)[3]-ye(e)[1], 0, ye(e)[1]-ye(e)[2], 0; 0, xe(e)[3]-xe(e)[2], 0, xe(e)[1]-xe(e)[3], 0, xe(e)[2]-xe(e)[1]; xe(e)[3]-xe(e)[2], ye(e)[2]-ye(e)[3], xe(e)[1]-xe(e)[3], ye(e)[3]-ye(e)[1], xe(e)[2]-xe(e)[1], ye(e)[1]-ye(e)[2]]

>> D = 1/(1-(1/3)^2)*[1 1/3 0; 1/3 1 0; 0 0 1/3]
D = 1/(1-(1/3)^2)*[1 1/3 0; 1/3 1 0; 0 0 1/3]

>> ke(e) = A2(e)/2*B(e)'*D*B(e)
ke(e) = A2(e)/2*B(e)'*D*B(e)

>> G(e)[a<=6, p<=2*nn] = p == 2*tri(e, ceil(a/2)) - mod(a, 2)
G(e)[a<=6, p<=2*nn] = p == 2*tri(e, ceil(a/2)) - mod(a, 2)

>> K = sum_(e=1)^(2*q^2) G(e)'*ke(e)*G(e)
K = sum_(e=1)^(2*q^2) G(e)'*ke(e)*G(e)

>> fx[p<=2*nn] = ix(ceil(p/2)) == 0
fx[p<=2*nn] = ix(ceil(p/2)) == 0

>> bc(A[p<=m, r<=m], c[p<=m])[p<=m, r<=m] = (1 - c[p])*(1 - c[r])*A[p,r] + c[p]*(p == r)
bc(A[p<=m, r<=m], c[p<=m])[p<=m, r<=m] = (1 - c[p])*(1 - c[r])*A[p,r] + c[p]*(p == r)

>> F[p<=2*nn] | mod(p, 2) == 0 and ix(p/2) == q = (1 - (iy(p/2) == 0 or iy(p/2) == q)/2)/q
F[p<=2*nn] | mod(p, 2) == 0 and ix(p/2) == q = (1 - (iy(p/2) == 0 or iy(p/2) == q)/2)/q

>> u = bc(K, fx)^-1*F
u = bc(K, fx)^-1*F

>> mid = u[2*node(q, q/2)]
mid = u[2*node(q, q/2)]

>> frac mid
595643302140209956431747104/88341215718319242138337515

>> q = 4
q = 4

>> digits = 12
digits = 12

>> mid
~11.2519923176

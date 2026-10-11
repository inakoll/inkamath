# A Pratt truss of four 3 m panels, 4 m deep, every bar a side of a 3-4-5
# triangle; pinned at node 1, on rollers at node 5, 10 kN down at nodes 2,
# 3 and 4, EA = 1. Expected, worked by hand: the bar forces by the method of
# joints; the midspan deflection by virtual work, a unit load at node 3,
# sum of N n L/EA = 2195/8; each reaction 15. The same truss in sympy
# agrees. The supports are struck by a function of the stiffness, bc,
# rather than by cells reading it, which would assemble it again for each
# (DESIGN.md, next in line: plain definitions remembered).
>> X = [0 0; 3 0; 6 0; 9 0; 12 0; 3 4; 6 4; 9 4]
X = [0 0; 3 0; 6 0; 9 0; 12 0; 3 4; 6 4; 9 4]

>> C = [1 2; 2 3; 3 4; 4 5; 6 7; 7 8; 1 6; 8 5; 2 6; 3 7; 4 8; 6 3; 8 3]
C = [1 2; 2 3; 3 4; 4 5; 6 7; 7 8; 1 6; 8 5; 2 6; 3 7; 4 8; 6 3; 8 3]

>> Lg = [3; 3; 3; 3; 3; 3; 5; 5; 4; 4; 4; 5; 5]
Lg = [3; 3; 3; 3; 3; 3; 5; 5; 4; 4; 4; 5; 5]

>> dx(e) = X[C[e,2],1] - X[C[e,1],1]
dx(e) = X[C[e,2],1] - X[C[e,1],1]

>> dy(e) = X[C[e,2],2] - X[C[e,1],2]
dy(e) = X[C[e,2],2] - X[C[e,1],2]

>> sum_(e=1)^13 (Lg[e]^2 <> dx(e)^2 + dy(e)^2)
0

>> t(e) = [-dx(e) -dy(e) dx(e) dy(e)]
t(e) = [-dx(e) -dy(e) dx(e) dy(e)]

>> ke(e) = 1/Lg[e]^3*t(e)'*t(e)
ke(e) = 1/Lg[e]^3*t(e)'*t(e)

>> G(e)[a<=4, p<=16] = p == 2*C[e, ceil(a/2)] - mod(a, 2)
G(e)[a<=4, p<=16] = p == 2*C[e, ceil(a/2)] - mod(a, 2)

>> K = sum_(e=1)^13 G(e)'*ke(e)*G(e)
K = sum_(e=1)^13 G(e)'*ke(e)*G(e)

>> fx[p<=16] = p == 1 or p == 2 or p == 10
fx[p<=16] = p == 1 or p == 2 or p == 10

>> bc(A[p<=m, q<=m], c[p<=m])[p<=m, q<=m] = (1 - c[p])*(1 - c[q])*A[p,q] + c[p]*(p == q)
bc(A[p<=m, q<=m], c[p<=m])[p<=m, q<=m] = (1 - c[p])*(1 - c[q])*A[p,q] + c[p]*(p == q)

>> F[p<=16] | p == 4 or p == 6 or p == 8 = -10
F[p<=16] | p == 4 or p == 6 or p == 8 = -10

>> u = bc(K, fx)^-1*F
u = bc(K, fx)^-1*F

>> N[e<=13] = 1/Lg[e]^2*t(e)*G(e)*u
N[e<=13] = 1/Lg[e]^2*t(e)*G(e)*u

>> frac N'
[45/4, 45/4, 45/4, 45/4, -15, -15, -75/4, -75/4, 10, 0, 10, 25/4, 25/4]

>> frac u[6]
-2195/8

>> frac [(K*u - F)[2], (K*u - F)[10]]
[15, 15]

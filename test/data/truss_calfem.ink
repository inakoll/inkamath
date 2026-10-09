# Three bars meeting at node 3 of a plane truss, the CALFEM manual's example
# exs3 (Austrell et al., Lund University): E = 200 GPa, areas 6, 3 and 10
# cm^2, P = 80 kN down at node 3. The manual prints a5, a6 and N1 to N3
# rounded; expected, the same truss solved exactly in sympy, and the method
# of joints at node 3. The lengths are typed, a root being inexact
# (DESIGN.md, next in line: an exact root of a perfect power).
>> X = [0 0; 0 6/5; 8/5 0; 8/5 6/5]
X = [0 0; 0 6/5; 8/5 0; 8/5 6/5]

>> C = [1 3; 3 4; 2 3]
C = [1 3; 3 4; 2 3]

>> Lg = [8/5; 6/5; 2]
Lg = [8/5; 6/5; 2]

>> Ar = [6; 3; 10]/10^4
Ar = [6; 3; 10]/10^4

>> Em = 2*10^11
Em = 2*10^11

>> dx(e) = X[C[e,2],1] - X[C[e,1],1]
dx(e) = X[C[e,2],1] - X[C[e,1],1]

>> dy(e) = X[C[e,2],2] - X[C[e,1],2]
dy(e) = X[C[e,2],2] - X[C[e,1],2]

>> t(e) = [-dx(e) -dy(e) dx(e) dy(e)]
t(e) = [-dx(e) -dy(e) dx(e) dy(e)]

>> ke(e) = Em*Ar[e]/Lg[e]^3*t(e)'*t(e)
ke(e) = Em*Ar[e]/Lg[e]^3*t(e)'*t(e)

>> G(e)[a<=4, p<=8] = p == 2*C[e, ceil(a/2)] - mod(a, 2)
G(e)[a<=4, p<=8] = p == 2*C[e, ceil(a/2)] - mod(a, 2)

>> K = sum_(e=1)^3 G(e)'*ke(e)*G(e)
K = sum_(e=1)^3 G(e)'*ke(e)*G(e)

>> S[f<=2, p<=8] = p == f + 4
S[f<=2, p<=8] = p == f + 4

>> F[p<=8] | p == 6 = -80000
F[p<=8] | p == 6 = -80000

>> u = S'*(S*K*S')^-1*S*F
u = S'*(S*K*S')^-1*S*F

>> Nf(e) = Em*Ar[e]/Lg[e]^2*t(e)*G(e)*u
Nf(e) = Em*Ar[e]/Lg[e]^2*t(e)*G(e)*u

>> frac u[5]
-48/120625

>> frac u[6]
-139/120625

>> frac [Nf(1); Nf(2); Nf(3)]
[-5760000/193;
 11120000/193;
  7200000/193]

>> [Nf(1); Nf(2); Nf(3)]
[~-29844.5596;
  ~57616.5803;
  ~37305.6995]

>> frac K*u - F
[ 5760000/193;
            0;
 -5760000/193;
  4320000/193;
            0;
            0;
            0;
 11120000/193]

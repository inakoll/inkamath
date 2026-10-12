# The contact-aided right-invariant EKF of Hartley et al., IJRR 2020, its
# theorems held in rationals. Each Gamma_m of (48) is cut at the same power
# N of (phi)x, so that (66) is the exponential of (6) cut at N, and the
# identities hold exactly; every expected value is 1.

# (6), (48), (66): SE_3(3) and its exponential; (14), (54): A, nilpotent, and
# Phi = expm(A dt), its series ending at the square; (50), (47): the step.
>> N = 6
N = 6

>> dt = 1/100
dt = 1/100

>> I3[j<=3, k<=3] = j == k
I3[j<=3, k<=3] = j == k

>> Z[j<=3, k<=3] = 0
Z[j<=3, k<=3] = 0

>> g = [0; 0; -981/100]
g = [0; 0; -981/100]

>> sk(f) = [0, -f[3], f[2]; f[3], 0, -f[1]; -f[2], f[1], 0]
sk(f) = [0, -f[3], f[2]; f[3], 0, -f[1]; -f[2], f[1], 0]

>> G(m, f) = sum_(q=0)^(N-m) sk(f)^q/!(q+m)
G(m, f) = sum_(q=0)^(N-m) sk(f)^q/!(q+m)

>> blk(X)[j<=3, k<=3] = X[j, k]
blk(X)[j<=3, k<=3] = X[j, k]

>> col(X, c)[j<=3] = X[j, c]
col(X, c)[j<=3] = X[j, c]

>> seg(x, c)[j<=3] = x[3*c + j - 3]
seg(x, c)[j<=3] = x[3*c + j - 3]

>> st(R, v, p, d) = [R, v, p, d; 0, 1, 0, 0; 0, 0, 1, 0; 0, 0, 0, 1]
st(R, v, p, d) = [R, v, p, d; 0, 1, 0, 0; 0, 0, 1, 0; 0, 0, 0, 1]

>> Exp(x) = st(G(0, seg(x, 1)), G(1, seg(x, 1))*seg(x, 2), G(1, seg(x, 1))*seg(x, 3), G(1, seg(x, 1))*seg(x, 4))
Exp(x) = st(G(0, seg(x, 1)), G(1, seg(x, 1))*seg(x, 2), G(1, seg(x, 1))*seg(x, 3), G(1, seg(x, 1))*seg(x, 4))

>> A = [Z, Z, Z, Z; sk(g), Z, Z, Z; Z, I3, Z, Z; Z, Z, Z, Z]
A = [Z, Z, Z, Z; sk(g), Z, Z, Z; Z, I3, Z, Z; Z, Z, Z, Z]

>> Phi = sum_(q=0)^2 (A*dt)^q/!q
Phi = sum_(q=0)^2 (A*dt)^q/!q

>> f(X, w, a) = st(blk(X)*G(0, w*dt), col(X, 4) + blk(X)*G(1, w*dt)*a*dt + g*dt, col(X, 5) + col(X, 4)*dt + blk(X)*G(2, w*dt)*a*dt^2 + g*dt^2/2, col(X, 6))
f(X, w, a) = st(blk(X)*G(0, w*dt), col(X, 4) + blk(X)*G(1, w*dt)*a*dt + g*dt, col(X, 5) + col(X, 4)*dt + blk(X)*G(2, w*dt)*a*dt^2 + g*dt^2/2, col(X, 6))

>> A^3 == 0*A
1

>> Phi == [I3, Z, Z, Z; sk(g)*dt, I3, Z, Z; sk(g)*dt^2/2, I3*dt, I3, Z; Z, Z, Z, I3]
1

# Theorem 2: two trajectories on the same inputs, one starting at
# exp(xi0) times the other, part by eta_k = exp(Phi^k xi0).
>> w = [1/10; -1/5; 1/2]
w = [1/10; -1/5; 1/2]

>> a = [1/2; 0; 981/100]
a = [1/2; 0; 981/100]

>> X0 = st(I3, [1/5; 0; 0], [0; 0; 1/2], [1/10; 1/20; 0])
X0 = st(I3, [1/5; 0; 0], [0; 0; 1/2], [1/10; 1/20; 0])

>> xi0 = [1/10; -1/5; 3/10; 1/2; -2/5; 1/5; 1; -1; 1/2; 3/10; 1/10; -1/5]
xi0 = [1/10; -1/5; 3/10; 1/2; -2/5; 1/5; 1; -1; 1/2; 3/10; 1/10; -1/5]

>> Xa_0 = X0
Xa_0 = X0

>> Xa_k = f(Xa_(k-1), w, a)
Xa_k = f(Xa_(k-1), w, a)

>> Xb_0 = Exp(xi0)*X0
Xb_0 = Exp(xi0)*X0

>> Xb_k = f(Xb_(k-1), w, a)
Xb_k = f(Xb_(k-1), w, a)

>> eta_k = Xb_k*Xa_k^-1
eta_k = Xb_k*Xa_k^-1

>> eta_1 == Exp(Phi*xi0)
1

>> eta_10 == Exp(Phi^10*xi0)
1

# Theorem 1 and (18)-(20): with the noise of the process left out and the
# measurement's covariance I3/100, the filter's error evolves alike along
# any two trajectories, each estimate starting at exp(xi0) times its truth.
>> H = [Z, Z, -I3, I3]
H = [Z, Z, -I3, I3]

>> Pi = [I3, Z]
Pi = [I3, Z]

>> b = [0; 0; 0; 0; 1; -1]
b = [0; 0; 0; 0; 1; -1]

>> I12[j<=12, k<=12] = j == k
I12[j<=12, k<=12] = j == k

>> P_0 = I12/100
P_0 = I12/100

>> Pp_k = Phi*P_(k-1)*Phi'
Pp_k = Phi*P_(k-1)*Phi'

>> K_k = Pp_k*H'*(H*Pp_k*H' + I3/100)^-1
K_k = Pp_k*H'*(H*Pp_k*H' + I3/100)^-1

>> P_k = (I12 - K_k*H)*Pp_k*(I12 - K_k*H)' + K_k*K_k'/100
P_k = (I12 - K_k*H)*Pp_k*(I12 - K_k*H)' + K_k*K_k'/100

>> T(X0, w, a)_0 = X0
T(X0, w, a)_0 = X0

>> T(X0, w, a)_k = f(T(X0, w, a)_(k-1), w, a)
T(X0, w, a)_k = f(T(X0, w, a)_(k-1), w, a)

>> E(X0, w, a)_0 = Exp(xi0)*X0
E(X0, w, a)_0 = Exp(xi0)*X0

>> E(X0, w, a)_k = Exp(K_k*Pi*f(E(X0, w, a)_(k-1), w, a)*T(X0, w, a)_k^-1*b)*f(E(X0, w, a)_(k-1), w, a)
E(X0, w, a)_k = Exp(K_k*Pi*f(E(X0, w, a)_(k-1), w, a)*T(X0, w, a)_k^-1*b)*f(E(X0, w, a)_(k-1), w, a)

>> er(X0, w, a, k) = E(X0, w, a)_k*T(X0, w, a)_k^-1
er(X0, w, a, k) = E(X0, w, a)_k*T(X0, w, a)_k^-1

>> er(X0, w, a, 1) == er(st(I3, [0; 0; 0], [0; 0; 0], [0; 0; 0]), [0; 0; 1], [0; 0; 981/100], 1)
1

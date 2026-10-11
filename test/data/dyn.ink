# A cantilever of four Hermite elements, EI = rhoA = L = 1, a unit tip load
# from t = 0, for dynamics.ink.
h = 1/4
ke = 1/h^3*[12 6*h -12 6*h; 6*h 4*h^2 -6*h 2*h^2; -12 -6*h 12 -6*h; 6*h 2*h^2 -6*h 4*h^2]
me = h/420*[156 22*h 54 -13*h; 22*h 4*h^2 13*h -3*h^2; 54 13*h 156 -22*h; -13*h -3*h^2 -22*h 4*h^2]
G(e)[a<=4, p<=10] = p == 2*e - 2 + a
S[f<=8, p<=10] = p == f + 2
K = S*(sum_(e=1)^4 G(e)'*ke*G(e))*S'
M = S*(sum_(e=1)^4 G(e)'*me*G(e))*S'
P[f<=8] | f == 7 = 1
# Newmark's average acceleration, beta = 1/4, gamma = 1/2
newmark(dt = 1/100, f_n[j<=8]) = {
    Ae = (M + dt^2/4*K)^-1
    u_0[j<=8] = 0
    v_0[j<=8] = 0
    a_0 = M^-1*f_0
    a_n = Ae*(f_n - K*(u_(n-1) + dt*v_(n-1) + dt^2/4*a_(n-1)))
    v_n = v_(n-1) + dt/2*(a_(n-1) + a_n)
    u_n = u_(n-1) + dt*v_(n-1) + dt^2/4*(a_(n-1) + a_n)
    tip_n = u_n[7]
}
# Central differences, stable below dt = 2/omega_max
central(dt = 1/1000, f_n[j<=8]) = {
    Mi = M^-1
    u_0[j<=8] = 0
    u_1 = dt^2/2*Mi*f_0
    u_n = 2*u_(n-1) - u_(n-2) + dt^2*Mi*(f_(n-1) - K*u_(n-1))
    tip_n = u_n[7]
}
nm = newmark(f_n = P)
cd = central(f_n = P)
fast = central(dt = 1/100, f_n = P)
# Sylvester: the negative pivots of LDL' of A count its negative eigenvalues
sc(A)_0 = A
sc(A[j<=m, l<=m])_k[j<=m, l<=m] = sc(A)_(k-1)[j,l] - (j > k and l > k)*sc(A)_(k-1)[j,k]*sc(A)_(k-1)[k,l]/sc(A)_(k-1)[k,k]
neg(A[j<=m, l<=m]) = sum_(k=1)^m (sc(A)_(k-1)[k,k] < 0)
# Central differences are stable where every omega^2 is below 4/dt^2
stable(dt) = neg(K - 4/dt^2*M) == 8

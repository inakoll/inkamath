# A cantilever's modes, K phi = omega^2 M phi, of Hermite elements with
# their consistent mass, EI = rhoA = L = 1. Expected: mpmath's eigenvalues
# of M^-1 K at 30 digits, of the same matrices in sympy. They converge from
# above to the continuum's (beta L)^4, 12.3623634 and 485.518819, beta L a
# root of cos x cosh x = -1 (Blevins, table 8-1); one element's first,
# 12.4801922, is the textbook omega = 3.533.
>> h(n) = 1/n
h(n) = 1/n

>> ke(n) = 1/h(n)^3*[12 6*h(n) -12 6*h(n); 6*h(n) 4*h(n)^2 -6*h(n) 2*h(n)^2; -12 -6*h(n) 12 -6*h(n); 6*h(n) 2*h(n)^2 -6*h(n) 4*h(n)^2]
ke(n) = 1/h(n)^3*[12 6*h(n) -12 6*h(n); 6*h(n) 4*h(n)^2 -6*h(n) 2*h(n)^2; -12 -6*h(n) 12 -6*h(n); 6*h(n) 2*h(n)^2 -6*h(n) 4*h(n)^2]

>> me(n) = h(n)/420*[156 22*h(n) 54 -13*h(n); 22*h(n) 4*h(n)^2 13*h(n) -3*h(n)^2; 54 13*h(n) 156 -22*h(n); -13*h(n) -3*h(n)^2 -22*h(n) 4*h(n)^2]
me(n) = h(n)/420*[156 22*h(n) 54 -13*h(n); 22*h(n) 4*h(n)^2 13*h(n) -3*h(n)^2; 54 13*h(n) 156 -22*h(n); -13*h(n) -3*h(n)^2 -22*h(n) 4*h(n)^2]

>> G(n, e)[a<=4, p<=2*n+2] = p == 2*e - 2 + a
G(n, e)[a<=4, p<=2*n+2] = p == 2*e - 2 + a

>> S(n)[f<=2*n, p<=2*n+2] = p == f + 2
S(n)[f<=2*n, p<=2*n+2] = p == f + 2

>> Kf(n) = S(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*S(n)'
Kf(n) = S(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*S(n)'

>> Mf(n) = S(n)*(sum_(e=1)^n G(n, e)'*me(n)*G(n, e))*S(n)'
Mf(n) = S(n)*(sum_(e=1)^n G(n, e)'*me(n)*G(n, e))*S(n)'

>> digits = 12
digits = 12

>> eig(Mf(1)^-1*Kf(1))
[~12.4801921538;
 ~1211.51980785]

>> eig(Mf(2)^-1*Kf(2))
[~12.3743191141;
 ~493.793927802;
 ~5648.58713391;
  ~47584.197757]

# Sylvester's law of inertia: the negative pivots of LDL' of K - s M count
# the eigenvalues below s, exactly, without M^-1 K (DESIGN.md, next in
# line: eig(K, M)); bisected, the first of three elements, 12.3648691229.
>> sc(A)_0 = A
sc(A)_0 = A

>> sc(A[j<=m, l<=m])_k[j<=m, l<=m] = sc(A)_(k-1)[j,l] - (j > k and l > k)*sc(A)_(k-1)[j,k]*sc(A)_(k-1)[k,l]/sc(A)_(k-1)[k,k]
sc(A[j<=m, l<=m])_k[j<=m, l<=m] = sc(A)_(k-1)[j,l] - (j > k and l > k)*sc(A)_(k-1)[j,k]*sc(A)_(k-1)[k,l]/sc(A)_(k-1)[k,k]

>> neg(A[j<=m, l<=m]) = sum_(k=1)^m (sc(A)_(k-1)[k,k] < 0)
neg(A[j<=m, l<=m]) = sum_(k=1)^m (sc(A)_(k-1)[k,k] < 0)

>> cnt(n, s) = neg(Kf(n) - s*Mf(n))
cnt(n, s) = neg(Kf(n) - s*Mf(n))

>> [cnt(2, 12), cnt(2, 13), cnt(2, 494), cnt(2, 47585)]
[0, 1, 2, 4]

>> br(n, r)_0 = [0; 2^30]
br(n, r)_0 = [0; 2^30]

>> br(n, r)_b = br(n, r)_(b-1) + (br(n, r)_(b-1)[2] - br(n, r)_(b-1)[1])/2*([1; 0] - (cnt(n, (br(n, r)_(b-1)[1] + br(n, r)_(b-1)[2])/2) >= r)*[1; 1])
br(n, r)_b = br(n, r)_(b-1) + (br(n, r)_(b-1)[2] - br(n, r)_(b-1)[1])/2*([1; 0] - (cnt(n, (br(n, r)_(b-1)[1] + br(n, r)_(b-1)[2])/2) >= r)*[1; 1])

>> ~br(3, 1)_60
[~12.3648691224;
 ~12.3648691233]

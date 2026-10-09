# The generalized eigenproblem, eig(A, B): every lambda of A phi = lambda
# B phi, B symmetric positive definite, as a column, smallest first
# (DESIGN.md, next in line). Every value was worked out apart from the
# interpreter, as its specification: the eigenvalues as the roots of
# det(A - x B) in sympy, exactly, and each bracket by bisecting B^-1 A
# over 2^e in Python's fractions with every test decided from them,
# printed as Number::Shown prints a double.

# det(A - x B) = (2 - 2x)^2 - (1 + x)^2 = (3 - x)(1 - 3x), by hand: 1/3
# and 3, which a halving reaches, so it is itself.
>> eig([2 -1; -1 2], [2 1; 1 2])
[~0.333333333;
           ~3]

# B diagonal: (6 - 2x)(3 - x) - 4, so 3 - sqrt(2) and 3 + sqrt(2).
>> eig([6 2; 2 3], [2 0; 0 1])
[~1.58578644;
 ~4.41421356]

# eig(A, B) is eig(B^-1*A) whatever A is: B the identity, B's default, is
# the standard problem, and a non-symmetric A asks Hermite's test of
# B^-1*A, as eig does, refused in eig's words where an eigenvalue is
# complex.
>> eig([2 1; 1 3], [1 0; 0 1])
[~1.38196601;
 ~3.61803399]

>> eig([1 2; 3 4], [1 0; 0 1])
[~-0.372281323;
   ~5.37228132]

>> eig([0 1; -1 0], [1 0; 0 1])
error: eig needs a matrix whose eigenvalues are all real

# det(A - x B) = (3x + 1)(x - 2), by hand; and beside a multiple of the
# identity, a scale.
>> eig([1 2; 3 4], [2 1; 1 2])
[~-0.333333333;
            ~2]

>> eig([1 2; 3 4], [2 0; 0 2])
[~-0.186140662;
   ~2.68614066]

>> eig([1 2; 3 4]/2)
[~-0.186140662;
   ~2.68614066]

>> eig([0 1; -1 0], [2 0; 0 2])
error: eig needs a matrix whose eigenvalues are all real

# A repeated eigenvalue is each of its copies. A = L D L' and B = L L', L
# lower triangular of ones: D = diag(1, 1, 4), then diag(1/3, 1/3, 2).
>> eig([1 1 1; 1 2 2; 1 2 6], [1 1 1; 1 2 2; 1 2 3])
[~1;
 ~1;
 ~4]

>> eig([1 1 1; 1 2 2; 1 2 8]/3, [1 1 1; 1 2 2; 1 2 3])
[~0.333333333;
 ~0.333333333;
           ~2]

# A free bar of two elements, each of stiffness and mass 1, its mass
# consistent: the rigid-body mode's 0 is certified by the counts at 0,
# unmarked; then 3 and 12.
>> eig([1 -1 0; -1 2 -1; 0 -1 1], [2 1 0; 1 4 1; 0 1 2]/6)
[ ~0;
  ~3;
 ~12]

# A cantilever's modes, K phi = omega^2 M phi, as modal.ink defines it:
# Hermite elements with their consistent mass, EI = rhoA = L = 1. mpmath's
# eigenvalues of M^-1 K at 30 digits agree; they converge from above to
# (beta L)^4, 12.3623634 and 485.518819 (Blevins, table 8-1).
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

>> eig(Kf(1), Mf(1))
[~12.4801921538;
 ~1211.51980785]

>> eig(Kf(2), Mf(2))
[~12.3743191141;
 ~493.793927802;
 ~5648.58713391;
  ~47584.197757]

# The same answers as eig(M^-1*K), bracket for bracket, where M^-1*K
# asks Hermite's test, as it is not symmetric.
>> eig(Kf(2), Mf(2)) == eig(Mf(2)^-1*Kf(2))
~1

# Eight unknowns, of four elements.
>> eig(Kf(4), Mf(4))
[~12.3631720798;
 ~486.650937534;
  ~3865.7172606;
 ~15044.8965027;
 ~52046.6722755;
 ~134241.343496;
 ~337385.710043;
 ~908306.291396]

>> digits = 9
digits = 9

# A pinned column's buckling, K phi = lambda K_G phi, K_G the geometric
# stiffness of the same elements: the least lambda is the critical load,
# 12 EI/L^2 of one element as the textbooks give it, then from above to
# Euler's pi^2 EI/L^2, 9.8696044, as 9.87465903 of four elements. Some
# loads are whole numbers, which a halving reaches.
>> kg(n) = 1/(30*h(n))*[36 3*h(n) -36 3*h(n); 3*h(n) 4*h(n)^2 -3*h(n) -h(n)^2; -36 -3*h(n) 36 -3*h(n); 3*h(n) -h(n)^2 -3*h(n) 4*h(n)^2]
kg(n) = 1/(30*h(n))*[36 3*h(n) -36 3*h(n); 3*h(n) 4*h(n)^2 -3*h(n) -h(n)^2; -36 -3*h(n) 36 -3*h(n); 3*h(n) -h(n)^2 -3*h(n) 4*h(n)^2]

>> P(n)[f<=2*n, p<=2*n+2] = p == f + 1 + (f == 2*n)
P(n)[f<=2*n, p<=2*n+2] = p == f + 1 + (f == 2*n)

>> Kc(n) = P(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*P(n)'
Kc(n) = P(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*P(n)'

>> Gc(n) = P(n)*(sum_(e=1)^n G(n, e)'*kg(n)*G(n, e))*P(n)'
Gc(n) = P(n)*(sum_(e=1)^n G(n, e)'*kg(n)*G(n, e))*P(n)'

>> eig(Kc(1), Gc(1))
[~12;
 ~60]

>> eig(Kc(2), Gc(2))
[~9.9438468;
        ~48;
 ~128.72282;
       ~240]

# Bisected as B^-1*A over 2^e, so 10^-150 is no tighter than 1, in A or B.
>> eig([2 -1; -1 2]/10^150, [2 1; 1 2])
[~3.33333333e-151;
          ~3e-150]

>> eig([2 -1; -1 2], [2 1; 1 2]/10^150)
[~3.33333333e+149;
          ~3e+150]

# Doubles, A's and B's, are read as the rationals they are (C275), so an
# answer is right for the data as stored, and inexact: 0.1/0.3 as stored is
# not 1/3, and a pencil of tenths as stored parts at the 16th digit from the
# tenths meant. A single value is a 1x1 matrix.
>> digits = 17
digits = 17

>> eig(1/10, 3/10)
~0.33333333333333331

>> eig(~0.1, ~0.3)
~0.33333333333333337

>> eig([9 -7 6; -7 -5 9; 6 9 -9]/10, [4 1 0; 1 4 1; 0 1 4]/10)
[~-5.6857093732557615;
 ~0.37866199639776654;
  ~3.8070473768579953]

>> eig([9 -7 6; -7 -5 9; 6 9 -9]/10*~1, [4 1 0; 1 4 1; 0 1 4]/10*~1)
[~-5.6857093732557615;
 ~0.37866199639776654;
  ~3.8070473768579949]

>> digits = 9
digits = 9

>> frac eig([~2 -1; -1 2], [2 1; 1 2])
error: ~0.333333333 was approximated, so it has no exact fraction

# B symmetric positive definite is what makes every eigenvalue real where
# A is symmetric, so a B without it is refused, in one set of words, and
# before A is looked at: B indefinite, singular or not symmetric.
>> eig([2 1; 1 2], [1 0; 0 -1])
error: eig needs B symmetric positive definite

>> eig([2 1; 1 2], [1 1; 1 1])
error: eig needs B symmetric positive definite

>> eig([2 1; 1 2], [2 1; 0 2])
error: eig needs B symmetric positive definite

>> eig([0 1; -1 0], [1 0; 0 -1])
error: eig needs B symmetric positive definite

# A lumped mass that gives a rotation none is singular: condensing the
# rotation out is the model's to write.
>> eig(Kf(1), [1/2 0; 0 0])
error: eig needs B symmetric positive definite

# Refused though the eigenvalues are real: B indefinite, which
# eig(B^-1*A) answers by Hermite's test.
>> eig([1 0; 0 1], [1 0; 0 -1])
error: eig needs B symmetric positive definite

>> eig([1 0; 0 -1]^-1*[1 0; 0 1])
[~-1;
  ~1]

# Shapes, by the signature.
>> eig([1 2; 3 4], [1 2 3])
error: eig takes A[j<=n, k<=n] and B[j<=n, k<=n], not a 2x2 matrix and a 1x3 matrix

>> eig([2 1; 1 2], [1 0 0; 0 1 0; 0 0 1])
error: eig takes A[j<=n, k<=n] and B[j<=n, k<=n], not a 2x2 matrix and a 3x3 matrix

# A complex cell in max's words, A's or B's, not one of B^-1*A.
>> eig([1 i; -i 1], [2 0; 0 2])
error: a comparison needs real numbers, not ~(i)

>> eig([2 1; 1 2], [2 i; -i 2])
error: a comparison needs real numbers, not ~(i)

# A cell that is itself inf has no value to read, in A or in B (C206).
>> eig([~1 0; 0 1]*10^310, [2 1; 1 2])
error: eig needs finite cells, not ~inf

>> eig([2 1; 1 2], [~1 0; 0 1]*10^310)
error: eig needs finite cells, not ~inf

# A bisection is a staircase in A and in B: refused where either moves,
# flat where neither does.
>> grad_(k = 2) eig([k -1; -1 2], [2 1; 1 2])
error: grad cannot differentiate eig yet

>> grad_(m = 1) eig([2 -1; -1 2], [m 1; 1 2])
error: grad cannot differentiate eig yet

>> grad_(a = 2) a*eig([2 -1; -1 2], [2 1; 1 2])
[~0.333333333;
           ~3]

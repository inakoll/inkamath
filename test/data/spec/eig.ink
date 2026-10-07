# The real eigenvalues and the largest singular value, in the prelude,
# written in inkamath (DESIGN.md, next in line). Written by hand, never
# recorded: eigenvalues exactly with sympy, real roots compared exactly,
# and each bracket by bisecting in Python's fractions with every test
# decided from the exact eigenvalue, apart from the interpreter.

# The eigenvalues as a column, smallest first, each with its multiplicity.
>> eig([3 0 0; 0 -1 0; 0 0 2])
[-1;
  2;
  3]

# (5 - sqrt(5))/2 and (5 + sqrt(5))/2.
>> eig([2 1; 1 3])
[~1.38196601;
 ~3.61803399]

# eigb(A, k)_m is the k-th one's bracket after m halvings, from [-B, B), B
# = 8 here: its lower end at most the eigenvalue, its upper end above it.
>> frac eigb([2 1; 1 3], 1)_64
[  796649166502997023/576460752303423488;
 1593298333005994047/1152921504606846976]

>> frac eigb([2 1; 1 3], 2)_64
[4171309190028240833/1152921504606846976;
  2085654595014120417/576460752303423488]

# A repeated eigenvalue is each of its copies, the count being exact with
# multiplicity: 1 twice, then -sqrt(2) and sqrt(2) twice each.
>> eig([2 1 1; 1 2 1; 1 1 2])
[1;
 1;
 4]

>> eig([1 0 1 0; 0 1 0 1; 1 0 -1 0; 0 1 0 -1])
[~-1.41421356;
 ~-1.41421356;
  ~1.41421356;
  ~1.41421356]

>> digits = 17
digits = 17

>> eig([2 1; 1 3])
[~1.3819660112501051;
 ~3.6180339887498949]

# Two eigenvalues 2*10^-12 apart, 1 - 10^-12 and 1 + 10^-12, are split at
# the first midpoint between them.
>> eig([1 1/10^12; 1/10^12 1])
[~0.99999999999900002;
  ~1.0000000000010001]

>> digits = 9
digits = 9

# A 0 is certified exactly, by the counts at 0: no bisection, so no cap
# and no mark. 0, 2 - sqrt(2) and 2 + sqrt(2).
>> eig([1 1 1; 1 1 1; 1 1 2])
[           0;
 ~0.585786438;
  ~3.41421356]

>> frac eigb([1 1 1; 1 1 1; 1 1 2], 1)_0
[0;
 0]

>> eig([0 0; 0 0])
[0;
 0]

# Not symmetric, and nilpotent: its roots are real, both 0.
>> eig([0 1; 0 0])
[0;
 0]

# An eigenvalue that is not 0 but below 2^-203 of B is the bracket the
# 256th halving leaves, marked, as rho's is.
>> eig([1/10^100 0; 0 1])
[0;
 1]  # approximated past a thousand digits

# Hilbert's 5x5 (mpmath: 3.287928772e-6, 3.058980402e-4, 0.01140749162,
# 0.2085342186, 1.567050691), and a symmetric 6x6 of tenths (NumPy:
# -2.36812234, -1.06219501, -0.30161529, 0.56986279, 0.93395189,
# 2.42811796), every test exact.
>> eig([1 1/2 1/3 1/4 1/5; 1/2 1/3 1/4 1/5 1/6; 1/3 1/4 1/5 1/6 1/7; 1/4 1/5 1/6 1/7 1/8; 1/5 1/6 1/7 1/8 1/9])
[~3.28792877e-06;
  ~0.00030589804;
   ~0.0114074916;
    ~0.208534219;
     ~1.56705069]

>> eig([9 -7 6 -1 -8 -9; -7 -5 9 6 2 1; 6 9 -9 -1 6 -3; -1 6 -1 4 8 8; -8 2 6 8 -6 -3; -9 1 -3 8 -3 9]/10)
[ ~-2.36812234;
  ~-1.06219501;
 ~-0.301615294;
  ~0.569862788;
  ~0.933951894;
   ~2.42811796]

# Not symmetric, its roots real: (5 - sqrt(33))/2 and (5 + sqrt(33))/2;
# and the companion matrix of (s + 1)^4.
>> eig([1 2; 3 4])
[~-0.372281323;
   ~5.37228132]

>> eig([-4 -6 -4 -1; 1 0 0 0; 0 1 0 0; 0 0 1 0])
[-1;
 -1;
 -1;
 -1]

# Complex roots are refused, i and -i, and 2 with i and -i: not the real
# ones alone.
>> eig([0 1; -1 0])
error: eig needs a matrix whose eigenvalues are all real

>> eig([2 0 0; 0 0 1; 0 -1 0])
error: eig needs a matrix whose eigenvalues are all real

# Hermite's test past a thousand digits is rounded, and this one, its
# matrix exact and its eigenvalue 1/3 + 7^-600 twice, is refused: in words
# that say the guard was approximated.
>> eig([1/3 + 1/7^600 1; 0 1/3 + 1/7^600])
error: eig needs a matrix whose eigenvalues are all real, by a guard approximated past a thousand digits

# A single value is a 1x1 matrix.
>> eig(-1/3)
~-0.333333333

>> eig(5)
5

# An inexact matrix is bisected by rounded tests, so its bracket certifies
# nothing.
>> eig([~2 1; 1 3])
[~1.38196601;
 ~3.61803399]

>> frac eigb([~2 1; 1 3], 1)_64
error: ~1.38196601 was approximated, so it has no exact fraction

# Bisected as A/B, so its cells' sum, 1.2e308, need not be below a
# double's largest power of two, and 10^-150 is no tighter than 1.
>> eig([~1 2; 2 1]*2*10^307)
[~-2e+307;
  ~6e+307]

>> eig([2 1; 1 2]/10^150)
[~1e-150;
 ~3e-150]

# One approximated past a thousand digits marks the answer: rt_20 is
# within a double of sqrt(2).
>> rt_0 = 1
rt_0 = 1

>> rt_n = (rt_(n-1) + 2/rt_(n-1))/2
rt_n = (rt_(n-1) + 2/rt_(n-1))/2

>> eig([rt_20 0; 0 1/2])
[        0.5;
 ~1.41421356]  # approximated past a thousand digits

# The largest singular value, sqrt of the largest eigenvalue of A'A, of a
# matrix of any size (NumPy: 9.525518091565107, 9.508032000695724).
>> smax([1 2; 3 4; 5 6])
~9.52551809

>> smax([1 2 3; 4 5 6])
~9.508032

>> smax([3 4])
5

>> smax([3; 4])
5

>> smax(-2)
2

>> smax(0*[1 2; 3 4])
0

# A is scaled before A'A is formed, which would be 10^400 and 10^-400.
>> smax([~1 2; 3 4]*10^200)
~5.4649857e+200

>> smax([1 2; 3 4]/10^200)
~5.4649857e-200

>> eig([1 2 3])
error: eig takes A[j<=n, k<=n], not a 1x3 matrix

# In abs's words, as rho is.
>> eig([1 i; -i 1])
error: a comparison needs real numbers, not i

>> smax([1 i])
error: a comparison needs real numbers, not i

# Bisections are staircases in A: refused where it moves, as rho is.
>> grad_(A = [2 1; 1 3]) eig(A)
error: grad cannot differentiate eig yet

>> grad_(a = 1) smax([a 2; 3 4])
error: grad cannot differentiate smax yet

>> grad_(a = 2) a*eig([2 1; 1 2])
[1;
 3]

# The names are the prelude's, which a session may take for itself.
>> eig = 7
eig = 7

>> eig*2
14

>> clear eig
clear eig

>> eig([2])
2

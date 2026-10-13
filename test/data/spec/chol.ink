# The Cholesky factor in the prelude, chol(A): L lower triangular with a
# positive diagonal and L*L' = A, by Cholesky's recurrence, reading A's
# lower triangle alone (DESIGN.md, next in line). Every value was worked
# out apart from the interpreter, as its specification: the exact ones in
# Python's fractions, the inexact ones by hand and in mpmath at 50 digits,
# printed as the interpreter prints a matrix.

# The factor of L*L', L lower triangular of rationals with a positive
# diagonal, is L itself, exactly: each pivot is the square of L's diagonal.
>> L = [2 0 0; 1 3 0; -1 1/2 1]
L = [2 0 0; 1 3 0; -1 1/2 1]

>> A = L*L'
A = L*L'

>> A
[ 4,   2,   -2;
  2,  10,  0.5;
 -2, 0.5, 2.25]

>> chol(A)
[ 2,   0, 0;
  1,   3, 0;
 -1, 0.5, 1]

>> chol(A) == L
1

>> frac chol([9/4 3/2; 3/2 5])
[3/2, 0;
   1, 2]

>> M[j<=12, k<=12] | j >= k = (j + k)/(j*k + 1)
M[j<=12, k<=12] | j >= k = (j + k)/(j*k + 1)

>> chol(M*M') == M
1

# A pivot that is not a rational's square has an inexact root, and so has
# every cell that reads it; the cells before it stay exact. sqrt(3 - 1).
>> chol([4 2; 2 3])
[2,           0;
 1, ~1.41421356]

# The second difference: sqrt(2), -1/sqrt(2), sqrt(3/2), -sqrt(2/3) and
# sqrt(4/3). Below the diagonal, 0 divided by sqrt(2) is inexact; above
# it, 0 is no cell's quotient.
>> chol([2 -1 0; -1 2 -1; 0 -1 2])
[  ~1.41421356,             0,           0;
 ~-0.707106781,   ~1.22474487,           0;
            ~0, ~-0.816496581, ~1.15470054]

>> chol(4)
2

>> chol(2)
~1.41421356

# Doubles are factored as doubles, the step's operations on them.
>> chol([4 2; 2 3]*~1)
[~2,           0;
 ~1, ~1.41421356]

# The upper triangle is not read, as LAPACK's and numpy's factors do not
# read it: a product rounded apart in its two triangles is factored as
# what its lower one says.
>> chol([4 100; 2 3])
[2,           0;
 1, ~1.41421356]

# Refused where a pivot is not above 0, as a singular matrix has no
# inverse: indefinite, singular, a negative number, and a pivot 0 at the
# second column of three.
>> chol([1 2; 2 1])
error: chol needs a positive definite matrix

>> chol([1 1; 1 1])
error: chol needs a positive definite matrix

>> chol(-4)
error: chol needs a positive definite matrix

>> chol([1 1 1; 1 1 1; 1 1 2])
error: chol needs a positive definite matrix

>> chol([1 2 3; 4 5 6])
error: chol takes A[j<=n, k<=n], not a 2x3 matrix

# A complex pivot is refused as any ordering of one is; a complex cell
# below the diagonal is factored as the recurrence takes it, L*L' = A
# with a quote that does not conjugate: i/2 and sqrt(3 + 1/4).
>> chol([i])
error: a comparison needs real numbers, not ~(i)

>> chol([4 0; i 3])
[       2,           0;
 ~(i*0.5), ~1.80277564]

# grad follows the definition: a root, products and a quotient, the
# guards reading places and a pivot's sign. L21 = a/2 and L22 = sqrt(3 -
# a^2/4), whose derivative at 2 is -1/(2 sqrt(2)).
>> grad_(a = 2) chol([4 a; a 3])
[  0,             0;
 0.5, ~-0.353553391]

>> grad_(a = 4) chol(a)
0.25

# log det A = 2 log L22 here, L11 being 2: d/da log(12 - a^2) at 2 is -4/8.
>> grad_(a = 2) 2*log(chol([4 a; a 3])[2,2])
~-0.5

# A square-root filter's time update, as the paper writes it.
>> S_n = chol(F*S_(n-1)*(F*S_(n-1))' + Q)
S_n = chol(F*S_(n-1)*(F*S_(n-1))' + Q)

>> tex ?S
S_n = \operatorname{chol}(F\,S_{n-1}\,(F\,S_{n-1})^\mathsf{T} + Q)

# README's section 2, its example once chol is in the prelude.
>> chol([4 2 -2; 2 10 1/2; -2 1/2 9/4])
[ 2,   0, 0;
  1,   3, 0;
 -1, 0.5, 1]

>> chol([4 2; 2 3])
[2,           0;
 1, ~1.41421356]

>> chol([1 2; 2 1])
error: chol needs a positive definite matrix

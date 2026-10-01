# Matrix literals, and the block expansion described in README.md section 2.

>> [1]
1

>> [1 2;3 4]
[1, 2;
 3, 4]

>> [1, 2; 3, 4]
[1, 2;
 3, 4]

>> [1; 2, 3]
[1, 0;
 2, 3]

# A matrix needs at least one element. These used to build a degenerate n x 0
# matrix and kill the process in the evaluator (DESIGN.md, C19).
>> []
error: a matrix needs at least one element

>> [;]
error: a matrix needs at least one element

>> a=[1 2;3 4]
a=[1 2;3 4]

>> a+a
[2, 4;
 6, 8]

>> a*a
[ 7, 10;
 15, 22]

>> a-a
[0, 0;
 0, 0]

>> 2*a
[2, 4;
 6, 8]

>> -a
[-1, -2;
 -3, -4]

# A single value stretches to the other side's size. It already did for '*'
# and inside a literal, where '[a; 1]' spreads the 1 across the block above
# it; '+', '-' and '/' reported 'these matrices have different sizes', so
# 'a*0.5' worked and 'a/2' did not (DESIGN.md, C38).
>> a/2
[0.5, 1;
 1.5, 2]

# A cell, written the way an array is indexed and numbered the way the rows
# and columns are: from one. The language had no way at all to read a value
# back out of a matrix (DESIGN.md, C40).
>> a[1,2]
2

>> a[2,1]
3

>> a[1,2]+a[2,1]
5

# The brackets bind to a name and nothing else, because a space between two
# blocks already means something: '[a [3 4]]' is one row of two blocks.
>> a[3,1]
error: row 3, column 1 is outside a 2x2 matrix

>> a[0,1]
error: row 0, column 1 is outside a 2x2 matrix

>> a[1]
error: a cell needs a row and a column, as 'm[1,2]'

>> a[1,1.5]
error: an index must be a whole number, not 1.5

# They follow whatever a name can be followed by, so the result of a call has
# cells too.
>> f(x)=[x, x^2]
f(x)=[x, x^2]

>> f(3)[1,2]
9

# Outside a matrix literal nothing is separated by juxtaposition, so the
# brackets index whatever is in front of them.
>> [1 2;3 4][2,1]
3

>> (a*a)[1,1]
7

# A quote transposes: the rows become the columns. It binds to what it follows
# before any operator does, as Julia's does, so 2*a' is 2*(a') and a^2' is
# a^(2'), which is a^2 (DESIGN.md, phase 14).
>> a'
[1, 3;
 2, 4]

>> [1 2 3]'
[1;
 2;
 3]

>> 2*a'
[2, 6;
 4, 8]

>> a'*[1; 1]
[4;
 6]

>> (a*a)'
[ 7, 15;
 10, 22]

>> a^2'
[ 7, 10;
 15, 22]

>> a''
[1, 2;
 3, 4]

>> a[1,2]'
2

>> [a' a']
[1, 3, 1, 3;
 2, 4, 2, 4]

# A subscript belongs to the name, as cell brackets do, so a quote after a
# term transposes the term, not its index.
>> r_0 = [1 2]
r_0 = [1 2]

>> r_n = r_(n-1)'
r_n = r_(n-1)'

>> r_1
[1;
 2]

>> r_2
[1, 2]

# So do cell brackets, whatever the index is written with.
>> q_n = r_n[1,1]
q_n = r_n[1,1]

>> q_1
1

# A single value is its own transpose.
>> 5'
5

# It does not conjugate: MATLAB's and Julia's quote is the conjugate
# transpose, which is the same only for real matrices.
>> [1 i]'
[1;
 i]

# A matrix defined by its cells: the brackets name the row and the column and
# bound them, which is the size, and the right-hand side is any cell. The names
# here are j and k because i is the imaginary unit (DESIGN.md, phase 14).
>> I[j<=2, k<=2] = j == k
I[j<=2, k<=2] = j == k

>> I
[1, 0;
 0, 1]

>> I[3,1]
error: row 3, column 1 is outside a 2x2 matrix

# The guard says which cells, the brackets how many. A cell no clause gives is
# 0, as a short row of a literal is padded.
>> U[j<=3, k<=3] | j <= k = 1
U[j<=3, k<=3] | j <= k = 1

>> U
[1, 1, 1;
 0, 1, 1;
 0, 0, 1]

# A size can come from a parameter.
>> H(n)[j<=n, k<=n] = 1/(j+k-1)
H(n)[j<=n, k<=n] = 1/(j+k-1)

>> frac H(3)
[  1, 1/2, 1/3;
 1/2, 1/3, 1/4;
 1/3, 1/4, 1/5]

# Numbers where the names were: one cell, which beats the general clause, as a
# base clause beats a sequence's, whichever was written first.
>> J[1,2] = 5
J[1,2] = 5

>> J
error: J has no size; write it as J[j<=rows, k<=cols]

>> J[j<=2, k<=2] = j == k
J[j<=2, k<=2] = j == k

>> J
[1, 5;
 0, 1]

# A size is written, the clauses of one matrix agree on it, and a cell is a
# single value.
>> M[r,c] = r + c
error: M has no size; write it as M[r<=rows, c<=cols]

>> V[j<=2, k<=2] | j == k = 1
V[j<=2, k<=2] | j == k = 1

>> V[j<=3, k<=3] = 2
V[j<=3, k<=3] = 2

>> V
error: the clauses of V give it different sizes

>> W[j<=2, k<=2] = [j k]
W[j<=2, k<=2] = [j k]

>> W
error: a cell of W must be a single value, not a 1x2 matrix

# A clause for one cell overrides that cell of a matrix written whole and leaves
# the rest, as a base clause overrides a sequence's general one at one index.
# The whole matrix is the size, and every cell no other clause gives.
>> T = [1 2; 3 4]
T = [1 2; 3 4]

>> T[1,1] = 9
T[1,1] = 9

>> T
[9, 2;
 3, 4]

# So does a clause for all cells, where its guard holds.
>> T[j<=2, k<=2] | j < k = 0
T[j<=2, k<=2] | j < k = 0

>> T
[9, 0;
 3, 4]

>> T[j<=3, k<=3] | j == k = 7
T[j<=3, k<=3] | j == k = 7

>> T
error: the clauses of T give it different sizes

# Writing the whole matrix again starts it over, as it does a sequence.
>> T = [1 2; 3 4]
T = [1 2; 3 4]

>> T
[1, 2;
 3, 4]

>> T[3,1] = 5
T[3,1] = 5

>> T
error: row 3, column 1 is outside a 2x2 matrix

# Inside one they index a name and nothing else, because there a space
# between two blocks already means something.
>> [[1 2] [3 4]]
[1, 2, 3, 4]

>> [a[1,1], a[2,2]]
[1, 4]

# Which leaves one form that changed: a row of blocks whose second block
# follows a name with a space. Write it with the comma README.md uses.
>> [a [3 4]]
error: a cell needs a row and a column, as 'm[1,2]'

>> [a, [3 4;5 6]]
[1, 2, 3, 4;
 3, 4, 5, 6]

>> a-1
[0, 1;
 2, 3]

# The order is kept, which is what makes this more than a convenience.
>> 1-a
[ 0, -1;
 -2, -3]

>> 2/a
[           2,   1;
 ~0.666666667, 0.5]

# Three or more rows or columns. The block offsets were a prefix sum that
# added only the previous element instead of the running total, so the result
# was allocated too small and every one of these reported 'Out of matrix
# range.' Correct for two blocks, which is every size the tests used to have
# (DESIGN.md, C22).
>> [1 2 3]
[1, 2, 3]

>> [1;2;3]
[1;
 2;
 3]

>> [1 2 3;4 5 6]
[1, 2, 3;
 4, 5, 6]

>> [a, a, a]
[1, 2, 1, 2, 1, 2;
 3, 4, 3, 4, 3, 4]

>> [a; a; a]
[1, 2;
 3, 4;
 1, 2;
 3, 4;
 1, 2;
 3, 4]

>> [a, a; a, a; a, a]
[1, 2, 1, 2;
 3, 4, 3, 4;
 1, 2, 1, 2;
 3, 4, 3, 4;
 1, 2, 1, 2;
 3, 4, 3, 4]

# A cell smaller than its block is extended to fill it.
>> [a, 1, a]
[1, 2, 1, 1, 2;
 3, 4, 1, 3, 4]

# A matrix of expressions expands to the size of what its cells evaluate to.
>> [a, a; a, a]
[1, 2, 1, 2;
 3, 4, 3, 4;
 1, 2, 1, 2;
 3, 4, 3, 4]

>> [a; a]
[1, 2;
 3, 4;
 1, 2;
 3, 4]

# A single value stretches to fill the band it sits in, which is what makes
# '[a, 1]' mean what it looks like.
>> [a, 1]
[1, 2, 1;
 3, 4, 1]

>> [a, 0]
[1, 2, 0;
 3, 4, 0]

# RECORDED AS IT IS, NOT AS IT SHOULD BE (DESIGN.md, C41). A block
# continues with its last value, which for a single value is the stretch above
# and reads as an ellipsis -- '[a, 0]' pads the band with zeros. For a larger
# block it repeats a corner instead: the second row under '[3 4]' is '4 4' and
# the second row under '[1 2 3]' is '3 3 3', which nobody wrote. Kept while it
# may still be the residue of an idea, and recorded so it cannot change in
# silence.
>> [a, [3 4]]
[1, 2, 3, 4;
 3, 4, 4, 4]

>> [[1 2 3], a]
[1, 2, 3, 1, 2;
 3, 3, 3, 3, 4]

# A matrix power is repeated multiplication. It used to square the
# accumulator, so 'a^n' computed 'a^(2^(n-1))' -- right at 1 and 2 and wrong
# everywhere else, with a^0 returning a (DESIGN.md, C23).
>> s=[1 1;0 1]
s=[1 1;0 1]

>> s^0
[1, 0;
 0, 1]

>> s^3
[1, 3;
 0, 1]

>> a^3
[37,  54;
 81, 118]

# A negative power is a power of the inverse, which a matrix of exact numbers
# has exactly; a singular one has none, as an exact zero has no reciprocal.
# There is no root, and only a square matrix has a power.
>> a^-1
[ -2,    1;
 1.5, -0.5]

>> a^-1*a == [1 0;0 1]
1

# The inverse times a matrix of exact numbers is solved for rather than
# inverted and multiplied, and answers as the product did, errors and all
# (DESIGN.md, next in line).
>> frac [1 2;3 4]^-1*[1;1]
[-1;
  1]

>> [1 2;2 4]^-1*[1;1]
error: a singular matrix has no inverse

>> [1 2;3 4]^-1*[1;1;1]
error: a matrix product needs as many columns on the left as rows on the right

>> frac a^-2
[ 11/2, -5/2;
 -15/4,  7/4]

>> s^-3
[1, -3;
 0,  1]

# An inexact exponent makes the power inexact, as it does a number's.
>> frac s^~2
error: 1 was approximated, so it has no exact fraction

# A large power ends: it multiplied once per unit of the exponent, which is
# two billion products here.
>> s^2147483647
[1, 2147483647;
 0,          1]

>> s^-2147483648
[1, -2147483648;
 0,           1]

>> [1 2;2 4]^-1
error: a singular matrix has no inverse

# A pivot is chosen by its size as a double, and a nonzero one too small for
# a double is still not zero: this was called singular.
>> [0,1;1/10^400,0]^-1 == [0,10^400;1,0]
1

>> s^(2+1/10^999)
error: a matrix power must be a whole number, not ~2

>> a^0.5
error: a matrix power must be a whole number, not 0.5

>> s^(2^40)
error: a matrix power must be between -2147483648 and 2147483647, not 1099511627776

>> [1 2 3]^2
error: only a square matrix has a power

>> 2^[1 2]
error: a matrix cannot be an exponent

# The rest of the matrix diagnostics, which used to be in the 2014 voice --
# capitalised, punctuated, newline-terminated, and one of them misspelled
# 'assigmentation' (DESIGN.md, D13).
>> [1 2]+[1 2 3]
error: these matrices have different sizes

>> [1 2;3 4]*[1 2 3]
error: a matrix product needs as many columns on the left as rows on the right

# Fibonacci in closed form, which needs the power and the cell together, and
# no base clause because a closed form has nothing to recur on.
>> fib_n=([1 1;1 0]^n)[1,2]
fib_n=([1 1;1 0]^n)[1,2]

>> fib_10
55

>> fib_0
0

# A matrix as an argument: two of the three ideas meeting, which the corpus
# had never exercised. With cells the 2x2 formulas are writable.
>> det(m)=m[1,1]*m[2,2]-m[1,2]*m[2,1]
det(m)=m[1,1]*m[2,2]-m[1,2]*m[2,1]

>> det(a)
-2

>> solve(m,b)=[(b[1,1]*m[2,2]-b[2,1]*m[1,2])/det(m); (m[1,1]*b[2,1]-m[2,1]*b[1,1])/det(m)]
solve(m,b)=[(b[1,1]*m[2,2]-b[2,1]*m[1,2])/det(m); (m[1,1]*b[2,1]-m[2,1]*b[1,1])/det(m)]

>> solve(a,[5;11])
[1;
 2]

# The answer checks itself: multiplying it back gives the right-hand side.
>> a*solve(a,[5;11])
[ 5;
 11]

>> !a
error: a matrix has no factorial

>> mm_n=[1 2;3 4]*(0.5)^n
mm_n=[1 2;3 4]*(0.5)^n

# 'lim' compares successive terms, and matrices have no size to compare. The
# message used to be the bare 'a matrix has no absolute value', which reads
# like an internal error and does not say what the interpreter was doing
# (DESIGN.md, C39).
>> lim mm
error: mm has no limit: a matrix has no absolute value


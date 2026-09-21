# Definitions in cases (MODERNIZATION.md, phase 10).
#
# A clause may carry a GUARD, written between the left-hand side and the '=',
# the way set-builder notation writes "such that":
#
#     name(parameters)_index | condition = body
#
# Guarded clauses are tried in the order they were written and the first whose
# guard holds is evaluated. The clause not chosen is not evaluated, which is
# what index dispatch has always done: a guard is dispatch with a condition
# instead of an index, so it needs no new evaluation rule and no laziness
# anywhere else (MODERNIZATION.md, Openings).
#
# Four decisions, each because the alternative was worse:
#
#   '==' for equality, not '='. 'f_n | n = 0 = 1' puts two '=' on one line
#   doing two different jobs -- one asks, one tells -- and a reader stumbles.
#
#   '<>' for inequality, not '!='. '!' is the prefix factorial, so '!=' would
#   need a disambiguation rule that nobody reading the line can see.
#
#   A guard is more specific than an index, an index more specific than
#   neither: guarded clauses first in written order, then the base clause for
#   that index, then the unguarded general clause. This keeps README.md
#   section 4's rule that a base case beats the general clause whatever the
#   order of definition.
#
#   No 'otherwise' clause. A plain unguarded definition still replaces the
#   whole definition (C11), so a default would have to be a second reserved
#   word; the complement is usually one character -- 'x < 0' and 'x >= 0' --
#   and when nothing applies the interpreter says so.
#
# Comparisons answer 1 and 0. There is no truth type because every value here
# is a number, and a guard holds when it is not zero.

# --- comparisons are numbers ------------------------------------------------

>> 3 < 5
1

>> 5 < 3
0

>> 2 == 2
1

>> 2 <> 2
0

>> 1 <= 1
1

>> 2 >= 3
0

# Ordering needs real numbers, as the factorial does (MODERNIZATION.md, C42).
>> i < 1
error: a comparison needs real numbers, not i

# Equality does not.
>> i == i
1

# A comparison needs single values. Cell by cell was considered and left out:
# it would answer with a matrix of ones and zeros that nothing in the language
# can reduce to a single truth, so it would invite an idiom it cannot finish.
>> [1 2] < 3
error: a comparison needs single values, not a 1x2 matrix

# --- a definition in cases --------------------------------------------------

# Absolute value, written the way a textbook writes it.
>> abs(x) | x < 0 = 0-x
abs(x) | x < 0 = 0-x

>> abs(x) | x >= 0 = x
abs(x) | x >= 0 = x

>> abs(0-3)
3

>> abs(3)
3

>> abs(0)
0

# '?' prints the clauses back in the order they were written, guards and all.
>> ?abs
abs(x) | x < 0 = 0-x
abs(x) | x >= 0 = x

# Three cases, and the middle one is the reason a comparison is not enough on
# its own: it has to be asked before the others are.
>> sign(x) | x == 0 = 0
sign(x) | x == 0 = 0

>> sign(x) | x < 0 = 0-1
sign(x) | x < 0 = 0-1

>> sign(x) | x > 0 = 1
sign(x) | x > 0 = 1

>> sign(0-7)
-1

>> sign(0)
0

# A comparison is a number, so the same function has a branchless form. It
# evaluates every case; the guarded one above evaluates the case it chose.
>> sgn(x) = (x>0) - (x<0)
sgn(x) = (x>0) - (x<0)

>> sgn(0-7)
-1

>> max(a,b) | a > b = a
max(a,b) | a > b = a

>> max(a,b) | a <= b = b
max(a,b) | a <= b = b

>> max(3,7)
7

# The guard is what makes a removable singularity writable: the quotient is
# x+2 everywhere except at 2, where it is 0/0 and there is nothing to divide.
>> q(x) | x == 2 = 4
q(x) | x == 2 = 4

>> q(x) | x <> 2 = (x^2-4)/(x-2)
q(x) | x <> 2 = (x^2-4)/(x-2)

>> q(3)
5

>> q(2)
4

# Nothing applies, and saying so is the whole price of not checking
# exhaustiveness: no language can check it over arbitrary conditions.
>> h(x) | x > 0 = 1
h(x) | x > 0 = 1

>> h(5)
1

>> h(0-1)
error: no clause of h applies

# A guard is any expression, true when it is not zero: a comparison is only
# the usual way to write one.
>> nonzero(x) | x = 1
nonzero(x) | x = 1

>> nonzero(x) | x == 0 = 0
nonzero(x) | x == 0 = 0

>> nonzero(5)
1

>> nonzero(0)
0

# NaN is not zero, so it passes a bare guard, while every comparison with it
# is false. Recorded because it is the one place the convention bites.
>> nonzero(0/0)
1

# A guard needs a single value, for the same reason an index does.
>> nonzero([1 2])
error: a guard needs a single value, not a 1x2 matrix

# --- a recurrence in cases --------------------------------------------------

# Pascal's rule. Its base case sits at an index the parameter decides, which
# is exactly what cannot be written today -- 'binom' has to go through
# factorials instead (MODERNIZATION.md, Openings).
>> c(k)_n | k < 0 = 0
c(k)_n | k < 0 = 0

>> c(k)_n | k > n = 0
c(k)_n | k > n = 0

>> c(k)_0 = 1
c(k)_0 = 1

>> c(k)_n = c(k-1)_(n-1) + c(k)_(n-1)
c(k)_n = c(k-1)_(n-1) + c(k)_(n-1)

>> c(2)_5
10

>> c(5)_10
252

>> [c(0)_4, c(1)_4, c(2)_4, c(3)_4, c(4)_4]
1 4 6 4 1

# The guard is tried before the base clause, which is what makes the first of
# these zero and the second one.
>> c(3)_0
0

>> c(0)_0
1

# A recurrence that stops itself: 'lim' written out by hand, which is worth
# having because lim's tolerance cannot be reached from the prompt
# (MODERNIZATION.md, Deferred). The tolerance here is deliberately coarse, so
# that the answer is one the unguarded recurrence would never give: Newton
# reaches 1.41421356 and this stops at the second iterate. The base clause is
# written first on purpose -- the guard reads the previous term, so at index
# zero it must not be reached.
>> root_0 = 1
root_0 = 1

>> root_n | abs(root_(n-1)^2 - 2) < 0.01 = root_(n-1)
root_n | abs(root_(n-1)^2 - 2) < 0.01 = root_(n-1)

>> root_n = (root_(n-1) + 2/root_(n-1))/2
root_n = (root_(n-1) + 2/root_(n-1))/2

>> root_6
1.41666667

>> root_50
1.41666667

# Index clauses and guards are one mechanism, written two ways.
>> fib_0 = 0
fib_0 = 0

>> fib_1 = 1
fib_1 = 1

>> fib_n = fib_(n-1) + fib_(n-2)
fib_n = fib_(n-1) + fib_(n-2)

>> fib_10
55

>> g_n | n == 0 = 0
g_n | n == 0 = 0

>> g_n | n == 1 = 1
g_n | n == 1 = 1

>> g_n = g_(n-1) + g_(n-2)
g_n = g_(n-1) + g_(n-2)

>> g_10
55

# --- a matrix in cases ------------------------------------------------------

# The Kronecker delta, and the identity matrix it defines.
>> d(row,col) | row == col = 1
d(row,col) | row == col = 1

>> d(row,col) | row <> col = 0
d(row,col) | row <> col = 0

>> [d(1,1), d(1,2); d(2,1), d(2,2)]
1 0
0 1

# The second-difference matrix, the way a numerical analysis course writes it:
# two on the diagonal, minus one beside it, zero further out. Three clauses
# and nine calls, and the matrix says what it means.
>> t(row,col) | row == col = 2
t(row,col) | row == col = 2

>> t(row,col) | abs(row-col) == 1 = 0-1
t(row,col) | abs(row-col) == 1 = 0-1

>> t(row,col) | abs(row-col) > 1 = 0
t(row,col) | abs(row-col) > 1 = 0

>> [t(1,1), t(1,2), t(1,3); t(2,1), t(2,2), t(2,3); t(3,1), t(3,2), t(3,3)]
2 -1 0
-1 2 -1
0 -1 2


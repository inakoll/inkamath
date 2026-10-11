# Definitions in cases (DESIGN.md, phase 10).
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
# anywhere else (DESIGN.md, Openings).
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
#   No 'otherwise' keyword. The unguarded clause for every call is the
#   default, tried after the guards wherever it is written, so a paper's
#   "otherwise" is that clause written last; and when nothing applies the
#   interpreter says so.
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

# Ordering needs real numbers, as the factorial does (DESIGN.md, C42).
>> i < 1
error: a comparison needs real numbers, not ~(i)

# Equality does not.
>> i == i
~1

# A comparison needs single values. Cell by cell was considered and left out:
# it would answer with a matrix of ones and zeros that nothing in the language
# can reduce to a single truth, so it would invite an idiom it cannot finish.
>> [1 2] < 3
error: a comparison needs single values, not a 1x2 matrix

# Equality is not cell by cell: two whole matrices are equal or not, which is
# one truth. So a matrix read back can be checked against what printed it, and
# two of different shapes are simply unequal, as a matrix and a number are.
>> [1 2;3 4]/3 == [1/3, 2/3; 1, 4/3]
1

>> [1 2;3 4]/3 == [~0.333333333, ~0.666666667; 1, ~1.33333333]
~0

>> [1 2] <> [2 1]
1

>> [1 2] == [1 2 3]
0

>> [1 1] == 1
0

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

# An unguarded clause that would answer every call -- a plain definition, or
# the general clause -- is the definition's default, and is tried after the
# guarded ones wherever it was written. That is what lets a definition be
# patched up: the cases can be added as an afterthought.
>> u(x) = 0
u(x) = 0

>> u(x) | x > 0 = 1
u(x) | x > 0 = 1

>> u(5)
1

>> u(0-5)
0

# A sequence is patched the same way, because its general clause is a default
# too.
>> p_0 = 1
p_0 = 1

>> p_n = p_(n-1)+1
p_n = p_(n-1)+1

>> p_n | n > 3 = 99
p_n | n > 3 = 99

>> p_5
99

>> p_2
3

# Re-typing a clause replaces that clause and leaves it where it stands.
# Order is what dispatch follows, so a clause that moved would answer
# differently: this one used to be appended, and re-entering 'root_0 = 1'
# unchanged put the base clause behind the guard that reads the term before
# it, which then recursed to the depth budget (DESIGN.md, C45).
>> w_0 = 1
w_0 = 1

>> w_n | n > 2 = 0
w_n | n > 2 = 0

>> w_n = w_(n-1)+1
w_n = w_(n-1)+1

>> w_2
3

>> w_0 = 10
w_0 = 10

>> ?w
w_0 = 10
w_n | n > 2 = 0
w_n = w_(n-1)+1

>> w_2
12

# A guarded clause is replaced by writing its left-hand side again. It used
# to be appended, so the old clause stayed in front of the new one and the
# correction never took effect (DESIGN.md, C46).
>> y(x) | x > 0 = 1
y(x) | x > 0 = 1

>> y(x) | x > 0 = 2
y(x) | x > 0 = 2

>> ?y
y(x) | x > 0 = 2

>> y(5)
2

# A clause is named by its left-hand side as the tokens spell it, and the
# tokens have to stay apart: joined without a separator, '[1 2][1,1]' and
# '[12][1,1]' are one name, so writing the second clause replaced the first
# (DESIGN.md, C55).
>> sg(x) | x < [1 2][1,1] = 10
sg(x) | x < [1 2][1,1] = 10

>> sg(x) | x < [12][1,1] = 20
sg(x) | x < [12][1,1] = 20

>> ?sg
sg(x) | x < [1 2][1,1] = 10
sg(x) | x < [12][1,1] = 20

>> sg(0)
10

# So does a space that begins the next element: '[1 -2]' is two elements and
# '[1 - 2]' one, '[pi [1]]' two and '[pi[1]]' one, so each pair is two
# names (C159).
>> sn(x) | x == [1 -2][1,1] = 10
sn(x) | x == [1 -2][1,1] = 10

>> sn(x) | x == [1 - 2][1,1] = 20
sn(x) | x == [1 - 2][1,1] = 20

>> ?sn
sn(x) | x == [1 -2][1,1] = 10
sn(x) | x == [1 - 2][1,1] = 20

>> sn(1)
10

>> sv(x) | x == [pi [1]][1,1] = 10
sv(x) | x == [pi [1]][1,1] = 10

>> sv(x) | x == [pi[1]][1,1] = 20
sv(x) | x == [pi[1]][1,1] = 20

>> ?sv
sv(x) | x == [pi [1]][1,1] = 10
sv(x) | x == [pi[1]][1,1] = 20

# A guard that does not hold must leave nothing behind. The index was bound
# before the guard was tested and never removed, so a rejected clause shadowed
# a global, clobbered an argument, and poisoned the guards written after it
# (DESIGN.md, C53).
>> nn = 7
nn = 7

>> rg_nn | nn > 5 = 100
rg_nn | nn > 5 = 100

>> rg_0 = nn
rg_0 = nn

>> rg_0
7

>> ar(x)_x | x > 5 = 1
ar(x)_x | x > 5 = 1

>> ar(x)_0 = x
ar(x)_0 = x

>> ar(7)_0
7

>> cg = 10
cg = 10

>> pg_cg | cg > 100 = 1
pg_cg | cg > 100 = 1

>> pg_0 | cg > 5 = 2
pg_0 | cg > 5 = 2

>> pg_0
2

# Nor a local the guard binds: the clause taken reads what the frame held
# before it, under grad as well (C300).
>> r(x) | (lu = x) > 5 = 1
r(x) | (lu = x) > 5 = 1

>> r(x) = lu
r(x) = lu

>> r(1)
error: lu is not defined

>> lu = 4
lu = 4

>> r(1)
4

>> clear lu
clear lu

>> rp(x) | (x = 9) > 10 = 1
rp(x) | (x = 9) > 10 = 1

>> rp(x) = x
rp(x) = x

>> rp(2)
2

>> rs_n | (lu = n) > 5 = 1
rs_n | (lu = n) > 5 = 1

>> rs_n = lu
rs_n = lu

>> rs_2
error: lu is not defined

>> rd[1] | (lu = 3) > 5 = 1
rd[1] | (lu = 3) > 5 = 1

>> rd[j<=2] | (lu = j) > 5 = 1
rd[j<=2] | (lu = j) > 5 = 1

>> rd[j<=2] = lu
rd[j<=2] = lu

>> rd
error: lu is not defined

>> hh(y) = y
hh(y) = y

>> lg(x) | hh(0 + (lu = 3)) > 5 = 1
lg(x) | hh(0 + (lu = 3)) > 5 = 1

>> lg(x) = lu*x
lg(x) = lu*x

>> grad_(x = 1) lg(x)
error: lu is not defined

# Every clause of a definition shares its parameters, because a call binds
# them once, for whichever clause ends up answering. A clause that names them
# differently could never be called correctly -- the argument would be bound
# under the other clause's name and the body would read a global instead -- so
# it is refused rather than accepted and left unreachable
# (DESIGN.md, C51).
>> pick(x) | x < 0 = 0-x
pick(x) | x < 0 = 0-x

>> pick(z) | 1 = z
error: pick takes (x), so a clause cannot take (z); write 'clear pick' first

>> pick(x, z) | 1 = x+z
error: pick takes (x), so a clause cannot take (x, z); write 'clear pick' first

>> pick(x) | 1 = x
pick(x) | 1 = x

>> pick(0-3)
3

# A base clause answers for one index rather than for every call, so it is
# not a default and keeps its place in written order. A guard added after it
# could never apply, and saying so beats doing nothing.
>> v_0 = 1
v_0 = 1

>> v_0 | 1 = 7
error: v_0 is already defined without a guard, so this clause can never apply

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

# NaN is not a number, so it is no truth, and a guard reading it is refused.
# It passed, as not zero, while every comparison with it was false: this
# answered 1, recorded as the one place the convention bit (DESIGN.md, a NaN
# reaches every term that reads it). The zero is inexact because an exact one
# cannot be divided by (phase 13).
>> nonzero(0/~0)
error: a guard needs a number, not -nan

# A guard needs a single value, for the same reason an index does.
>> nonzero([1 2])
error: nonzero needs single values, not a 1x2 matrix; write it by its cells

# A sequence's guard says it in a guard's words, as an 'and' says it in its
# own: this read "a guard needs a single value".
>> gm_n | [1 n] = 1
gm_n | [1 n] = 1

>> gm_n = 0
gm_n = 0

>> gm_2
error: a guard needs single values, not a 1x2 matrix

# --- a recurrence in cases --------------------------------------------------

# Pascal's rule. Its base case sits at an index the parameter decides, which
# is exactly what cannot be written today -- 'binom' has to go through
# factorials instead (DESIGN.md, Openings).
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
[1, 4, 6, 4, 1]

# The guard is tried before the base clause, which is what makes the first of
# these zero and the second one.
>> c(3)_0
0

>> c(0)_0
1

# A recurrence that stops itself: 'lim' written out by hand, which is worth
# having because lim's tolerance cannot be reached from the prompt
# (DESIGN.md, Deferred). The tolerance here is deliberately coarse, so
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
~1.41666667

>> root_50
~1.41666667

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
[1, 0;
 0, 1]

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
[ 2, -1,  0;
 -1,  2, -1;
  0, -1,  2]

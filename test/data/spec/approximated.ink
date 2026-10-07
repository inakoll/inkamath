# A truth read from a number approximated past a thousand digits (DESIGN.md,
# next in line). A comparison, 'and' or 'or' that reads one answers 1 or 0
# approximated, a double as what it read is; and a clause a guard that read
# one chose, or a refusal it decided, says so, since the guard may have
# decided wrongly. The interpreter carries no bound on how far an
# approximated number is from its exact value, so a comparison it decides
# rightly is marked as much as one it decides wrongly.
>> rt_0 = 1
rt_0 = 1

>> rt_n = (rt_(n-1) + 2/rt_(n-1))/2
rt_n = (rt_(n-1) + 2/rt_(n-1))/2

>> c = 14142135623730950488/10^19
c = 14142135623730950488/10^19

# rt_20 is above the square root of 2, and c below it, so rt_20 > c is 1.
# Approximated at the twelfth step, rt_20 is the double one unit below
# 1.4142135623730951, the double nearest c, and the comparison says 0.
>> rt_20 > c
0  # approximated past a thousand digits

>> rt_20 < c
1  # approximated past a thousand digits

>> rt_20 == c
0  # approximated past a thousand digits

>> rt_20 <> c
1  # approximated past a thousand digits

# Decided rightly, and marked all the same.
>> rt_20 > 1
1  # approximated past a thousand digits

# Still exact at the eleventh step, and so a truth exactly.
>> rt_11 > c
1

# Inexact by '~', which was never exact to lose, is not marked, wrong as it
# is here: the double nearest rt_11 is the double nearest c.
>> ~rt_11 > c
0

# '~' of what was approximated keeps the mark, and so does what reads it.
>> ~rt_20 > c
0  # approximated past a thousand digits

# Far from its threshold, and on the wrong side of it: (rt_20 - c)*10^16 is
# about 1.7e-5, and -2.22 approximated. No gap tells a safe comparison.
>> (rt_20 - c)*10^16 > -1
0  # approximated past a thousand digits

# Which is also why a root, inexact by nature, keeps the mark of what it
# reads: this one is about 4.1e-11, real.
>> (rt_20 - c)^(1/2)
~9.12432968e-25+i*~1.49011612e-08  # approximated past a thousand digits

>> (rt_11 - c)^(1/2)
~4.10940897e-11

# 'and' and 'or' are approximated where a side they read is; a side they do
# not read cannot mark them.
>> rt_20 > 1 and 1 > 0
1  # approximated past a thousand digits

>> 1 > 0 and rt_20 > 1
1  # approximated past a thousand digits

>> 0 > 1 and rt_20 > 1
0

>> 1 > 0 or rt_20 > c
1

>> rt_20 > c or 1 > 0
1  # approximated past a thousand digits

# Whole matrices too.
>> [1 rt_20] == [1 rt_20]
1  # approximated past a thousand digits

>> [1 rt_11] == [1 rt_11]
1

# A truth approximated is a double, and what is computed from it is
# approximated as from any other.
>> (rt_20 > 1)*(1/3)
~0.333333333  # approximated past a thousand digits

>> sgn(x) = (x > 0) - (x < 0)
sgn(x) = (x > 0) - (x < 0)

>> sgn(rt_11 - c)
1

>> sgn(rt_20 - c)
-1  # approximated past a thousand digits

# A count of truths, remembered term by term and read again: the nine terms
# past the bound count none of the nine they should.
>> cnt_0 = 0
cnt_0 = 0

>> cnt_n = cnt_(n-1) + (rt_n > c)
cnt_n = cnt_(n-1) + (rt_n > c)

>> cnt_11
11

>> cnt_20
11  # approximated past a thousand digits

>> cnt_20 - cnt_11
0  # approximated past a thousand digits

# It has no exact fraction, and is no index.
>> frac rt_20 > 1
error: 1 was approximated past a thousand digits, so it has no exact fraction

>> u_n = n
u_n = n

>> u_(rt_20 > 1)
error: an index must be exact, and 1 was approximated

>> sum_(k=1)^(rt_20 > 1) k
error: an index must be exact, and 1 was approximated

# A guard that reads an approximated number chooses its clause by an
# approximated truth: the answer is approximated, whether the guard held
# or the clause chosen is one after it. g(rt_20) is 1 exactly.
>> g(x) = 2
g(x) = 2

>> g(x) | x > c = 1
g(x) | x > c = 1

>> g(rt_20)
2  # approximated past a thousand digits

>> g(rt_11)
1

>> f(x) = 2
f(x) = 2

>> f(x) | x > 1 = 1
f(x) | x > 1 = 1

>> f(rt_20)
1  # approximated past a thousand digits

>> frac g(rt_20)
error: 2 was approximated past a thousand digits, so it has no exact fraction

# A guard that is never read cannot mark the answer.
>> p(x) = 3
p(x) = 3

>> p(x) | 1 > 0 = 5
p(x) | 1 > 0 = 5

>> p(x) | x > c = 6
p(x) | x > c = 6

>> p(rt_20)
5

# A refusal it decided says so.
>> h(x) | x > c = 1
h(x) | x > c = 1

>> h(rt_20)
error: no clause of h applies, by a guard approximated past a thousand digits

>> h(1)
error: no clause of h applies

>> hs_n | rt_n > c = n
hs_n | rt_n > c = n

>> hs_11
11

>> hs_20
error: no clause of hs applies, by a guard approximated past a thousand digits

# Cell by cell, each cell by the guards it read.
>> M[j<=2, k<=2] = j
M[j<=2, k<=2] = j

>> M[j<=2, k<=2] | j == 1 and rt_20 > c = 7
M[j<=2, k<=2] | j == 1 and rt_20 > c = 7

>> M
[1, 1;
 2, 2]  # approximated past a thousand digits

>> M[1,2]
1  # approximated past a thousand digits

>> M[2,1]
2

# grad differentiates the clause chosen, and says so: the slope at rt_20
# is 1.
>> gl(x) = 2*x
gl(x) = 2*x

>> gl(x) | x > c = x
gl(x) | x > c = x

>> grad_(x = rt_20) gl(x)
2  # approximated past a thousand digits

>> grad_(x = rt_11) gl(x)
1

# A part the clause chosen lacks is 0 approximated, and a refusal says so.
>> grad_(x = rt_20) g(x)
0  # approximated past a thousand digits

>> grad_(x = rt_20) h(x)
error: no clause of h applies, by a guard approximated past a thousand digits

>> grad_(x = rt_20) x*(x > c and 1 > 0)
0  # approximated past a thousand digits

# The prelude's guards. exp past a thousand reads its argument, here 2^4000
# approximated to inf.
>> exp(2^4000)
inf  # approximated past a thousand digits

>> max(rt_20, 3)
3  # approximated past a thousand digits

>> max(rt_11, 3)
3

# log folds its mantissa by a square it rounds first, inexact by '~', so an
# exact argument whose square passes the thousand digits is not marked;
# ilogb takes a threshold below 2^-3321 as passed, as every number it is
# given is above it, rather than reading 2^-3584 approximated to 0, so that
# the answer it chose stays exact and the thresholds after it too.
>> log(10^999)
~2300.28251

>> ilogb(1/10^950)
-3156

>> log(1/10^950)
~-2187.45584

>> tex ?ilogbs
\operatorname{ilogbs}(x, k, s) = \begin{cases} k & \text{if } k + s > 3321 \\ k + s & \text{if } k + s < -3321 \\ k + s & \text{if } x \ge 2^{k + s} \\ k & \text{otherwise} \end{cases}

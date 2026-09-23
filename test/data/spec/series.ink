# A specification, not a recording: what 'sum' and 'prod' should answer, written
# before they exist (CLAUDE.md, section 3). The notation is the one on paper,
# spelled with the language's own '_' and '^'.

# The index runs from the lower bound to the upper, both included.
>> sum_(k=1)^10 k
55

>> prod_(k=1)^5 k
120

>> sum_(k=1)^4 k^2
30

# The body runs to the next '+' or '-', as it does on paper: a sum is a term.
>> sum_(k=1)^3 k + 100
106

>> sum_(k=1)^3 k*2
12

>> 2*sum_(k=1)^3 k
12

>> sum_(k=1)^3 -k - 1
-7

>> sum_(k=1)^3 (k+1)
9

# A bound is any expression; the upper one is a number, a name, or something
# in parentheses, and a name there is never a call: 'n (k+1)' is the bound n
# and the body (k+1).
>> n=4
n=4

>> sum_(k=1)^n k
10

>> sum_(k=1)^(n+1) k
15

>> sum_(k=n)^(2*n) 1
5

>> sum_(k=1)^n (k+1)
14

# An empty sum is 0 and an empty product 1.
>> sum_(k=1)^0 k
0

>> prod_(k=1)^0 k
1

>> sum_(k=5)^1 k
0

# The index is bound for the body and nowhere else: it neither reads nor
# changes a name outside. The bounds are evaluated before it is bound, so the
# upper n below is the n above.
>> k=100
k=100

>> sum_(k=1)^3 k
6

>> k
100

>> sum_(n=1)^n n
10

# The body sees everything around it: a global, a parameter, an outer index.
>> x=2
x=2

>> sum_(k=0)^3 x^k
15

>> sum_(m=1)^3 sum_(j=1)^m m*j
25

>> h(k) = sum_(j=1)^3 k
h(k) = sum_(j=1)^3 k

>> h(5)
15

# Which is what the exponential series was waiting for: no base clause, and no
# term written twice.
>> ex(x)_n = sum_(k=0)^n x^k/!k
ex(x)_n = sum_(k=0)^n x^k/!k

>> ex(1)_10
2.7182818

>> lim ex(1)
2.71828183

# Matrices sum cell by cell, and multiply in order: the first factor is on the
# left. [k 1;0 1] for k=1 then k=2 is [2 2;0 1]; the other way it is [2 3;0 1].
>> sum_(k=1)^3 [k, k^2]
[6, 14]

>> prod_(k=1)^2 [k 1;0 1]
[2, 2;
 0, 1]

# In a matrix a sum is a cell like any other: the cell ends where the next
# token starts a new expression, and a leading sign does not start one, so
# '-1' below is subtracted from the sum, as '[0 -1]' is from 0 (C52).
>> [sum_(k=1)^3 k 1]
[6, 1]

>> [sum_(k=1)^3 k*2 1]
[12, 1]

>> [sum_(k=1)^3 k -1]
5

# Without an upper bound the series is summed to its limit, by the rule 'lim'
# follows, and reported the same way when it does not get there.
>> sum_(k=0) 1/!k
2.71828183

>> sum_(k=0) 1/2^k
2

>> prod_(k=1) (1+1/2^k)
2.38423103

>> sum_(k=1) k
error: the sum did not converge within 100 terms (last partial sum 5050)

# Converging and being summed are not the same thing: 1/k^2 converges, too
# slowly for a hundred terms to show it -- README.md section 4, and C36.
>> sum_(k=1) 1/k^2
error: the sum did not converge within 100 terms (last partial sum 1.6349839)

# A term is a step, whatever its body: a sum that reads no name still has to
# end, and ends where any other evaluation does.
>> sum_(k=1)^(10^7) 1
error: evaluation gave up after 1000000 steps

# A sum where no call is running -- the index of a definition's left-hand side
# is evaluated as the definition is made -- still has somewhere to bind k.
>> f_(sum_(k=1)^2 k) = 5
f_(sum_(k=1)^2 k) = 5

>> f_3
5

# An index must be a whole number, as anywhere else.
>> sum_(k=1)^2.5 k
error: an index must be a whole number, not 2.5

# 'sum' and 'prod' are reserved, as 'lim' is.
>> sum = 3
error: expected an index after 'sum', as in sum_(k=1)^n, not '='

>> prod(x) = x
error: expected an index after 'prod', as in prod_(k=1)^n, not '('

>> sum_k^3 k
error: expected an index after 'sum', as in sum_(k=1)^n, not 'k'

>> sum_(k=1)^-3 k
error: expected the last index after '^', as in sum_(k=1)^n, not '-'

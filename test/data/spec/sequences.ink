# SPECIFICATION -- see definitions.ink.
#
# A sequence is ONE definition with several clauses: constant-index base
# cases, plus at most one general clause. This is how a recurrence is written
# on paper, and it is the syntax worth keeping.

>> s_0 = 1
s_0 = 1

>> s_n = s_(n-1)/2
s_n = s_(n-1)/2

>> s_0
1

>> s_1
0.5

>> s_10
0.0009765625

# A base case always wins over the general clause, whatever the order of
# definition.
>> u_n = 2*n
u_n = 2*n

>> u_3
6

>> u_3 = 100
u_3 = 100

>> u_3
100

>> u_4
8

# A bare name never means "the limit". Asking for a sequence without an index
# is an error, because there is no single value to give.
>> s
error: s is a sequence; index it (s_0) or take its limit (lim s)

# The limit is explicit, and says what it did.
>> lim s
0

>> exp(x)_n = exp(x)_(n-1) + x^n/!n
exp(x)_n = exp(x)_(n-1) + x^n/!n

>> lim exp(1)
2.71828183

# A divergent sequence reports non-convergence instead of handing back the
# term it happened to stop on.
>> lim u
error: u did not converge within 30 terms (last term 60)

# A later plain definition replaces the whole sequence rather than hiding
# beneath it.
>> u = 5
u = 5

>> u
5

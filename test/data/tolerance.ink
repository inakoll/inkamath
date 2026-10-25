# A tolerance relative to the term (DESIGN.md): a step, and the remainder
# its ratio implies, are measured in each cell against |the cell's term|.
# A walk that never gets within 1e-10 of its own size, as one to 0 does
# not, answers the first term within it measured against max(1, |term|),
# absolute below 1 (a tolerance floor for small limits, C401). Every value
# was worked out apart from the interpreter: the terms in Python's
# fractions, walked by docs/language.md section 4's rule written again in Python
# with the steps so measured, and what an inexact answer reads rounded to
# doubles as the interpreter rounds it.

# The partial sums of e. Measured against e, the step 1/13! is under 1e-10
# of it, and the remainder too, so the walk stops a term before 1/14!:
# float(S_13) - float(e), where it was float(S_14) - float(e), -8.15347789e-13
# (sequences.ink).
>> exp(x)_0 = 1
exp(x)_0 = 1

>> exp(x)_n = exp(x)_(n-1) + x^n/!n
exp(x)_n = exp(x)_(n-1) + x^n/!n

>> lim exp(1) - e
~-1.22861721e-11

# Past about 225,000 a double cannot show a step within 1e-10 of a term,
# and C353's walk stopped where the exact one never did. Relative, the
# steps 0.8^k from 10^14 stop at k = 2, the remainder 2.56 within 1e-10 of
# the term: 10^14 + 1.44, which the subtraction reads as a double, 10^14
# + 1.4375.
>> s(x)_0 = x*10^14
s(x)_0 = x*10^14

>> s(x)_k = s(x)_(k-1) + 0.8^k
s(x)_k = s(x)_(k-1) + 0.8^k

>> lim s(1) - 10^14
~1.4375

# Each cell against its own term: here both scaled steps are 2^-k near
# enough, and the walk stops at k = 34, where it stopped at 54 for the
# cell of 10^6, whose double then read 10^6 exactly.
>> m_0 = [0, 0]
m_0 = [0, 0]

>> m_k = [10^6*(1 - 1/2^k), 1 - 1/2^k]
m_k = [10^6*(1 - 1/2^k), 1 - 1/2^k]

>> lim m - [10^6, 1]
[~-5.82076609e-05, ~-5.82076609e-11]

# Not against the largest cell: a cell below 1 beside one of 10^6 still
# settles within 1e-10 of itself, at k = 81, where measured against 10^6 it
# would stop at k = 34, 5.65e-5 from 1.
>> n_0 = [0, 0]
n_0 = [0, 0]

>> n_k = [10^6*(1 - 1/2^k), 1 - (3/4)^k]
n_k = [10^6*(1 - 1/2^k), 1 - (3/4)^k]

>> lim n - [10^6, 1]
[~0, ~-7.58512142e-11]

# A limit of 0 has no relative step: its walk answers at the cap the
# first term within 1e-10, 2^-34.
>> h_0 = 1
h_0 = 1

>> h_n = h_(n-1)/2
h_n = h_(n-1)/2

>> lim h
~5.82076609e-11

# Below 1 a cell settles its own digits: (1 - 2^-k)/10^8 at k = 34, where
# it stopped at k = 7, 0.8% off, 1 - 2^-7 (C401).
>> q_0 = 0
q_0 = 0

>> q_k = (1 - 1/2^k)/10^8
q_k = (1 - 1/2^k)/10^8

>> lim q*10^8
~1

# The square root of 2/10^20 by Newton from 1 halves down to it and then
# closes: 1.6873769e-10 where the absolute 1e-10 stopped it, 19% off. Its
# terms pass a thousand digits at the seventh.
>> N(x)_0 = 1
N(x)_0 = 1

>> N(x)_k = (N(x)_(k-1) + x/N(x)_(k-1))/2
N(x)_k = (N(x)_(k-1) + x/N(x)_(k-1))/2

>> lim N(2/10^20)
~1.41421356e-10  # approximated past a thousand digits

# A series' partial sums are terms as any: 2 - 2^-k stops a term sooner,
# and 10^-8 (1 - 2^-n) at n = 34, where it stopped at 7.
>> 2 - sum_(k=0) 1/2^k
~1.16415322e-10

>> sum_(k=1) 1/2^k/10^8
~1e-08

# A walk that goes on past the term it keeps and meets a term refused, or
# one that is NaN, ends on the term kept, as at the cap: each stopped there
# before it walked on (2^-34; 10^-8 (1 - 2^-7)).
>> w_0 = 1
w_0 = 1

>> w_k | k < 40 = w_(k-1)/2
w_k | k < 40 = w_(k-1)/2

>> lim w
~5.82076609e-11

>> v_0 = 1
v_0 = 1

>> v_k | k < 40 = v_(k-1)/2
v_k | k < 40 = v_(k-1)/2

>> v_k = ~0/~0
v_k = ~0/~0

>> lim v
~5.82076609e-11

>> sum_(k=1) (1/2^k + 0/(k - 20))/10^8
~9.921875e-09

# What did not converge still does not: steps that do not shrink imply no
# remainder whatever the term, as C102's 10^17 + k in exact terms.
>> c_0 = 10^17
c_0 = 10^17

>> c_k = c_(k-1) + 1
c_k = c_(k-1) + 1

>> lim c
error: c did not converge within 100 terms (last term 100000000000000100, its steps in a ratio of 1); for a fixed count, read a term, as c_100

>> sum_(k=1) 1/k^2
error: the sum did not converge within 100 terms (last partial sum ~1.6349839)

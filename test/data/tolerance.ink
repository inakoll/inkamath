# A tolerance relative to the term (DESIGN.md): a step, and the remainder
# its ratio implies, are measured in each cell against max(1, |the cell's
# term|), so the 1e-10 is absolute below 1 and relative above. Every value
# was worked out apart from the interpreter: the terms in Python's
# fractions, walked by README section 4's rule written again in Python with
# the steps so measured, and what an inexact answer reads rounded to
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

# Below 1 nothing moves: a limit of 0 has no relative step, and a cell far
# below 1 still settles few of its own digits (README section 4).
>> h_0 = 1
h_0 = 1

>> h_n = h_(n-1)/2
h_n = h_(n-1)/2

>> lim h
~5.82076609e-11

>> q_0 = 0
q_0 = 0

>> q_k = (1 - 1/2^k)/10^8
q_k = (1 - 1/2^k)/10^8

>> lim q*10^8
~0.9921875

# A series' partial sums are terms as any: 2 - 2^-k stops a term sooner.
>> 2 - sum_(k=0) 1/2^k
~1.16415322e-10

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

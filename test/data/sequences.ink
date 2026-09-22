# Indexed references and general terms (README.md section 4).

>> f_0=1+2
f_0=1+2

>> f_1=3+4
f_1=3+4

>> f_0
3

>> f_1
7

# A general term applies to every index that has no singular definition.
>> u_n=2*n
u_n=2*n

>> u_1
2

>> u_2
4

>> u_0=2
u_0=2

>> u_1=3
u_1=3

>> u_0
2

>> u_1
3

>> u_2
4

>> u_3
6

# Series: 'lim' iterates the general clause until two terms agree. It used to
# happen on its own whenever a sequence was named without an index, which is
# why 'exp(1)' read like a function call (MODERNIZATION.md, phase 4 item 2).
# The first term is written out; an indexed clause no longer falls back to an
# implicit zero, which was the additive identity and so wrong for a product
# (MODERNIZATION.md, phase 4 item 4).
>> exp(x)_0=1
exp(x)_0=1

>> exp(x)_n=exp(x)_(n-1)+x^n/!n
exp(x)_n=exp(x)_(n-1)+x^n/!n

>> lim exp(1)
2.71828183

# Not floating point: the series stops once the step is under 1e-10 and the
# remainder the steps imply is too.
>> lim exp(1)-e
-8.149037e-13

>> cos(x)=(lim exp(i*x)+lim exp(-i*x))/2
cos(x)=(lim exp(i*x)+lim exp(-i*x))/2

>> cos(pi/3)
0.5

>> sin(x)=(lim exp(i*x)-lim exp(-i*x))/(2*i)
sin(x)=(lim exp(i*x)-lim exp(-i*x))/(2*i)

>> sin(pi/6)
0.5

# A recurrence written the textbook way -- an initial value plus a general
# term -- then asked for its limit (MODERNIZATION.md C12).
>> s_0=1
s_0=1

>> s_n=s_(n-1)/2
s_n=s_(n-1)/2

>> s_20
9.53674316e-07

>> s
error: s is a sequence; index it (s_0) or take its limit (lim s)

>> lim s
5.82076609e-11

# 'lim' walks the terms the sequence has, which means walking them through the
# same dispatch an index walks. It used to evaluate the general clause
# directly, so a guarded clause was invisible to it and the limit contradicted
# every term the user could ask for (MODERNIZATION.md, C54).
>> lg_0 = 1
lg_0 = 1

>> lg_n | n > 0 = lg_(n-1)/2
lg_n | n > 0 = lg_(n-1)/2

>> lg_n = lg_(n-1)*10
lg_n = lg_(n-1)*10

>> lg_10
0.0009765625

>> lim lg
5.82076609e-11

# The same the other way: terms that settle were reported as divergence,
# quoting a last term the sequence does not have.
>> lt_0 = 0
lt_0 = 0

>> lt_n | n > 2 = 5
lt_n | n > 2 = 5

>> lt_n = n
lt_n = n

>> lim lt
5

# And a general clause that is guarded is still a general clause.
>> lc_0 = 1
lc_0 = 1

>> lc_n | 1 = lc_(n-1)/2
lc_n | 1 = lc_(n-1)/2

>> lc_10
0.0009765625

>> lim lc
5.82076609e-11

# A small step is not a small remainder. Every step here is 1e-11 and the
# series diverges; comparing successive terms alone called that convergence
# and answered 1e-11 (MODERNIZATION.md, C36).
>> w_0=0
w_0=0

>> w_n=w_(n-1)+1e-11
w_n=w_(n-1)+1e-11

>> lim w
error: w did not converge within 100 terms (last term 1e-09)

# And a series that converges too slowly to be summed term by term: after n
# terms of 1/n^2 the remainder is about 1/n, so raising the cap does not help
# -- at a hundred thousand terms it answers 1.64492407 where pi^2/6 is
# 1.64493407. Saying so is the honest answer (MODERNIZATION.md, C36).
>> z_1=1
z_1=1

>> z_n=z_(n-1)+1/n^2
z_n=z_(n-1)+1/n^2

>> lim z
error: z did not converge within 100 terms (last term 1.63508193)

# Which is a mathematical problem and not a limitation of 'lim'. What is left
# of the series after n terms is 1/n - 1/(2n^2) + 1/(6n^3) - ..., and a clause
# may add it: twenty corrected terms give every digit that is printed, where
# a hundred thousand raw ones do not.
>> y_n=z_n+1/n-1/(2*n^2)+1/(6*n^3)-1/(30*n^5)
y_n=z_n+1/n-1/(2*n^2)+1/(6*n^3)-1/(30*n^5)

>> y_20
1.64493407

>> pi^2/6
1.64493407

# The same move without knowing the tail: Aitken's delta-squared, written as
# an ordinary clause because a clause may index another sequence at any
# expression, including one that reaches forward. Twenty accelerated terms of
# the alternating series beat a hundred raw ones.
>> q_1=1
q_1=1

>> q_n=q_(n-1)+(0-1)^(n-1)/n
q_n=q_(n-1)+(0-1)^(n-1)/n

>> q_100
0.688172179

>> r_n=q_n-(q_(n+1)-q_n)^2/(q_(n+2)-2*q_(n+1)+q_n)
r_n=q_n-(q_(n+1)-q_n)^2/(q_(n+2)-2*q_(n+1)+q_n)

>> r_20
0.693134637

# Recursion is bounded: past the budget the interpreter says so rather than
# dying (MODERNIZATION.md, C1).
>> g_0=1
g_0=1

>> g_n=g_(n-1)+1
g_n=g_(n-1)+1

>> g_10
11

>> g_500
error: evaluation nests more than 256 references deep

# A base case at a negative index keeps its place in the ordering. These
# clauses used to be keyed by size_t, so k_(-1) sorted above k_0 and the
# iteration started from a wrapped index (MODERNIZATION.md, C17).
>> k_(-1)=0
k_(-1)=0

>> k_0=5
k_0=5

>> k_n=k_(n-1)+1
k_n=k_(n-1)+1

>> k_1
6

# A divergent sequence says so rather than handing back the term the loop
# happened to stop on -- this used to be 35, the 30th term.
>> lim k
error: k did not converge within 100 terms (last term 105)

# With no base clause there is no term to compare the first one against. It
# used to be compared with a default-constructed zero, so a sequence whose
# first term was zero was reported as having converged to it -- this answered
# 0 (MODERNIZATION.md, C27).
>> d_n=n-1
d_n=n-1

>> lim d
error: d did not converge within 100 terms (last term 99)

# A difference that is NaN answers false to every comparison, so it has to be
# tested for convergence rather than against it. This answered inf*-nan.
>> o_0=2
o_0=2

>> o_n=o_(n-1)^2
o_n=o_(n-1)^2

>> lim o
error: o did not converge within 100 terms (last term inf+i*-nan)

# A recurrence has no implicit value below its lowest clause. The old fallback
# was zero -- the additive identity, right for a sum and wrong for a product
# (MODERNIZATION.md, phase 4 item 4).
>> fact_n=fact_(n-1)*n
fact_n=fact_(n-1)*n

>> fact_5
error: evaluation nests more than 256 references deep

>> fact_0=1
fact_0=1

>> fact_5
120


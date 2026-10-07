# The arithmetic-geometric mean, from the project's own 2014 test data. Two
# mutually recursive sequences: this is the input that used to overflow the
# stack and kill the process, and the reason the original suite ran every
# evaluation in a thread with a timeout (DESIGN.md, C1).

>> am(x,y)_0 = (x+y)/2
am(x,y)_0 = (x+y)/2

>> gm(x,y)_0 = (x*y)^0.5
gm(x,y)_0 = (x*y)^0.5

>> am(x,y)_n = (am(x,y)_(n-1) + gm(x,y)_(n-1))/2
am(x,y)_n = (am(x,y)_(n-1) + gm(x,y)_(n-1))/2

>> gm(x,y)_n = (am(x,y)_(n-1) * gm(x,y)_(n-1))^0.5
gm(x,y)_n = (am(x,y)_(n-1) * gm(x,y)_(n-1))^0.5

>> gm(1,2)_10
~1.45679103

# The tree is 2^(n+1)-1 calls for 2n+1 answers, so the same answers were
# computed thousands of times over; this term used to give up after a million
# steps, and is memoised on the call's context now (DESIGN.md,
# phase 9).
>> gm(1,2)_20
~1.45679103

# The two sequences share a limit; that is what makes it a mean.
>> lim am(1,2)
~1.45679103

>> lim gm(1,2)
~1.45679103

# Far terms fill from the base up, arguments and all.
>> gm(1,2)_300
~1.45679103

>> ?gm
gm(x,y)_0 = (x*y)^0.5
gm(x,y)_n = (am(x,y)_(n-1) * gm(x,y)_(n-1))^0.5

# A term has a million steps of its own, not counting the terms it computes,
# which have theirs (C149). Each w here takes some fifty thousand, so w_25
# cold used to give up, and answered once w_20 had been asked: whether a term
# answered depended on what came before it. It is 1/2^25.
>> w_0 = 1
w_0 = 1

>> w_n = w_(n-1)/2 + sum_(k=1)^50000 0
w_n = w_(n-1)/2 + sum_(k=1)^50000 0

>> w_25
~2.98023224e-08

# So do the terms a limit walks: each p takes some fifteen thousand steps, and
# the walk used to give up unless p_50 had been asked first. p_k is
# 4 - 4*(3/4)^k.
>> p_0 = ~0
p_0 = ~0

>> p_k = 3/4*p_(k-1) + 1 + sum_(j=1)^15000 0
p_k = 3/4*p_(k-1) + 1 + sum_(j=1)^15000 0

>> lim p
~4

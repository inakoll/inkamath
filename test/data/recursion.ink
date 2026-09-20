# The arithmetic-geometric mean, from the project's own 2014 test data. Two
# mutually recursive sequences: this is the input that used to overflow the
# stack and kill the process, and the reason the original suite ran every
# evaluation in a thread with a timeout (MODERNIZATION.md, C1).

>> am(x,y)_0 = (x+y)/2
am(x,y)_0 = (x+y)/2

>> gm(x,y)_0 = (x*y)^0.5
gm(x,y)_0 = (x*y)^0.5

>> am(x,y)_n = (am(x,y)_(n-1) + gm(x,y)_(n-1))/2
am(x,y)_n = (am(x,y)_(n-1) + gm(x,y)_(n-1))/2

>> gm(x,y)_n = (am(x,y)_(n-1) * gm(x,y)_(n-1))^0.5
gm(x,y)_n = (am(x,y)_(n-1) * gm(x,y)_(n-1))^0.5

>> gm(1,2)_10
1.45679103

# The tree is 2^(n+1)-1 calls for 2n+1 answers, so the same answers were
# computed thousands of times over; this term used to give up after a million
# steps, and is memoised on the call's context now (MODERNIZATION.md,
# phase 9).
>> gm(1,2)_20
1.45679103

# The two sequences share a limit; that is what makes it a mean.
>> lim am(1,2)
1.45679103

>> lim gm(1,2)
1.45679103

>> ?gm
gm(x,y)_0 = (x*y)^0.5
gm(x,y)_n = (am(x,y)_(n-1) * gm(x,y)_(n-1))^0.5


# SPECIFICATION -- see definitions.ink.
#
# NOT WIRED INTO CI YET. Evaluating these kills the process (C1: unbounded
# recursion overflows the stack), and doctest's may_fail tolerates a failed
# assertion, not a dead process. The test case exists but is marked skip;
# remove the decorator in the commit that lands the evaluation budget.

# Unbounded recursion is reported, not fatal. This pair is the
# arithmetic-geometric mean from the project's own 2014 test data; it
# currently overflows the stack and kills the process (C1).
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

>> lim gm(1,2)
1.45679103

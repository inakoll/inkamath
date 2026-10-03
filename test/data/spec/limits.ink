# The limit of a sequence of matrices is the limit of each cell
# (DESIGN.md, Deferred, then next in line). Its steps are measured by the
# largest of its cells', and the rule is the one a number's limit stops by:
# the step and what is left of the series both within the tolerance. A
# Markov chain settles on its steady state.
>> t = [0.9 0.1; 0.5 0.5]
t = [0.9 0.1; 0.5 0.5]

>> k_0 = [1 0]
k_0 = [1 0]

>> k_n = k_(n-1)*t
k_n = k_(n-1)*t

>> lim k
[~0.833333333, ~0.166666667]

# Four cells halving: the largest decides, and it is within the tolerance
# from the 36th term, which is the answer.
>> mm_n = [1 2;3 4]*(0.5)^n
mm_n = [1 2;3 4]*(0.5)^n

>> lim mm
[~1.45519152e-11, ~2.91038305e-11;
 ~4.36557457e-11, ~5.82076609e-11]

# Terms that change size have no limit: one does not stretch over the other
# here, as it would in a difference.
>> sz_n | n < 3 = [1 2]
sz_n | n < 3 = [1 2]

>> sz_n = 1
sz_n = 1

>> lim sz
error: sz has no limit: its terms are 1x2, then 1x1

# A function's limit too, as a number's is.
>> walk(p)_0 = [1 0]
walk(p)_0 = [1 0]

>> walk(p)_k = walk(p)_(k-1)*[1-p, p; p, 1-p]
walk(p)_k = walk(p)_(k-1)*[1-p, p; p, 1-p]

>> lim walk(1/4)
[~0.5, ~0.5]

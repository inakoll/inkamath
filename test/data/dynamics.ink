# dyn.ink's cantilever, stepped by Newmark's average acceleration and by
# central differences. Expected: the same recurrences in mpmath at 60
# digits, and the central differences' limit 2/omega_max = 0.00209852348809
# from mpmath's eigenvalues; a step of 1/100 is within it for Newmark's,
# which is unconditionally stable, and far past it for central differences.
>> use dyn (nm, cd, fast, stable, K, P)
use dyn (nm, cd, fast, stable, K, P)

# The static tip deflection, P L^3/(3 EI).
>> frac (K^-1*P)[7]
1/3

>> digits = 12
digits = 12

>> [nm.tip_10, cd.tip_10]
[~0.0334066821391, ~0.00103853865017]

>> fast.tip_10
~-4.09971740487e+14

>> [stable(2098/10^6), stable(2099/10^6)]
[1, 0]

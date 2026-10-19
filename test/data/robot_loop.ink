# A robot's loop, robot.ink, from an end-to-end exploration: a cart, an
# IMU, a position fix that fails, a Kalman filter and an LQR.

# The LQR gain by doubling under lim, for Q = diag(1, 1/10) and R = 1/10:
# scipy's solve_discrete_are gives K = [3.11977442, 2.6856746].
>> use robot (gain)
use robot (gain)

>> gain([1 0; 0 1/10], ~1/10)
[~3.11977442, ~2.6856746]

# With the position not weighted, no stabilizing solution exists: scipy's,
# X = [0 0; 0 10.050125], Z's third block here, leaves F - GK an eigenvalue
# of 1, so Z's first block does not vanish nor its second converge.
>> gain([0 0; 0 1/10], ~1/10)
error: Z did not converge within 100 terms (last term [            ~1,   ~1.0000125;
             ~0,           ~0;
 ~1.2676506e+27, ~0.100502506;
   ~0.100502506,  ~0.10050125;
             ~0,           ~0;
             ~0,   ~10.050125], its steps in a ratio of 1); for a fixed count, read a term, as Z_100

# The loop's gain falls to a fixed one there, a clause a guard chose by an
# inexact limit, so inexact.
>> kg(q) | gain([q 0; 0 1/10], ~1/10)[1,1] > 0 = gain([q 0; 0 1/10], ~1/10)
kg(q) | gain([q 0; 0 1/10], ~1/10)[1,1] > 0 = gain([q 0; 0 1/10], ~1/10)

>> kg(q) = [3 5/2]
kg(q) = [3 5/2]

>> kg(0)
[~3, ~2.5]

>> kg(1)
[~3.11977442, ~2.6856746]

# The fix fails from step 300 to 499. Flagged, the filter takes no fix from
# 300, is lost from 415, when its position's variance passes 1/400, and the
# fix at 500 recovers it. --check holds the compiled step's flags to these
# over 600 steps.
>> use robot (fc, fc2)
use robot (fc, fc2)

>> [fc.fix_290, fc.fix_300, fc.fix_490, fc.fix_500]
[~1, 0, 0, ~1]

>> [fc.lost_414, fc.lost_415, fc.lost_499, fc.lost_500]
[~0, ~1, ~1, ~0]

# Frozen at its last value and said good, the fix at 300 is taken, its value
# seen twice only; from 310 it is seen frozen and refused, so the filter is
# lost 11 steps later than flagged, from 426.
>> [fc2.frozen_300, fc2.frozen_310, fc2.frozen_490, fc2.frozen_500]
[~0, ~1, ~1, ~0]

>> [fc2.fix_300, fc2.fix_310, fc2.fix_490, fc2.fix_500]
[~1, ~0, ~0, ~1]

>> [fc2.lost_425, fc2.lost_426, fc2.lost_499, fc2.lost_500]
[~0, ~1, ~1, ~0]

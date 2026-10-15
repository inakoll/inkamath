# A robot's loop, robot.ink, from an end-to-end exploration: a cart, its
# sensors, a Kalman filter and an LQR.

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
             ~0,   ~10.050125])

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

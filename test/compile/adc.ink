# An analog-to-digital reading: the input rounded down to steps of q, held
# while it moves by less than a step, so that noise does not toggle it, and
# an alarm while the reading is outside [lo, hi]. Whether it rose asks for the
# reading before only once there is one.
q = 1/4
lo = -1
hi = 1
v_n = q*floor(x_n/q)
held_0 = 0
held_n = v_n
held_n | held_(n-1) - v_n < q and v_n - held_(n-1) < q = held_(n-1)
alarm_n = held_n < lo or held_n > hi
rising_n = n > 0 and held_n > held_(n-1)

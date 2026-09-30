# Instances of a file's models, one inside another, fed the step's input: a
# parameter given a constant is a field of its instance, one given the
# session's parameter follows it, and the file's own names are constants.
use filters
k = 1/2
fast = filters.lowpass(a = k, u_n = x_n)
slow = filters.lowpass(u_n = fast.v_n)
h = filters.highpass(a = 1/4, u_n = x_n)
y_n = slow.v_n + h.v_n + filters.a0

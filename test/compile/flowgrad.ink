# C446: through flow's total and divergence, cost(v, v) is 3v^2, its
# derivative 6n; grad's v is this file's, not flow's function v.
use flow
cost(W, a) = a*flow.total(W)
y_n = grad_(v = n) cost(v, v)

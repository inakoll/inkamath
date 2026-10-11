# The prelude's exp, log, tanh, ilogb, sin, cos, atan, asin, acos and acosh,
# which 'inkamath --compile' writes as C functions into inkamath_prelude.h
# beside this, for the interpreter to call on a double (DESIGN.md, next in
# line). The sequence only calls each; its index is a double in the step, so
# no call is folded. The second writes their parts, the `_dx` functions, for
# grad.
c_n = exp(n) + log(n + 1) + tanh(n) + ilogb(n + 1) + sin(n) + cos(n) + atan(n) + asin(n) + acos(n) + acosh(n + 1)
d_n = grad_(t = n) (exp(t) + log(t + 1) + tanh(t) + sin(t) + cos(t) + atan(t) + asin(t) + acos(t) + acosh(t + 1))

# The prelude's exp, log, tanh, ilogb, sin and cos, which 'inkamath
# --compile' writes as C functions into inkamath_prelude.h beside this, for
# the interpreter to call on a double (DESIGN.md, next in line). The sequence only calls each;
# its index is a double in the step, so no call is folded.
c_n = exp(n) + log(n + 1) + tanh(n) + ilogb(n + 1) + sin(n) + cos(n)

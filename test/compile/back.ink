# Terms with no base clause read back before the step's first index, where
# the interpreter answers them (MODERNIZATION.md, C71): a_(-1) is 1/8, its
# guard failing, and c_(-1) would read u_(-2), which nothing gives.
a_n | n > 2 = u_(n-1)
a_n = 1/8
b_n = a_(n-1)/2
c_n | n < 0 = u_(n-1)
c_n = 1
e_n = c_(n-1)

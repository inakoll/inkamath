# Exact matrix products, of matrices defined by their cells. The answer sums
# every cell, so that every cell counts in it.
>> A[j<=20, k<=20] = j*k + j - k
A[j<=20, k<=20] = j*k + j - k

>> B[j<=20, k<=20] = (j - k)/(j + k)
B[j<=20, k<=20] = (j - k)/(j + k)

>> u[j<=20, k<=1] = 1
u[j<=20, k<=1] = 1

>> u'*(sum_(k=1)^100 A*B)*u
~12963500.3


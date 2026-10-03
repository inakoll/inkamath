# Exact matrix products, of matrices defined by their cells.
>> A[j<=20, k<=20] = j*k + j - k
A[j<=20, k<=20] = j*k + j - k

>> B[j<=20, k<=20] = (j - k)/(j + k)
B[j<=20, k<=20] = (j - k)/(j + k)

>> (sum_(k=1)^100 A*B)[20, 20]
~-100643.13


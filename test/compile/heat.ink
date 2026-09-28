# Heat along a rod by finite elements: three nodes inside, the ends held at 0,
# the first node heated by q, in implicit Euler steps of dt. A derives from the
# parameters alone, so its inverse is taken where they are set, not each step.
h = 1/4
dt = 1/100
K[j<=3, k<=3] | j == k = 2/h
K[j<=3, k<=3] | (j-k)^2 == 1 = -1/h
I[j<=3, k<=3] = j == k
B[j<=3, k<=1] = j == 1
A = (h*I + dt*K)^-1*h
u_0 = 0*B
u_n = A*(u_(n-1) + dt*B*q_n)

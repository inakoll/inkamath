# A convolutional layer as Goodfellow, Bengio and Courville (2016, §9.5)
# write it: the cross-correlation deep learning calls convolution, of an
# image V of two channels padded by zeros, at stride 2, by a kernel K whose
# row 2*(i-1)+l holds output channel i's weights on input channel l; then
# the gradient of a loss with respect to the kernel, by grad and by the
# section's sum g, given G, the loss's gradient with respect to the output.
# Every value below is worked out apart from the interpreter, exactly, in
# Python from the same sums, the gradients a second way by central
# differences of exact fractions; none was recorded.
>> V = [1 0 1 2 0; 0 2 0 1 2; 0 2 0 0 0; 1 1 0 0 0; 2 1 0 2 0;; 0 2 2 2 0; 2 2 1 0 0; 0 2 0 1 1; 0 2 0 2 1; 2 2 0 0 2]
V = [1 0 1 2 0; 0 2 0 1 2; 0 2 0 0 0; 1 1 0 0 0; 2 1 0 2 0;; 0 2 2 2 0; 2 2 1 0 0; 0 2 0 1 1; 0 2 0 2 1; 2 2 0 0 2]

>> K = [1 1 -1; 0 -1 1; 1 -1 1;; -1 1 -1; 0 1 1; 0 0 0;; 1 0 0; 0 -1 -1; 1 -1 -1;; 1 0 1; 0 0 1; 0 0 1]
K = [1 1 -1; 0 -1 1; 1 -1 1;; -1 1 -1; 0 1 1; 0 0 0;; 1 0 0; 0 -1 -1; 1 -1 -1;; 1 0 1; 0 0 1; 0 0 1]

>> pad(V, p)[l<=2, j<=5+2*p, k<=5+2*p] | j > p and j <= 5+p and k > p and k <= 5+p = V[l, j-p, k-p]
pad(V, p)[l<=2, j<=5+2*p, k<=5+2*p] | j > p and j <= 5+p and k > p and k <= 5+p = V[l, j-p, k-p]

>> pad(V, p)[l<=2, j<=5+2*p, k<=5+2*p] = 0
pad(V, p)[l<=2, j<=5+2*p, k<=5+2*p] = 0

>> c(K, V, s)[i<=2, j<=3, k<=3] = sum_(l=1)^2 sum_(m=1)^3 sum_(n=1)^3 pad(V, 1)[l, (j-1)*s+m, (k-1)*s+n]*K[2*(i-1)+l, m, n]
c(K, V, s)[i<=2, j<=3, k<=3] = sum_(l=1)^2 sum_(m=1)^3 sum_(n=1)^3 pad(V, 1)[l, (j-1)*s+m, (k-1)*s+n]*K[2*(i-1)+l, m, n]

# With a bias for each output channel.
>> Z = c(K, V, 2) + [1/2;; -1/2]
Z = c(K, V, 2) + [1/2;; -1/2]

>> Z
[3.5,  8.5, -0.5;
 2.5,  2.5,  4.5;
 1.5, -0.5,  1.5;;
 0.5, -0.5, -1.5;
 1.5,  7.5,  0.5;
 0.5,  2.5,  1.5]

>> G = [1 -1 2; 0 1 -2; 1 1 1;; 2 0 -1; 1 -1 0; 0 2 1]
G = [1 -1 2; 0 1 -2; 1 1 1;; 2 0 -1; 1 -1 0; 0 2 1]

>> J(Q) = sum_(i=1)^2 sum_(j=1)^3 sum_(k=1)^3 G[i,j,k]*c(Q, V, 2)[i,j,k]
J(Q) = sum_(i=1)^2 sum_(j=1)^3 sum_(k=1)^3 G[i,j,k]*c(Q, V, 2)[i,j,k]

>> g(G, V, s)[r<=4, k<=3, l<=3] = sum_(m=1)^3 sum_(n=1)^3 G[ceil(r/2), m, n]*pad(V, 1)[mod(r-1, 2)+1, (m-1)*s+k, (n-1)*s+l]
g(G, V, s)[r<=4, k<=3, l<=3] = sum_(m=1)^3 sum_(n=1)^3 G[ceil(r/2), m, n]*pad(V, 1)[mod(r-1, 2)+1, (m-1)*s+k, (n-1)*s+l]

>> g(G, V, 2)
[ 1, -3, 2;
  9,  2, 1;
  1,  4, 1;;
  6,  2, 4;
  4,  0, 3;
 -4, -1, 4;;
  0,  0, 1;
  0,  2, 6;
 -2, -1, 5;;
  4,  2, 6;
  0,  2, 5;
 -2,  4, 4]

>> grad_(Q = K) J(Q) == g(G, V, 2)
1

# Through a ReLU the gradient is G where Z is positive and 0 where it is
# not, no cell of Z being 0.
>> relu(z)[i,j,k] | z[i,j,k] > 0 = z[i,j,k]
relu(z)[i,j,k] | z[i,j,k] > 0 = z[i,j,k]

>> relu(z)[i,j,k] = 0
relu(z)[i,j,k] = 0

>> H(Q) = sum_(i=1)^2 sum_(j=1)^3 sum_(k=1)^3 G[i,j,k]*relu(c(Q, V, 2) + [1/2;; -1/2])[i,j,k]
H(Q) = sum_(i=1)^2 sum_(j=1)^3 sum_(k=1)^3 G[i,j,k]*relu(c(Q, V, 2) + [1/2;; -1/2])[i,j,k]

>> D[i<=2, j<=3, k<=3] = G[i,j,k]*(Z[i,j,k] > 0)
D[i<=2, j<=3, k<=3] = G[i,j,k]*(Z[i,j,k] > 0)

>> g(D, V, 2)
[ 0, -3,  2;
  4,  2, -1;
 -1,  0,  1;;
  4,  2,  2;
 -2,  0,  3;
 -4, -1,  4;;
  0,  0,  1;
  2,  2,  6;
 -1,  1,  5;;
  4,  2,  6;
  2,  2,  5;
 -2,  4,  4]

>> grad_(Q = K) H(Q) == g(D, V, 2)
1

# The gradient of J with respect to the image's first row, which W puts in
# place of V's.
>> JV(U) = sum_(i=1)^2 sum_(j=1)^3 sum_(k=1)^3 G[i,j,k]*c(K, U, 2)[i,j,k]
JV(U) = sum_(i=1)^2 sum_(j=1)^3 sum_(k=1)^3 G[i,j,k]*c(K, U, 2)[i,j,k]

>> W(u)[l<=2, j<=5, k<=5] | l == 1 and j == 1 = u[k]
W(u)[l<=2, j<=5, k<=5] | l == 1 and j == 1 = u[k]

>> W(u)[l<=2, j<=5, k<=5] = V[l,j,k]
W(u)[l<=2, j<=5, k<=5] = V[l,j,k]

>> (grad_(u = [1; 0; 1; 2; 0]) JV(W(u)))'
[-3, -1, 1, -1, -1]

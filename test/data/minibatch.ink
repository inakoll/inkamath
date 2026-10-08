# A minibatch as time (MANIFESTO.md, Data): a stream of samples, a minibatch
# being index arithmetic over it. A small convolutional network (Goodfellow,
# Bengio and Courville 2016, §9.5), a 2x2 kernel over a 3x3 image padded by
# zeros, ReLU, 2x2 max pooling and a dense head, is trained by grad on a
# batch of two images given as a tensor, and on the stream of the same two,
# each step of the weights reading the samples 2m-2 and 2m-1: the two train
# alike, step for step. Worked out apart from the interpreter, by
# backpropagation in exact fractions, both ways; none was recorded. The
# second is not compiled yet, its weights slow sequences that read each other
# (DESIGN.md, next in line).
>> pad(X)[i<=5, j<=5] | i > 1 and i < 5 and j > 1 and j < 5 = X[i-1, j-1]
pad(X)[i<=5, j<=5] | i > 1 and i < 5 and j > 1 and j < 5 = X[i-1, j-1]

>> pad(X)[i<=5, j<=5] = 0
pad(X)[i<=5, j<=5] = 0

>> conv(K, X)[i<=4, j<=4] = sum_(u=1)^2 sum_(v=1)^2 K[u,v]*pad(X)[i+u-1, j+v-1]
conv(K, X)[i<=4, j<=4] = sum_(u=1)^2 sum_(v=1)^2 K[u,v]*pad(X)[i+u-1, j+v-1]

>> relu(z)[i,j] | z[i,j] > 0 = z[i,j]
relu(z)[i,j] | z[i,j] > 0 = z[i,j]

>> relu(z)[i,j] = 0
relu(z)[i,j] = 0

>> pool(r)[i<=2, j<=2] = max(max(r[2*i-1,2*j-1], r[2*i-1,2*j]), max(r[2*i,2*j-1], r[2*i,2*j]))
pool(r)[i<=2, j<=2] = max(max(r[2*i-1,2*j-1], r[2*i-1,2*j]), max(r[2*i,2*j-1], r[2*i,2*j]))

>> s(K, d, W, a, X) = a + sum_(i=1)^2 sum_(j=1)^2 W[i,j]*pool(relu(conv(K, X) + d))[i,j]
s(K, d, W, a, X) = a + sum_(i=1)^2 sum_(j=1)^2 W[i,j]*pool(relu(conv(K, X) + d))[i,j]

>> batch(eta = 1/1024, X_n[b<=2, i<=3, j<=3], y_n[b<=2]) = {
..     L(K, d, W, a, X, y) = sum_(b=1)^2 (s(K, d, W, a, X[b]) - y[b])^2/4
..     K_0 = [1 -1; 2 0]
..     d_0 = 1/4
..     W_0 = [1 -1; 2 1]
..     a_0 = 1/2
..     K_n = K_(n-1) - eta*grad_(G = K_(n-1)) L(G, d_(n-1), W_(n-1), a_(n-1), X_n, y_n)
..     d_n = d_(n-1) - eta*grad_(e = d_(n-1)) L(K_(n-1), e, W_(n-1), a_(n-1), X_n, y_n)
..     W_n = W_(n-1) - eta*grad_(U = W_(n-1)) L(K_(n-1), d_(n-1), U, a_(n-1), X_n, y_n)
..     a_n = a_(n-1) - eta*grad_(c = a_(n-1)) L(K_(n-1), d_(n-1), W_(n-1), c, X_n, y_n)
.. }
batch(eta = 1/1024, X_n[b<=2, i<=3, j<=3], y_n[b<=2]) = { ... }

>> stream(eta = 1/1024, x_n[i<=3, j<=3], t_n) = {
..     L(K, d, W, a, m) = ((s(K, d, W, a, x_(2*m-2)) - t_(2*m-2))^2/2 + (s(K, d, W, a, x_(2*m-1)) - t_(2*m-1))^2/2)/2
..     K_0 = [1 -1; 2 0]
..     d_0 = 1/4
..     W_0 = [1 -1; 2 1]
..     a_0 = 1/2
..     K_m = K_(m-1) - eta*grad_(G = K_(m-1)) L(G, d_(m-1), W_(m-1), a_(m-1), m)
..     d_m = d_(m-1) - eta*grad_(e = d_(m-1)) L(K_(m-1), e, W_(m-1), a_(m-1), m)
..     W_m = W_(m-1) - eta*grad_(U = W_(m-1)) L(K_(m-1), d_(m-1), U, a_(m-1), m)
..     a_m = a_(m-1) - eta*grad_(c = a_(m-1)) L(K_(m-1), d_(m-1), W_(m-1), c, m)
.. }
stream(eta = 1/1024, x_n[i<=3, j<=3], t_n) = { ... }

>> A = [0 2 2; 2 1 3; 2 1 2]
A = [0 2 2; 2 1 3; 2 1 2]

>> B = [1 0 3; 2 2 0; 3 1 1]
B = [1 0 3; 2 2 0; 3 1 1]

>> p = batch(X_n = [0 2 2; 2 1 3; 2 1 2;; 1 0 3; 2 2 0; 3 1 1], y_n = [1; 2])
p = batch(X_n = [0 2 2; 2 1 3; 2 1 2;; 1 0 3; 2 2 0; 3 1 1], y_n = [1; 2])

>> q = stream(x_n = A*(1 - mod(n, 2)) + B*mod(n, 2), t_n = 1 + mod(n, 2))
q = stream(x_n = A*(1 - mod(n, 2)) + B*mod(n, 2), t_n = 1 + mod(n, 2))

>> q.K_1 == p.K_1
1

>> q.W_3 == p.W_3
1

>> frac p.a_1
1997/4096

>> frac p.d_1
871/4096

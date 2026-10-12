# The exponential as a paper writes it: the limit of its partial sums, its
# term a sequence of its own read at the limit's index (DESIGN.md, a sequence
# with parameters read at lim's walking index). Of a number, and of a
# rotation vector's hat, whose exponential is the rotation Rodrigues' formula
# gives. Their values, and the sine's below, are held to mpmath's by
# cli.cmake's compile_series_values.
I3[j<=3, k<=3] = j == k
hat(w) = [0, -w[3], w[2]; w[3], 0, -w[1]; -w[2], w[1], 0]
t(x)_0 = 1
t(x)_k = t(x)_(k-1)*x/k
ex(x)_0 = 1
ex(x)_k = ex(x)_(k-1) + t(x)_k
T(A)_0 = I3
T(A)_k = T(A)_(k-1)*A/k
E(A)_0 = I3
E(A)_k = E(A)_(k-1) + T(A)_k

expo(x_n, w_n[j<=3]) = {
    y_n = lim ex(x_n)
    R_n = lim E(hat(w_n))
}
taylor = expo(x_n = n/10 - 2, w_n = [n/20; -1/3; 1/4])

# The sine from its first term at 1, a base clause where the walk starts.
s(x)_1 = x
s(x)_k = -s(x)_(k-1)*x^2/((2*k - 1)*(2*k - 2))
sn(x)_0 = 0
sn(x)_k = sn(x)_(k-1) + s(x)_k
sine(x_n) = {
    z_n = lim sn(x_n)
}
wave = sine(x_n = n/10 - 5)

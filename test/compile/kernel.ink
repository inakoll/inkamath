# Calls compiled where they are made (DESIGN.md, phase 15): functions,
# guarded or not, a model without memory used once per step as a kernel, and
# a recursion unrolled because its argument is a constant; and one of the
# prelude, ceil, a function of the header's own.
relu(t) = t
relu(t) | t < 0 = 0
conv(K, I, N = 3) = {
    at(j, k) = I[j, k]
    at(j, k) | j < 1 or k < 1 or j > N or k > N = 0
    out[j<=N, k<=N] = sum_(p=1)^3 sum_(q=1)^3 K[p, q]*at(j+p-2, k+q-2)
}
power(x, e) = {
    half = power(x, floor(e/2)).v
    v = half*half
    v | e == 0 = 1
    v | mod(e, 2) == 1 = x*half*half
}
lap = [0, 1, 0; 1, 4, 1; 0, 1, 0]/8
u_0 = [0, 0, 0; 0, 16, 0; 0, 0, 0]
u_n = conv(lap, u_(n-1)).out
y_n = relu(x_n - 1/2) + power(2, 5).v + ceil(x_n)

# Series as a paper writes them, unbounded (DESIGN.md, an unbounded sum
# compiled): e^x and sin x by Taylor, e^x again by a term of its own, a
# product from 1, and the RIEKF's Gamma_0 and Gamma_1 of a rotation vector,
# (48) of Hartley et al., IJRR 2020: its exponential and its left Jacobian.
# Their values are held to closed forms by cli.cmake's compile_sums_values.
hat(w) = [0, -w[3], w[2]; w[3], 0, -w[1]; -w[2], w[1], 0]
G(m, w) = sum_(q=0) hat(w)^q/!(q + m)
t(x)_0 = 1
t(x)_k = t(x)_(k-1)*x/k

sums(x_n, w_n[j<=3]) = {
    y_n = sum_(q=0) x_n^q/!q
    z_n = sum_(q=0) (-1)^q*x_n^(2*q + 1)/!(2*q + 1)
    u_n = sum_(q=0) t(x_n)_q
    p_n = prod_(q=1) (1 + (x_n/10)^(2^q))
    R_n = G(0, w_n)
    J_n = G(1, w_n)
}
summed = sums(x_n = n/10 - 2, w_n = [n/20; -1/3; 1/4])

# A factorial of what moves, NaN where the interpreter refuses it, below 0
# and between whole numbers; exact in doubles up to !22.
half(x_n) = {
    f_n = !(x_n/2)
}
halving = half(x_n = n/4 - 2)

# Odd terms that cancel, as newton.ink's evens: a step of 0 (C353).
odds(x_n) = {
    y_n = sum_(q=1) (x_n^q/(q^2*2^q) + (-x_n)^q/(q^2*2^q))
}
cancelled = odds(x_n = n/100 - 0.121)

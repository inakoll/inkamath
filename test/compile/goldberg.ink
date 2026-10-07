# D. Goldberg, "What Every Computer Scientist Should Know About
# Floating-Point Arithmetic" (1991), held to the step. Sections and theorems
# are the paper's; data/goldberg.ink has its numbers.

# Cancellation: $100 a day at 6% compounded daily. In float, written as it
# is, hundreds of units off; by Theorem 4's ln(1 + x), a few.
deposit(i = 6/100, m = 365) = {
    x = i/m
    L(y) | 1 + y == 1 = y
    L(y) = y*log(1 + y)/((1 + y) - 1)
    naive_n = 100*((1 + x)^m - 1)/x
    thm4_n = 100*(exp(m*L(x)) - 1)/x
}
year = deposit()

# Theorem 3: Heron's formula (6) parts from the exact area of a needle
# whose sides the target holds, where s rounds; Kahan's (7) does not.
area(a = 9, b = 453/100, c = 453/100) = {
    s = (a + b + c)/2
    heron_n = (s*(s - a)*(s - b)*(s - c))^(1/2)
    kahan_n = ((a + (b + c))*(c - (a - b))*(c + (a - b))*(a + (b - c)))^(1/2)/4
}
needle = area(b = 9/2 + 3/2^21, c = 9/2 + 3/2^21)
needled = area(b = 9/2 + 3/2^50, c = 9/2 + 3/2^50)

# The IEEE Standard, Operations and Ambiguity: ((2e-30 + 1e30) - 1e30) - 1e-30
# is -1e-30 for 1e-30, which --check passes, measuring absolutely below 1
# (Next in line); (x + y) + z against x + (y + z) at 1e30, -1e30, 1 parts.
assoc(x_n) = {
    s_n = ((2*10^-30 + x_n) - x_n) - 10^-30
    t_n = (x_n + (0 - x_n)) + 1
    w_n = x_n + ((0 - x_n) + 1)
}
big = assoc(x_n = 10^30)

# Theorem 7: (m/n)*n is m for n = 2^i + 2^j; 49 is not of that form.
thm7(x_n) = {
    t_n = (x_n/10)*10 - x_n
    u_n = (x_n/49)*49 - x_n
}
seven = thm7(x_n = n + 1)

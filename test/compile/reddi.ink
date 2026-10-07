# Reddi, Kale and Kumar, ICLR 2018 (arXiv 1904.09237; MANIFESTO.md, A paper
# conformance suite): Adam, Algorithm 1 with eq. (1), and AMSGrad, Algorithm
# 2, on the losses of Theorems 1 and 2, projected onto [-1, 1], x-hat the
# step before the projection. The iterates start at 1, and are held from
# there (C151).
adam(C = 3, p = 3, alpha = 9/10, b1 = 0, b2 = 1/(1 + C^2), x1 = 1) = {
    f(t, x) = -x
    f(t, x) | mod(t, p) == 1 = C*x
    P(y) = y
    P(y) | y > 1 = 1
    P(y) | y < -1 = -1
    g_t = grad_(x = x_t) f(t, x)
    m_0 = 0
    m_t = b1*m_(t-1) + (1 - b1)*g_t
    v_0 = 0
    v_t = b2*v_(t-1) + (1 - b2)*g_t^2
    h_t = x_(t-1) - alpha/(t - 1)^(1/2)*m_(t-1)/v_(t-1)^(1/2)
    x_1 = x1
    x_t = P(h_t)
    R_0 = 0
    R_t = R_(t-1) + f(t, x_t) - f(t, -1)
}
amsgrad(C = 3, alpha = 9/10, b2 = 1/(1 + C^2)) = {
    f(t, x) = -x
    f(t, x) | mod(t, 3) == 1 = C*x
    P(y) = y
    P(y) | y > 1 = 1
    P(y) | y < -1 = -1
    g_t = grad_(x = x_t) f(t, x)
    v_0 = 0
    v_t = b2*v_(t-1) + (1 - b2)*g_t^2
    w_0 = 0
    w_t = max(w_(t-1), v_t)
    x_1 = 1
    x_t = P(x_(t-1) - alpha/(t - 1)^(1/2)*g_(t-1)/w_(t-1)^(1/2))
    R_0 = 0
    R_t = R_(t-1) + f(t, x_t) - f(t, -1)
}
thm1 = adam()
fixed = amsgrad()
gen = adam(C = 20, p = 20, alpha = 1, b1 = 1/2, b2 = 1/2, x1 = -1)

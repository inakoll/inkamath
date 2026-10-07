# Reddi, Kale, Kumar, "On the Convergence of Adam and Beyond", ICLR 2018
# (arXiv 1904.09237). Every expected value below is the paper's or worked
# out by hand from its equations; none was recorded.
#
# Section 3: f_t(x) = Cx when t mod 3 = 1, -x otherwise, on F = [-1, 1].
# Adam is Algorithm 1 with eq. (1), no bias correction (footnote 1), and
# the proof of Theorem 1 (Appendix A) takes beta1 = 0, beta2 = 1/(1+C^2),
# alpha_t = alpha/sqrt(t), alpha < sqrt(1-beta2). x-hat is the step before
# the projection, as Algorithm 1 writes it.
>> adam(C = 3, p = 3, alpha = 9/10, b1 = 0, b2 = 1/(1 + C^2), x1 = 1) = {
..     f(t, x) = -x
..     f(t, x) | mod(t, p) == 1 = C*x
..     P(y) = y
..     P(y) | y > 1 = 1
..     P(y) | y < -1 = -1
..     g_t = grad_(x = x_t) f(t, x)
..     m_0 = 0
..     m_t = b1*m_(t-1) + (1 - b1)*g_t
..     v_0 = 0
..     v_t = b2*v_(t-1) + (1 - b2)*g_t^2
..     h_t = x_(t-1) - alpha/(t - 1)^(1/2)*m_(t-1)/v_(t-1)^(1/2)
..     x_1 = x1
..     x_t = P(h_t)
..     R_T = sum_(t=1)^T (f(t, x_t) - f(t, -1))
.. }
adam(C = 3, p = 3, alpha = 9/10, b1 = 0, b2 = 1/(1 + C^2), x1 = 1) = { ... }

>> a = adam()
a = adam()

# v_1 = (1-beta2)C^2, v_2 = beta2 v_1 + 1 - beta2, v_3 likewise.
>> a.v_1
8.1

>> a.v_3
1.071

# x_2 = 1 - alpha/sqrt(1-beta2) = 1 - sqrt(9/10), eq. (4) at t = 0.
>> a.x_2
~0.0513167019

# x_(3t+1) = 1 for every t, the claim proved by induction.
>> a.x_4
1

>> sum_(k=0)^100 (a.x_(3*k+1) == 1)
101

>> sum_(t=1)^300 (a.x_t > 0)
300

# R_3 = C - x_2 - x_3 + C - 2, and R_T >= (2C-4)T/3.
>> a.R_3
~3.41070233

>> a.R_300 >= (2*3 - 4)*300/3
1

# Eq. (6)'s second inequality needs 1/sqrt(3t+3) >= 1/sqrt(2(3t+1)),
# false at t = 0; the claim T_2 >= T_1 still holds there, since with
# b = beta2 it reads 1/sqrt(2(2-b)) + 1/sqrt(3(1+2b-b^2)) >= 1.
>> 1/(3*0 + 3)^(1/2) >= 1/(2*(3*0 + 1))^(1/2)
0

>> q(b) = 1/(2*(2 - b))^(1/2) + 1/(3*(1 + 2*b - b^2))^(1/2)
q(b) = 1/(2*(2 - b))^(1/2) + 1/(3*(1 + 2*b - b^2))^(1/2)

>> q(1/5) >= 1
1

# AMSGrad, Algorithm 2 with beta_1t = 0: v-hat the running max of v.
>> ams(C = 3, alpha = 9/10, b2 = 1/(1 + C^2)) = {
..     f(t, x) = -x
..     f(t, x) | mod(t, 3) == 1 = C*x
..     P(y) = y
..     P(y) | y > 1 = 1
..     P(y) | y < -1 = -1
..     g_t = grad_(x = x_t) f(t, x)
..     v_0 = 0
..     v_t = b2*v_(t-1) + (1 - b2)*g_t^2
..     w_0 = 0
..     w_t = max(w_(t-1), v_t)
..     x_1 = 1
..     x_t = P(x_(t-1) - alpha/(t - 1)^(1/2)*g_(t-1)/w_(t-1)^(1/2))
..     R_T = sum_(t=1)^T (f(t, x_t) - f(t, -1))
.. }
ams(C = 3, alpha = 9/10, b2 = 1/(1 + C^2)) = { ... }

>> z = ams()
z = ams()

# x_3 = x_2 + alpha/sqrt(2 w_2), w_2 = v_1 = 8.1.
>> z.x_3
~0.2749235

# Theorem 4's bound with beta_1t = 0, D = 2: v-hat_300 is the 3-cycle's
# fixed point 8.199/0.999, the sum of g^2 1100.
>> z.w_300
~8.20720721

>> z.R_300 <= 2^2*300^(1/2)/z.alpha*z.w_300^(1/2) + z.alpha*(1 + log(300))^(1/2)/(1 - z.b2)^(1/2)*1100^(1/2)
1

# Theorem 2 (Appendix B): period C, the C that eq. (7) asks of
# beta1 = beta2 = 1/2 is 20, and eq. (9) holds exactly.
>> gen = adam(C = 20, p = 20, alpha = 1, b1 = 1/2, b2 = 1/2, x1 = -1)
gen = adam(C = 20, p = 20, alpha = 1, b1 = 1/2, b2 = 1/2, x1 = -1)

>> c7(C, b1, b2, y) = ((1 - b1)*b1^(C-1)*C <= 1 - b1^(C-1)) and (b2^((C-2)/2)*C^2 <= 1) and (3*(1 - b1)/(2*(1 - b2)^(1/2))*(1 + y*(1 - y^(C-1))/(1 - y)) + b1^(C/2-1)/(1 - b1) < C/3)
c7(C, b1, b2, y) = ((1 - b1)*b1^(C-1)*C <= 1 - b1^(C-1)) and (b2^((C-2)/2)*C^2 <= 1) and (3*(1 - b1)/(2*(1 - b2)^(1/2))*(1 + y*(1 - y^(C-1))/(1 - y)) + b1^(C/2-1)/(1 - b1) < C/3)

>> c(C) = c7(C, 1/2, 1/2, (1/2)/(1/2)^(1/2))
c(C) = c7(C, 1/2, 1/2, (1/2)/(1/2)^(1/2))

>> sum_(j=1)^9 c(2*j)
0

>> c(20)
1

# m_20 = -(1 - b1^19) + (1-b1) b1^19 C, from m_0 = 0.
>> frac gen.m_20
-524277/524288

>> sum_(k=0)^99 (gen.m_(20*k+20) == -(1 - (1/2)^19) + (1/2)*(1/2)^19*20 + (1/2)^20*gen.m_(20*k))
100

>> sum_(k=0)^100 (gen.m_(20*k) <= 0)
101

# x_(kC) = 1 from some T', and each period after it a regret of 2.
>> sum_(k=1)^100 (gen.x_(20*k) == 1)
100

>> gen.R_2000 - gen.R_20 >= 2*(2000 - 20)/20
1

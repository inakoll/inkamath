# CTM's inverted pendulum, digital LQR: pend/digINVSS.html at
# https://www.ece.ualberta.ca/~tchen/ctm/examples/, "Discrete state-space",
# "Control design via pole placement" and "Reference input". A and B are
# test/data/ctms.ink's, from pend/invpen.html. The zero-order hold at Ts =
# 0.01 by Van Loan's block e^([A, B; 0, 0] Ts) = [F, G; 0, 1], a 20-term
# series of exact terms within 6e-17 of scipy's expm; the gain by 12
# doublings of the Riccati equation (SDA, as blocks.ink's 'dare'), settled
# from 9 to 9e-13 of scipy's solve_discrete_are, Q = diag(5000, 0, 100, 0),
# R = 1: K = [-61.9933, -33.5040, 95.0597, 18.8300], the poles of F - GK
# 0.9157 +- 0.0728i and 0.9535 +- 0.0079i. The page's dlqr ran on its
# 4-place F and G, which give its 0.9156 +- 0.0729i. The reference gain the
# page found by trial, -61.55, computed: 1/(C (I - F + GK)^-1 G) = K[1],
# -61.9933, as the cart's position is an integrator. The series is a call,
# so remembered: as a plain definition, read again by each of F's cells at
# each step, it took --check 8 minutes.
#
#     cart: 100 steps from 0, against exact values until 0 and inexact ones from there
#     cart.s: within 0; the interpreter's terms about <e> from the exact ones
#     cart.u: within 0; the interpreter's terms about <e> from the exact ones
pendulum(ref = 2/10) = {
    A = [0, 1, 0, 0; 0, -2/11, 147/55, 0; 0, 0, 0, 1; 0, -5/11, 343/11, 0]
    B = [0; 20/11; 0; 50/11]
    zoh(T) = sum_(q=0)^20 ([A, B; 0, 0]*T)^q/!q
    F[j<=4, k<=4] = zoh(1/100)[j, k]
    G[j<=4] = zoh(1/100)[j, 5]
    w(Q)_k = (F^0 + v(Q)_k*h(Q)_k)^(0-1)
    a(Q)_0 = F
    a(Q)_k = a(Q)_(k-1)*w(Q)_(k-1)*a(Q)_(k-1)
    v(Q)_0 = G*G'
    v(Q)_k = v(Q)_(k-1) + a(Q)_(k-1)*w(Q)_(k-1)*v(Q)_(k-1)*a(Q)_(k-1)'
    h(Q)_0 = Q
    h(Q)_k = h(Q)_(k-1) + a(Q)_(k-1)'*h(Q)_(k-1)*w(Q)_(k-1)*a(Q)_(k-1)
    X = h([5000, 0, 0, 0; 0, 0, 0, 0; 0, 0, 100, 0; 0, 0, 0, 0])_12
    K = (1 + G'*X*G)^(0-1)*G'*X*F
    s_0 = [0; 0; 0; 0]
    s_n = F*s_(n-1) + G*u_(n-1)
    u_n = ref/([1, 0, 0, 0]*(F^0 - F + G*K)^(0-1)*G) - K*s_n
}
cart = pendulum()

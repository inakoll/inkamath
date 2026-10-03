# An LQR gain by its Riccati recurrence (MANIFESTO.md, A paper conformance
# suite): a double integrator, the cost-to-go iterated from Q, and the gain
# read off each iterate, exactly.
lqr(r = 1) = {
    A = [1, 1; 0, 1]
    B = [0; 1]
    Q = [1, 0; 0, 1]
    P_0 = Q
    K_n = (r + B'*P_(n-1)*B)^(0-1)*B'*P_(n-1)*A
    P_n = A'*P_(n-1)*A - A'*P_(n-1)*B*K_n + Q
}
gain = lqr()

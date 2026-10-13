# The Cholesky factor compiled (DESIGN.md, next in line): a tracker of
# position and velocity whose position sensor, of deviation 1/1000, is
# 10,000 times finer than its prior, 10. 'factored' carries its
# covariance as a factor, S_n with P_n = S_n S_n', and updates it in the
# Joseph form written as a square, N_n N_n' with N_n = [(I - K H) Sp,
# K r]; 'naive' carries P_n and updates it as (I - K H) Pp. Exactly, both
# are the same filter, and in doubles each step computes the
# interpreter's doubles, by the same operations:
#
#     factored: 100 steps from 0, against exact values until 1 and inexact ones from there
#     factored.<s>: within 0, from 1 for Sp, K and N; the interpreter's terms about <x> from the exact ones
#
# a line for each of Sp, K, N, S and x; naive's are within a rounding of
# its terms, exact in the interpreter but L's.
#
# In float, the first gain is 1 - 5e-9, which rounds to 1: naive's P_1
# has 0 where the exact one has 20001/20001000100, about 1e-6, so its
# first pivot is not above 0 and chol is NaN, where the interpreter's
# factor is the root of that fraction, the double 0.00099999999750012493:
#
#     naive: 100 steps from 0 in float, against exact values until 1 and inexact ones from there
#     naive.L[1,1]: nan at 1, where the interpreter gives 0.00099999999750012493; the interpreter's term about <x> from the exact one
#
# and its program fails. factored's square keeps K r's part of P_1, so
# every one of its terms stays within the float tolerance: thousands of
# units of a float and more for Sp, N and S, whose cells close on 0, and
# a few for its estimate x.
sqrtkf(z_n) = {
    F = [1 1; 0 1]
    H = [1 0]
    Q = [1/100 0; 0 1/100]
    I = [1 0; 0 1]
    S_0 = [10 0; 0 10]
    x_0 = [0; 0]
    Sp_n = chol(F*S_(n-1)*(F*S_(n-1))' + Q)
    K_n = Sp_n*(H*Sp_n)'/((H*Sp_n)*(H*Sp_n)' + 1/1000000)
    N_n = [(I - K_n*H)*Sp_n, K_n/1000]
    S_n = chol(N_n*N_n')
    x_n = F*x_(n-1) + K_n*(z_n - H*F*x_(n-1))
}
kalman(z_n) = {
    F = [1 1; 0 1]
    H = [1 0]
    Q = [1/100 0; 0 1/100]
    I = [1 0; 0 1]
    P_0 = [100 0; 0 100]
    x_0 = [0; 0]
    Pp_n = F*P_(n-1)*F' + Q
    K_n = Pp_n*H'/(H*Pp_n*H' + 1/1000000)
    P_n = (I - K_n*H)*Pp_n
    L_n = chol(P_n)
    x_n = F*x_(n-1) + K_n*(z_n - H*F*x_(n-1))
}
factored = sqrtkf(z_n = n/2)
naive = kalman(z_n = n/2)

# chol of a single value is its root, a function of the header's own as
# the prelude's functions of a number are, and grad through chol follows
# its definition, here 1/(2 sqrt(a - 1/4)), each term held to the
# interpreter's:
#
#     sole: 100 steps from 0, against exact values until 0 and inexact ones from there
#     sole.<s>: within <e>; the interpreter's terms about <x> from the exact ones
#
# for g and y.
single() = {
    y_n = chol(n + 1)
    g_n = grad_(a = n + 4) chol([4 1; 1 a])[2,2]
}
sole = single()

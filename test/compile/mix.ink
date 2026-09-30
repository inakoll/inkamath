# A model compiled by name, 'inkamath --compile mix.ink mix -o mix.h': two
# inputs mixed and smoothed, one parameter's default read from the file.
tau = 1/4
mix(g = 1/2, a = tau, u_n, b_n) = {
    m_n = g*u_n + (1-g)*b_n
    v_0 = 0
    v_n = a*m_n + (1-a)*v_(n-1)
}

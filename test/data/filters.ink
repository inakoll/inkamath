# A library for models.ink: two filters over a default of the file's.
a0 = 1/10

lowpass(a = a0, u_n) = {
    v_0 = 0
    v_n = a*u_n + (1-a)*v_(n-1)
}

highpass(a = a0, u_n) = {
    low = lowpass(a, u_n)
    v_n = u_n - low.v_n
}

# Unnamed instances of a model with memory, each kept as one of its own and
# named after where it is written and the cell it is written for: a bank of
# filters, one to a row, and one more smoothing the last row.
smooth(a = 1/2, u_n) = {
    v_0 = 0
    v_n = a*u_n + (1-a)*v_(n-1)
}
bank_n[j<=3, k<=1] = smooth(a = j/4, u_n = x_n).v_n
y_n = smooth(u_n = bank_n[3,1]).v_n

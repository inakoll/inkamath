# One index on a column, compiled (DESIGN.md, next in line): a delay line
# defined by one index, a weighted sum of it, and a row read out of a matrix,
# each term held to the interpreter's by the program --check writes.
line(u_n) = {
    tap_0[j<=3] = 0
    tap_n[j<=3] = tap_(n-1)[j-1]
    tap_n[j<=3] | j == 1 = u_n
    y_n = [1 2 3]*tap_n
    r_n = [1 2; 3 4][2]*tap_n[1]
}
fir = line(u_n = n)

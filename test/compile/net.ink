# A tiny network over the last three samples of x: a delay line that starts
# empty, a layer of two units, each the positive part of what reaches it, and
# a weighted sum of the two.
W = [1/2, -1/4, 1/4; -1/2, 1, 1/2]
v = [1, -1/2]
w_0[j<=3, k<=1] = 0
w_n[j<=3, k<=1] = w_(n-1)[j-1, 1]
w_n[j<=3, k<=1] | j == 1 = x_n
z_n = W*w_n
h_n[j<=2, k<=1] = z_n[j,1]
h_n[j<=2, k<=1] | z_n[j,1] < 0 = 0
y_n = v*h_n

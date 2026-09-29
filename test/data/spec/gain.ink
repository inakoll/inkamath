# A module for scopes.ink: a gain with a bias, fed an input, and a running
# sum of what comes out.
k = 2
b = 1
x_n = input
y_n = k*x_n + b
s_0 = 0
s_n = s_(n-1) + y_n

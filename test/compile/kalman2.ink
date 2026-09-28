# The tracker measuring its position, zp, and the sum of position and
# velocity, zs: two measurements, so its gain inverts a 2x2 matrix.
dt = 1
F = [1 dt; 0 1]
H = [1 0; 1 1]
Q = [1/100 0; 0 1/100]
R = [1 0; 0 1/4]
I[j<=2, k<=2] = j == k
x_0 = [0; 0]
P_0 = [10 0; 0 10]
z_n = [zp_n; zs_n]
xp_n = F*x_(n-1)
Pp_n = F*P_(n-1)*F' + Q
K_n = Pp_n*H'*(H*Pp_n*H' + R)^-1
x_n = xp_n + K_n*(z_n - H*xp_n)
P_n = (I - K_n*H)*Pp_n

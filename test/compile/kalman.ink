# A constant-velocity tracker: the state is [position; velocity], and the host
# measures the position, z, once every dt.
dt = 1
F = [1 dt; 0 1]
H = [1 0]
Q = [1/100 0; 0 1/100]
R = 1
I[j<=2, k<=2] = j == k
x_0 = [0; 0]
P_0 = [10 0; 0 10]
xp_n = F*x_(n-1)
Pp_n = F*P_(n-1)*F' + Q
K_n = Pp_n*H'*(H*Pp_n*H' + R)^-1
x_n = xp_n + K_n*(z_n - H*xp_n)
P_n = (I - K_n*H)*Pp_n
p_n = x_n[1,1]

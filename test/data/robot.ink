# A robot's loop, for robot_loop.ink. Its LQR gain for a cart, a double
# integrator, by doubling the Riccati equation, its three iterates stacked in
# one term so that lim walks them together.
dt = 1/100
F = [1 dt; 0 1]
G = [dt^2/2; dt]
blk(Z, i)[j<=2, k<=2] = Z[2*i - 2 + j, k]
dbl(a, v, h) = [a*(v*h + F^0)^-1*a; v + a*(v*h + F^0)^-1*v*a'; h + a'*h*(v*h + F^0)^-1*a]
Z(Q, R)_0 = [F; G*G'/R; Q]
Z(Q, R)_k = dbl(blk(Z(Q, R)_(k-1), 1), blk(Z(Q, R)_(k-1), 2), blk(Z(Q, R)_(k-1), 3))
gain(Q, R) = (R + G'*blk(lim Z(Q, R), 3)*G)^-1*G'*blk(lim Z(Q, R), 3)*F

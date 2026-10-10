# Robertson's chemical kinetics (Robertson 1966), for explicit.ink and
# implicit.ink: the stiff problem of Hairer and Wanner, Solving Ordinary
# Differential Equations II, sections IV.1 and IV.10, and ROBER of the
# Mazzia-Magherini test set for IVP solvers, whose f and Jacobian these are.
k1 = 0.04
k2 = 3*10^7
k3 = 10^4
f(y) = [-k1*y[1] + k3*y[2]*y[3]; k1*y[1] - k3*y[2]*y[3] - k2*y[2]^2; k2*y[2]^2]
J(y) = [-k1, k3*y[3], k3*y[2]; k1, -k3*y[3] - 2*k2*y[2], -k3*y[2]; 0, 2*k2*y[2], 0]
I = [1 0 0; 0 1 0; 0 0 1]
y0 = [1; 0; 0]

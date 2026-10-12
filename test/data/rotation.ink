# Rotations in rationals, from an exploration of quaternions and rigid
# bodies. Every expected value is 1, an identity, or a value worked outside
# the interpreter: by Fractions for R(q), by sympy for the arm.

# A unit quaternion of rationals [w; x; y; z], Hamilton's convention: its
# rotation matrix (Sola 2017, eq. 115) is exact and orthogonal, q and -q are
# one rotation, and Shepperd's method (1978), the largest of 1 + tr R and the
# diagonal, reads q back, through square roots of perfect squares.
>> R(q) = [q[1]^2+q[2]^2-q[3]^2-q[4]^2, 2*(q[2]*q[3]-q[1]*q[4]), 2*(q[2]*q[4]+q[1]*q[3]); 2*(q[2]*q[3]+q[1]*q[4]), q[1]^2-q[2]^2+q[3]^2-q[4]^2, 2*(q[3]*q[4]-q[1]*q[2]); 2*(q[2]*q[4]-q[1]*q[3]), 2*(q[3]*q[4]+q[1]*q[2]), q[1]^2-q[2]^2-q[3]^2+q[4]^2]
R(q) = [q[1]^2+q[2]^2-q[3]^2-q[4]^2, 2*(q[2]*q[3]-q[1]*q[4]), 2*(q[2]*q[4]+q[1]*q[3]); 2*(q[2]*q[3]+q[1]*q[4]), q[1]^2-q[2]^2+q[3]^2-q[4]^2, 2*(q[3]*q[4]-q[1]*q[2]); 2*(q[2]*q[4]-q[1]*q[3]), 2*(q[3]*q[4]+q[1]*q[2]), q[1]^2-q[2]^2-q[3]^2+q[4]^2]

>> I3[j<=3, k<=3] = j == k
I3[j<=3, k<=3] = j == k

>> tq(R) = R[1,1] + R[2,2] + R[3,3]
tq(R) = R[1,1] + R[2,2] + R[3,3]

>> fromR(R) | tq(R) >= R[1,1] and tq(R) >= R[2,2] and tq(R) >= R[3,3] = [(1 + tq(R))^(1/2)/2; (R[3,2] - R[2,3])/(2*(1 + tq(R))^(1/2)); (R[1,3] - R[3,1])/(2*(1 + tq(R))^(1/2)); (R[2,1] - R[1,2])/(2*(1 + tq(R))^(1/2))]
fromR(R) | tq(R) >= R[1,1] and tq(R) >= R[2,2] and tq(R) >= R[3,3] = [(1 + tq(R))^(1/2)/2; (R[3,2] - R[2,3])/(2*(1 + tq(R))^(1/2)); (R[1,3] - R[3,1])/(2*(1 + tq(R))^(1/2)); (R[2,1] - R[1,2])/(2*(1 + tq(R))^(1/2))]

>> fromR(R) | R[1,1] >= R[2,2] and R[1,1] >= R[3,3] = [(R[3,2] - R[2,3])/(2*(1 + 2*R[1,1] - tq(R))^(1/2)); (1 + 2*R[1,1] - tq(R))^(1/2)/2; (R[1,2] + R[2,1])/(2*(1 + 2*R[1,1] - tq(R))^(1/2)); (R[1,3] + R[3,1])/(2*(1 + 2*R[1,1] - tq(R))^(1/2))]
fromR(R) | R[1,1] >= R[2,2] and R[1,1] >= R[3,3] = [(R[3,2] - R[2,3])/(2*(1 + 2*R[1,1] - tq(R))^(1/2)); (1 + 2*R[1,1] - tq(R))^(1/2)/2; (R[1,2] + R[2,1])/(2*(1 + 2*R[1,1] - tq(R))^(1/2)); (R[1,3] + R[3,1])/(2*(1 + 2*R[1,1] - tq(R))^(1/2))]

>> fromR(R) | R[2,2] >= R[3,3] = [(R[1,3] - R[3,1])/(2*(1 + 2*R[2,2] - tq(R))^(1/2)); (R[1,2] + R[2,1])/(2*(1 + 2*R[2,2] - tq(R))^(1/2)); (1 + 2*R[2,2] - tq(R))^(1/2)/2; (R[2,3] + R[3,2])/(2*(1 + 2*R[2,2] - tq(R))^(1/2))]
fromR(R) | R[2,2] >= R[3,3] = [(R[1,3] - R[3,1])/(2*(1 + 2*R[2,2] - tq(R))^(1/2)); (R[1,2] + R[2,1])/(2*(1 + 2*R[2,2] - tq(R))^(1/2)); (1 + 2*R[2,2] - tq(R))^(1/2)/2; (R[2,3] + R[3,2])/(2*(1 + 2*R[2,2] - tq(R))^(1/2))]

>> fromR(R) = [(R[2,1] - R[1,2])/(2*(1 + 2*R[3,3] - tq(R))^(1/2)); (R[1,3] + R[3,1])/(2*(1 + 2*R[3,3] - tq(R))^(1/2)); (R[2,3] + R[3,2])/(2*(1 + 2*R[3,3] - tq(R))^(1/2)); (1 + 2*R[3,3] - tq(R))^(1/2)/2]
fromR(R) = [(R[2,1] - R[1,2])/(2*(1 + 2*R[3,3] - tq(R))^(1/2)); (R[1,3] + R[3,1])/(2*(1 + 2*R[3,3] - tq(R))^(1/2)); (R[2,3] + R[3,2])/(2*(1 + 2*R[3,3] - tq(R))^(1/2)); (1 + 2*R[3,3] - tq(R))^(1/2)/2]

>> q = [1; 2; 2; 4]/5
q = [1; 2; 2; 4]/5

>> R(q) == [-3/5, 0, 4/5; 16/25, -3/5, 12/25; 12/25, 4/5, 9/25]
1

>> R(q)'*R(q) == I3
1

>> R(-q) == R(q)
1

>> fromR(R(q)) == q
1

# Quaternion kinematics, q' = q [0; w]/2 for a body rate w (Sola 2017,
# sec. 4.6). A first-order step multiplies |q|^2 by exactly 1 + h^2|w|^2/4,
# as Om(w) is skew; the Cayley step keeps |q| = 1 exactly, even for a rate
# that turns.
>> Om(w) = [0, -w[1], -w[2], -w[3]; w[1], 0, w[3], -w[2]; w[2], -w[3], 0, w[1]; w[3], w[2], -w[1], 0]
Om(w) = [0, -w[1], -w[2], -w[3]; w[1], 0, w[3], -w[2]; w[2], -w[3], 0, w[1]; w[3], w[2], -w[1], 0]

>> I4[j<=4, k<=4] = j == k
I4[j<=4, k<=4] = j == k

>> h = 1/10
h = 1/10

>> e_0 = [1; 0; 0; 0]
e_0 = [1; 0; 0; 0]

>> e_n = e_(n-1) + h/2*Om([1/2; -1/3; 1/4])*e_(n-1)
e_n = e_(n-1) + h/2*Om([1/2; -1/3; 1/4])*e_(n-1)

>> e_5'*e_5 == (1 + h^2*(1/4 + 1/9 + 1/16)/4)^5
1

>> y_0 = [1; 0; 0; 0]
y_0 = [1; 0; 0; 0]

>> y_n = (I4 - h/4*Om([1; n/20; 0]))^-1*(I4 + h/4*Om([1; n/20; 0]))*y_(n-1)
y_n = (I4 - h/4*Om([1; n/20; 0]))^-1*(I4 + h/4*Om([1; n/20; 0]))*y_(n-1)

>> y_5'*y_5
1

# Gimbal lock: ZYX angles read back (Diebel 2006, sec. 5.6) from a pitch of
# exactly 90 degrees, each turn a rational point of the circle. The first
# column of the matrix is then [0; 0; -1], and the yaw is not determined.
>> Cz(c, s) = [c, -s, 0; s, c, 0; 0, 0, 1]
Cz(c, s) = [c, -s, 0; s, c, 0; 0, 0, 1]

>> Cy(c, s) = [c, 0, s; 0, 1, 0; -s, 0, c]
Cy(c, s) = [c, 0, s; 0, 1, 0; -s, 0, c]

>> Cx(c, s) = [1, 0, 0; 0, c, -s; 0, s, c]
Cx(c, s) = [1, 0, 0; 0, c, -s; 0, s, c]

>> ang(M) = [atan2(M[2,1], M[1,1]); -asin(M[3,1]); atan2(M[3,2], M[3,3])]
ang(M) = [atan2(M[2,1], M[1,1]); -asin(M[3,1]); atan2(M[3,2], M[3,3])]

>> ang(Cz(3/5, 4/5)*Cy(0, 1)*Cx(8/17, 15/17))
error: atan2 needs y or x other than 0

# A planar 3-link arm: the RNEA (Featherstone 2008, table 5.1) against
# Euler-Lagrange in sympy (Spong et al. 2006, sec. 7.4), and its mass matrix
# by unit accelerations, by grad and by the CRBA (table 6.2), all exact.
>> use arm (tau, H, Hg, Hc)
use arm (tau, H, Hg, Hc)

>> c = [3/5; 5/13; 8/17]
c = [3/5; 5/13; 8/17]

>> s = [4/5; 12/13; -15/17]
s = [4/5; 12/13; -15/17]

>> tau(c, s, [1/2; -1; 1/3], [2; 1/3; -1]) == [11415733/497250; 1903211/994500; 4422877/994500]
1

>> H(c, s) == [1463/221, 4259/1326, 707/663; 4259/1326, 109/51, 29/51; 707/663, 29/51, 1/3]
1

>> Hg(c, s) == H(c, s) and Hc(c, s) == H(c, s)
1

# The derivative of exp(hat(w)) v at w = 0 is -hat(v) (Sola 2017, sec. 4.3),
# by grad through Rodrigues' formula, its series near 0.
>> hat(w) = [0, -w[3], w[2]; w[3], 0, -w[1]; -w[2], w[1], 0]
hat(w) = [0, -w[3], w[2]; w[3], 0, -w[1]; -w[2], w[1], 0]

>> nrm(w) = (w'*w)^(1/2)
nrm(w) = (w'*w)^(1/2)

>> rod(w) | w'*w < 1/10^8 = I3 + (1 - w'*w/6)*hat(w) + (1/2 - w'*w/24)*hat(w)*hat(w)
rod(w) | w'*w < 1/10^8 = I3 + (1 - w'*w/6)*hat(w) + (1/2 - w'*w/24)*hat(w)*hat(w)

>> rod(w) = I3 + sin(nrm(w))/nrm(w)*hat(w) + (1 - cos(nrm(w)))/(w'*w)*hat(w)*hat(w)
rod(w) = I3 + sin(nrm(w))/nrm(w)*hat(w) + (1 - cos(nrm(w)))/(w'*w)*hat(w)*hat(w)

>> v = [1; 2; 3]
v = [1; 2; 3]

>> J(w)[j<=3, k<=3] = (grad_(u = w) (rod(u)*v)[j])[k]
J(w)[j<=3, k<=3] = (grad_(u = w) (rod(u)*v)[j])[k]

>> J([0; 0; 0]) == -hat(v)
1

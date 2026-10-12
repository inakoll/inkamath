# A rotation composed with itself, as a matrix and as a quaternion: q0 =
# (1, 2, 2, 4)/5 is a unit quaternion of rationals, so every power is exact,
# R_n exactly orthogonal and q_n exactly unit; its angle, of cosine -23/25, is
# no rational multiple of pi (Niven), so no power is the identity. The step
# rounds, and o and u measure how far it leaves SO(3) and the unit sphere
# (Sola 2017, eq. 115 for Rq).
L(p) = [p[1], -p[2], -p[3], -p[4]; p[2], p[1], -p[4], p[3]; p[3], p[4], p[1], -p[2]; p[4], -p[3], p[2], p[1]]
Rq(q) = [q[1]^2+q[2]^2-q[3]^2-q[4]^2, 2*(q[2]*q[3]-q[1]*q[4]), 2*(q[2]*q[4]+q[1]*q[3]); 2*(q[2]*q[3]+q[1]*q[4]), q[1]^2-q[2]^2+q[3]^2-q[4]^2, 2*(q[3]*q[4]-q[1]*q[2]); 2*(q[2]*q[4]-q[1]*q[3]), 2*(q[3]*q[4]+q[1]*q[2]), q[1]^2-q[2]^2-q[3]^2+q[4]^2]
I3[j<=3, k<=3] = j == k
spin(q0 = [1; 2; 2; 4]/5) = {
    R_0 = I3
    R_n = R_(n-1)*Rq(q0)
    q_0 = [1; 0; 0; 0]
    q_n = L(q_(n-1))*q0
    E(M) = M'*M - I3
    o_n = sum_(j=1)^3 sum_(k=1)^3 E(R_n)[j,k]^2
    u_n = q_n'*q_n - 1
}
twirl = spin()

# CTM's DC motor speed under a digital PID: motor/digital.html at
# https://www.ece.ualberta.ca/~tchen/ctm/examples/, "Continuous to Discrete
# Conversion" and "PID Controller". The motor of motor/motor.html, held at Ts
# = 0.12 by Van Loan's block, a 24-term series of exact terms; the page's
# controller K (278.67 z^2 - 309.33 z + 78.67)/((z - 1)(1.6 z + 1)), its
# numerator Kp = 100, Ki = 200, Kd = 10 by Tustin (scipy's bilinear gives
# the same), K = 0.2425, as a difference equation. The hold is exact for the
# controller's steps, so y is the motor's speed at the samples: by scipy, a
# peak of 1.0276 at 1.2 s and within 2% of 1 from 1.56 s, as the page's 3%
# and 2 s. The series is a call, so remembered, as in pendloop.ink:
#
#     motor: 100 steps from 0, against exact values until 14 and inexact ones from there
#     motor.x: within 3.6e-15; the interpreter's terms about <e> from the exact ones
#     motor.e: within 3.3e-16; the interpreter's terms about <e> from the exact ones
#     motor.u: within 1.4e-14; the interpreter's terms about <e> from the exact ones
servo(r = 1, k = 2425/10000) = {
    A = [-10, 1; -2/100, -2]
    B = [0; 2]
    zoh(T) = sum_(q=0)^24 ([A, B; 0, 0]*T)^q/!q
    F[j<=2, c<=2] = zoh(12/100)[j, c]
    G[j<=2] = zoh(12/100)[j, 3]
    w = 2/(12/100)
    x_0 = [0; 0]
    x_n = F*x_(n-1) + G*u_(n-1)
    e_n | n < 0 = 0
    e_n = r - x_n[1]
    u_n | n < 0 = 0
    u_n = (6/10*u_(n-1) + u_(n-2) + k*((10*w^2 + 100*w + 200)*e_n + (400 - 20*w^2)*e_(n-1) + (10*w^2 - 100*w + 200)*e_(n-2))/w)/(16/10)
}
motor = servo()

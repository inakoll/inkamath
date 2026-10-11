# Robertson's kinetics (Robertson 1966; Hairer and Wanner II, IV.1 and
# IV.10; ROBER of the Mazzia-Magherini test set), as test/data/robertson.ink
# writes them, compiled: forward Euler from t = 40 just inside and just
# outside its stability limit, h = 2/3392.788124454384, the fast
# eigenvalue's there, and backward Euler from the start, its Newton
# iteration twelve times a step. y2^2 is written y2*y2, so that no libm's
# pow decides where 'outside' parts. Each report was worked out apart from
# the interpreter: the step emulated in Python as its header writes it, in
# doubles and in floats, against the interpreter's run emulated, exact in
# fractions while its terms fit a thousand digits and doubles after.
k1 = 0.04
k2 = 3*10^7
k3 = 10^4
f(y) = [-k1*y[1] + k3*y[2]*y[3]; k1*y[1] - k3*y[2]*y[3] - k2*y[2]*y[2]; k2*y[2]*y[2]]
J(y) = [-k1, k3*y[3], k3*y[2]; k1, -k3*y[3] - 2*k2*y[2], -k3*y[2]; 0, 2*k2*y[2], 0]
I = [1 0 0; 0 1 0; 0 0 1]
euler(r = 1, y40 = [0.7158270687194010; 9.185534764557751e-06; 0.2841637457458290]) = {
    h = 2/3392.788124454384*r
    y_0 = y40
    y_n = y_(n-1) + h*f(y_(n-1))
}
inside = euler(r = 99/100)
outside = euler(r = 101/100)
backward(h = 1/10) = {
    nw(yp)_0 = yp
    nw(yp)_k = nw(yp)_(k-1) - (I - h*J(nw(yp)_(k-1)))^-1*(nw(yp)_(k-1) - yp - h*f(nw(yp)_(k-1)))
    y_0 = [1; 0; 0]
    y_n = nw(y_(n-1))_12
}
rober = backward()

# A cantilever of n Hermite cubic elements, Euler-Bernoulli. Its nodal
# values are exact for any n, the element holding the beam's homogeneous
# solutions (Tong 1969): P L^3/(3 EI) under a tip load, w L^4/(8 EI) under a
# uniform load taken consistently. Taken lumped, forces alone, the tip
# misses by the consistent load's tip moment w h^2/12 times L^2/(2 EI),
# w L^4/(24 EI n^2): 27/28 over n^2 here. Expected, those forms and the
# same meshes solved in sympy.
>> Lb = 3
Lb = 3

>> EI = 7
EI = 7

>> P = 5
P = 5

>> w = 2
w = 2

>> h(n) = Lb/n
h(n) = Lb/n

>> ke(n) = EI/h(n)^3*[12 6*h(n) -12 6*h(n); 6*h(n) 4*h(n)^2 -6*h(n) 2*h(n)^2; -12 -6*h(n) 12 -6*h(n); 6*h(n) 2*h(n)^2 -6*h(n) 4*h(n)^2]
ke(n) = EI/h(n)^3*[12 6*h(n) -12 6*h(n); 6*h(n) 4*h(n)^2 -6*h(n) 2*h(n)^2; -12 -6*h(n) 12 -6*h(n); 6*h(n) 2*h(n)^2 -6*h(n) 4*h(n)^2]

>> G(n, e)[a<=4, p<=2*n+2] = p == 2*e - 2 + a
G(n, e)[a<=4, p<=2*n+2] = p == 2*e - 2 + a

>> S(n)[f<=2*n, p<=2*n+2] = p == f + 2
S(n)[f<=2*n, p<=2*n+2] = p == f + 2

>> Kf(n) = S(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*S(n)'
Kf(n) = S(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*S(n)'

>> fP(n)[f<=2*n] | f == 2*n - 1 = P
fP(n)[f<=2*n] | f == 2*n - 1 = P

>> fe(n) = w*h(n)*[1/2; h(n)/12; 1/2; -h(n)/12]
fe(n) = w*h(n)*[1/2; h(n)/12; 1/2; -h(n)/12]

>> fw(n) = S(n)*sum_(e=1)^n G(n, e)'*fe(n)
fw(n) = S(n)*sum_(e=1)^n G(n, e)'*fe(n)

>> fl(n)[f<=2*n] | mod(f, 2) == 1 = w*h(n)*(1 - (f == 2*n - 1)/2)
fl(n)[f<=2*n] | mod(f, 2) == 1 = w*h(n)*(1 - (f == 2*n - 1)/2)

>> tip(n, f) = (Kf(n)^-1*f)[2*n - 1]
tip(n, f) = (Kf(n)^-1*f)[2*n - 1]

>> sum_(n=1)^8 (tip(n, fP(n)) <> P*Lb^3/(3*EI))
0

>> sum_(n=1)^8 (tip(n, fw(n)) <> w*Lb^4/(8*EI))
0

>> frac [tip(1, fl(1)); tip(2, fl(2)); tip(4, fl(4)); tip(8, fl(8))] - w*Lb^4/(8*EI)
[  27/28;
  27/112;
  27/448;
 27/1792]

# Every node, deflection and slope: w x^2 (6L^2 - 4Lx + x^2)/(24 EI) and its
# derivative at x = 1, 2 and 3.
>> frac (Kf(3)^-1*fw(3))'
[43/84, 19/21, 34/21, 26/21, 81/28, 9/7]

# eig(A, B) of eight unknowns, the cantilever of geneig.ink at four
# elements, apart from it so that each file stays within its timeout.
# Values as in geneig.ink, worked out apart from the interpreter, its
# definitions repeated.

>> h(n) = 1/n
h(n) = 1/n

>> ke(n) = 1/h(n)^3*[12 6*h(n) -12 6*h(n); 6*h(n) 4*h(n)^2 -6*h(n) 2*h(n)^2; -12 -6*h(n) 12 -6*h(n); 6*h(n) 2*h(n)^2 -6*h(n) 4*h(n)^2]
ke(n) = 1/h(n)^3*[12 6*h(n) -12 6*h(n); 6*h(n) 4*h(n)^2 -6*h(n) 2*h(n)^2; -12 -6*h(n) 12 -6*h(n); 6*h(n) 2*h(n)^2 -6*h(n) 4*h(n)^2]

>> me(n) = h(n)/420*[156 22*h(n) 54 -13*h(n); 22*h(n) 4*h(n)^2 13*h(n) -3*h(n)^2; 54 13*h(n) 156 -22*h(n); -13*h(n) -3*h(n)^2 -22*h(n) 4*h(n)^2]
me(n) = h(n)/420*[156 22*h(n) 54 -13*h(n); 22*h(n) 4*h(n)^2 13*h(n) -3*h(n)^2; 54 13*h(n) 156 -22*h(n); -13*h(n) -3*h(n)^2 -22*h(n) 4*h(n)^2]

>> G(n, e)[a<=4, p<=2*n+2] = p == 2*e - 2 + a
G(n, e)[a<=4, p<=2*n+2] = p == 2*e - 2 + a

>> S(n)[f<=2*n, p<=2*n+2] = p == f + 2
S(n)[f<=2*n, p<=2*n+2] = p == f + 2

>> Kf(n) = S(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*S(n)'
Kf(n) = S(n)*(sum_(e=1)^n G(n, e)'*ke(n)*G(n, e))*S(n)'

>> Mf(n) = S(n)*(sum_(e=1)^n G(n, e)'*me(n)*G(n, e))*S(n)'
Mf(n) = S(n)*(sum_(e=1)^n G(n, e)'*me(n)*G(n, e))*S(n)'

>> digits = 12
digits = 12

# Eight unknowns, of four elements.
>> eig(Kf(4), Mf(4))
[~12.3631720798;
 ~486.650937534;
  ~3865.7172606;
 ~15044.8965027;
 ~52046.6722755;
 ~134241.343496;
 ~337385.710043;
 ~908306.291396]

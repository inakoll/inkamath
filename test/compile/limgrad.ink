# A derivative of a limit compiled (DESIGN.md, next in line): grad through
# lim and through a sum without an upper bound, forward mode's walk of a
# value and one part, each by lim's rule, once for each cell of each name,
# as Derivative::Limit walks it. Every point is a double, or a constant the
# step folds, so each term is the interpreter's within 0. The stops below
# are worked out by convergence.hpp's rule over Python's doubles and
# fractions, apart from the interpreter and the compiler, none recorded.

# 'twowalk': with b = 1 - a, s(a, b)_k is 1 - a^k on the way to 1, its part
# by b (1 - a^k)/(1 - a), and its part by a, a sum of k*a^(k-1) terms,
# slower. From a = 1/4 to 0.4975, the walk by b stops where the value alone
# does, at 18 to 33, and the walk by a 3 to 6 terms later, so that v's cell
# by b, taken where a's stops, would be 1.5e-11 to 1.9e-10 off; and g.a,
# L + a*q, reads L where a's walk stops, 1.1e-11 to 9.7e-11 from the value
# lim s(a, b) alone answers, as o reads it beside the part by a. A grad by a
# vector or by two names gives way to forward mode at the limit, whose terms
# move, as the interpreter's does; r's limit does not move, so r stays in
# reverse mode, an ulp from forward mode's at 64 of its steps.
s(a, b)_0 = 0
s(a, b)_k = a*s(a, b)_(k-1) + b
walks(u_n) = {
    v_n = grad_(w = [u_n; 1 - u_n]) lim s(w[1], w[2])
    g_n = grad_(a = u_n, b = 1 - u_n) (a*lim s(a, b) + b)
    r_n = grad_(w = [u_n, 1 - u_n]) w[1, 1]*w[1, 2]*lim s(u_n, 1/2)/3
    o_n = lim s(u_n, 1 - u_n) + grad_(a = u_n) lim s(a, 1 - u_n)
}
twowalk = walks(u_n = ~(1/4 + n/400))

# 'tozero': (t/2)^k never closes on its own size, nor its part, so each walk
# answers the first term where the value and the part are both within
# 1e-10 absolute, at 14 to 20, the value alone at 13 to 18: z after its 100
# terms, and in 'toend' c where its terms end at 40. In float, c keeps the
# same terms, within 13 units of a float; z's terms reach 0, and its walk
# closes on 0 at 55 to 80, as a walk of values alone to 0 does in float.
z(a)_0 = 1
z(a)_k = a*z(a)_(k-1)/2
c(a)_0 = 1
c(a)_k | k < 40 = a*c(a)_(k-1)/2
zero(u_n) = {
    y_n = grad_(t = u_n) lim z(t)
}
tozero = zero(u_n = ~(1/4 + n/400))
ended(u_n) = {
    x_n = grad_(t = u_n) lim c(t)
}
toend = ended(u_n = ~(1/4 + n/400))

# 'blowup': Newton's root converges at 0 and its part does not, the
# interpreter's refusal and the step's NaN: its 100th term is
# (4^100 - 1)/(3*2^100), 4.225502e+29 in doubles as in fractions.
# 'rootmix' is at 0 every third step.
r(x)_0 = 1
r(x)_k = (r(x)_(k-1) + x/r(x)_(k-1))/2
rooted(u_n) = {
    y_n = grad_(t = u_n) lim r(t)
}
blowup = rooted(u_n = ~0)
rootmix = rooted(u_n = ~(mod(n, 3)/4))

# 'nanheld': (x - x)*(2^k/x) is 0, and its part ~0 times -2^k/x^2, which
# overflows from 61 at x = 10^-145: NaN where the value is not. The part
# 2^-k is near with the value first at 34, the term kept and answered at 61,
# its remainder 2^-34 the estimate. 'nanout', at 10^-151, overflows from 21,
# before any term is kept: the interpreter's refusal and the step's NaN,
# its sign the platform's.
zn(x)_0 = x
zn(x)_k = zn(x)_(k-1)/2 + (x - x)*(2^k/x)
fails(u_n) = {
    y_n = grad_(t = u_n) lim zn(t)
}
nanheld = fails(u_n = ~(10^-145))
nanout = fails(u_n = ~(10^-151))

# 'atthird': a limit at a constant point folds exactly, its part too: walked
# in doubles, g3's part at 3/7 stops at 31 as exactly but 4.4e-16 from it,
# and s's parts at [3/7; 4/7], at 32 and 28, 2.2e-16 each, times n.
g3(x)_0 = 1
g3(x)_k = x*g3(x)_(k-1) + 1
constant(u_n) = {
    y_n = grad_(t = 3/7) (u_n*lim g3(t))
    z_n = grad_(w = [3/7; 4/7]) (u_n*lim s(w[1], w[2]))
}
atthird = constant(u_n = n)

# 'alongside': a sequence walked beside the limit, nt read at its index,
# carries its part in its window; a limit of matrices stops by its largest
# cell's step, the part's by its own; and the model's own sequence hq reads
# its parameter h, 4x, at 34 terms, at 2 where x is 0.
nt(x)_0 = 1
nt(x)_k = nt(x)_(k-1)*x/k/2
nu(x)_0 = 0
nu(x)_k = nu(x)_(k-1) + nt(x)_k
mx(x)_0 = [1; 0]
mx(x)_k = [x/2, 0; 1/4, x/3]*mx(x)_(k-1) + [1; x]
beside(u_n, h = 1/2) = {
    hq(x)_0 = 0
    hq(x)_k = h*hq(x)_(k-1) + x^2
    e_n = grad_(t = u_n) lim nu(t)
    m_n = grad_(t = u_n) lim mx(t)
    q_n = grad_(t = u_n) lim hq(t)
}
alongside = beside(u_n = ~(n/50 - 1))

# 'vee': abs and max are continuous where their guards step, so a limit of
# terms that read them is differentiated, 2(|x| - max(x, 0)), -2x below 0
# and 0 above; so is a comparison of the index alone, and so is exp, whose
# guards reduce its argument: 2e^x, once --check no longer refuses it
# (DESIGN.md, the entry's prelude). A sum without an upper bound is walked
# as a limit is, 4t for t^2/2^k.
ab(x)_0 = 0
ab(x)_k = ab(x)_(k-1)/2 + abs(x) - max(x, 0)
gi(x)_0 = 1
gi(x)_k = gi(x)_(k-1)/2 + x*(k > 2)
ee(x)_0 = 1
ee(x)_k = ee(x)_(k-1)/2 + exp(x)
kinks(u_n) = {
    y_n = grad_(t = u_n) lim ab(t)
    x_n = grad_(t = u_n) lim gi(t)
    w_n = grad_(t = u_n) sum_(k=0) t^2/2^k
    e_n = grad_(t = u_n) lim ee(t)
}
vee = kinks(u_n = ~((2*n - 99)/100))

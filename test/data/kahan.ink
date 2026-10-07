# W. Kahan, "How Futile are Mindless Assessments of Roundoff in
# Floating-Point Computation?" (2006), §2, §6, §7, §9 and §10. Every
# expected value is the paper's, typed by hand, never recorded.

# §2: where binary arithmetic and decimal disagree, exact beside rounded.
>> (4/3 - 1)*3 - 1
0

>> ((~(4/3) - 1)*3 - 1)*2^52
-1

>> ~0.4*10 == 4
1

>> ~0.7*10 == 7
1

>> ~0.4*7 == ~0.7*4
0

>> 0.4*7 == 0.7*4
1

>> u = 1/10
u = 1/10

>> t = 3/10
t = 3/10

>> (3*u - t)/(2*u - t + u)
error: division by zero

>> (3*~u - ~t)/(2*~u - ~t + ~u)
2

# §6, a smooth surprise: G(x) is 1 for every real x, yet 0 in double at
# every n to 9999, held here to 999 for the sanitizers' time; T computed
# as Th is, through log, gives 1 back.
>> T(z) = (exp(z) - 1)/z
T(z) = (exp(z) - 1)/z

>> T(z) | z == 0 = 1
T(z) | z == 0 = 1

>> Q(y) = abs(y - (y^2 + 1)^(1/2)) - 1/(y + (y^2 + 1)^(1/2))
Q(y) = abs(y - (y^2 + 1)^(1/2)) - 1/(y + (y^2 + 1)^(1/2))

>> G(x) = T(Q(x)^2)
G(x) = T(Q(x)^2)

>> sum_(n=1)^999 G(n)
0

>> Th(t, z) = (t - 1)/log(t)
Th(t, z) = (t - 1)/log(t)

>> Th(t, z) | t == 1 = t
Th(t, z) | t == 1 = t

>> Th(t, z) | t == 0 = -1/z
Th(t, z) | t == 0 = -1/z

>> Gk(x) = Th(exp(Q(x)^2), Q(x)^2)
Gk(x) = Th(exp(Q(x)^2), Q(x)^2)

>> sum_(n=1)^999 Gk(n)
999

# §7: Spike(x) at the double nearest 4/3 and its neighbours, and at 4/3,
# where it has no value: the log of 0 (C156).
>> Spike(x) = 1 + x^2 + log(abs(1 + 3*(1 - x)))/80
Spike(x) = 1 + x^2 + log(abs(1 + 3*(1 - x)))/80

>> digits = 16
digits = 16

>> Spike(~(4/3) - ~(1/2^52))
~2.344560789927811

>> Spike(~(4/3))
~2.327232110413813

>> Spike(~(4/3) + ~(1/2^52))
~2.335896450170813

>> digits = 9
digits = 9

>> Spike(4/3)
error: log needs a number above 0, not 0

# §9: ProSolveur's ill-conditioned system, solved exactly: x = 3B and
# y = -3A.
>> [4194304 4194303; 4194303 4194302]^-1*[0; 3]
[ 12582909;
 -12582912]

# §10: 128 square roots, then 128 squares, each rounded: correctly
# rounded roots take every X in (0, 1) to 0 and every X > 1 to 1 (C153).
>> r(x)_0 = x
r(x)_0 = x

>> r(x)_k = r(x)_(k-1)^(1/2)
r(x)_k = r(x)_(k-1)^(1/2)

>> s(x)_0 = r(x)_128
s(x)_0 = r(x)_128

>> s(x)_k = s(x)_(k-1)^2
s(x)_k = s(x)_(k-1)^2

>> s(0)_128
0

>> s(1)_128
1

>> s(3/4)_128
0

>> s(2)_128
1

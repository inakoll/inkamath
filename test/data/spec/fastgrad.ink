# The prelude's exp, log, tanh, sin and cos called compiled under grad
# (DESIGN.md, next in line): a call of the prelude's own definition on a
# real double whose jet is the value and one part is answered by the
# header's value and part functions, which perform the definition's
# operations on the same doubles. So nothing they answer moves, and where
# they would answer otherwise the definition is walked. Values that move
# from a refusal are mpmath's, at nine digits; every other one is what
# walking the definitions answers.
>> digits = 17
digits = 17

# A part exact, as the seed's 1 and data's decimals make it, through the
# three whose definitions round it at their '~' before any arithmetic; the
# value read as well.
>> grad_(x = ~1) exp(x)
~2.7182818284590451

>> grad_(x = ~(-3.5)) exp(x)*x
~-0.075493458555796256

>> grad_(x = ~1) cos(x)
~-0.8414709848078965

>> grad_(x = ~100000.5) sin(x/3)
~0.12018128987656208

>> grad_(x = ~2) sin(x)*cos(x)
~-0.65364362086361183

# A part that is a double, which tanh and log take.
>> grad_(x = ~3) log(~2*x)
~0.33333333333333331

>> grad_(x = ~0.5) tanh(~1*x)
~0.7864477329659274

>> grad_(v = [~1; ~2]) exp(v[1]*v[2])
[~14.778112197861301;
 ~7.3890560989306504]

>> grad_(w = ~0.3) log(1/(1 + exp(-w*1.5)))
~0.584041149076167

# tanh and log meet an exact part exactly first, in -2*x and x/2^k, so they
# walk it: rounded first, these would be ~4.7146912433387059e-313 and inf.
>> grad_(x = ~3) log(x)
~0.33333333333333331

>> grad_(x = ~0.5) tanh(x)
~0.7864477329659274

>> grad_(t = ~0) tanh(~0.5 + t*599492/10^318)
~4.7146912432892993e-313

>> grad_(t = ~(1.5e-300)) log(t*10^200*10^200)
~6.6666666666666663e+299

# Where the clause taken has no part, past 20 for tanh and 1000 for exp, the
# header's part is 0 and the walk has none, which an exact 0, a product with
# an infinity and a floor tell apart; so is a part of 0.
>> grad_(x = ~25) tanh(~1*x)
0

>> 1/grad_(x = ~25) tanh(~1*x)
error: division by zero

>> grad_(x = ~1001) exp(x)*~(10^400)
0

>> grad_(x = ~25) floor(tanh(~1*x))*x
1

>> grad_(x = ~0) exp(x*0)
0

>> 1/grad_(x = ~0) exp(x*0)
inf

# Where a reduction's floor jumps the header's part is NaN, and the walk
# refuses in its own words.
>> grad_(x = ~0.34657359027997264) exp(x)
error: floor jumps at x = ~0.34657359

>> grad_(x = ~0.17328679513998632) tanh(~1*x)
error: floor jumps at x = ~0.173286795

>> grad_(x = ~0.7853981633974483) sin(x)
error: floor jumps at x = ~0.785398163

>> grad_(x = ~0.7853981633974483) cos(x)
error: floor jumps at x = ~0.785398163

# Walked as before: a grad of a grad, whose jets carry four parts; an exact
# point; a complex one.
>> grad_(s = ~1) grad_(t = ~2) exp(s*t)
~22.167168296791953

>> grad_(x = 1) exp(x)
~2.7182818284590451

>> grad_(x = ~1 + i) exp(x)
error: a comparison needs real numbers, not 1+i

>> digits = 9
digits = 9

# A compiled call is one step and one reference deep. Walked under grad exp
# is some 5 steps, tanh 7, sin and cos 8 and log 20, so this softplus ran
# out of its million.
>> grad_(t = ~1) sum_(k=1)^50000 log(1 + exp(-t*k/50000))
~-8528.00195

# Remembered as walked, a call asked again costs no step, nor any depth.
>> grad_(t = ~1) sum_(k=1)^600000 exp(t)
~1630969.1

# Walked under grad exp nests 3 references deeper, tanh 4, and log, sin and
# cos 5, so each ran out of depth; one call deeper than the last answer is
# refused, as are what still walks: an exact point, an exact part through
# tanh or log, and a grad inside.
>> dive(k, t) = dive(k - 1, t)
dive(k, t) = dive(k - 1, t)

>> dive(k, t) | k < 1 = exp(t)
dive(k, t) | k < 1 = exp(t)

>> grad_(t = ~3) dive(254, t)
~20.0855369

>> grad_(t = ~3) dive(255, t)
error: evaluation nests more than 256 references deep

>> grad_(t = ~3) (exp(t) + dive(255, t))
~40.1710738

>> grad_(t = 3) dive(254, t)
error: evaluation nests more than 256 references deep

>> dive(k, t) | k < 1 = tanh(~1*t)
dive(k, t) | k < 1 = tanh(~1*t)

>> grad_(t = ~0.5) dive(254, t)
~0.786447733

>> dive(k, t) | k < 1 = tanh(t)
dive(k, t) | k < 1 = tanh(t)

>> grad_(t = ~0.5) dive(254, t)
error: evaluation nests more than 256 references deep

>> dive(k, t) | k < 1 = log(~2*t)
dive(k, t) | k < 1 = log(~2*t)

>> grad_(t = ~3) dive(254, t)
~0.333333333

>> dive(k, t) | k < 1 = log(t)
dive(k, t) | k < 1 = log(t)

>> grad_(t = ~3) dive(254, t)
error: evaluation nests more than 256 references deep

>> dive(k, t) | k < 1 = sin(t)
dive(k, t) | k < 1 = sin(t)

>> grad_(t = ~1) dive(254, t)
~0.540302306

>> dive(k, t) | k < 1 = cos(t)
dive(k, t) | k < 1 = cos(t)

>> grad_(t = ~1) dive(254, t)
~-0.841470985

>> dive(k, t) | k < 1 = grad_(s = ~2) exp(s*t)
dive(k, t) | k < 1 = grad_(s = ~2) exp(s*t)

>> grad_(t = ~1) dive(254, t)
error: evaluation nests more than 256 references deep

# A call whose jet has no part is the value alone, as outside grad.
>> deep(k, a, t) = deep(k - 1, a, t)
deep(k, a, t) = deep(k - 1, a, t)

>> deep(k, a, t) | k < 1 = exp(a)*t
deep(k, a, t) | k < 1 = exp(a)*t

>> grad_(t = ~1) deep(254, ~2, t)
~7.3890561

# A session's exp is its own definition, never the header's.
>> exp(x) = x*x
exp(x) = x*x

>> grad_(x = ~3) exp(x)
6

# The prelude's exp, log, tanh and ilogb called compiled (DESIGN.md, next in
# line): a call of the prelude's own definition on a real double, not one
# past a thousand digits, is answered by the C function --compile writes for
# it, which performs the definition's operations on the same doubles. So
# nothing it answers moves. Expected values are those of the design's own
# arithmetic, emulated apart from the interpreter at seventeen digits, and
# mpmath's at nine.
>> digits = 17
digits = 17

>> exp(~1)
~2.7182818284590455

>> exp(~(-1))
~0.36787944117144233

>> exp(~30)
~10686474581524.461

>> exp(~709.78)
~1.7928227943945155e+308

>> exp(~(-745))
~4.9406564584124654e-324

>> log(~2)
~0.69314718055994529

>> log(~0.1)
~-2.3025850929940455

>> log(~(5e-324))
~-744.44007192138122

>> log(~1.7976931348623157e308)
~709.78271289338397

>> tanh(~0.5)
~0.46211715726000974

>> tanh(~(-1e-9))
~-1.0000000000000001e-09

>> tanh(~19.5)
1

>> ilogb(~0.1)
-4

>> ilogb(~(5e-324))
-1074

>> ilogb(~1.7976931348623157e308)
1023

# An answer of 0 has the definition's sign, whose minus is a subtraction
# from 0 (C33), so never -0, as the compiled one's is since C98. Past
# -745.13 exp is 0.
>> tanh(~0)
0

>> 1/tanh(~0)
inf

>> 1/tanh(~0*(-1))
inf

>> 1/log(~1)
inf

>> 1/exp(~(-800))
inf

# An exact argument is reduced exactly and rounded once, where C rounds it
# first: the definition answers it, and the double nearest it is another
# call.
>> exp(2/3)
~1.947734041054676

>> exp(~(2/3))
~1.9477340410546757

>> log(1/10)
~-2.3025850929940459

>> tanh(2/3)
~0.5827829453479102

>> tanh(~(2/3))
~0.58278294534791009

>> digits = 9
digits = 9

# Not a double that is real: refused in its name, as a matrix is (C147).
>> exp(~1 + i)
error: exp needs real numbers, not 1+i

>> exp(i*pi)
error: exp needs real numbers, not i*~3.14159265

>> log(i)
error: log needs real numbers, not i

>> tanh(i)
error: tanh needs real numbers, not i

>> exp([~1 ~2])
error: exp needs single values, not a 1x2 matrix; write it by its cells

# Out of log's domain, or not finite, as the definition answers it, where
# the C function would be NaN or inf.
>> log(~0)
error: division by zero

>> log(~(-2))
error: division by zero

>> log(~1/0)
inf

>> exp(~0/0)
error: a comparison needs a number, not -nan

>> exp(~1/0)
inf

# ilogb tries no power of two past 2^3321 (C101).
>> ilogb(~1/0)
3321

# A double approximated past a thousand digits stays so through the
# definition, which says it.
>> a = (10^600 + 1)/10^600
a = (10^600 + 1)/10^600

>> exp(a*a)
~2.71828183  # approximated past a thousand digits

>> log(a*a)
0  # approximated past a thousand digits

# grad carries its parts through the definitions, as before.
>> grad_(x = ~1) exp(x)
~2.71828183

>> grad_(x = ~3) log(x)
~0.333333333

>> grad_(x = ~0.5) tanh(x)
~0.786447733

# A compiled call is one step and one reference deep, as reading a value
# is. Through the definition exp is some 33 steps and log some 111, so
# these lines ran out of their million until each call had its own (C149);
# their sums are mpmath's.
>> sum_(k=1)^40000 exp(~k/40000)
~68732.1323

>> sum_(k=1)^10000 log(~k)
~82108.9278

# Through the definition exp nests 3 references deeper, tanh 5, ilogb 14 and
# log 16, so each of these ran out of depth. dive(254) is the deepest whose
# clause may read a value, so a call any deeper than one is refused.
>> dive(k) = dive(k - 1)
dive(k) = dive(k - 1)

>> dive(k) | k < 1 = exp(~3)
dive(k) | k < 1 = exp(~3)

>> dive(254)
~20.0855369

>> dive(k) | k < 1 = tanh(~3)
dive(k) | k < 1 = tanh(~3)

>> dive(254)
~0.995054754

>> dive(k) | k < 1 = ilogb(~3)
dive(k) | k < 1 = ilogb(~3)

>> dive(254)
1

>> dive(k) | k < 1 = log(~3)
dive(k) | k < 1 = log(~3)

>> dive(254)
~1.09861229

# Which calls walk the definition, seen by their depth: an exact argument
# and an approximated one.
>> dive(k) | k < 1 = exp(3)
dive(k) | k < 1 = exp(3)

>> dive(254)
error: evaluation nests more than 256 references deep

>> dive(k) | k < 1 = log(3)
dive(k) | k < 1 = log(3)

>> dive(254)
error: evaluation nests more than 256 references deep

>> dive(k) | k < 1 = exp(a*a)
dive(k) | k < 1 = exp(a*a)

>> dive(254)
error: evaluation nests more than 256 references deep

# A session's clause on a name of the prelude starts a definition of its
# own, as a model's or a file's does, so no clause of exp holds at ~1 or ~3.
>> exp(x) | x > 5000 = 7
exp(x) | x > 5000 = 7

>> exp(~1)
error: no clause of exp applies

>> dive(k) | k < 1 = exp(~3)
dive(k) | k < 1 = exp(~3)

>> dive(254)
error: evaluation nests more than 256 references deep

>> dive(250)
error: no clause of exp applies

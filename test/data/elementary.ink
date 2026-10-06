# exp, log and tanh written again in the prelude (DESIGN.md, next in line):
# each an exact reduction, then a polynomial of fixed degree in doubles,
# accurate to a few units in the last place, by the operations the compiled
# step performs on the same doubles. An exact argument is reduced exactly and
# rounded where its reduction ends; a double is computed as C computes it.
# Expected values are mpmath's at nine digits, and at seventeen those of the
# design's own arithmetic, emulated apart from the interpreter.
>> exp(1)
~2.71828183

>> exp(0)
1

>> exp(-1)
~0.367879441

>> exp(30)
~1.06864746e+13

# The largest and the smallest a double holds, as C's exp gives them: past
# 709.78 none, and below -745.13 none but 0. Between 709.44 and 709.78 the
# power of two is 2^1024, and between -745.13 and -744.79 it is 2^-1075, so
# each is taken in two halves.
>> exp(709.78)
~1.79282279e+308

>> exp(710)
inf

>> exp(-745)
~4.94065646e-324

>> exp(-746)
0

# Past a thousand either way, its value there, no double lying between.
>> exp(10^400)
inf

>> exp(-10^400)
0

>> log(10)
~2.30258509

>> log(2)
~0.693147181

>> log(1/2)
~-0.693147181

>> log(1/1000000)
~-13.8155106

# Wherever a double is, and past it for an exact number: reached by halving
# or doubling, these ran out of depth.
>> log(2^-1074)
~-744.440072

>> log(~1.7976931348623157e308)
~709.782713

>> log(10^400)
~921.034037

>> log(1/10^400)
~-921.034037

# From 2^3322 a power of two passes a thousand digits and is approximated to
# inf, which an exact number past a double passes as one, so ilogb tries
# none past 2^3321, above every exact number; and inf is the one number its
# own double, whose log is itself (C101).
>> log(2^3072)
~2129.34814

>> log(10^999)
~2300.28251

>> log(~1/0)
inf

>> log(0)
error: division by zero

>> log(-1)
error: division by zero

>> tanh(1/2)
~0.462117157

>> tanh(-1/2)
~-0.462117157

>> tanh(0)
0

# Near 0, where 1 - 2/(exp(2*x) + 1) cancelled: it gave seven digits here.
>> tanh(1/10^9)
~1e-09

>> tanh(3)
~0.995054754

>> tanh(21)
1

>> tanh(-21)
-1

# Nothing they give is exact, exp(0) no more than before.
>> frac exp(0)
error: 1 was approximated, so it has no exact fraction

>> frac log(1)
error: 0 was approximated, so it has no exact fraction

>> frac tanh(0)
error: 0 was approximated, so it has no exact fraction

# NaN is no number to compare, and each of the three asks a guard first, as
# log did: exp and tanh answered NaN.
>> exp(0/~0)
error: a comparison needs a number, not -nan

>> tanh(0/~0)
error: a comparison needs a number, not -nan

# Every digit, where the arithmetic shows. exp(1) is a unit in the last place
# above e, which is the double nearest e: a polynomial in doubles is not
# correctly rounded, and is within 1.3 units of exp everywhere.
>> digits = 17
digits = 17

>> exp(1)
~2.7182818284590455

>> exp(1) - e
~4.4408920985006262e-16

>> log(10)
~2.3025850929940459

# An exact argument is reduced exactly. Rounded first, it would be the
# logarithm of the double nearest it: right for that double, and 9e-5 from the
# exact one's.
>> log(1 + 1/10^12)
~9.9999999999949996e-13

>> log(~(1 + 1/10^12))
~1.000088900581841e-12

>> exp(709.78)
~1.7928227943945644e+308

>> exp(~709.78)
~1.7928227943945155e+308

>> digits = 9
digits = 9

# ilogb, C's name for the power of two at or below a number, which log
# reduces by: thirteen guarded steps from 2^-4096, so exact, and right for
# every double and every exact number.
>> ilogb(1)
0

>> ilogb(3)
1

>> ilogb(1/3)
-2

>> ilogb(2^-1074)
-1074

>> ilogb(10^400)
1328

>> ilogb(10^999)
3318

>> frac ilogb(~3)
1

# Of 0 and below, refused as log is, however 0 is written (C100).
>> ilogb(0)
error: division by zero

>> ilogb(~0)
error: division by zero

>> ilogb(-1)
error: division by zero

# grad differentiates the definitions: floor's derivative is 0, and a guard
# at its threshold takes its own side, so 1, 2 and the powers of two answer.
>> grad_(x = 0) exp(x)
1

>> grad_(x = 1) exp(x)
~2.71828183

>> grad_(x = -1) exp(x)
~0.367879441

>> grad_(x = 800) exp(x)
inf

# Past a thousand, the derivative of a constant.
>> grad_(x = 1001) exp(x)
0

>> grad_(x = 1) log(x)
1

>> grad_(x = 2) log(x)
0.5

>> grad_(x = 1/2) log(x)
2

>> grad_(x = 3) log(x)
~0.333333333

>> grad_(x = 0) log(x)
error: division by zero

>> grad_(x = 0) tanh(x)
1

>> grad_(x = 1/2) tanh(x)
~0.786447733

>> grad_(x = -1/2) tanh(x)
~0.786447733

>> grad_(x = 21) tanh(x)
0

# Their definitions, as the prelude writes them.
>> ?exp
exp(x) = expk(x, floor(x*1.4426950408889634 + 1/2))
exp(x) | x > 1000 = exp(1000)
exp(x) | x < -1000 = exp(-1000)

>> ?log
log(x) = logk(x, ilogb(x))
log(x) | x <= 0 = 1/0
log(x) | 2*x == x = x

>> ?tanh
tanh(x) = tanhp(x)
tanh(x) | x < 0 = -tanhp(-x)

>> tex ?log
\operatorname{log}(x) = \begin{cases} \frac{1}{0} & \text{if } x \le 0 \\ x & \text{if } 2\,x = x \\ \operatorname{logk}(x, \operatorname{ilogb}(x)) & \text{otherwise} \end{cases}

>> tex ?tanh
\operatorname{tanh}(x) = \begin{cases} -\operatorname{tanhp}(-x) & \text{if } x < 0 \\ \operatorname{tanhp}(x) & \text{otherwise} \end{cases}

>> tex ?ilogbs
\operatorname{ilogbs}(x, k, s) = \begin{cases} k & \text{if } k + s > 3321 \\ k + s & \text{if } x \ge 2^{k + s} \\ k & \text{otherwise} \end{cases}

# Where a reduction ends in '~', which has no form on paper.
>> tex ?expk
error: tex cannot show '~', which has no form on paper

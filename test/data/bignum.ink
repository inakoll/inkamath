# Exact numbers past 64 bits: phase 13, step 2 (MODERNIZATION.md). This was
# the specification, written before they existed (CLAUDE.md, section 3); every
# expected output is as it was specified, computed from the exact value by
# Python's Fraction and the reference printer, apart from the interpreter.
#
# An exact number stays exact while its reduced numerator and denominator
# have a thousand digits or fewer each -- as many as 'digits' can show -- and
# is approximated past that, as it is past 64 bits today. A whole number too
# large for a double is then infinite: the double's limit, not the bound's.

# Where 64 bits ran out, nothing does.
>> 2^63
9223372036854775808

>> !21
51090942171709440000

>> !25
15511210043330985984000000

>> 9223372036854775808
9223372036854775808

>> 123456789012345678901234567890
123456789012345678901234567890

>> frac !52/(!5*!47)
2598960

# A literal is exact however long.
>> frac 3.14159265358979323846
157079632679489661923/50000000000000000000

>> frac 1.50000000000000000000000000000000000000000000000001
150000000000000000000000000000000000000000000000001/100000000000000000000000000000000000000000000000000

# Sequences stay exact for as long as they are asked.
>> fib_0 = 0
fib_0 = 0

>> fib_1 = 1
fib_1 = 1

>> fib_n = fib_(n-1) + fib_(n-2)
fib_n = fib_(n-1) + fib_(n-2)

>> fib_93
12200160415121876738

>> fib_100
354224848179261915075

>> frac fib_100/fib_99
354224848179261915075/218922995834555169026

>> ~fib_100
~3.54224848e+20

>> h_1 = 1
h_1 = 1

>> h_n = h_(n-1) + 1/n
h_n = h_(n-1) + 1/n

>> h_50
~4.49920534

>> frac h_50
13943237577224054960759/3099044504245996706400

>> h_100
~5.18737752

# Exact digits as far as they are asked for: a partial sum of e overtakes
# the built-in one, which has 17.
>> ex(x)_n = sum_(k=0)^n x^k/!k
ex(x)_n = sum_(k=0)^n x^k/!k

>> digits = 50
digits = 50

>> ex(1)_40
~2.7182818284590452353602874713526624977572470936999

>> e
~2.7182818284590451

>> digits = 9
digits = 9

# Past a thousand digits a number is approximated. Exact Newton's method
# doubles its digits every step, and crosses the bound at the twelfth; the
# iteration goes on, approximately.
>> rt_0 = 1
rt_0 = 1

>> rt_n = (rt_(n-1) + 2/rt_(n-1))/2
rt_n = (rt_(n-1) + 2/rt_(n-1))/2

>> frac rt_5
886731088897/627013566048

>> digits = 50
digits = 50

>> rt_11
~1.4142135623730950488016887242096980785696718753769

>> 2^(1/2)
~1.4142135623730951

>> digits = 9
digits = 9

>> frac rt_12
error: ~1.41421356 was approximated, so it has no exact fraction

>> rt_20
~1.41421356

# 2^3321 has a thousand digits and 2^3322 one more; a double has no room for
# it, so it is infinite.
>> frac 2^3321/2^3320
2

>> 2^3322
inf

>> !449 > !448
1

>> !450
inf


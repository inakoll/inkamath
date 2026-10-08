# A failed evaluation yields a diagnostic instead of a value.

>> 1+
error: unexpected end of input after '+'

>> )
error: unexpected ')'

>> [1 2
error: missing ']' after '2'

>> (1+2
error: missing ')' after '2'

>> 1+2)
error: unexpected ')'

>> @
error: unexpected character '@'

# A line that is only a comment reaches the empty-expression check like any
# other empty input. The lexer used to return early and leave the parser with
# no tokens at all (DESIGN.md, C18).
>> # nothing to evaluate
error: empty expression

>> 2 3
error: unexpected '3' -- the operator '*' is probably missing

>> f(1+2
error: missing ')' after function parameters

# An undefined name is reported rather than quietly treated as zero.
>> undefined
error: undefined is not defined

# Operands are evaluated left to right, so the first diagnostic is the
# leftmost one. C++ does not order the two sides of `f(a) + f(b)`, and this
# reported 'bbb' under GCC and 'aaa' under Clang (DESIGN.md, C24).
>> aaa+bbb
error: aaa is not defined

>> undefined(2)_3
error: undefined is not defined

# A line cannot exhaust the C++ stack. The token count bounds the parser's
# recursion, the evaluator's and the destructor's alike, since the tree has at
# most one node per token. Nested parentheses used to segfault at about 8000
# deep, and a flat sum at about 30000 terms (DESIGN.md, C20).
>> ((((((((((((((((((((((((((((((((1))))))))))))))))))))))))))))))))
1

# Defining a self-reference is just a binding; evaluating one is bounded
# rather than fatal. These used to crash on an uninitialised pointer before
# they ever recursed (DESIGN.md, C16).
>> r=r
r=r

>> r
error: evaluation nests more than 256 references deep

>> w_n=w
w_n=w

# The bare 'w' in the body names the sequence itself, which no longer means
# "iterate until it stops changing", so this is caught before it recurses.
>> w_1
error: w is a sequence; index it (w_0) or take its limit (lim w)

# Malformed parameters used to be swallowed by a catch inside the constructor,
# leaving the definition half-built and reporting nothing (DESIGN.md,
# C14).
>> f(x=1, y)=x+y
error: a positional argument cannot follow a keyword argument

# An index must be a whole number (README.md section 4). It used to be
# truncated in silence, so 'f_(0.5)=1' meant 'f_0=1' (DESIGN.md, C13).
>> f_(0.5)=1
error: an index must be a whole number, not 0.5

>> f_0=1
f_0=1

>> f_(2+i)
error: an index must be a whole number, not 2+i

>> f_(0.5)
error: an index must be a whole number, not 0.5

# An index that no index can hold is out of range, not "not a whole number".
# The value was cast to an int regardless -- undefined behaviour, which GCC's
# sanitizer does not report without float-cast-overflow -- and the whole-number
# check then fired on the wreckage (DESIGN.md, C56).
>> ix=[1 2;3 4]
ix=[1 2;3 4]

>> ix[2147483648,1]
error: an index must be between -2147483648 and 2147483647, not 2147483648

>> ix[0-1e20,1]
error: an index must be between -2147483648 and 2147483647, not -100000000000000000000

>> ix[1.5,1]
error: an index must be a whole number, not 1.5

# A factorial needs a whole number that is not negative. The loop multiplied
# 'i' while 'i <= n', so a fraction was truncated to '120', a negative gave
# the empty product '1', and the imaginary part was dropped before the loop
# ever saw it: these answered 120, 1, 2 and 1 (DESIGN.md, C42).
>> !5.5
error: a factorial needs a whole number, not 5.5

>> !(0-3)
error: a factorial cannot be negative

>> !(2+i*3)
error: a factorial needs a real number, not 2+i*3

>> !i
error: a factorial needs a real number, not i

# 171! overflows a double, so counting past it can only reach infinity -- and
# the counter is itself a double, which stops advancing at 2^53. '!(10^20)'
# and '!1e16' ran for ever and took the session with them, since a hung
# process loses every definition in it (DESIGN.md, C47).
>> !~171
inf

>> !(10^20)
inf  # approximated past a thousand digits

>> !1e400
inf  # approximated past a thousand digits

# A part that is NaN is present but has no sign, and answers false to every
# comparison, so the imaginary unit used to be dropped while its magnitude was
# still printed: this read '-nan*inf' (DESIGN.md, C31). The zero is inexact
# because an exact one cannot be divided by (phase 13).
>> i/~0
-nan+i*inf

# A real product or quotient of real numbers is the real one. Taken as a
# complex one, an infinity times the zero imaginary part made a NaN there:
# these read 'inf+i*-nan' and '-nan+i*-nan' (DESIGN.md, C88).
>> 1/~0
inf

>> 0/~0
-nan

>> 10^400*~1
inf

# An exact number past a double's range is finite all the same, so its
# product with a 0 part of the other is 0, not inf times 0: 10^400 i is
# i*inf, which read '-nan+i*inf', and its real part 0 (DESIGN.md, C207).
>> 10^400*i
i*inf

>> re(10^400*i)
0

>> 10^400/i
-i*inf

>> 10^400*~0
0

# So is an exact number approximated past a double's range, as 10^2000/3
# is: an exact 0 times it is 0, and its product or quotient with i a part 0,
# where each read -nan (DESIGN.md, C209).
>> 0*(10^2000/3)
0  # approximated past a thousand digits

>> (10^2000/3)*i
i*inf  # approximated past a thousand digits

>> (10^2000/3)/i
-i*inf  # approximated past a thousand digits

# With a double, such an exact number is the double of what it makes, as
# its power is (C175): 10^400 2^-332 is about 1.14e300 and 10^-400 1e300
# 1e-100, which read inf and 0, the exact number made a double first, and
# so did 2^1024 - 2^1023, 2^1023 (DESIGN.md, C242).
>> 10^400*~2^-332
~1.14298739e+300

>> 10^400/~2^332
~1.14298739e+300

>> 10^-400*~1e300
~1e-100

>> ~1e300/10^400
~1e-100

>> 10^-400/~2^-1074
~2.02402253e-77

>> 10^400*(~2^-332*i)
i*~1.14298739e+300

>> 10^400/(~2^332*i)
-i*~1.14298739e+300

>> 2^1024 - ~2^1023
~8.98846567e+307

>> ~2^1023 - 2^1024
~-8.98846567e+307

>> ~-2^1023 + 2^1024
~8.98846567e+307

# Nor is it 0 or inf to a comparison with a double, which it is compared with
# exactly: 10^-400 is above 0 and below 2^-1074, and 10^400 above the largest
# double and below inf, where the first read 0 and the second inf (DESIGN.md,
# C262). Two exact numbers compared exactly already.
>> 10^-400 == ~0
0

>> 10^-400 <> ~0
1

>> 10^-400 > ~0
1

>> -10^-400 < ~0
1

>> 10^-400 == ~0*i
0

>> 10^-400 < ~2^-1074
1

>> 10^400 == 1/~0
0

>> 10^400 < 1/~0
1

>> -10^400 > -1/~0
1

>> 10^400 > ~1.7976931348623157e308
1

>> ~1.7976931348623157e308 < 10^400
1

>> max(~0, 10^-400)
1e-400

>> min(~0, -10^-400)
-1e-400

>> 10^-400 == 10^-401
0

>> 10^400 < 0/~0
error: a comparison needs a number, not -nan

# NaN is not a number, so a comparison of it has no answer, and is refused
# rather than guessed false, as C has it (DESIGN.md, a NaN reaches every term
# that reads it). Nor are two NaNs equal, or a matrix with one equal to any.
>> 0/~0 > 0
error: a comparison needs a number, not -nan

>> 0/~0 == 0/~0
error: a comparison needs a number, not -nan

>> [1 0/~0] == [1 2]
error: a comparison needs a number, not -nan

# A power of NaN is NaN, as every other operation on it is, where C's pow
# absorbs it: both answered 1, where an aware step does not.
>> 1^(0/~0)
-nan

>> (0/~0)^~0
-nan

# An exponent outside int's range used to be converted to one anyway, which is
# undefined: this answered 0 (DESIGN.md, C28).
>> 2^2147483648
inf  # approximated past a thousand digits

>> 0.5^3000000000
0  # approximated past a thousand digits

# A real power of a real number is the real power. Taken as a complex one it
# squared an infinity into a NaN imaginary part, and put a square root a bit
# away from the double nearest it.
>> (~2)^1024
inf

>> digits = 17
digits = 17

>> 2^0.5
~1.4142135623730951

>> digits = 9
digits = 9

# A negative power was one over the positive power, which overflows first:
# both answered 0, next to the smallest double.
>> 2^-1074
~4.94065646e-324

>> (2*i)^-1074
~-4.94065646e-324

# Mathematics writes a multiplication by writing nothing; this language does
# not. The hint used to be given only for '(' -- '2 3' above is the same
# mistake and got only 'unexpected' (DESIGN.md, C35).
>> 2pi
error: unexpected 'pi' -- the operator '*' is probably missing

>> 3(4)
error: unexpected '(' -- the operator '*' is probably missing

# An operand that begins with a prefix or a bracket is the same mistake.
>> 1 ~2
error: unexpected '~' -- the operator '*' is probably missing

>> 2 !3
error: unexpected '!' -- the operator '*' is probably missing

>> 2 [1 2]
error: unexpected '[' -- the operator '*' is probably missing

# 'frac' and 'digits' are about a whole line, and say so anywhere else
# (DESIGN.md, phase 13).
>> frac x = 1
error: frac shows an answer, not a definition

>> 2*digits
error: digits can only begin a line

>> digits = [1 2]
error: digits must be a whole number of at least 1, not [1, 2]

>> digits = x = 3
error: digits takes a number, not a definition

# They were names before, and a definition of one says why it is refused.
>> frac(x) = x
error: frac is reserved, so it cannot be defined

>> frac_n = n
error: frac is reserved, so it cannot be defined

>> frac = 3
error: frac is reserved, so it cannot be defined

>> digits(x) = 1
error: digits is reserved, so it cannot be defined

>> digits_n | n > 1 = n
error: digits is reserved, so it cannot be defined

>> frac (1/3)
1/3

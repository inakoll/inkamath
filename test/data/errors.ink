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
# no tokens at all (MODERNIZATION.md, C18).
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
# reported 'bbb' under GCC and 'aaa' under Clang (MODERNIZATION.md, C24).
>> aaa+bbb
error: aaa is not defined

>> undefined(2)_3
error: undefined is not defined

# A line cannot exhaust the C++ stack. The token count bounds the parser's
# recursion, the evaluator's and the destructor's alike, since the tree has at
# most one node per token. Nested parentheses used to segfault at about 8000
# deep, and a flat sum at about 30000 terms (MODERNIZATION.md, C20).
>> ((((((((((((((((((((((((((((((((1))))))))))))))))))))))))))))))))
1

# Defining a self-reference is just a binding; evaluating one is bounded
# rather than fatal. These used to crash on an uninitialised pointer before
# they ever recursed (MODERNIZATION.md, C16).
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
# leaving the definition half-built and reporting nothing (MODERNIZATION.md,
# C14).
>> f(x=1, y)=x+y
error: a positional argument cannot follow a keyword argument

# An index must be a whole number (README.md section 4). It used to be
# truncated in silence, so 'f_(0.5)=1' meant 'f_0=1' (MODERNIZATION.md, C13).
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
# check then fired on the wreckage (MODERNIZATION.md, C56).
>> ix=[1 2;3 4]
ix=[1 2;3 4]

>> ix[2147483648,1]
error: an index must be between -2147483648 and 2147483647, not 2.14748365e+09

>> ix[0-1e20,1]
error: an index must be between -2147483648 and 2147483647, not -1e+20

>> ix[1.5,1]
error: an index must be a whole number, not 1.5

# A factorial needs a whole number that is not negative. The loop multiplied
# 'i' while 'i <= n', so a fraction was truncated to '120', a negative gave
# the empty product '1', and the imaginary part was dropped before the loop
# ever saw it: these answered 120, 1, 2 and 1 (MODERNIZATION.md, C42).
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
# process loses every definition in it (MODERNIZATION.md, C47).
>> !171
inf

>> !(10^20)
inf

>> !1e400
inf

# A part that is NaN is present but has no sign, and answers false to every
# comparison, so the imaginary unit used to be dropped while its magnitude was
# still printed: these read 'inf*-nan' and '-nan*-nan' (MODERNIZATION.md, C31).
>> 1/0
inf+i*-nan

>> 0/0
-nan+i*-nan

# An exponent outside int's range used to be converted to one anyway, which is
# undefined: this answered 0 (MODERNIZATION.md, C28).
>> 2^2147483648
inf

>> 0.5^3000000000
0

# Mathematics writes a multiplication by writing nothing; this language does
# not. The hint used to be given only for '(' -- '2 3' above is the same
# mistake and got only 'unexpected' (MODERNIZATION.md, C35).
>> 2pi
error: unexpected 'pi' -- the operator '*' is probably missing

>> 3(4)
error: unexpected '(' -- the operator '*' is probably missing


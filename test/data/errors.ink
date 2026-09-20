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
error: unexpected '3'

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

# An index must be a whole number (README.md section 4.2, which calls
# 'f_(0.5)=1' ill-formed). It used to be truncated in silence, so that line
# meant 'f_0=1' (MODERNIZATION.md, C13).
>> f_(0.5)=1
error: an index must be a whole number, not 0.5

>> f_0=1
f_0=1

>> f_(2+i)
error: an index must be a whole number, not 2+i

>> f_(0.5)
error: an index must be a whole number, not 0.5


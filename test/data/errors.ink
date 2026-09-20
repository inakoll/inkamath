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

>> 2 3
error: unexpected '3'

>> f(1+2
error: missing ')' after function parameters

# An undefined name is reported rather than quietly treated as zero.
>> undefined
error: undefined is not defined

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

>> w_1
error: evaluation nests more than 256 references deep


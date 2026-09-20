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

# Undefined identifiers evaluate to zero without an error (README.md section 6).
>> undefined
0

>> undefined(2)_3
0


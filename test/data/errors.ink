# A failed evaluation yields a diagnostic instead of a value.

>> 1+
Error : Unexpected end of input before '+'

>> )
Error : Unexpected operator ')'

>> [1 2
Error : Missing operator ']' after '(2,0)'

>> (1+2
Error : Missing operator ')' after '(2,0)'

>> 1+2)
Error : Syntax error before ')'

>> @
Error : Unexpected character : @

>> 2 3
Error : Syntax error before '(3,0)'

# Undefined identifiers evaluate to zero without an error (README.md section 6).
>> undefined
0

>> undefined(2)_3
0


# Errors are reported on stdout and the evaluation yields zero.
# The transcript records both, error text first.

>> 1+
Error : Unexpected end of input before '+'
0

>> )
Error : Unexpected operator ')'
0

>> [1 2
Error : Missing operator ']' after '(2,0)'
0

>> (1+2
Error : Missing operator ')' after '(2,0)'
0

>> 1+2)
Error : Syntax error before ')'
0

>> @
Error : Unexpected character : @
0

>> 2 3
Error : Syntax error before '(3,0)'
0

# Undefined identifiers evaluate to zero without an error (README.md section 6).
>> undefined
0

>> undefined(2)_3
0


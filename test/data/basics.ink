# Arithmetic, precedence and built-in constants.
# Examples are taken from README.md so the tests double as documentation checks.

>> 1+1
2

>> 3*(4+5)
27

>> 1+2*3^3*2+1
110

>> -3+1
-2

>> 2^3^2
512

>> 10/4
2.5

>> !5
120

>> !0
1

# 'e', 'pi' and the imaginary unit 'i' are the only built-ins.
>> [pi, e]
3.14159265 2.71828183

>> i
i

>> i*i
-1

>> 2+3*i
2+i*3

>> (1+i)*(1-i)
2

# 'i' is the imaginary unit only when it is not the start of a longer name.
# Every identifier beginning with 'i' used to be a syntax error.
>> ii=3
3

>> ii
3

>> index=7
7

>> index+1
8

>> i*2
i*2

# '#' starts an inkamath comment.
>> 1 + 2 # this computes 1 + 2
3


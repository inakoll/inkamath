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

# Unary plus, which README.md section 1 has always listed and the parser has
# never had: '+5' was 'unexpected' (MODERNIZATION.md, C37). It binds exactly
# as unary minus does.
>> +5
5

>> +2^2
4

>> -2^2
-4

# 'e', 'pi' and the imaginary unit 'i' are the only built-ins.
>> [pi, e]
3.14159265 2.71828183

# They carry every digit a double holds. The 2014 literals stopped at
# fourteen, and the imaginary part here was 4.58636533e-14 -- the error in pi,
# not the error of the arithmetic (MODERNIZATION.md, C34).
>> e^(i*pi)
-1+i*1.2246468e-16

>> i
i

>> i*i
-1

>> 2+3*i
2+i*3

>> (1+i)*(1-i)
2

# The principal square root of a negative number, which used to depend on how
# the minus was written: unary minus left a -0 imaginary part, putting the
# value on the far side of the branch cut, so this answered 'i*-2' where
# '(0-4)^0.5' answered 'i*2'.
>> (-4)^0.5
1.2246468e-16+i*2

>> (0-4)^0.5
1.2246468e-16+i*2

# 'i' is the imaginary unit only when it is not the start of a longer name.
# Every identifier beginning with 'i' used to be a syntax error.
>> ii=3
ii=3

>> ii
3

>> index=7
index=7

>> index+1
8

>> i*2
i*2

# '#' starts an inkamath comment.
>> 1 + 2 # this computes 1 + 2
3


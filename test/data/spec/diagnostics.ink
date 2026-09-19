# SPECIFICATION -- see definitions.ink.
#
# C13: the interpreter stops answering every question with a number. Today
# each of these silently yields 0, which is why a typo and a correct program
# are indistinguishable from the outside.

>> undefined
error: undefined is not defined

>> undefined(2)_3
error: undefined is not defined

# Identifiers may start with 'i'; only a bare 'i' is the imaginary unit.
>> i
i

>> i*i
-1

>> ii = 3
ii = 3

>> ii
3

# Diagnostics quote what the user typed, not the interpreter's own numeric
# type. Today this reads: Missing operator ']' after '(2,0)'.
>> [1 2
error: missing ']' after '2'

>> 1+
error: unexpected end of input after '+'

>> 2 3
error: unexpected '3'

# An index must be an integer.
>> f_0 = 1
f_0 = 1

>> f_(0.5)
error: index must be an integer, got 0.5

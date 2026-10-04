# 'and' and 'or', for guards and conditions (DESIGN.md, next in line).
# A truth is a number here, and so is their answer: 1 or 0, exactly, as a
# comparison's is. Each reads its left side first and its right only when
# the left has not decided, so a guard can ask whether a term exists before
# it reads it. There is no 'not': every comparison has its opposite, '<' and
# '>=', '==' and '<>'.

>> 1 < 2 and 2 < 3
1

>> 1 < 2 and 3 < 2
0

>> 2 < 1 or 2 < 3
1

>> 2 < 1 or 3 < 2
0

# Any value that is not zero is true, as in a guard.
>> 5 and 1/2
1

>> 2 and 0
0

>> 0 or ~0.5
1

>> i and 1
1

# 'and' binds tighter than 'or', and both looser than a comparison and than
# arithmetic, as in every language that has them.
>> 1 == 1 or 1 == 2 and 1 == 3
1

>> (1 == 1 or 1 == 2) and 1 == 3
0

>> 2 - 2 or 3 - 3
0

>> 1 and 1 and 0
0

# The right side is read only when the left has not decided.
>> 0 and nope
0

>> 1 or nope
1

>> 1 and nope
error: nope is not defined

# Which is what lets a guard ask about a term before it reads it: at n = 0,
# c_(n-1) is never asked for.
>> c_0 = 1
c_0 = 1

>> c_n = 2*c_(n-1)
c_n = 2*c_(n-1)

>> cap_n = c_n
cap_n = c_n

>> cap_n | n > 0 and c_(n-1) >= 4 = 4
cap_n | n > 0 and c_(n-1) >= 4 = 4

>> cap_0
1

>> cap_3
4

>> cap_4
4

# In a matrix defined by its cells, the guards the products used to spell:
# a walk on four places that stops at either end.
>> T[j<=4, k<=4] | j == 1 or j == 4 = j == k
T[j<=4, k<=4] | j == 1 or j == 4 = j == k

>> T[j<=4, k<=4] | j > 1 and j < 4 = (k == j+1)/2 + (k == j-1)/2
T[j<=4, k<=4] | j > 1 and j < 4 = (k == j+1)/2 + (k == j-1)/2

>> T
[  1,   0,   0,   0;
 0.5,   0, 0.5,   0;
   0, 0.5,   0, 0.5;
   0,   0,   0,   1]

# A truth is one value, as a comparison's operands are.
>> [1 1] and 1
error: and needs single values, not a 1x2 matrix

>> 0 or [0 0]
error: or needs single values, not a 1x2 matrix

# Nor is a NaN a truth: it is refused, where it held as any value that is
# not zero does (DESIGN.md, a NaN reaches every term that reads it). The
# right side is still read only where the left has not decided.
>> 0/~0 or 0
error: or needs a number, not -nan

>> 1 and 0/~0
error: and needs a number, not -nan

>> 0 and 0/~0
0

# They are words of the language, so not names.
>> and = 3
error: expected a value before 'and'

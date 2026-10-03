# Inside a matrix literal, and inside an argument list, a sign with a space
# before it and none after it begins the next element, as it does in MATLAB:
# '[1 -1]' is two numbers (DESIGN.md, C52 and its sequel). Spaced on both
# sides, or on neither, it subtracts, as it does everywhere else.
>> [1 -1]
[1, -1]

>> [1 - 1]
0

>> [1-1]
0

>> [1 +1]
[1, 1]

>> a = 2
a = 2

>> [a -1]
[2, -1]

>> [a - 1]
1

>> [a' -1]
[2, -1]

>> [1/2 -1/4; 1/4 1/2]
[ 0.5, -0.25;
 0.25,   0.5]

# A sign after an operator is that operator's operand, and parentheses hold
# an expression of their own.
>> [1 * -1]
-1

>> [(1 -1) 2]
[0, 2]

# A sum's body ends where the next element begins.
>> [sum_(k=1)^3 k -1]
[6, -1]

# In an argument list, alike.
>> g(x, y) = x + 10*y
g(x, y) = x + 10*y

>> g(1 -2)
-19

>> g(1 - 2)
error: g expects 2 arguments, got 1

# Outside brackets a space means nothing.
>> 1 -1
0

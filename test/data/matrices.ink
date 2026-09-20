# Matrix literals, and the block expansion described in README.md section 2.

>> [1]
1

>> [1 2;3 4]
1 2
3 4

>> [1, 2; 3, 4]
1 2
3 4

>> [1; 2, 3]
1 0
2 3

# A matrix needs at least one element. These used to build a degenerate n x 0
# matrix and kill the process in the evaluator (MODERNIZATION.md, C19).
>> []
error: a matrix needs at least one element

>> [;]
error: a matrix needs at least one element

>> a=[1 2;3 4]
a=[1 2;3 4]

>> a+a
2 4
6 8

>> a*a
7 10
15 22

>> a-a
0 0
0 0

>> 2*a
2 4
6 8

>> -a
-1 -2
-3 -4

# Three or more rows or columns. The block offsets were a prefix sum that
# added only the previous element instead of the running total, so the result
# was allocated too small and every one of these reported 'Out of matrix
# range.' Correct for two blocks, which is every size the tests used to have
# (MODERNIZATION.md, C22).
>> [1 2 3]
1 2 3

>> [1;2;3]
1
2
3

>> [1 2 3;4 5 6]
1 2 3
4 5 6

>> [a, a, a]
1 2 1 2 1 2
3 4 3 4 3 4

>> [a; a; a]
1 2
3 4
1 2
3 4
1 2
3 4

>> [a, a; a, a; a, a]
1 2 1 2
3 4 3 4
1 2 1 2
3 4 3 4
1 2 1 2
3 4 3 4

# A cell smaller than its block is extended to fill it.
>> [a, 1, a]
1 2 1 1 2
3 4 1 3 4

# A matrix of expressions expands to the size of what its cells evaluate to.
>> [a, a; a, a]
1 2 1 2
3 4 3 4
1 2 1 2
3 4 3 4

>> [a; a]
1 2
3 4
1 2
3 4

# A matrix power is repeated multiplication. It used to square the
# accumulator, so 'a^n' computed 'a^(2^(n-1))' -- right at 1 and 2 and wrong
# everywhere else, with a^0 returning a (MODERNIZATION.md, C23).
>> s=[1 1;0 1]
s=[1 1;0 1]

>> s^0
1 0
0 1

>> s^3
1 3
0 1

>> a^3
37 54
81 118

# There is no inverse and no root here, and only a square matrix has a power.
>> a^0.5
error: a matrix power must be a whole number, not 0.5

>> a^(0-1)
error: a matrix power cannot be negative

>> [1 2 3]^2
error: only a square matrix has a power

>> 2^[1 2]
error: a matrix cannot be an exponent

# The rest of the matrix diagnostics, which used to be in the 2014 voice --
# capitalised, punctuated, newline-terminated, and one of them misspelled
# 'assigmentation' (MODERNIZATION.md, D13).
>> [1 2]+[1 2 3]
error: these matrices have different sizes

>> [1 2;3 4]*[1 2 3]
error: a matrix product needs as many columns on the left as rows on the right

>> !a
error: a matrix has no factorial

>> mm_n=[1 2;3 4]*(0.5)^n
mm_n=[1 2;3 4]*(0.5)^n

>> lim mm
error: a matrix has no absolute value


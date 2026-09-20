# Matrix literals, and the block expansion described in README.md section 3.

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


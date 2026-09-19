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

>> a=[1 2;3 4]
1 2
3 4

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


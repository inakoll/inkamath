# A file's own sequences, checked without an instance: Fibonacci as
# docs/language.md writes it, ahead of its index. Every term to fib_78, the
# last below 2^53, is a double, so 79 steps hold to the bit, 'inkamath
# --check fib.ink --steps 79 -o fib.c':
#
#     fib.ink: 79 steps from 0, against exact values
#     fib: within 0
fib_0 = 0
fib_1 = 1
fib_(n+2) = fib_(n+1) + fib_n

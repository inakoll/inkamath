# References: identifiers name expressions, not values (README.md section 4.1).

>> a = 1
1

>> b = a+a
2

>> a = 2
2

>> b
4

# Parameters, positional and named (README.md section 4.1).
>> f(x, y)=x^2+y
0

>> f(2, 1)
5

>> f(x=3, y=2)
11

# Surplus arguments and missing ones are both errors. Missing ones used to
# fall back to the calling scope and then to zero, reporting nothing.
>> g=1+2
3

>> g
3

>> g(1)
error: g takes no arguments

# Scoping (README.md section 6).
>> h(x)=x^2
0

>> x
0

>> h(2)
4

>> h(x=4)
16

>> x
0

>> x=5
5

>> h
error: h expects 1 argument, got 0


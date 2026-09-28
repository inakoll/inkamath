# floor: the largest whole number not above x. The one function built in,
# because nothing else in the language can compute it short of a search, and
# everything else that rounds is a line of it: which rule a model rounds by
# -- halves up, to the cent, toward zero -- is the model's to say, as an
# acceleration is (README.md, section 4), and no built-in rule would be right
# for every model (MODERNIZATION.md, next in line).

# Exactly, of an exact number, however large.
>> floor(7/2)
3

>> floor(-7/2)
-4

>> floor(3)
3

>> floor(10^30/7)
142857142857142857142857142857

# In doubles 0.7+0.1 is a hair under 0.8, and the floor falls a whole unit;
# exact, it does not.
>> floor((0.7+0.1)*10)
8

>> floor((~0.7+0.1)*10)
7

# Of an inexact number it is inexact, as whatever an inexact number touches
# is, even where it prints whole; and an approximation past the bound says so.
>> frac floor(pi)
error: 3 was approximated, so it has no exact fraction

>> floor(2^3322)
inf  # approximated past a thousand digits

# A matrix cell by cell, as '+' and '/' are.
>> floor([1/2, -1/2; 5/2, 7])
[0, -1;
 2,  7]

# What it refuses.
>> floor(i)
error: floor needs a real number, not i

>> floor(1, 2)
error: floor expects 1 argument, got 2

# The rest is a line each, by whichever rule the model needs.
>> round(x) = floor(x + 1/2)
round(x) = floor(x + 1/2)

>> round(5/2)
3

>> round(-5/2)
-2

>> ceil(x) = -floor(-x)
ceil(x) = -floor(-x)

>> ceil(7/2)
4

>> mod(a, b) = a - b*floor(a/b)
mod(a, b) = a - b*floor(a/b)

>> mod(7, 3)
1

>> mod(-7, 3)
2

>> cents(x) = floor(100*x + 1/2)/100
cents(x) = floor(100*x + 1/2)/100

>> cents(909.2907)
909.29

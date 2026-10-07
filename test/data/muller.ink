# W. Kahan, "How Futile are Mindless Assessments of Roundoff in
# Floating-Point Computation?" (2006), §5: J.-M. Muller's recurrence.
# Every expected value is the paper's, typed by hand, never recorded: x_80
# as a fraction, the exact and 53-bit columns of its table, and the pole of
# x_80 as a function of x_2.

>> E(y, z) = 108 - (815 - 1500/z)/y
E(y, z) = 108 - (815 - 1500/z)/y

>> x_0 = 4
x_0 = 4

>> x_1 = 17/4
x_1 = 17/4

>> x_n = E(x_(n-1), x_(n-2))
x_n = E(x_(n-1), x_(n-2))

>> frac x_80
206795153138256918939565417139009598365577843034794672964/41359030627651383817474849310671104336332210648235594113

>> x_80 == 5 - 2/(1 + (5/3)^80)
1

>> digits = 22
digits = 22

>> x_80
~4.999999999999999996426

# To 23 digits the paper prints 4.9999999999999999964263, a slip in its
# last: the 23rd digit is 2 and the 24th 0, as the fraction above says.

>> digits = 14
digits = 14

>> x_2
~4.4705882352941

>> x_13
~4.9973912683813

# The 53-bit column: IEEE double, from x_0 and x_1 rounded.
>> w_0 = ~4
w_0 = ~4

>> w_1 = ~(17/4)
w_1 = ~(17/4)

>> w_n = E(w_(n-1), w_(n-2))
w_n = E(w_(n-1), w_(n-2))

>> w_7
~4.9455373955305

>> w_14
~-7.8172365784593

>> w_15
~168.93916767106

>> w_80
100

# x_n = y_(n+1)/y_n, with y_n linear in x_2 for x_1 = 17/4, so x_80 has
# its pole where y_80 vanishes: 2.24e-100 below x_2 = 76/17.
>> y(a, b)_1 = 1
y(a, b)_1 = 1

>> y(a, b)_2 = a
y(a, b)_2 = a

>> y(a, b)_3 = a*b
y(a, b)_3 = a*b

>> y(a, b)_k = 108*y(a, b)_(k-1) - 815*y(a, b)_(k-2) + 1500*y(a, b)_(k-3)
y(a, b)_k = 108*y(a, b)_(k-1) - 815*y(a, b)_(k-2) + 1500*y(a, b)_(k-3)

>> digits = 13
digits = 13

>> -y(17/4, 0)_80/(y(17/4, 1)_80 - y(17/4, 0)_80) - 76/17
~-2.241902748434e-100

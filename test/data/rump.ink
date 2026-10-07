# S. M. Rump, "Verification methods: Rigorous results using floating-point
# arithmetic", Acta Numerica 19 (2010), §1.2, eqs. (1.3) to (1.5): his
# 1983 example, whose exact value is a/2b - 2. Every expected value is the
# paper's, typed by hand, never recorded.
>> f(a, b) = 333.75*b^6 + a^2*(11*a^2*b^2 - b^6 - 121*b^4 - 2) + 5.5*b^8 + a/(2*b)
f(a, b) = 333.75*b^6 + a^2*(11*a^2*b^2 - b^6 - 121*b^4 - 2) + 5.5*b^8 + a/(2*b)

>> frac f(77617, 33096)
-54767/66192

# The paper prints -0.827386, a slip for -0.827396: 54767/66192 is
# 0.8273960...
>> digits = 6
digits = 6

>> f(77617, 33096)
~-0.827396

# (1.5), the same polynomial rearranged, which IEEE single, double and
# extended all evaluate to 1.1726...; the paper gives the double's digits.
>> g(a, b) = 21*b*b - 2*a*a + 55*b*b*b*b - 10*a*a*b*b + a/(2*b)
g(a, b) = 21*b*b - 2*a*a + 55*b*b*b*b - 10*a*a*b*b + a/(2*b)

>> frac g(77617, 33096)
-54767/66192

>> digits = 13
digits = 13

>> g(~77617, ~33096)
~1.172603940053

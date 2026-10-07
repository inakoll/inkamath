# rho and abscissa where the cap of 256 halvings ends the bisection, apart
# from charpoly.ink so that each file stays within its timeout. Values as in
# charpoly.ink, worked out apart from the interpreter.

# A 0 is never within 2^-53 of itself, so the cap ends its bisection and
# it is marked, as is every answer the cap ends, right or not: the tests
# cannot tell a nilpotent matrix's rho, or a marginal system's abscissa,
# from a value below the last bracket (C192).
>> rho([0 1; 0 0])
0  # approximated past a thousand digits

>> abscissa([0 1; 0 0])
0  # approximated past a thousand digits

>> rho([0 0; 0 0])
0  # approximated past a thousand digits

# Below 2^-256 of B, the bracket at the cap is no answer: -2^-254 for
# -10^-100, and 0 for an unstable 10^-80 and a radius of 10^-100 (C192).
>> abscissa([-10^-100 0; 0 -1])
~-3.45446742e-77  # approximated past a thousand digits

>> abscissa([10^-80 0; 0 -1])
0  # approximated past a thousand digits

>> abscissa([10^-80 0; 0 -1]) > 0
0  # approximated past a thousand digits

>> rho([0 1; 10^-200 0])
0  # approximated past a thousand digits

>> rho([0 1; 10^-200 0]) > 0
0  # approximated past a thousand digits

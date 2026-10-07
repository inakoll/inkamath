# rho and abscissa where the cap of 256 halvings ends the bisection, apart
# from charpoly.ink so that each file stays within its timeout. Values as in
# charpoly.ink, worked out apart from the interpreter.

# A 0 is never within 2^-53 of itself, so its bisection runs to the cap:
# a nilpotent matrix's rho, a marginal system's abscissa.
>> rho([0 1; 0 0])
0

>> abscissa([0 1; 0 0])
0

>> rho([0 0; 0 0])
0

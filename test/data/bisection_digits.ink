# rho and abscissa of a matrix whose tests pass a thousand digits, apart
# from charpoly.ink so that each file stays within its timeout. Values as in
# charpoly.ink, worked out apart from the interpreter.

# Ten by ten, the late tests read numbers past a thousand digits, so the
# answer is marked (NumPy: 1.7463140334187077 and 1.239222528823944).
>> A = [6 -8 -6 -5 -6 6 7 2 -9 -8; -3 -1 2 0 -4 -6 4 4 -9 -7; -1 -2 7 0 -2 -1 3 2 -6 5; 5 9 5 -4 -3 3 3 4 7 -4; 8 -9 -8 9 8 -4 -7 -4 -9 7; 3 2 -5 -1 -6 5 0 -9 -5 4; 0 -2 -5 -8 2 3 0 8 8 -6; 2 2 -5 -4 0 5 -4 4 3 -5; -2 6 7 3 -9 3 -5 6 8 -1; 9 5 -3 7 -2 -8 2 7 3 -2]/10
A = [6 -8 -6 -5 -6 6 7 2 -9 -8; -3 -1 2 0 -4 -6 4 4 -9 -7; -1 -2 7 0 -2 -1 3 2 -6 5; 5 9 5 -4 -3 3 3 4 7 -4; 8 -9 -8 9 8 -4 -7 -4 -9 7; 3 2 -5 -1 -6 5 0 -9 -5 4; 0 -2 -5 -8 2 3 0 8 8 -6; 2 2 -5 -4 0 5 -4 4 3 -5; -2 6 7 3 -9 3 -5 6 8 -1; 9 5 -3 7 -2 -8 2 7 3 -2]/10

>> rho(A)
~1.74631403  # approximated past a thousand digits

>> abscissa(A)
~1.23922253  # approximated past a thousand digits

# clamp from the prelude (DESIGN.md): clamp(x, lo, hi) =
# max(lo, min(hi, x)), as written. Every answer worked out by hand from that
# line and max's and min's clauses, each at a tie its first argument; never
# recorded.

# Inside, below and above, and at each bound, which is then the bound itself.
>> frac clamp(1/3, 0, 1)
1/3

>> clamp(-2, 0, 1)
0

>> clamp(5, 0, 1)
1

>> clamp(0, 0, 1)
0

>> clamp(1, 0, 1)
1

>> clamp(-7/2, -3, 3)
-3

# Inexact where any argument is: a guard reading a double is an inexact
# truth, and so is the clause it chooses (C241), a bound or x.
>> clamp(~0.25, 0, 1)
~0.25

>> clamp(~2, 0, 1)
~1

>> clamp(1/2, 0, ~1)
~0.5

>> clamp(2, 0, ~1)
~1

# lo above hi gives lo, whatever x: min(hi, x) is at most hi, below lo, so
# max takes lo.
>> clamp(1/2, 1, 0)
1

>> clamp(-5, 1, 0)
1

>> clamp(5, 1, 0)
1

# Single values, as max and min take; a matrix is clamped by its cells.
>> clamp([1; 2], 0, 1)
error: clamp needs single values, not a 2x1 matrix; write it by its cells

>> clamp(1/2, [0, 0], 1)
error: clamp needs single values, not a 1x2 matrix; write it by its cells

>> cl[j<=3] = clamp([-1; 1/2; 2][j], 0, 1)
cl[j<=3] = clamp([-1; 1/2; 2][j], 0, 1)

>> cl
[  0;
 0.5;
   1]

# grad through the clause taken: 1 inside, 0 below and above, and 0 at
# each kink, where the bound is taken, its own slope 1.
>> grad_(t = 1/2) clamp(t, 0, 1)
1

>> grad_(t = -1) clamp(t, 0, 1)
0

>> grad_(t = 2) clamp(t, 0, 1)
0

>> grad_(t = 0) clamp(t, 0, 1)
0

>> grad_(t = 1) clamp(t, 0, 1)
0

>> grad_(t = 1/4) clamp(3*t, 0, 1)
3

>> grad_(t = 1/3) clamp(3*t, 0, 1)
0

>> grad_(l = 0) clamp(0, l, 1)
1

>> grad_(h = 1) clamp(1, 0, h)
1

# Reverse mode, by a matrix: the same slopes, cell by cell.
>> grad_(v = [-1, 0, 1/2, 1, 2]) sum_(k=1)^5 clamp(v[1,k], 0, 1)
[0, 0, 1, 0, 0]

>> grad_(v = [0, 0, 1]) clamp(v[1,1], v[1,2], v[1,3])
[0, 1, 0]

>> grad_(v = [1, 0, 1]) clamp(v[1,1], v[1,2], v[1,3])
[0, 0, 1]

>> ?clamp
clamp(x, lo, hi) = max(lo, min(hi, x))

>> tex ?clamp
\operatorname{clamp}(x, \mathit{lo}, \mathit{hi}) = \max(\mathit{lo}, \min(\mathit{hi}, x))

# A prelude name: a session's clamp, even a guarded clause alone, is a
# definition of its own, which joins nothing, and clear gives the prelude's
# back.
>> clamp(x) | x > 1 = 1
clamp(x) | x > 1 = 1

>> clamp(5)
1

>> clamp(1/2)
error: no clause of clamp applies

>> clamp(5, 0, 1)
error: clamp expects 1 argument, got 3

>> clear clamp
clear clamp

>> clamp(5, 0, 1)
1

>> clear clamp
error: clamp comes with the language, so it cannot be cleared

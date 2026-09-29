# Files and their scopes (MODERNIZATION.md, phase 15). 'use gain' runs the
# definitions of gain.ink, beside the file that names it, into a scope of
# their own: they read their own names, then the prelude's and the built-ins',
# never the session's, and the session reaches them qualified, 'gain.k'.

>> use gain
use gain

>> gain.k
2

>> k
error: k is not defined

# An input is declared, 'x_n = input', and is a definition the module lacks.
# Asked for, it says so; supplied from the session, as any definition of the
# module is replaced, it is read.
>> gain.y_1
error: gain.x_1 is an input, and nothing defines it

>> gain.x_n = n
gain.x_n = n

>> gain.y_3
7

>> gain.s_3
15

# The session's names are the session's and the module's the module's: a
# parameter is replaced from the session by naming it in the module.
>> k = 10
k = 10

>> gain.y_3
7

>> gain.k = 3
gain.k = 3

>> gain.y_3
10

>> k
10

# What the session supplies reads the session's names, where it was written.
>> step = 1/2
step = 1/2

>> gain.x_n = step*n
gain.x_n = step*n

>> gain.y_2
4

>> ?gain.k
gain.k = 3

>> ?gain.b
b = 1

# Names brought in unqualified are those listed, and no others.
>> use gain (y)
use gain (y)

>> y_2
4

>> s_2
error: s is not defined

# A file is loaded once: a second 'use' finds it, and what was replaced stays
# replaced.
>> use gain
use gain

>> gain.k
3

# A file that cannot be read, or does not parse, is not loaded at all, and
# says where.
>> use nothere
error: nothere.ink cannot be opened

>> use bad
error: bad.ink, line 3: missing ')' after '2'

>> bad.fine
error: bad is not used

# An input in the session is declared as in a file.
>> z_n = input
z_n = input

>> z_2
error: z_2 is an input, and nothing defines it

>> z_n = n^2
z_n = n^2

>> z_2
4

# The prelude, the outermost scope, is a file too: 'ceil' and 'mod', a line of
# floor each, replaceable as the built-ins are. Not 'round', whose rule is the
# model's to choose (floor.ink).
>> ceil(7/2)
4

>> mod(-7, 3)
2

>> ?mod
mod(a, b) = a - b*floor(a/b)

>> round(5/2)
error: round is not defined

# 'input' and 'use' are words of the language, so not names.
>> input = 3
error: input is reserved, so it cannot be defined

# A model's history of its inputs (DESIGN.md, next in line). The step's
# window holds, before the stream, what the model's history says, where it
# held an implicit 0, and the interpreter reads the same history before the
# instance's argument. 'past' is a delay by one step, and 'twin' reads two
# inputs, each with a history of its own. 'lit' is a decimator
# whose filter reaches before the stream, and whose hold reads y_-1, before
# y's first tick, which init folds from the history into y's window.
#
#     past: 100 steps from 0, against exact values
#     past.c: within 0
#
#     twin: 100 steps from 0, against exact values
#     twin.c: within 0
#
#     lit: 100 steps from 0, against exact values
#     lit.f: within 0
#     lit.y: within 0, from 1
#     lit.z: within 0
#
# A read the left of an 'and' keeps from before the stream is no read: at 0
# the left decides, as in the interpreter, and 'edge' needs no history.
#
#     rise: 100 steps from 0, against exact values
#     rise.c: within 0
#
# Where it does not decide, the read on its right before the stream is a term
# the history gives: 'ajar' reads x_-3 to x_-1 at 0 to 2.
#
#     ajar: 100 steps from 0, against exact values
#     ajar.c: within 0
#
# A slow sequence's base clause that reads the input before the stream reads
# the history too: 'sown' seeds y_0 with x_-1.
#
#     sown: 100 steps from 0, against exact values
#     sown.<name>: within 0, for each of y and z
#
# An instance within a model: 'nest' reads an input with no history of its
# own, whose argument is the outer input, so init folds the outer history
# into the inner window; in 'twice' the inner history beats the outer one.
#
#     nest: 100 steps from 0, against exact values
#     nest.<name>: within 0, for each of c, inner.c and inner.u
#
#     twice: 100 steps from 0, against exact values
#     twice.<name>: within 0, for each of c, inner.c and inner.x
#
# The inner history holds where the argument, a hold, starts at the stream:
# 'relayed' reads inner.x_-1 at 0.
#
#     relayed: 100 steps from 0, against exact values
#     relayed.<name>: within 0, for each of c, y, inner.c and inner.x
#
# An instance written unnamed, whose terms the interpreter cannot name
# (C84), is said not to be asked, where its lines read 'within 0':
#
#     veiled: 100 steps from 0, against exact values
#     veiled.c_bare.<name>: not asked, as the interpreter cannot name it, for
#     each of c and u
#     veiled.c: within 0
#
# A term the step computes and the interpreter cannot give parts, where it
# was skipped: 'pole' reported 'within 0', its step inf at 3.
#
#     pole: 100 steps from 0, against exact values
#     pole.y: inf at 3, where the interpreter gives none: division by zero
#
# And one the interpreter gives that no double holds parts too, saying so:
# 'huge' passed 'within 1.5e+284', its step inf at 31 where the term is
# 10^310.
#
#     huge: 100 steps from 0, against exact values
#     huge.y: inf at 31, where the interpreter's term is too large for a double
#
# What 'inkamath --compile' refuses, each model a file of its own compiled
# by name with -o. A read before the stream that no history gives, in a
# clause, beyond a term of the history, and on the right of an 'and' whose
# left does not decide at 0:
#
#     open(x_n) = {
#         c_n = x_(n-1)
#     }
#     short(x_n) = {
#         x_(-1) = 0
#         c_n = x_(n-2)
#     }
#     level(x_n) = {
#         c_n = n >= 0 and x_n > x_(n-1)
#     }
#     half(u_n, v_n) = {
#         u_n | n < 0 = 0
#         c_n = u_(n-1) + v_(n-1)
#     }
#
#     inkamath: cannot compile c: c_0 reads x_-1, before the stream, where x has no history
#     inkamath: cannot compile c: c_0 reads x_-2, before the stream, where x has no history
#     inkamath: cannot compile c: c_0 reads x_-1, before the stream, where x has no history
#     inkamath: cannot compile c: c_0 reads v_-1, before the stream, where v has no history
#
# A hold before a slow sequence's first tick, whose samples read the input
# before the stream where no history gives it, is refused as a hold whose
# samples could give a term; today it compiles, and --check parts at 0:
#
#     held(x_n) = {
#         y_m = x_(2*m)
#         z_n = y_(floor(n/2) - 1)
#     }
#
#     inkamath: cannot compile z: z_0 reads y_-1, before y's first tick, where its samples could give a term
#
# A history that reaches into the stream, whose guard is not its index below
# a constant, that reads a parameter, or that is not a single value (C83):
#
#     early(x_n) = {
#         x_n | n < 2 = 0
#         c_n = x_(n-1)
#     }
#     even(x_n) = {
#         x_n | n^2 > 4 = 0
#         c_n = x_(n-3)
#     }
#     mute(u_n, x_n) = {
#         x_n | n < 0 and u_n > 0 = 0
#         c_n = x_(n-1)
#     }
#     biased(a = 1, x_n) = {
#         x_n | n < 0 = a
#         c_n = x_(n-1)
#     }
#     swap(x_n) = {
#         x_n | n < 0 = [0; 0]
#         c_n = [0 1; 1 0]*x_(n-1)
#     }
#
#     inkamath: cannot compile x: its history reaches x_0, in the stream
#     inkamath: cannot compile x: a history whose guard is not its index below a constant
#     inkamath: cannot compile x: a history whose guard is not its index below a constant
#     inkamath: cannot compile x: a history that reads a
#     inkamath: cannot compile x: a history of another shape
#
# And what the seven programs that skip a term today report, once 'down' in
# decimate.ink states x_n | n < 0 = 0: each sequence from the step that
# computes its first term, which is said where it is after the first step.
#
#     gain.K: within <x>, from 1
#     pairs.w: within 0, from 2
#     odd.z: within 0, from 2
#     boxcar.y: within 0, from 1
#     boxcar.q: within 0, from 3
#     boxcar.<name>: within 0, for each of f, t, w and z
#     stride.h: within 0, from 1
#     frame.p: within 0, from 2
#     trail.w: within 0, from 1
#
# and every other line of theirs as it is.
delay(x_n) = {
    x_n | n < 0 = 0
    c_n = x_(n-1)
}
past = delay(x_n = n^2)

both(u_n, v_n) = {
    u_n | n < 0 = 1
    v_n | n < 0 = 0
    c_n = u_(n-1) - v_(n-1)
}
twin = both(u_n = n, v_n = 2*n)

warmed(x_n) = {
    x_n | n < 0 = 1
    f_n = (x_n + x_(n-1))/2
    y_m = f_(2*m + 1)
    z_n = y_(floor((n - 1)/2))
}
lit = warmed(x_n = n^2)

edge(x_n) = {
    c_n = n > 0 and x_n > x_(n-1)
}
rise = edge(x_n = (n - 3)^2)

gate(x_n) = {
    x_n | n < 0 = 5
    c_n = n < 4 and x_(n-3) > 0
}
ajar = gate(x_n = n)

seed(x_n) = {
    x_n | n < 0 = 1
    y_0 = x_(-1)
    y_m = y_(m-1) + x_(2*m)
    z_n = y_(floor(n/2))
}
sown = seed(x_n = n)

bare(u_n) = {
    c_n = u_(n-1)
}
wrap(x_n) = {
    x_n | n < 0 = 3
    inner = bare(u_n = x_n)
    c_n = inner.c_n
}
nest = wrap(x_n = n)

shift(x_n) = {
    x_n | n < 0 = 5
    inner = delay(x_n = x_n)
    c_n = inner.c_n + x_(n-1)
}
twice = shift(x_n = n)

relay(x_n) = {
    y_m = x_(2*m)
    inner = delay(x_n = y_(floor(n/2)))
    c_n = inner.c_n
}
relayed = relay(x_n = n + 1)

veil(x_n) = {
    x_n | n < 0 = 3
    c_n = bare(u_n = x_n).c_n
}
veiled = veil(x_n = n)

inv(x_n) = {
    y_n = 1/(x_n - 3)
}
pole = inv(x_n = n)

grow(x_n) = {
    y_0 = 1
    y_n = y_(n-1)*10^10 + x_n
}
huge = grow(x_n = n)

# A guard compiled on its own reads a term that is compiled on the way, and
# that term's reads are its own: 'd' starts at 1, where y_(n-1) exists, and
# the guarded 'a' that reads it with it. They started at 0 while the reader's
# name sorted before the term's, reading the 0 init leaves (DESIGN.md, C92):
#
#     kin.d: 1 at 0, where the interpreter gives none: y has no clause for index -1
order(x_n) = {
    y_0 = 1
    y_n = 2*y_(n-1)
    d_n = y_n - y_(n-1)
    a_n | d_n > 0 = 1
    a_n = 0
}
kin = order(x_n = 1)

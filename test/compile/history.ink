# A model's history of its inputs (DESIGN.md, next in line). The step's
# window holds, before the stream, what the model's history says, where it
# held an implicit 0, and the interpreter reads the same history before the
# instance's argument. 'past' is a delay by one step. 'lit' is a decimator
# whose filter reaches before the stream, and whose hold reads y_-1, before
# y's first tick, which init folds from the history into y's window.
#
#     past: 100 steps from 0, against exact values
#     past.c: within 0
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
# A term the step computes and the interpreter cannot give parts, where it
# was skipped: 'pole' reported 'within 0', its step inf at 3.
#
#     pole: 100 steps from 0, against exact values
#     pole.y: inf at 3, where the interpreter gives none: division by zero
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
#
#     inkamath: cannot compile c: c_0 reads x_-1, before the stream, where x has no history
#     inkamath: cannot compile c: c_0 reads x_-2, before the stream, where x has no history
#     inkamath: cannot compile c: c_0 reads x_-1, before the stream, where x has no history
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
#     inkamath: cannot compile x: a history that reads a
#     inkamath: cannot compile x: a history that is not a single value
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

inv(x_n) = {
    y_n = 1/(x_n - 3)
}
pole = inv(x_n = n)

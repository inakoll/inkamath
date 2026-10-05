# exp, log and tanh as the prelude writes them (DESIGN.md, next in line),
# compiled. A call to a function of the prelude is a C function of the
# header's own, emitted once before the step, as every function of the
# prelude it calls is: inlined, ilogb alone writes its argument 3^13 times.
# The step computes the doubles the interpreter computes, by the same
# operations, so every term agrees to the bit; 'a' is a double, so every
# argument is one too. Clang fuses a multiply and an add within an expression
# even as strict C, so this holds on a target without the instruction, as
# CI's x86-64 is.
#
#     els: 100 steps from 0, against exact values until 0 and inexact ones from there
#     els.<s>: within 0; the interpreter's terms about <x> from the exact ones
#
# a line for each of e, l, t, h and g, each estimate the seeds'. The program
# defines els_exp, els_log and els_tanh, and calls no pow of e.
#
# From an exact 'a' the interpreter reduces each argument exactly and the
# step its double, whose rounding a function multiplies by its condition, x
# for exp: from a = 1/3, e is 'within' 3.2e-06, 13 units in the last place
# of its largest term.
curves(a = 1/4) = {
    e_n = exp(a*n - 12)
    l_n = log(a*n/2 + 1/1024)
    t_n = tanh(a*n/2 - 6)
    h_0 = 0
    h_n = tanh(2*h_(n-1) - a)
    g_n = log(exp(a*n - 12))
}
els = curves(a = ~(1/4))

# --steps n (DESIGN.md, next in line): --check steps an instance n times
# where it stepped it a hundred, n from 1 to 100000, and says so on its
# first line. Every line after covers the n steps: the values, the
# estimate's three runs, the flips and the straddles. The steps are the
# step's calls, so n of them compare terms from the first to the first
# plus n - 1: a model of N steps after its initial term is checked whole
# with N + 1. Without --steps, a hundred, and no report moves.

# A coast in the manner of Apollo 11's (apollo11.ink), whose check could
# hold only a tenth of its steps: a body thrown up at 64 and falling back
# under g = 1, by semi-implicit Euler, N = 1000 steps of h = T/N = 1/8. So
# v_n = 64 - n/8 and y_n = 8n - n(n+1)/128, every term a multiple of 1/64
# below 2^11, which a double and a float hold exactly: the step computes
# each to the bit. The coast whole is y_0 to y_1000 = 2875/16, 1001 steps,
# 'inkamath --check steps.ink coast --steps 1001 -o coast.c':
#
#     coast: 1001 steps from 0, against exact values
#     coast.v: within 0
#     coast.y: within 0
#
# With --float too:
#
#     coast: 1001 steps from 0 in float, against exact values
#     coast.v: within 0, 0 units of a float
#     coast.y: within 0, 0 units of a float
#
# And with --steps 1, its initial term alone:
#
#     coast: 1 step from 0, against exact values
#     coast.v: within 0
#     coast.y: within 0
fall(N = 1000, T = 125, g = 1, v0 = 64) = {
    h = T/N
    v_0 = v0
    v_n = v_(n-1) - g*h
    y_0 = 0
    y_n = y_(n-1) + h*v_n
}
coast = fall()

# A mode that runs away, as Doyle 1978's loop past its margin does
# (doyle.ink), only later: x_n = 4^n, a power of two the step computes to
# the bit until 4^512 = 2^1024, past the largest double, where the step's
# product is inf and the interpreter's term, exact, is no double. Its terms
# stay exact to the last, 4^999 being 602 digits. A hundred and fifty
# steps do not reach it, 'inkamath --check steps.ink runaway --steps 150':
#
#     runaway: 150 steps from 0, against exact values
#     runaway.x: within 0
#
# and a thousand do, the program exiting with a failure:
#
#     runaway: 1000 steps from 0, against exact values
#     runaway.x: inf at 512, where the interpreter's term is too large for a double
grow(r = 4) = {
    x_0 = 1
    x_n = r*x_(n-1)
}
runaway = grow()

# A slow filter, a tenth of the way to its input in a hundred steps and 63
# per cent in a thousand: y_n = 1 - (999/1000)^n, in lowest terms over
# 10^(3n), whose numerator is odd and not a multiple of 5. Its denominator
# is a thousand digits at 333 and past them at 334, where the interpreter
# approximates the term and computes in doubles from there; the first line
# says so as it says it of any check whose terms turn inexact, 'inkamath
# --check steps.ink horizon --steps 1000':
#
#     horizon: 1000 steps from 0, against exact values until 334 and inexact ones from there
#     horizon.y: within <d>; the interpreter's terms about <e> from the exact ones
#
# <d> is the doubles' own drift, 1.1e-16 by the step's operations simulated
# in Python against the exact terms and, from 334, against doubles from the
# term rounded there; <e> is the seeds', a few hundred roundings each damped
# by 999/1000 a step, between 1e-16 and 1e-13 and past no tolerance.
lag(a = 1/1000) = {
    y_0 = 0
    y_n = y_(n-1) + a*(1 - y_(n-1))
}
horizon = lag()

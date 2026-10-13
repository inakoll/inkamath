# A function of the prelude that reads a cell at one of its parameters,
# called on single values, is written where it is called, its places
# constants there (C356): hurwitzc(q, i) is q's ith coefficient, q itself
# at 1 and 0 past it. Each term is the interpreter's:
#
#     placed: 100 steps from 0, against exact values
#     placed.y: within 0
places(x_n) = {
    y_n = hurwitzc(x_n, 1) + 2*hurwitzc(x_n, 2)
}
placed = places(x_n = n/3)

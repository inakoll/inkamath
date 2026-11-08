# check() as Model() steps it (DESIGN.md), run with -I one
# -I two: probe, in two, reads gains beside it, else in one, so p = 10,
# never app/gains.ink's 1, which comes first only for a file in app. With
# p = 10, scale(2) = 20 and y_n = n + c; with 1, y has no clause, refused.
use probe
scale(x) = probe.p*x
deep(c = 0) = {
    y_n | probe.p > 5 = n + c
}

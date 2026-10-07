# The oracle (DESIGN.md, next in line): 'inkamath --check drift.ink calm
# -o calm.c' compiles an instance and writes a C program that steps it on the
# inputs the interpreter gives the instance, holding each term to the exact one.
smooth(a = 1/4, u_n) = {
    v_0 = 0
    v_n = a*u_n + (1-a)*v_(n-1)
}
calm = smooth(u_n = n/10)

# A tenth at every step, which a double is not: what rounding puts in, each
# step multiplies by ten, until the compiled term is not the exact one.
tenfold(c = 1) = {
    d_0 = c
    d_n = 10*d_(n-1) - 9*c
}
wild = tenfold(c = 1/10)

# A turn by the angle whose cosine is 3/5, so exact at every step, and a
# matrix held cell by cell.
turn(c = 3/5, s = 4/5) = {
    p_0 = [1; 0]
    p_n = [c, -s; s, c]*p_(n-1)
}
spin = turn()

# A parameter made inexact: the interpreter's terms are doubles from the
# first that reads it, and the program says so.
rough = smooth(a = ~(1/4), u_n = n/10)

# A guard exactly on its threshold: the interpreter's tenth is never below a
# tenth, and the compiled one is from the first step, by a rounding. The
# program reports the clause each takes before the values that follow it.
# A trillionth below it, the guard holds out to the sixth step, and its
# margin says by how little.
edge(c = 1, w = 0) = {
    d_0 = c
    d_n = 10*d_(n-1) - 9*c
    g_n | d_n < c - w = 1
    g_n = 0
}
brink = edge(c = 1/10)
ledge = edge(c = 1/10, w = 1/10^12)

# The same threshold cell by cell: each cell's guard is asked alone, and the
# first cell whose compiled guard takes another clause is reported, by its
# place, before the values that follow it.
cut(c = 1/10) = {
    d_0 = [c; 2*c]
    d_n = 10*d_(n-1) - 9*[c; 2*c]
    g_n[j<=2] | d_n[j] < c*j = 1
    g_n[j<=2] = 0
}
rift = cut()

# The same threshold asked every second step, by a sequence with no base
# clause (DESIGN.md, next in line: what several rates left refused). The
# clause a slow term takes is, as its value is, the latest computed at the
# step, so a flip is reported at the step that computes the term taking
# another clause, with that term's margin, before the values it makes part:
#
#     sill: 100 steps from 0, against exact values
#     sill.g: at 2 the compiled step takes 'g_m | d_(2*m) < c = 1' and the interpreter 'g_m = 0'; the guard of the first is exactly on its threshold
#     sill.d: <x> at 9, where the interpreter gives 0.10000000000000001
#     sill.g: 1 at 2, where the interpreter gives 0
sparse(c = 1/10) = {
    d_0 = c
    d_n = 10*d_(n-1) - 9*c
    g_m | d_(2*m) < c = 1
    g_m = 0
}
sill = sparse()

# And cell by cell, each cell's flip by its place:
#
#     seam: 100 steps from 0, against exact values
#     seam.g[1,1]: at 2 the compiled step takes 'g_m[j<=2] | d_(2*m)[j] < c*j = 1' and the interpreter 'g_m[j<=2] = 0'; the guard of the first is exactly on its threshold
#     seam.d[1,1]: <x> at 9, where the interpreter gives 0.10000000000000001
#     seam.g[1,1]: 1 at 2, where the interpreter gives 0
strided(c = 1/10) = {
    d_0 = [c; 2*c]
    d_n = 10*d_(n-1) - 9*[c; 2*c]
    g_m[j<=2] | d_(2*m)[j] < c*j = 1
    g_m[j<=2] = 0
}
seam = strided()

# A negation is a subtraction from 0, as in the interpreter (C33), so the
# negation of +0 is +0, where C's minus gives -0 and 1/(-x_n) -inf (C98):
#
#     naught: 100 steps from 0, against exact values
#     naught.y: within 0
negated(x_n) = {
    y_n | 1/(-x_n) > 0 = 1
    y_n = 0
}
naught = negated(x_n = ~0)

# A subtraction is an addition of a subtraction from 0, as in the
# interpreter, so -0 less +0 is +0, where C's minus gives -0 (C109):
#
#     zeroed: 100 steps from 0, against exact values
#     zeroed.y: within 0
subtracted(x_n) = {
    y_n | 1/(x_n*(-1) - x_n) > 0 = 1
    y_n = 0
}
zeroed = subtracted(x_n = ~0)

# An input of -0 is fed as -0, not as C's integer 0 (C120):
#
#     mirrored: 100 steps from 0, against exact values
#     mirrored.y: within 0
inverted(x_n) = {
    y_n | 1/x_n > 0 = 1
    y_n = 0
}
mirrored = inverted(x_n = ~0*(-1))

# A call given to a function is computed once, however often the function
# reads it, so calls nested ten deep are ten temporaries (C140):
#
#     nests: 100 steps from 0, against exact values
#     nests.s: within 0
#     nests.x: within 0
nested(u_n) = {
    f(p) = p/2 + p/4 + p/4
    h(a, b) = a + b - a*b + b*a
    x_0 = 0
    x_n = h(f(f(f(f(f(f(f(f(f(f(x_(n-1))))))))))), f(f(u_n)))
    s_n = grad_(t = u_n) f(f(f(t*t)))
}
nests = nested(u_n = n)

# A guard written as a sequence of its own, which reads the guarded sequence
# back: below the base clauses that guard asks lower still, so no guard gives
# a term there, and the guard starts at 1 in the step as in the interpreter
# (C146).
latched(x_n) = {
    w_0 = 0
    m_n = w_(n-1) <= x_n
    w_n | m_n = w_(n-1) + 1
    w_n = w_(n-1) - 1/2
}
latch = latched(x_n = n/3)

# A term the interpreter runs out of steps for, a thousand-term sum reading
# one at each term, is no answer, and not compared: 'toil' at 2 (C148). A
# limit of such sums answers, each term it walks having its own steps (C149).
#
#     toil: 100 steps from 0, against exact values until 0 and inexact ones from there
#     toil.y: within 0; the interpreter ran out of steps at 2, not compared
heavy(x_n) = {
    s = sum_(j=1)^1000 ~1/j^2
    y_n | n == 2 = sum_(k=1)^1000 x_n*s/k^2
    y_n = x_n
}
toil = heavy(x_n = ~1)

# A sequence based at 1 beside one based at 0: the step starts it at 1, and
# the interpreter has no term of it at 0 (C151).
#
#     late: 100 steps from 0, against exact values
#     late.v: within 0
#     late.x: within 0, from 1
offset() = {
    v_0 = 0
    v_n = v_(n-1) + 1
    x_1 = 1
    x_n = x_(n-1)/2
}
late = offset()

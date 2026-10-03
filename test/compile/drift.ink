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

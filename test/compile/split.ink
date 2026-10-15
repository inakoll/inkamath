# A compiled step split into functions (DESIGN.md): a list of the step over
# 65,536 bytes is written as functions of about 16,384, and what a later one
# reads crosses in a struct of the step's values. Each report below is the
# unsplit header's, byte for byte, in double and with --float.

# 'carried', a step of about 128 KB whole, y's 1000 cells: the first of the
# 8 functions of y's list computes an inverse, an array, and the flag of a
# product's cells tested for NaN, and every later one reads both. Over 4
# steps:
#
#     carried: 4 steps from 0, against exact values
#     carried.x: within <e>
#     carried.y: within <e>
cut(v_n[i<=3]) = {
    x_0 = [1 2 3; 4 5 6; 7 8 10]
    x_n = (x_(n-1) + v_n*v_n')^-1*x_(n-1)/2
    y_0[i<=1000] = i
    y_n[i<=1000] = y_(n-1)[i]/2 + ((x_(n-1) + v_n*v_n')^-1)[1,1] + (x_(n-1)*v_n)[2,1]
}
carried = cut(v_n = [n; 1; 2])

# 'apart', a step of about 73 KB whole, x's 1000 cells, whose temporaries
# only the first cell reads: cut, nothing crosses, and the header has no
# struct of values, which C would refuse empty.
g(x) = [x[2]+1, x[3]+1]*[x[4]+1, 2; x[5]+1, 3]
f(x)[i<=1000] | i == 1 = (g(x)[1,1] + g(x)[1,2])/1000
f(x)[i<=1000] = x[i]/2
alone() = {
    x_0[i<=1000] = i
    x_n = f(x_(n-1))
}
apart = alone()

# 'twins', a bank of two instances of 'held', each y's list of about 140 KB
# whole cut into 9 functions, carrying an inverse, a product's cell and the
# flag of each tested for NaN from its first function to the 8 after it.
# The two are stepped one after the other, so that their values share the
# model's place (DESIGN.md, a split step's scratch in the model struct).
# Over 4 steps:
#
#     twins: 4 steps from 0, against exact values
#     twins.a_held.v: within 0
#     twins.a_held.x: within <e>
#     twins.a_held.y: within <e>
#     twins.a: within <e>
#     twins.b_held.v: within 0
#     twins.b_held.x: within <e>
#     twins.b_held.y: within <e>
#     twins.b: within <e>
held(v_n[i<=3]) = {
    x_0 = [1 2 3; 4 5 6; 7 8 10]
    x_n = (x_(n-1) + v_n*v_n')^-1*x_(n-1)/2
    y_0[i<=1000] = i
    y_n[i<=1000] = y_(n-1)[i]/2 + ((x_(n-1) + v_n*v_n')^-1)[1,1] + (x_(n-1)*v_n)[2,1]
}
twice(v_n[i<=3]) = {
    a_n = held(v_n = v_n).y_n[1]
    b_n = held(v_n = 2*v_n).y_n[1]
}
twins = twice(v_n = [n; 1; 2])

# 'mine', an instance named as its model: the scratch is the model's last
# member, not the instance's, whose struct closes on the same name.
mine(v_n[i<=3]) = {
    mine = held(v_n = v_n)
    a_n = mine.y_n[1]
}

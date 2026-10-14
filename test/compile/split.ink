# A compiled step split into functions (DESIGN.md): a list of the step over
# 65,536 bytes is written as functions of about 16,384, and what a later one
# reads crosses in a struct of the step's values. Each report below is the
# unsplit header's, byte for byte, in double and with --float.

# 'carried', a step of about 76 KB whole, y's 500 cells: the first function
# of y's list computes an inverse, an array, and the flag of a product's cells
# tested for NaN, and every later one but the last, the list's NaN test,
# reads both. Over 4 steps:
#
#     carried: 4 steps from 0, against exact values
#     carried.x: within <e>
#     carried.y: within <e>
cut(v_n[i<=3]) = {
    x_0 = [1 2 3; 4 5 6; 7 8 10]
    x_n = (x_(n-1) + v_n*v_n')^-1*x_(n-1)/2
    y_0[i<=500] = i
    y_n[i<=500] = y_(n-1)[i]/2 + ((x_(n-1) + v_n*v_n')^-1)[1,1] + (x_(n-1)*v_n)[2,1]
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

# 'paired', y's list and then z's, each about 125 KB whole and cut into 8
# functions, each carrying an inverse, a product's cell and the flag of
# each tested for NaN from its first function to the 7 after it. No
# function reads or stores values of both lists, so that z's share the
# place of y's (DESIGN.md, a split step's values by their lifetimes). Over
# 4 steps:
#
#     paired: 4 steps from 0, against exact values
#     paired.x: within <e>
#     paired.y: within <e>
#     paired.z: within <e>
turns(v_n[i<=3]) = {
    x_0 = [1 2 3; 4 5 6; 7 8 10]
    x_n = (x_(n-1) + v_n*v_n')^-1*x_(n-1)/2
    y_0[i<=1000] = i
    y_n[i<=1000] = y_(n-1)[i]/2 + ((x_(n-1) + v_n*v_n')^-1)[1,1] + (x_(n-1)*v_n)[2,1]
    z_0[i<=1000] = i
    z_n[i<=1000] = z_(n-1)[i]/3 + ((x_(n-1) + v_n*v_n'/2)^-1)[2,2] + (x_(n-1)*v_n)[3,1]
}
paired = turns(v_n = [n; 1; 2])

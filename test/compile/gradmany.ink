# A gradient with respect to several names compiled (DESIGN.md, next in
# line), the second increment's: one grad by every weight of a model, its
# body written once and swept back once, each name's part its seed's
# adjoints, in the interpreter's order. A definition of the grad compiles as
# an instance does, a struct of its parts, each a sequence of the step, so
# 'm.g.W[0]' is g.W_n. Every number below is worked out apart from the
# interpreter and the compiler, none recorded. Not wired: nothing compiles a
# grad by several names before the second increment.

# 'pair', two layers, tanh between them by its cells, trained on its square
# error by one grad of its four weights, from an inexact start, on inputs in
# thirds. By backpropagation written out in NumPy, (V*h + d - y) times h'
# for V, 1 for d, times V'*(1 - h^2) for c and that times x' for W, h =
# tanh(W*x + c); step 1 within 2.8e-17 of a complex step of 1e-30 for each
# cell:
#
#     W_1  = [0.496231452695 -0.25; 0.25386748378 0.5; -0.50193374189 0.25]
#     c_1  = [0.011305641914; 0.113397548661; -0.11919877433]
#     V_1  = [0.496161276044 -0.499032009603 0.250967990397]
#     d_1  = 0.0232452122872
#     W_99 = [0.440253295218 -0.0790955787301; 0.231927003032 0.285457453347;
#             -0.431361116583 0.300886481769]
#     c_99 = [0.00670145040213; 0.0782478608944; -0.0751812972149]
#     V_99 = [0.127026710341 -0.117310863253 0.13092307325]
#     d_99 = -0.0443452221259
#
# Every value its sweep multiplies by is a double in the interpreter too, or
# a cell of x, exact there but meeting only doubles; so the step's parts are
# the interpreter's operation for operation, and each is a sequence of the
# step from 1, where g's first term reads W_0:
#
#     pair: 100 steps from 0, against exact values until 0 and inexact ones from there
#     pair.V: within 0; the interpreter's terms about <e> from the exact ones
#     pair.W: within 0; the interpreter's terms about <e> from the exact ones
#     pair.c: within 0; the interpreter's terms about <e> from the exact ones
#     pair.d: within 0; the interpreter's terms about <e> from the exact ones
#     pair.g.V: within 0, from 1; the interpreter's terms about <e> from the exact ones
#     pair.g.W: within 0, from 1; the interpreter's terms about <e> from the exact ones
#     pair.g.c: within 0, from 1; the interpreter's terms about <e> from the exact ones
#     pair.g.d: within 0, from 1; the interpreter's terms about <e> from the exact ones
#
# Its header, 'inkamath --compile gradmany.ink net -o net.h', calls tanh's
# part function once for each of the three units, at a part of 1, its slope:
# 'net_tanh_dx(' written 4 times, its definition and three slopes. Four
# grads of one name each, 'W_n = W_(n-1) - eta*grad_(A = W_(n-1)) L(A,
# c_(n-1), V_(n-1), d_(n-1), x_n, y_n)' and its like for c, V and d, write
# it 7 times at a206c5d, a sweep each for W and c, in 15,792 bytes; the one
# grad's header is smaller.
net(eta = 1/4, x_n[i<=2], y_n) = {
    h(z)[i<=3] = tanh(z[i])
    L(W, c, V, d, x, y) = (V*h(W*x + c) + d - y)^2/2
    W_0 = ~[1/2 -1/4; 1/4 1/2; -1/2 1/4]
    c_0 = ~[0; 1/8; -1/8]
    V_0 = ~[1/2 -1/2 1/4]
    d_0 = ~0
    g_n = grad_(W = W_(n-1), c = c_(n-1), V = V_(n-1), d = d_(n-1)) L(W, c, V, d, x_n, y_n)
    W_n = W_(n-1) - eta*g.W_n
    c_n = c_(n-1) - eta*g.c_n
    V_n = V_(n-1) - eta*g.V_n
    d_n = d_(n-1) - eta*g.d_n
}
pair = net(x_n = [mod(n, 5) - 2; mod(3*n, 7) - 3]/3, y_n = mod(n, 3)/2 - 1/2)

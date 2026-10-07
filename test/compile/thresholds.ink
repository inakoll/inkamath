# Thresholds met exactly. The interpreter's terms land on a guard's
# threshold, so it takes the clause the exact value decides; a float step
# lands a rounding beside it and takes the other. In double both hold.

# A dying ReLU (Lu et al. 2019, "Dying ReLU and initialization"): a bias
# trained toward a negative target, b_n + 0.729 = 0.9^n, meets 0 at n = 3
# and dies there, the gradient through the dead clause 0. In float it is
# alive by a hair at 3, takes one step more, and dies at -0.0729:
#
#     dies.alive: at 3 the compiled step takes 'alive_n | b_n > 0 = 1' and the interpreter 'alive_n = 0'; the guard of the first is exactly on its threshold
#     dies.b: -0.0728999898 at 4, where the interpreter gives 0
neuron(eta = 1/10, t = -729/1000, b0 = 271/1000) = {
    relu(z) = 0
    relu(z) | z > 0 = z
    loss(c) = (relu(c) - t)^2/2
    b_0 = b0
    b_n = b_(n-1) - eta*grad_(c = b_(n-1)) loss(c)
    alive_n | b_n > 0 = 1
    alive_n = 0
}
dies = neuron()

# A deadbeat Luenberger observer (Luenberger 1971; Franklin, Powell &
# Workman, Digital Control, 8.3): L places both eigenvalues of A - L C at 0,
# so the estimation error is exactly 0 from n = 2. The flip is the other
# way: the interpreter takes the guarded clause, the float step misses it:
#
#     beat.locked: at 2 the compiled step takes 'locked_n = 0' and the interpreter 'locked_n | e_n[1] == 0 and e_n[2] == 0 = 1'; the guard of the second is exactly on its threshold
observer(x0 = [3/10; -7/10], u_n) = {
    A = [1 1; 0 1]
    B = [1/2; 1]
    C = [1 0]
    L = [2; 1]
    x_0 = x0
    x_n = A*x_(n-1) + B*u_(n-1)
    xh_0 = [0; 0]
    xh_n = A*xh_(n-1) + B*u_(n-1) + L*(C*x_(n-1) - C*xh_(n-1))
    e_n = x_n - xh_n
    locked_n | e_n[1] == 0 and e_n[2] == 0 = 1
    locked_n = 0
}
beat = observer(u_n = mod(n, 3)/10 - 1/10)

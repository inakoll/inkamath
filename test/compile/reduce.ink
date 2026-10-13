# Reductions over an index (DESIGN.md, a maximum, minimum or argmax over an
# index), unrolled as a sum is, a later term kept where it ranks strictly
# higher, so the first of equals. 'pooled': the largest of four inputs, the
# first index of the largest and of the smallest, and a max-pooling of
# windows of two. At 0 inputs 2 and 4 tie at 1, at 5 inputs 1, 2 and 3 at
# 1/2, and at 10 inputs 3 and 4 at 1/2; equal rationals are equal doubles,
# and distinct ones keep their order, so the step's decisions are the
# interpreter's. 'shied': a term NaN where the interpreter refuses it, no
# clause of r applying from step 2, and the maximum and the argmin NaN with
# it. Each is to report:
#
#     pooled: 100 steps from 0, against exact values
#     pooled.a: within 0
#     pooled.l: within 0
#     pooled.m: within 0
#     pooled.p: within 0
#
#     shied: 100 steps from 0, against exact values
#     shied.q: within 0
#     shied.w: within 0
pool(x_n[j<=4]) = {
    m_n = max_(k=1)^4 x_n[k]
    a_n = argmax_(k=1)^4 x_n[k]
    l_n = argmin_(k=1)^4 x_n[k]
    p_n[i<=2] = max_(k=1)^2 x_n[2*i - 2 + k]
}
pooled = pool(x_n = [n/10; 1 - n/10; 1/2; 1 - n/20])

r(v) | v > 0 = v
shy(x_n[j<=2]) = {
    q_n = max_(k=1)^2 r(x_n[k])
    w_n = argmin_(k=1)^2 r(x_n[k])
}
shied = shy(x_n = [2 - n; 1])

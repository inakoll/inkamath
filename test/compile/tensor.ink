# Tensors of rank 3 compiled (DESIGN.md, next in line). A tensor is a stack of
# matrices of one size along its first index, B×T×D (test/data/tensor.ink),
# and the step keeps one as C keeps 'double O[B][T][D]': slice after slice,
# row by row, as the interpreter stores its cells. Whatever meets a tensor
# meets it slice by slice, as in the interpreter. Every number below is worked
# out apart from the interpreter and the compiler: by hand, with exact
# fractions in Python, and with NumPy and mpmath at 40 digits; none is
# recorded. Today 'inkamath --compile tensor.ink' refuses every tensor and
# every definition that reads one: 'cannot compile heads.X: a tensor'.
#
# 'heads', multi-head attention over a batch (Vaswani et al., 2017, section
# 3.2), test/data/tensor.ink's conformance model as a model: two sequences of
# three tokens of width 4 at each step, a tensor input, two heads of width 2,
# each head's weights a slice, softmax a function of three indices whose size
# is read from its argument. The input is tensor.ink's X times n/50, so O_50
# is tensor.ink's O, NumPy's to the digits shown there; the scores stay within
# 28 of 0, far from exp's overflow. NumPy's doubles are within 6.7e-15 of
# mpmath's exact terms over the hundred steps, at O_93[1,3,3], about 16.05;
# the step's are held to the interpreter's within less than 1e-13:
#
#     heads: 100 steps from 0, against exact values until 0 and inexact ones from there
#     heads.O: within <e>; the interpreter's terms about <e> from the exact ones
#
# O_99[1,1,1] is -3.94503113117967 by mpmath. Its header, 'inkamath --compile
# tensor.ink mha -o mha.h', holds:
#
#      *     mha_step(&m, X);  once for each index, the first 0
#      *     m.O[0][b][i][j]  is then O_n, slice b+1, row i+1 and column j+1
#      *
#      * A step takes X_n (2x3x4), the input at its index. An input of more than one
#      * cell is a pointer to its cells, row by row, slice after slice. After a step,
#      * m.name[k] is name_(n-k) for each sequence: X and O. The parameters are
#      * fields holding the model's defaults once mha_init has run: d = 2.0. After
#      * assigning one, call mha_update.
#      */
#     ...
#     typedef struct mha {
#         double d;
#         long long index_;
#         double X[1][2][3][4];
#         double O[1][2][3][4];
#     } mha;
#     ...
#     static inline void mha_step(mha* m_, const double X[24]) {
#         ++m_->index_;
#         memcpy(m_->X[0], X, sizeof m_->X[0]);
mha(d = 2, X_n[b<=2, t<=3, c<=4]) = {
    WQ = [-1 -1; 1 0; 2 -1; 2 -1;; 0 2; -1 1; 1 0; -1 1]
    WK = [0 2; 1 0; -1 1; 0 1;; 2 0; 2 1; 1 2; 2 2]
    WV = [2 -1; 0 2; 0 -1; 1 2;; 1 2; 1 -1; 2 0; 2 2]
    WO = [2 -1 2 1; 0 2 2 1;; -1 1 -1 -1; -1 -1 2 0]
    sm(z)[b,t,s] = exp(z[b,t,s])/sum_(r=1)^3 exp(z[b,t,r])
    head(h, x) = sm((x*WQ[h])*(x*WK[h])'/d^(1/2))*(x*WV[h])
    O_n = sum_(h=1)^2 head(h, X_n)*WO[h]
}
heads = mha(X_n = [1 0 1 0; 0 1 0 1; 1 1 0 0;; 0 0 1 1; 1 0 0 1; 0 1 1 0]*n/50)

# 'whirl', a batch of two states turned a quarter at each step, R^2 = -I, and
# pushed by an input of period 2, so s has period 4: s_0 = [1 0;; 0 1], then
# [1 0;; -1 2], [1 2;; -2 1], [-1 0;; -1 0] and s_0 again. Each other
# sequence meets the tensor another way: d and e a tensor its slices, e a
# single value every cell, k a matrix every slice on either side and the
# quote each slice; p is slice by slice, a 2x1x1 tensor, [1;; 1] at 0 and
# [1;; 5] at 1; g reads slices, c a cell, l is a literal of c's terms,
# [c_n 1;; 1 c_n], q is defined by three indices whose sizes are read from s;
# h samples s every second step and u is NaN where no clause applies. Its
# parameter s0 and its input's history are tensors. Every term is a whole
# number or a half, and none passes 5, so each is exact in a double and in a
# float:
#
#     whirl: 100 steps from 0, against exact values
#     whirl.<name>: within 0, for each of d, s, c, g, k, l, p and q
#     whirl.<name>: within 0, from 1, for each of e, h and u
#
# and 'inkamath --check tensor.ink whirl --float':
#
#     whirl: 100 steps from 0 in float, against exact values
#     whirl.<name>: within 0, 0 units of a float, for each of d, s, c, g, k, l, p and q
#     whirl.<name>: within 0, 0 units of a float, from 1, for each of e, h and u
#
# Its header, 'inkamath --compile tensor.ink ring -o ring.h', writes NaN, so a
# tensor term with a NaN cell is NaN in every cell, the loop gaining the
# slices':
#
#     /* Using it:
#      *
#      *     ring m;
#      *     ring_init(&m);
#      *     ring_step(&m, X);  once for each index, the first 0
#      *     m.u[0][b][i][j]  is then u_n, slice b+1, row i+1 and column j+1
#      *
#      * A step takes X_n (2x1x2), the input at its index. An input of more than one
#      * cell is a pointer to its cells, row by row, slice after slice. After a step,
#      * m.name[k] is name_(n-k) for each sequence: X (k <= 1), d, s (k <= 1), c, e,
#      * g, k, l, p, q and u. At another rate, m.name[k] is name_(m-k), m its latest
#      * term's index: h, computed at the steps 2*m + 1. The parameters are fields
#      * holding the model's defaults once ring_init has run: s0 (2x1x2). After
#      * assigning one, call ring_update. A term the interpreter would refuse is NaN,
#      * and so is every term that reads one, through a guard or a comparison as
#      * through arithmetic. Built with -ffinite-math-only, which -ffast-math
#      * implies, GCC removes the tests that make it so, and Clang warns of each NaN.
#      */
#     ...
#     typedef struct ring {
#         double s0[2][1][2];
#         long long index_;
#         double X[2][2][1][2];
#         double d[1][2][1][2];
#         double s[2][2][1][2];
#         double c[1];
#         double e[1][2][1][2];
#         double g[1][1][2];
#         double h[1][2][1][2];
#         double k[1][2][2][1];
#         double l[1][2][1][2];
#         double p[1][2][1][1];
#         double q[1][2][1][2];
#         double u[1][2][1][2];
#     } ring;
#     ...
#     static inline void ring_init(ring* m_) {
#         memset(m_, 0, sizeof *m_);
#         m_->s0[0][0][0] = 1.0;
#         m_->s0[0][0][1] = 0.0;
#         m_->s0[1][0][0] = 0.0;
#         m_->s0[1][0][1] = 1.0;
#         m_->X[0][0][0][0] = 1.0;
#         m_->X[0][0][0][1] = 1.0;
#         m_->X[0][1][0][1] = 1.0;
#         m_->index_ = -1;
#         ring_update(m_);
#     }
#     ...
#     static inline void ring_step(ring* m_, const double X[4]) {
#         ++m_->index_;
#         memcpy(m_->X[1], m_->X[0], sizeof m_->X[1]);
#         memcpy(m_->s[1], m_->s[0], sizeof m_->s[1]);
#         memcpy(m_->X[0], X, sizeof m_->X[0]);
#     ...
#         m_->s[0][0][0][0] = m_->index_ == 0 ? m_->s0[0][0][0] : m_->s[1][0][0][0] * 0.0 + m_->s[1][0][0][1] * -1.0 + m_->X[0][0][0][0];
#         m_->s[0][0][0][1] = m_->index_ == 0 ? m_->s0[0][0][1] : m_->s[1][0][0][0] * 1.0 + m_->s[1][0][0][1] * 0.0 + m_->X[0][0][0][1];
#         m_->s[0][1][0][0] = m_->index_ == 0 ? m_->s0[1][0][0] : m_->s[1][1][0][0] * 0.0 + m_->s[1][1][0][1] * -1.0 + m_->X[0][1][0][0];
#         m_->s[0][1][0][1] = m_->index_ == 0 ? m_->s0[1][0][1] : m_->s[1][1][0][0] * 1.0 + m_->s[1][1][0][1] * 0.0 + m_->X[0][1][0][1];
#         if (isnan(m_->s[0][0][0][0]) || isnan(m_->s[0][0][0][1]) || isnan(m_->s[0][1][0][0]) || isnan(m_->s[0][1][0][1]))
#             for (int b_ = 0; b_ < 2; ++b_)
#                 for (int i_ = 0; i_ < 1; ++i_)
#                     for (int j_ = 0; j_ < 2; ++j_) m_->s[0][b_][i_][j_] = NAN;
#
# In float, 'inkamath --compile tensor.ink ring --float -o ring.h':
#
#     typedef struct ring {
#         float s0[2][1][2];
#         long long index_;
#         float X[2][2][1][2];
#     ...
#     static inline void ring_step(ring* m_, const float X[4]) {
#     ...
#         m_->s[0][1][0][0] = m_->index_ == 0 ? m_->s0[1][0][0] : m_->s[1][1][0][0] * 0.0f + m_->s[1][1][0][1] * -1.0f + m_->X[0][1][0][0];
ring(s0 = [1 0;; 0 1], X_n[b<=2, t<=1, c<=2]) = {
    X_n | n < 0 = [1 1;; 0 1]
    R = [0 1; -1 0]
    s_0 = s0
    s_n = s_(n-1)*R + X_n
    d_n = X_n - X_(n-1)
    e_n = (s_n - s_(n-1))/2
    p_n = s_n*s_n'
    g_n = s_n[2] - s_n[1]
    c_n = s_n[2,1,2]
    l_n = [c_n 1;; 1 c_n]
    q_n[b,t,c] = b*s_n[b,t,c]
    k_n = [0 2; 2 0]*s_n' + [1; 2]
    h_m = s_(2*m + 1)
    u_n | n > 1 = s_(n-1)
}
whirl = ring(X_n = [1 (-1)^n;; 0 2])

# A cell of a tensor that parts from the interpreter's is named by its slice,
# row and column, as a matrix's is by its row and column: 'spread' is
# test/compile/drift.ink's 'wild' in its last cell, a tenth, which a double is
# not, times ten at every step, and 'inkamath --check tensor.ink spread' says
#
#     spread: 100 steps from 0, against exact values
#     spread.d[2,1,2]: <x> at 9, where the interpreter gives 0.10000000000000001
#
# its program failing, as wild's does.
tenfold(c = 1) = {
    d_0 = [1 1;; 1 c]
    d_n = 10*d_(n-1) - 9*[1 1;; 1 c]
}
spread = tenfold(c = 1/10)

# 'batch', a linear layer trained by minibatch gradient descent, each step's
# minibatch two slices of two samples, the loss the mean square over the four
# and its gradient grad's, through slice-wise products and a slice read of a
# tensor that carries its part. Each minibatch's columns are orthogonal, the
# sum over its slices of X[b]'X[b] being 4I, so with eta = 1/2 every step
# lands on that minibatch's least squares exactly: w_n = (1/4) sum over b of
# X_n[b]'Y_n[b], [(2n + 3)/4; ((-1)^n (n - 1) + n - 2)/4] -- w_1 = [5/4; -1/4],
# w_2 = [7/4; 1/4], w_98 = [199/4; 193/4], w_99 = [201/4; -1/4]. Every value
# a forward pass computes, the loss's own included, is a multiple of 1/16
# below 2^15, exact in a double and in a float:
#
#     batch: 100 steps from 0, against exact values
#     batch.w: within 0
#
# and 'inkamath --check tensor.ink batch --float':
#
#     batch: 100 steps from 0 in float, against exact values
#     batch.w: within 0, 0 units of a float
#
# Its header, 'inkamath --compile tensor.ink sgd -o sgd.h', holds:
#
#     typedef struct sgd {
#         double eta;
#         long long index_;
#         double X[1][2][2][2];
#         double Y[1][2][2][1];
#         double w[2][2][1];
#     } sgd;
#     ...
#     static inline void sgd_step(sgd* m_, const double X[8], const double Y[4]) {
sgd(eta = 1/2, X_n[b<=2, t<=2, c<=2], Y_n[b<=2, t<=2, k<=1]) = {
    loss(v, X, Y) = sum_(b=1)^2 (X*v - Y)[b]'*(X*v - Y)[b]/4
    w_0 = [0; 0]
    w_n = w_(n-1) - eta*grad_(v = w_(n-1)) loss(v, X_n, Y_n)
}
batch = sgd(X_n = [1 (-1)^n; 1 -(-1)^n;; 1 1; 1 -1], Y_n = [n; 1;; n; 2])

# What 'inkamath --compile' refuses of a file of its own, in the interpreter's
# words where it has them: two indices of a tensor, a tensor's power, tensors
# of different numbers of slices, a literal whose slices differ; in the
# compiler's words for a matrix, which a tensor is a stack of, a comparison;
# and for now, a tensor in a limit, a derivative with respect to a tensor and
# a tensor's cells under a guard that is not a constant, as one that reads the
# index, whose clause --check could not follow cell by cell.
# T and U are the session's constants, so fields, and y and P, which
# test/cli.cmake's compile_tensor_refused refused, compile:
#
#     T = [1 2; 3 4;; 5 6; 7 8]
#     U = [1 2; 3 4;; 5 6; 7 8;; 9 10; 11 12]
#     a_n = (n*T)[2,1]
#     b_n = (n*T)^2
#     c_n | n*T > 1 = 1
#     c_n = 0
#     d_n = n*T + U
#     f_n = [n 1;; 2 3 4]
#     g_n = lim p(n*T)
#     h_n = grad_(V = n*T) sum_(b=1)^2 [1 1]*V[b]*[1; 1]
#     m_n[b<=2, i<=1, j<=1] | n > 2 = b
#     m_n[b<=2, i<=1, j<=1] = 0
#     p(A)_0 = A
#     p(A)_k = p(A)_(k-1)/2
#     y_0 = 0
#     y_n = y_(n-1) + T[2,1,2]
#     P[b<=2, j<=2, k<=2] = b
#
#     cannot compile a: a 2x2x2 tensor takes one index or three, not two
#     cannot compile b: only a matrix has a power, not a 2x2x2 tensor
#     cannot compile c: a comparison of matrices
#     cannot compile d: a 2x2x2 tensor and a 3x2x2 tensor have different numbers of slices
#     cannot compile f: the slices of a tensor have one size, not 1x2 and 1x3
#     cannot compile g: a tensor in a limit, for now
#     cannot compile h: a derivative with respect to a tensor, for now
#     cannot compile m: a tensor's cells under a guard that is not a constant, for now
#
# Two recorded refusals move with it: compile_tensor_refused, above, and
# inputs_batch, test/compile/inputs.ink's 'batch', which compiles, its step
# reading the second slice of its input:
#
#     m_->y[0] = m_->x[0][1][0][0] * 1.0 + m_->x[0][1][0][1] * 1.0;
#
# Every header in test/compile/expected stays byte for byte as it is, and
# every program --check writes for an instance that meets no tensor.

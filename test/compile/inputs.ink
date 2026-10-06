# A model's input of more than one cell (DESIGN.md, next in line; C83). The
# model states the input's size in its signature, x_n[j<=2], and the step
# takes the input as a pointer to its cells, row by row, where it took one
# double. 'fan' is C83's own case; 'grid' reads a matrix at each step; 'chan'
# reads cells of a column, applies a function of cells to it and defines a
# term by cells whose size is inferred from it; 'twirl' has a history of the
# input's size; 'duo' is history.ink's 'lit' on two channels, and samples
# and holds the input itself; 'mean3' reads a size from a parameter, and
# 'mean2' from the instance's, which the check compiles in; 'track' takes a
# single value and a column, and 'twain' two columns of different sizes;
# 'nestv' gives an instance within a model a column built from a single
# value. Each reports so, every term exact:
#
#     fan: 100 steps from 0, against exact values
#     fan.y: within 0
#
#     grid: 100 steps from 0, against exact values
#     grid.<name>: within 0, for each of d and t
#
#     chan: 100 steps from 0, against exact values
#     chan.<name>: within 0, for each of g, h and s
#
#     twirl: 100 steps from 0, against exact values
#     twirl.c: within 0
#
#     duo: 100 steps from 0, against exact values
#     duo.y: within 0, from 1
#     duo.<name>: within 0, for each of f, z, p and q
#
#     mean3: 100 steps from 0, against exact values
#     mean3.y: within 0, its thirds rounded but their sums exact to n = 99
#
#     mean2: 100 steps from 0, against exact values
#     mean2.y: within 0
#
#     track: 100 steps from 0, against exact values
#     track.u: within 0
#
#     twain: 100 steps from 0, against exact values
#     twain.y: within 0
#
#     nestv: 100 steps from 0, against exact values
#     nestv.<name>: within 0, for each of y, inner.y and inner.x
#
# The header of 'dot', compiled by name, holds these lines as they are here,
# with what the step computes in place of the '...':
#
#     /* Using it:
#      *
#      *     dot m;
#      *     dot_init(&m);
#      *     dot_step(&m, x);  once for each index, the first 0
#      *     m.y[0]  is then y_n
#      *
#      * A step takes x_n (2x1), the input at its index. An input of more than one
#      * cell is a pointer to its cells, row by row. After a step, m.name[k] is
#      * name_(n-k) for each sequence: x and y.
#      */
#     ...
#     typedef struct dot {
#         long long index_;
#         double x[1][2][1];
#         double y[1];
#     } dot;
#     ...
#     static inline void dot_step(dot* m_, const double x[2]) {
#         ++m_->index_;
#         memcpy(m_->x[0], x, sizeof m_->x[0]);
#     ...
#
# 'ctl''s takes its inputs in the order of the signature, a single value as
# before:
#
#      * A step takes r_n and s_n (2x1), the inputs at its index. An input of more
#      * than one cell is a pointer to its cells, row by row. After a step, m.name[k]
#      * is name_(n-k) for each sequence: r, s and u.
#     ...
#     static inline void ctl_step(ctl* m_, double r, const double s[2]) {
#         ++m_->index_;
#         m_->r[0] = r;
#         memcpy(m_->s[0], s, sizeof m_->s[0]);
#
# 'turn''s init writes each cell of its history that is not 0 into the
# window, as a parameter's matrix is written, and its step moves the window
# before copying the input in:
#
#     static inline void turn_init(turn* m_) {
#         memset(m_, 0, sizeof *m_);
#         m_->x[0][0][0] = 1.0;
#         m_->x[0][1][0] = -1.0;
#         m_->index_ = -1;
#         turn_update(m_);
#     }
#     ...
#     static inline void turn_step(turn* m_, const double x[2]) {
#         ++m_->index_;
#         memcpy(m_->x[1], m_->x[0], sizeof m_->x[1]);
#         memcpy(m_->x[0], x, sizeof m_->x[0]);
#
# And 'avg''s first comment says, as of any size read from a parameter:
#
#      * ... Compiled in as constants, these cannot change: d.
#
# Every header in test/compile/expected stays byte for byte as it is, and
# every program --check writes for an instance whose inputs are single
# values.
#
# What 'inkamath --compile' refuses, each model a file of its own, 'lone'
# with 'dot', compiled by name with -o. A history of another size than the
# input, stated or not (test/cli.cmake's 'swap', which said "a history that
# is not a single value"); a default and an instance within a model each
# giving another size than its model states; a tensor, refused here once,
# compiles since (test/compile/tensor.ink). A size that reads the index is
# the interpreter's refusal, where the file is read:
#
#     broad(x_n[j<=2]) = {
#         x_n | n < 0 = 0
#         c_n = x_(n-1)
#     }
#     swap(x_n) = {
#         x_n | n < 0 = [0; 0]
#         c_n = [0 1; 1 0]*x_(n-1)
#     }
#     nil(x_n[j<=2] = 0) = {
#         y_n = [1 2]*x_n
#     }
#     dot(x_n[j<=2]) = {
#         y_n = [1 2]*x_n
#     }
#     lone(u_n) = {
#         inner = dot(x_n = u_n)
#         y_n = inner.y_n
#     }
#     grow(x_n[j<=n+1]) = {
#         y_n = x_n[1]
#     }
#
#     inkamath: cannot compile x: a history of another shape
#     inkamath: cannot compile x: a history of another shape
#     inkamath: cannot compile x: a single value, where nil takes a 2x1 matrix
#     inkamath: cannot compile inner.x: a single value, where dot takes a 2x1 matrix
#     inkamath: grow.ink, line 1: x is an input of grow, so its size cannot read the index n
#
# And what 'inkamath --check' refuses, of 'v = dot(x_n = n)' beside 'dot'
# above, in the interpreter's words, where it reads the input; where the
# model states no size, test/cli.cmake's 'check_matrix_input' stands as it is:
#
#     inkamath: v.x_(0): v.x_0 is a single value, where dot takes a 2x1 matrix
dot(x_n[j<=2]) = {
    y_n = [1 2]*x_n
}
fan = dot(x_n = [n; 1])

trace(x_n[j<=2, k<=2]) = {
    t_n = x_n[1,1] + x_n[2,2]
    d_n = x_n*[1; -1]
}
grid = trace(x_n = [n, 1; 2, n^2])

relu(x_n[j<=3]) = {
    rl(z)[i,j] = z[i,j]*(z[i,j] > 0)
    h_n = rl(x_n)
    g_n[j] = x_n[j]*(x_n[j] > 0)
    s_n = x_n[1] - x_n[3]
}
chan = relu(x_n = [n - 2; 3 - n; (-1)^n])

turn(x_n[j<=2]) = {
    x_n | n < 0 = [1; -1]
    c_n = [0 1; 1 0]*x_(n-1)
}
twirl = turn(x_n = [n; n^2])

stereo(x_n[j<=2]) = {
    x_n | n < 0 = [0; 0]
    f_n = (x_n + x_(n-1))/2
    y_m = f_(2*m + 1)
    z_n = y_(floor((n - 1)/2))
    p_m = x_(2*m)
    q_n = p_(floor(n/2))
}
duo = stereo(x_n = [n^2; 2*n])

avg(d = 3, x_n[j<=d]) = {
    y_n = sum_(j=1)^d x_n[j]/d
}
mean3 = avg(x_n = [n; 2*n; 3*n])
mean2 = avg(d = 2, x_n = [n; 3*n])

ctl(r_n, s_n[j<=2]) = {
    K = [3/2 1/2]
    u_n = r_n - K*s_n
}
track = ctl(r_n = 1, s_n = [n; 1])

two(a_n[j<=2], b_n[j<=3]) = {
    y_n = [1 1]*a_n + [1 1 1]*b_n
}
twain = two(a_n = [n; 1], b_n = [1; n; n^2])

outer(u_n) = {
    inner = dot(x_n = [u_n; 1])
    y_n = inner.y_n
}
nestv = outer(u_n = n)

# An input the instance gives is fed to the step as the interpreter reads
# it, not compiled from the model's default for it: 'fed' gives 1 where
# 'doubled' would give n. It was stepped on the default (DESIGN.md, C94):
#
#     fed.x: 0 at 0, where the interpreter gives 1
doubled(x_n = n) = {
    y_n = 2*x_n
}
fed = doubled(x_n = 1)

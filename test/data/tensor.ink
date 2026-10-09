# Tensors of rank 3 (DESIGN.md, next in line; MANIFESTO.md, Tensors), the
# least the language needs to write attention per batch and per head. A
# tensor is a stack of matrices of one size, its slices, along its first
# index, the batch's place on paper. Whatever meets a tensor meets it slice by
# slice, so that (T op M)[b] = T[b] op M and (M op T)[b] = M op T[b] for +, -,
# * and /, and T'[b] = T[b]'.
#
# The literal stacks matrices with ';;', one semicolon more than separates
# rows, and prints so: what is printed is typed back.
>> T = [1 2; 3 4;; 5 6; 7 8]
T = [1 2; 3 4;; 5 6; 7 8]

>> T
[1, 2;
 3, 4;;
 5, 6;
 7, 8]

>> ?T
T = [1 2; 3 4;; 5 6; 7 8]

>> [1 2;; 3 4]
[1, 2;;
 3, 4]

>> [1, 2;
..  3, 4;;
..  5, 6;
..  7, 8] == T
1

# Each slice is a matrix literal, blocks included.
>> I2 = [1 0; 0 1]
I2 = [1 0; 0 1]

>> [I2;; 2*I2]
[1, 0;
 0, 1;;
 2, 0;
 0, 2]

# A tensor of one slice is not its slice, as a batch of one is still a batch:
# it prints with its separator, and is not equal to the matrix.
>> [1 2; 3 4;;]
[1, 2;
 3, 4;;]

>> [1 2; 3 4;;] == [1 2; 3 4]
0

>> [7;;]
[7;;]

>> [7;;] + 1
[8;;]

# Nor is a tensor of one cell a single value, where one is needed.
>> two = [2;;]
two = [2;;]

>> T[two]
error: a tensor is not a single value

>> nz(x) | x = 1
nz(x) | x = 1

>> nz([7;;])
error: nz needs single values, not a 1x1x1 tensor; write it by its cells

# Slices of two sizes are refused, not padded: a short row's zeros are one
# fill rule, and C41 records why there is to be no fourth. A tensor is not a
# block, and there is no fourth index to stack along.
>> [1 2; 3 4;; 5 6]
error: the slices of a tensor have one size, not 2x2 and 1x2

>> [T, T]
error: a tensor cannot be a block of a literal, only a matrix can

>> [T;; T]
error: a tensor cannot be a block of a literal, only a matrix can

>> [1;;;2]
error: a tensor has at most three indices, and ';;;' would give it a fourth

>> [;; 1 2]
error: a matrix needs at least one element

# Arguments are not slices.
>> h(a;; b) = a
error: arguments are separated by ',', not ';;'

>> nz(1;; 2)
error: arguments are separated by ',', not ';;'

# Only a touching ';;' stacks. With a space, the empty row is a row of zeros,
# as it was.
>> [1 2; ; 3 4]
[1, 2;
 0, 0;
 3, 4]

# One index is a slice, as on a matrix it is a row; three are a cell. Two
# name neither, and are refused; a row of a slice is read by chaining.
>> T[2]
[5, 6;
 7, 8]

>> T[2,1,2]
6

>> T[2][1]
[5, 6]

>> T[2][2,1]
7

>> [1 2;; 3 4][2]
[3, 4]

>> T[2,1]
error: a 2x2x2 tensor takes one index or three, not two

>> T[3]
error: slice 3 is outside a 2x2x2 tensor

>> T[1,3,1]
error: slice 1, row 3, column 1 is outside a 2x2x2 tensor

>> a = [1 2; 3 4]
a = [1 2; 3 4]

>> a[1,2,1]
error: a 2x2 matrix takes one index or two, not three

# A definition by three indices is a tensor, with guards, a clause for one
# cell, and sizes from parameters, as a matrix by two.
>> P[b<=2, j<=2, k<=3] = 100*b + 10*j + k
P[b<=2, j<=2, k<=3] = 100*b + 10*j + k

>> P
[111, 112, 113;
 121, 122, 123;;
 211, 212, 213;
 221, 222, 223]

>> P[b<=2, j<=2, k<=3] | b == 2 and j == k = 0
P[b<=2, j<=2, k<=3] | b == 2 and j == k = 0

>> P[1,2,3] = 7
P[1,2,3] = 7

>> P
[111, 112, 113;
 121, 122,   7;;
   0, 212, 213;
 221,   0, 223]

>> P[1] = 0
error: P is defined by a slice, a row and a column, so a clause names all three

# A clause for one cell of a value written whole names it as reading it
# would: a tensor's cell by three indices, a matrix's by no more than two.
>> R = [1 2; 3 4;; 5 6; 7 8]
R = [1 2; 3 4;; 5 6; 7 8]

>> R[1,2] = 9
R[1,2] = 9

>> R
error: a clause for one cell of R, a 2x2x2 tensor, names its slice, row and column

>> N = [1 2; 3 4]
N = [1 2; 3 4]

>> N[1,1,2] = 9
N[1,1,2] = 9

>> N
error: a clause for one cell of N, a 2x2 matrix, names no slice

>> w_n = [1 2;; 3 4]
w_n = [1 2;; 3 4]

>> w_2[2] = 9
w_2[2] = 9

>> w_2
error: a clause for one cell of w_2, a 2x1x2 tensor, names its slice, row and column

>> H[1,1,1] = 5
H[1,1,1] = 5

>> H
error: H has no size; write it as H[b<=slices, j<=rows, k<=cols]

>> Z(n)[b<=n, j<=1, k<=n] = b*k
Z(n)[b<=n, j<=1, k<=n] = b*k

>> Z(2)
[1, 2;;
 2, 4]

>> F[a<=2, b<=2, c<=2, d<=2] = 1
error: a tensor has at most three indices, and F names four

# A single value meets every cell; a matrix meets every slice; two tensors
# meet slice for slice, and need as many slices.
>> T + T
[ 2,  4;
  6,  8;;
 10, 12;
 14, 16]

>> T/2
[0.5, 1;
 1.5, 2;;
 2.5, 3;
 3.5, 4]

>> 10 - T
[9, 8;
 7, 6;;
 5, 4;
 3, 2]

>> T + I2
[2, 2;
 3, 5;;
 6, 6;
 7, 9]

>> T + [1 2]
error: these matrices have different sizes

>> U = [1 2; 3 4;; 5 6; 7 8;; 9 10; 11 12]
U = [1 2; 3 4;; 5 6; 7 8;; 9 10; 11 12]

>> T + U
error: a 2x2x2 tensor and a 3x2x2 tensor have different numbers of slices

>> T + [7;;]
error: a 2x2x2 tensor and a 1x1x1 tensor have different numbers of slices

>> T == U
0

>> I2 - T
[ 0, -2;
 -3, -3;;
 -4, -6;
 -7, -7]

>> -[1 2;; 3 4]
[-1, -2;;
 -3, -4]

# So '*' is a product batched over the first index: a slice's product is the
# product of slices, and a matrix is shared by every slice, as a projection
# is shared by a batch.
>> T*[1; 1]
[ 3;
  7;;
 11;
 15]

>> [1 1]*T
[ 4,  6;;
 12, 14]

# A slice of one cell is a single value, so it scales the matrix it meets.
>> [1;; 2]*[1 2]
[1, 2;;
 2, 4]

>> T*T
[ 7,  10;
 15,  22;;
 67,  78;
 91, 106]

>> (T*T)[2] == T[2]*T[2]
1

>> 2*T == T + T
1

>> T*[1 2 3]
error: a matrix product needs as many columns on the left as rows on the right

>> T*U
error: a 2x2x2 tensor and a 3x2x2 tensor have different numbers of slices

# The quote transposes each slice; the batch stays first.
>> T'
[1, 3;
 2, 4;;
 5, 7;
 6, 8]

>> T'[2] == T[2]'
1

# Whole tensors are equal or not. What has no batched meaning is refused.
>> T == T
1

>> T < 1
error: a comparison needs single values, not a 2x2x2 tensor

>> T^2
error: only a matrix has a power, not a 2x2x2 tensor

# Cell by cell, as on a matrix, and printed as a matrix's cells are.
>> T/3
[~0.333333333, ~0.666666667;
            1,  ~1.33333333;;
  ~1.66666667,            2;
  ~2.33333333,  ~2.66666667]

>> frac T/3
[1/3, 2/3;
   1, 4/3;;
 5/3,   2;
 7/3, 8/3]

>> [1 2;; 3 4]*i
[  ~(i), ~(i*2);;
 ~(i*3), ~(i*4)]

>> floor(T/3)
[0, 0;
 1, 1;;
 1, 2;
 2, 2]

>> aff(x) = 2*x + 1
aff(x) = 2*x + 1

>> aff(T)
[ 3,  5;
  7,  9;;
 11, 13;
 15, 17]

# A contraction is a sum over an index, as on paper: over the batch, over a
# diagonal, or the batched product written by its cells.
>> sum_(b=1)^2 T[b]
[ 6,  8;
 10, 12]

>> tr[b<=2] = sum_(j=1)^2 T[b,j,j]
tr[b<=2] = sum_(j=1)^2 T[b,j,j]

>> tr
[ 5;
 13]

>> C[b<=2, j<=2, k<=2] = sum_(m=1)^2 T[b,j,m]*T[b,m,k]
C[b<=2, j<=2, k<=2] = sum_(m=1)^2 T[b,j,m]*T[b,m,k]

>> C == T*T
1

# A sequence of tensors, whole or by its cells.
>> y_0 = [1 2;; 3 4]
y_0 = [1 2;; 3 4]

>> y_n = y_(n-1)*[0 1; 1 0]
y_n = y_(n-1)*[0 1; 1 0]

>> y_3
[2, 1;;
 4, 3]

>> c_n[b<=2, j<=1, k<=2] = n*b + k
c_n[b<=2, j<=1, k<=2] = n*b + k

>> c_2
[3, 4;;
 5, 6]

# A limit is taken cell by cell, its step the largest cell's. Terms that
# change shape have none, and the rank is part of the shape.
>> p_n = T - T/2^n
p_n = T - T/2^n

>> lim p
[~1, ~2;
 ~3, ~4;;
 ~5, ~6;
 ~7, ~8]

>> q_n | n < 3 = [1 2; 3 4;;]
q_n | n < 3 = [1 2; 3 4;;]

>> q_n = [1 2; 3 4]
q_n = [1 2; 3 4]

>> lim q
error: q has no limit: its terms are 1x2x2, then 2x2

# A single value's gradient with respect to a tensor is shaped as the tensor,
# through slices and batched products; a tensor's with respect to a single
# value is shaped as itself. Anything else is a Jacobian.
>> grad_(W = T) sum_(b=1)^2 sum_(j=1)^2 sum_(k=1)^2 W[b,j,k]^2
[ 2,  4;
  6,  8;;
 10, 12;
 14, 16]

>> grad_(W = T) sum_(b=1)^2 [1 1]*W[b]*[1; 1]
[1, 1;
 1, 1;;
 1, 1;
 1, 1]

>> grad_(x = 3) x^2*T
[ 6, 12;
 18, 24;;
 30, 36;
 42, 48]

# A quotient by a matrix meets each slice, its derivative too.
>> grad_(x = 1) T/[x 1; 1 x]
[-1,  0;
  0, -4;;
 -5,  0;
  0, -8]

>> grad_(W = T) 2*W
error: grad of a tensor with respect to a tensor is a Jacobian, which it does not give

# tex sets a definition by three indices as it sets one by two. A literal
# with ';;' has no form on paper.
>> G[b<=2, j<=2, k<=2] = b + j*k
G[b<=2, j<=2, k<=2] = b + j*k

>> tex ?G
G_{b,j,k} = b + j\,k, \quad 1 \le b \le 2,\ 1 \le j \le 2,\ 1 \le k \le 2

>> tex ?T
error: tex cannot show ';;', which has no form on paper

# The conformance model: multi-head attention over a batch (Vaswani et al.,
# 2017, section 3.2), two sequences of three tokens of width 4, two heads of
# width 2. A head is a parameter, head_i as the paper writes it, so nothing
# needs a fourth index; each head's weights are a slice. Concat(head_1,
# head_2) W^O is written as the sum of each head times its block of rows of
# W^O, which is the same product, since a tensor is not a block. Softmax is
# defined by its cells. The output is NumPy's, to the digits shown.
>> X = [1 0 1 0; 0 1 0 1; 1 1 0 0;; 0 0 1 1; 1 0 0 1; 0 1 1 0]
X = [1 0 1 0; 0 1 0 1; 1 1 0 0;; 0 0 1 1; 1 0 0 1; 0 1 1 0]

>> WQ = [-1 -1; 1 0; 2 -1; 2 -1;; 0 2; -1 1; 1 0; -1 1]
WQ = [-1 -1; 1 0; 2 -1; 2 -1;; 0 2; -1 1; 1 0; -1 1]

>> WK = [0 2; 1 0; -1 1; 0 1;; 2 0; 2 1; 1 2; 2 2]
WK = [0 2; 1 0; -1 1; 0 1;; 2 0; 2 1; 1 2; 2 2]

>> WV = [2 -1; 0 2; 0 -1; 1 2;; 1 2; 1 -1; 2 0; 2 2]
WV = [2 -1; 0 2; 0 -1; 1 2;; 1 2; 1 -1; 2 0; 2 2]

>> WO = [2 -1 2 1; 0 2 2 1;; -1 1 -1 -1; -1 -1 2 0]
WO = [2 -1 2 1; 0 2 2 1;; -1 1 -1 -1; -1 -1 2 0]

>> d = 2
d = 2

>> Q(h) = X*WQ[h]
Q(h) = X*WQ[h]

>> K(h) = X*WK[h]
K(h) = X*WK[h]

>> V(h) = X*WV[h]
V(h) = X*WV[h]

>> S(h) = Q(h)*K(h)'/d^(1/2)
S(h) = Q(h)*K(h)'/d^(1/2)

>> A(h)[b<=2, t<=3, s<=3] = exp(S(h)[b,t,s])/sum_(r=1)^3 exp(S(h)[b,t,r])
A(h)[b<=2, t<=3, s<=3] = exp(S(h)[b,t,s])/sum_(r=1)^3 exp(S(h)[b,t,r])

>> head(h) = A(h)*V(h)
head(h) = A(h)*V(h)

>> O = sum_(h=1)^2 head(h)*WO[h]
O = sum_(h=1)^2 head(h)*WO[h]

>> Q(1)
[1, -2;
 3, -1;
 0, -1;;
 4, -2;
 1, -2;
 3, -1]

>> head(2)[1] == A(2)[1]*V(2)[1]
~1

>> O
[~-1.64201703, ~7.34628901, ~8.36586961,  ~1.60632582;
 ~-1.79333934, ~6.14902833, ~8.65878633,  ~1.35809773;
 ~-1.33385894, ~4.98688922, ~6.86182413, ~0.743290194;;
 ~-4.64691442, ~3.96933649, ~1.63730236, ~-2.39740875;
 ~-5.07666278, ~3.94180217, ~2.00373981, ~-2.63526971;
 ~-3.79310867, ~3.54243362, ~2.49110811, ~-1.97050588]

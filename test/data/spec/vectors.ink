# One index is a row (DESIGN.md, next in line). A vector is a column, as on
# paper, so one index reads its element; on a matrix it reads a row, which is
# a 1xn matrix and keeps its orientation, so that a[1]*v is a row times a
# column. A row vector is read through its transpose, r'[2], as a paper
# writes it x transposed. Two indices still name a cell of anything.
>> a = [1 2; 3 4]
a = [1 2; 3 4]

>> a[1]
[1, 2]

>> a[2]
[3, 4]

>> v = [1; 4; 9]
v = [1; 4; 9]

>> v[2]
4

>> a[1]*[1; 1]
3

# A row vector's second row is not there; its transpose is a column.
>> r = [1 2 3]
r = [1 2 3]

>> r[2]
error: row 2 is outside a 1x3 matrix

>> r'[2]
2

>> r[1]
[1, 2, 3]

# A number is a 1x1 matrix, whose one row is itself.
>> x = 5
x = 5

>> x[1]
5

# Out of range, or not whole, as for a cell.
>> a[3]
error: row 3 is outside a 2x2 matrix

>> a[0]
error: row 0 is outside a 2x2 matrix

>> a[1.5]
error: an index must be a whole number, not 1.5

# A row is a matrix, so a second index reads a row of the row: its only one
# is the first, and a[1][2] is not the cell a[1,2], which two indices name.
>> a[1][1]
[1, 2]

>> a[1][2]
error: row 2 is outside a 1x2 matrix

# Brackets index only what they touch. A space before them means what it
# means anywhere else: in a literal it separates blocks, so this is a row of
# two blocks, as '[a, [3 4]]' is, not the row a[3 4] of a; outside one it is
# a product without its operator.
>> [a [3 4]]
[1, 2, 3, 4;
 3, 4, 4, 4]

>> a [1]
error: unexpected '[' -- the operator '*' is probably missing

# After whatever brackets can follow, as two indices can.
>> f(x) = [x; x^2]
f(x) = [x; x^2]

>> f(3)[2]
9

>> s_n = [n; 2*n]
s_n = [n; 2*n]

>> s_3[2]
6

>> (a*a)[2]
[15, 22]

# Defined by one index, a vector is a column, as long as its bound.
>> w[j<=3] = j^2
w[j<=3] = j^2

>> w
[1;
 4;
 9]

>> w[3]
9

>> w[2] = 7
w[2] = 7

>> w
[1;
 7;
 9]

>> g[j<=4] | j > 2 = 1
g[j<=4] | j > 2 = 1

>> g
[0;
 0;
 1;
 1]

>> H(m)[j<=m] = j*m
H(m)[j<=m] = j*m

>> H(3)
[3;
 6;
 9]

# The delay line of README.md section 4, without the column it never needed.
>> tap_0[j<=3] = 0
tap_0[j<=3] = 0

>> tap_n[j<=3] = tap_(n-1)[j-1]
tap_n[j<=3] = tap_(n-1)[j-1]

>> tap_n[j<=3] | j == 1 = n^2
tap_n[j<=3] | j == 1 = n^2

>> tap_4
[16;
  9;
  4]

# A matrix defined by its rows and columns has no clause for a whole row:
# one index on the left would name a row where the definition names cells.
>> M[j<=2, k<=2] = j + k
M[j<=2, k<=2] = j + k

>> M[1] = 0
error: M is defined by a row and a column, so a clause names both

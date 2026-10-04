# A sequence's terms defined by their cells, as a matrix is (README.md,
# section 2; DESIGN.md, next in line). The brackets after the index
# name the row and the column and bound them; every cell sees the index, and
# a guard says which cells a clause gives, as it does for a matrix. A cell no
# clause gives is 0.

# A layer of a network: each unit the positive part of what reaches it.
>> W = [1, -1; 2, 1; -1, 1]
W = [1, -1; 2, 1; -1, 1]

>> a_n = n - 2
a_n = n - 2

>> b_n = 1
b_n = 1

>> z_n = W*[a_n; b_n]
z_n = W*[a_n; b_n]

>> h_n[j<=3, k<=1] = z_n[j,1]
h_n[j<=3, k<=1] = z_n[j,1]

>> h_n[j<=3, k<=1] | z_n[j,1] < 0 = 0
h_n[j<=3, k<=1] | z_n[j,1] < 0 = 0

>> z_0
[-3;
 -3;
  3]

>> h_0
[0;
 0;
 3]

>> h_4
[1;
 5;
 0]

# A delay line: the newest sample on top, the others shifted down a place.
# A base clause by its cells beats the general ones, as any base clause does.
>> x_n = n^2
x_n = n^2

>> w_0[j<=3, k<=1] = 0
w_0[j<=3, k<=1] = 0

>> w_n[j<=3, k<=1] = w_(n-1)[j-1, 1]
w_n[j<=3, k<=1] = w_(n-1)[j-1, 1]

>> w_n[j<=3, k<=1] | j == 1 = x_n
w_n[j<=3, k<=1] | j == 1 = x_n

>> w_1
[1;
 0;
 0]

>> w_4
[16;
  9;
  4]

>> ?w
w_0[j<=3, k<=1] = 0
w_n[j<=3, k<=1] = w_(n-1)[j-1, 1]
w_n[j<=3, k<=1] | j == 1 = x_n

# A cell no clause gives is 0.
>> e_n[j<=2, k<=2] | j == k = n
e_n[j<=2, k<=2] | j == k = n

>> e_3
[3, 0;
 0, 3]

# The size can come from the index, as it can from a parameter.
>> t_n[j<=n, k<=1] = j
t_n[j<=n, k<=1] = j

>> t_3
[1;
 2;
 3]

>> t_0
error: a size must be at least 1, not 0

# Written whole as well, the whole term is the size and every cell no clause
# gives, as it is for a matrix.
>> m_n = [1; 2]
m_n = [1; 2]

>> m_n[j<=2, k<=1] | j == 2 = n
m_n[j<=2, k<=1] | j == 2 = n

>> m_3
[1;
 3]

# One cell, of every term or of one: as 'M[1,2] = 5' is for a matrix, the rest
# of the term comes from the other clauses. A clause wins where it is at least
# as specific in both the index and the cell, so 'g_2[2,1]' beats everything
# and anything beats the general clauses; and it is not a base clause, since
# it does not give a whole term.
>> g_n = [n, 0; 0, n]
g_n = [n, 0; 0, n]

>> g_n[1,2] = 1
g_n[1,2] = 1

>> g_3
[3, 1;
 0, 3]

>> g_2[2,1] = 9
g_2[2,1] = 9

>> g_2
[2, 1;
 9, 2]

# A base term names one index and every cell, a cell of every term every index
# and one cell: neither is the more specific, so which is meant is asked, and
# the clause that names both says it.
>> g_0 = [7, 7; 7, 7]
g_0 = [7, 7; 7, 7]

>> g_0
error: g_0 and g_n[1,2] both give row 1, column 2 of g_0; write g_0[1,2] to say which

>> g_0[1,2] = 1
g_0[1,2] = 1

>> g_0
[7, 1;
 7, 7]

# A guarded clause for a whole term beside clauses for a term's cells was
# never asked (C82), so it is refused where it is written.
>> qw_n[j<=2] = n + j
qw_n[j<=2] = n + j

>> qw_n | n > 1 = [0; 0]
error: qw is defined by its cells, so a clause for all of it cannot be guarded; guard its cells

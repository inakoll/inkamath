# A sequence's terms defined by their cells, as a matrix is (README.md,
# section 2; MODERNIZATION.md, next in line). The brackets after the index
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

# A term is given by its cells together: one cell of a term is not a clause.
>> v_n[1,1] = 5
error: a term's cells are named, as 'v_n[j<=2, k<=2]', not given one at a time

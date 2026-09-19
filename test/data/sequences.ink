# Indexed references and general terms (README.md sections 4.2 and 4.3).

>> f_0=1+2
3

>> f_1=3+4
7

>> f_0
3

>> f_1
7

# A general term applies to every index that has no singular definition.
>> u_n=2*n
0

>> u_1
2

>> u_2
4

>> u_0=2
2

>> u_1=3
3

>> u_0
2

>> u_1
3

>> u_2
4

>> u_3
6

# Series: the interpreter iterates the general term until it converges.
>> exp(x)_n=exp(x)_(n-1)+x^n/!n
nan*nan

>> exp(1)
2.71828183

>> exp(1)-e
-7.69606601e-13

>> cos(x)=(exp(i*x)+exp(-i*x))/2
nan*nan

>> cos(pi/3)
0.5

>> sin(x)=(exp(i*x)-exp(-i*x))/(2*i)
nan*nan

>> sin(pi/6)
0.5


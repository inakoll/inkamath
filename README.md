inkamath
========

Inkamath is an mathematical interpreter written in standard C++.
© 2013 iNaKoll. Please see accompanied license file.

Inkamath supports the definition of functions, matrices and sequences of complex numbers.
The syntax is meant to be simple, powerful and as close as possible to the common math syntax.
Here is an overview of what Inkamath is capable of : 


InkaMath 0.7

>> [pi, e]
3.14159265 2.71828183


>> A=[a a; a a]
0 0
0 0


>> a=[1 2;3 4]
1 2
3 4


>> A
1 2 1 2
3 4 3 4
1 2 1 2
3 4 3 4


>> exp(x)_n=exp(x)_(n-1)+x^n/!n
0


>> exp(1)
2.71828183


>> cos(x)=(exp(i*x)+exp(-i*x))/2
0


>> sin(x)=(exp(i*x)-exp(-i*x))/(2*i)
0


>> sin(pi/6)
0.5

>> 3(4+5)
Error : Syntax error before '('
The operator '*' is probably missing.
0

>> 3*(4+5)
27

Comments and contributions are welcomed.

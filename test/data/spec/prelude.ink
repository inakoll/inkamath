# exp, log and tanh in the prelude beneath the session, as ceil and mod are
# (DESIGN.md, next in line): exp is the built-in power of e, accurate however
# large its argument; log a series where it converges fast, reached by
# halving or doubling; tanh from exp. grad differentiates all three, so an
# activation and a log loss need no definition of their own.
>> exp(1)
~2.71828183

>> exp(0)
1

>> exp(30)
~1.06864746e+13

>> exp(0-1)
~0.367879441

>> log(10)
~2.30258509

>> log(2)
~0.693147181

>> log(1/1000000)
~-13.8155106

>> tanh(1/2)
~0.462117157

# log(0) has no value, and says so as 1/0 does.
>> log(0)
error: division by zero

>> grad_(x = 1) exp(x)
~2.71828183

>> grad_(x = 3) log(x)
~0.333333333

>> grad_(x = 1/2) tanh(x)
~0.786447733

# A logistic regression trained by gradient descent on its log loss: the
# gradient grad gives beside the one written by hand, and the weights NumPy
# gives after ten steps.
>> X = [1, 0; 1, 1; 1, 2; 1, 3]
X = [1, 0; 1, 1; 1, 2; 1, 3]

>> y = [0; 0; 1; 1]
y = [0; 0; 1; 1]

>> sig(z) = 1/(1 + exp(-z))
sig(z) = 1/(1 + exp(-z))

>> loss(w) = 0 - sum_(r=1)^4 (y[r]*log(sig(X[r]*w)) + (1 - y[r])*log(1 - sig(X[r]*w)))
loss(w) = 0 - sum_(r=1)^4 (y[r]*log(sig(X[r]*w)) + (1 - y[r])*log(1 - sig(X[r]*w)))

>> loss([0; 0])
~2.77258872

>> hand(w) = sum_(r=1)^4 (sig(X[r]*w) - y[r])*X[r]'
hand(w) = sum_(r=1)^4 (sig(X[r]*w) - y[r])*X[r]'

>> w_0 = [0; 0]
w_0 = [0; 0]

>> w_n = w_(n-1) - grad_(v = w_(n-1)) loss(v)/2
w_n = w_(n-1) - grad_(v = w_(n-1)) loss(v)/2

>> u_0 = [0; 0]
u_0 = [0; 0]

>> u_n = u_(n-1) - hand(u_(n-1))/2
u_n = u_(n-1) - hand(u_(n-1))/2

>> u_10
[~-2.10089641;
  ~1.71241565]

>> (w_10 - u_10)'*(w_10 - u_10) < 1/10^16
1

# Training with the gradient written by hand: every X[r] reads the data.
>> use train (X, y)
use train (X, y)

>> sig(z) = 1/(1 + exp(-z))
sig(z) = 1/(1 + exp(-z))

>> hand(w) = sum_(r=1)^200 (sig(X[r]*w) - y[r])*X[r]'
hand(w) = sum_(r=1)^200 (sig(X[r]*w) - y[r])*X[r]'

>> u_0 = [0; 0; 0]
u_0 = [0; 0; 0]

>> u_n = u_(n-1) - hand(u_(n-1))/200
u_n = u_(n-1) - hand(u_(n-1))/200

>> u_20
[~-0.0216833001;
    ~1.96128279;
     ~1.8474728]


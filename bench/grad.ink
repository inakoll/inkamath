# Training with grad, on the same data, to the same weights as hand.ink.
>> use train (X, y)
use train (X, y)

>> sig(z) = 1/(1 + exp(-z))
sig(z) = 1/(1 + exp(-z))

>> loss(w) = 0 - sum_(r=1)^200 (y[r]*log(sig(X[r]*w)) + (1 - y[r])*log(1 - sig(X[r]*w)))
loss(w) = 0 - sum_(r=1)^200 (y[r]*log(sig(X[r]*w)) + (1 - y[r])*log(1 - sig(X[r]*w)))

>> w_0 = [0; 0; 0]
w_0 = [0; 0; 0]

>> w_n = w_(n-1) - grad_(v = w_(n-1)) loss(v)/200
w_n = w_(n-1) - grad_(v = w_(n-1)) loss(v)/200

>> w_20
[~-0.0216833001;
    ~1.96128279;
     ~1.8474728]


# A four-tap FIR filter: each output weighs the last four input samples.
b = [1 2 3 4]/10
y_n = sum_(k=0)^3 b[1,k+1]*x_(n-k)

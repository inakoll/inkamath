# A transformer (Vaswani et al. 2017) trimmed to one pre-norm block of two
# heads, width 4, its unembedding the embedding's transpose, scored on
# reversing two sequences of three tokens: layer norm by cells, attention, a
# ReLU feed-forward, residuals and softmax cross-entropy, weights in eighths.
# Written by hand from NumPy, never recorded; G against a backward pass by
# hand, itself within 4e-15 of complex steps and 2e-10 of central differences.
>> tf(Q = [1 -2; 3 0; -1 2; 0 1;; -2 1; 0 3; 2 -1; 1 0]/8) = {
..     oh(S)[b<=2, t<=3, v<=4] = S[b,t] == v
..     X = oh([1 2 3; 4 1 2])
..     E = [3 -1 2 0; -2 4 1 -3; 0 2 -4 1; 1 -3 0 4]/8
..     P = [2 0 -1 1; -1 3 0 -2; 0 -2 2 3]/8
..     K = [2 1; -1 0; 0 3; 1 -2;; 0 -1; 2 1; -3 0; 1 2]/8
..     V = [1 0; -2 1; 0 2; 3 -1;; -1 2; 1 0; 2 -2; 0 1]/8
..     O = [2 -1 0 1; 0 3 -2 1;; 1 0 -3 2; -2 1 1 0]/8
..     W1 = [3 -2 1 0; -1 0 2 4; 2 1 -3 1; 0 4 1 -2]/8
..     W2 = [1 0 -2 3; -3 2 1 0; 0 -1 2 -2; 2 1 0 -1]/8
..     mu(x)[b,t] = sum_(j=1)^4 x[b,t,j]/4
..     ln(x)[b,t,j] = (x[b,t,j] - mu(x)[b,t])/(sum_(k=1)^4 (x[b,t,k] - mu(x)[b,t])^2/4 + 1/100000)^(1/2)
..     sm(z)[b,t,s] = exp(z[b,t,s])/sum_(r=1)^3 exp(z[b,t,r])
..     att(a, W) = sum_(h=1)^2 sm((a*W[h])*(a*K[h])'/2^(1/2))*(a*V[h])*O[h]
..     relu(z)[b,t,f] = z[b,t,f]*(z[b,t,f] > 0)
..     blk(u) = u + relu(ln(u)*W1)*W2
..     x = X*E + P
..     fwd(W) = ln(blk(x + att(ln(x), W)))*E'
..     ce(Z) = sum_(b=1)^2 sum_(t=1)^3 (log(sum_(v=1)^4 exp(Z[b,t,v])) - sum_(v=1)^4 X[b,4-t,v]*Z[b,t,v])/6
..     Z = fwd(Q)
..     J = ce(Z)
..     G = grad_(W = Q) ce(fwd(W))
.. }
tf(Q = [1 -2; 3 0; -1 2; 0 1;; -2 1; 0 3; 2 -1; 1 0]/8) = { ... }

>> t = tf()
t = tf()

# The first sequence's logits, then the loss over both.
>> t.Z[1]
[ ~0.531503432,  ~-1.20039818, ~0.0363982871,  ~0.85171958;
 ~-0.236944873,   ~1.32755986, ~0.0630845949, ~-1.24672363;
 ~-0.620638314, ~0.0971673149,  ~0.227918321,  ~0.29128097]

>> t.J
~1.85968609

# Exact gradients check a backward pass (MANIFESTO.md): G, by W^Q's heads.
>> t.G
[  ~0.0194071966,   ~-0.0292368776;
  ~-0.0359691761,    ~0.0510334092;
  ~-0.0196623888,    ~0.0278097002;
   ~0.0362243682,   ~-0.0496062319;;
  ~0.00219961497,  ~-0.00274811096;
   ~0.0017525969,   ~0.00176819737;
  ~-0.0022379402, ~-9.37927284e-05;
 ~-0.00171427167,   ~0.00107370632]

# A limit recomputed: each argument is a new sequence, walked until it
# converges, so every term is a call with its memo key.
>> s(a)_0 = 0
s(a)_0 = 0

>> s(a)_n = s(a)_(n-1)*~0.5 + a
s(a)_n = s(a)_(n-1)*~0.5 + a

>> sum_(k=1)^3000 lim s(k)
~9003000


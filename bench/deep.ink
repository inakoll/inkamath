# A deep recurrence: filled from its base, one term after another. Not a
# contraction, so that every term counts in the answer.
>> d_0 = 0
d_0 = 0

>> d_n = d_(n-1)*~1.0 + 1.25
d_n = d_(n-1)*~1.0 + 1.25

>> d_200000
250000


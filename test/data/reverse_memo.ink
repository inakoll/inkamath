# Reverse mode remembers a call made again with the same parts (C323), as
# reverse.ink has it, here over 600,000 terms, apart from it so that each
# file stays within its timeout: each term a step, where an exp walked for
# each gave up after a million steps; e times 600,000.
>> grad_(v = [~1 1]) sum_(k=1)^600000 exp(v[1,1])
[~1630969.1, ~0]

# The functions flowgrad.ink uses: v is flow's own, a function of three
# arguments, which grad's v in another file does not name (C446).
v(W, i, j) = W*i*j
divergence(W) = v(W, 1, 2) + W
total(W) = divergence(W)

# -I's order (DESIGN.md, next in line), run with -I one -I two: gains beside
# this file, not one's; shared from one, not two's; tools from two, whose
# own uses find unit and shared beside it, before one's. So v is [1, 2, 3,
# 20] and y_n = n + 5.
use gains
use shared
use tools
v = [gains.k, shared.s, tools.t, tools.w]
gain(c = 0) = {
    y_n = gains.k*n + shared.s + tools.t + c
}
g = gain()

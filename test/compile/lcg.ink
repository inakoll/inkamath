# MANIFESTO.md's Data says an LCG is a recurrence over mod, which needs a
# qualifier (DESIGN.md, next in line): the interpreter's terms are exact, a
# compiled step's only while the product stays within its type's integers.
# glibc's 1103515245*r passes 2^53 at the second step, where the double run
# parts; Park and Miller's 16807*q stays below it, and parts in float: its
# products pass 2^24 from the second step, 2^31 - 1 rounds to 2^31, and the
# third is past the thousandth a float run is held to. Python's integers,
# doubles and numpy's float32 part at the same steps.
#
#     lcgs: 100 steps from 0, against exact values
#     lcgs.q: within 0
#     lcgs.r: 325238528 at 2, where the interpreter gives 325238575
#
#     lcgs: 100 steps from 0 in float, against exact values
#     lcgs.q: 338690048 at 3, where the interpreter gives 226205932
#     lcgs.r: 0 at 2, where the interpreter gives 325238575
noise() = {
    r_0 = 777
    r_n = mod(1103515245*r_(n-1) + 12345, 2^31)
    q_0 = 777
    q_n = mod(16807*q_(n-1), 2^31 - 1)
}
lcgs = noise()

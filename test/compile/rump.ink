# S. M. Rump, Acta Numerica 19 (2010), §1.2, eq. (1.5): his 1983 example
# rearranged, whose exact value is -0.827396..., 1.1726039400531... in
# double and 1.172603... in float, as the paper gives them:
#
#     rump.g: 1.1726039400531787 at 0, where the interpreter gives -0.82739605994682142
#   in float:
#     rump.g: 1.17260396 at 0, where the interpreter gives -0.82739605994682142
example(a = 77617, b = 33096) = {
    g_n = 21*b*b - 2*a*a + 55*b*b*b*b - 10*a*a*b*b + a/(2*b)
}
rump = example()

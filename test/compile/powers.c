/* The prelude's exp, log and tanh scale by 2^k, which a step computes as
 * pow(2.0, k), and Clang as exp2(k), where the interpreter shifts an exact k
 * (DESIGN.md, exp, log and tanh): they agree only where both are exact. */
#include <math.h>
#include <stdio.h>

int main(void) {
    int failed = 0;
    for (int k = -1100; k <= 1100; ++k) {
        volatile double whole = k; /* computed by the library, not folded */
        const double    exact = ldexp(1.0, k);
        if (pow(2.0, whole) != exact || exp2(whole) != exact) {
            printf("2^%d: pow %.17g, exp2 %.17g, not %.17g\n", k, pow(2.0, whole), exp2(whole),
                   exact);
            failed = 1;
        }
    }
    return failed;
}

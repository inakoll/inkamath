/* b and e against the interpreter's answers for u_n = n + 1: b_0 is a_(-1)/2,
 * and e_0 is c_(-1), which reads u_(-2), before the first input, so NaN (C71). */
#include "back.h"

#include <math.h>
#include <stdio.h>

int main(void) {
    static const double b[]    = {0.0625, 0.0625, 0.0625, 0.0625, 1.5};
    int                 failed = 0;
    back                m;
    back_init(&m);
    for (int n = 0; n < 5; ++n) {
        back_step(&m, n + 1.0);
        const int e_ok = n == 0 ? isnan(m.e[0]) : m.e[0] == 1.0;
        if (m.b[0] != b[n] || !e_ok) {
            printf("at %d, b is %.17g and e %.17g\n", n, m.b[0], m.e[0]);
            failed = 1;
        }
    }
    return failed;
}

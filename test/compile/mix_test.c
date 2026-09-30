/* The compiled model against the interpreter's answers for the instance
 * mix(u_n = 1, b_n = 0), then mix(g = 1, ...) from the fourth term on, all of
 * them exact in a double (MODERNIZATION.md, phase 15). */
#include "mix.h"

#include <stdio.h>

int main(void) {
    static const double exact[] = {0.0, 0.125, 0.21875, 0.4140625};
    int                 failed  = 0;
    mix                 m;
    mix_init(&m);
    for (int n = 0; n < 4; ++n) {
        if (n == 3) {
            m.g = 1.0;
            mix_update(&m);
        }
        mix_step(&m, 1.0, 0.0);
        if (m.v[0] != exact[n]) {
            printf("v_%d is %.17g, not %.17g\n", n, m.v[0], exact[n]);
            failed = 1;
        }
    }
    return failed;
}

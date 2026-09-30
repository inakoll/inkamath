/* The chain fed 0, 1, 2, 0, 1, 2 against the interpreter's answers,
 * frac y_n and frac h.low.v_n for the same input (MODERNIZATION.md, phase 15). */
#include "chain.h"

#include <math.h>
#include <stdio.h>

int main(void) {
    static const double y[]    = {1.0 / 10,       9.0 / 10,          633.0 / 400,
                                  -1601.0 / 8000, 118157.0 / 160000, 4834451.0 / 3200000};
    static const double low[]  = {0.0, 0.25, 0.6875, 0.515625, 0.63671875, 0.9775390625};
    int                 failed = 0;
    chain               m;
    chain_init(&m);
    for (int n = 0; n < 6; ++n) {
        chain_step(&m, (double)(n % 3));
        if (fabs(m.y[0] - y[n]) > 1e-15 || m.h.low.v[0] != low[n]) {
            printf("at %d, y is %.17g and h.low.v %.17g, not %.17g and %.17g\n", n, m.y[0],
                   m.h.low.v[0], y[n], low[n]);
            failed = 1;
        }
    }
    return failed;
}

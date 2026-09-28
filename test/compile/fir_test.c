/* The compiled filter fed x_n = n*n - 3n, against its exact outputs computed
 * with Python's fractions; before its fourth sample it has no output. */
#include "fir.h"

#include <math.h>
#include <stdio.h>

int main(void) {
    static const struct {
        int    n;
        double y;
    } exact[]        = {{3, -1.0}, {4, -1.0}, {10, 41.0}, {57, 2861.0}, {100, 9311.0}};
    const int count  = (int)(sizeof exact / sizeof exact[0]);
    int       next   = 0;
    int       failed = 0;
    fir       m;
    fir_init(&m);
    for (int n = 0; next < count; ++n) {
        fir_step(&m, (double)n * n - 3.0 * n);
        if (n < 3 && m.y[0] != 0.0) {
            printf("y_%d is %.17g before the filter has four samples\n", n, m.y[0]);
            failed = 1;
        }
        if (n != exact[next].n) continue;
        if (fabs(m.y[0] - exact[next].y) > 1e-14 * fmax(1.0, fabs(exact[next].y))) {
            printf("y_%d is %.17g, not %.17g\n", n, m.y[0], exact[next].y);
            failed = 1;
        }
        ++next;
    }
    return failed;
}

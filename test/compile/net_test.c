/* The compiled network fed eight samples, against the exact outputs
 * computed with Python's fractions (DESIGN.md, phase 14, step 2).
 * Every value is a sum of quarters and eighths, so the comparison is exact. */
#include "net.h"

#include <stdio.h>

int main(void) {
    static const struct {
        double x, h1, h2, y;
    } exact[] = {{1.0, 0.0, 0.0, 0.0},   {-2.0, 0.0, 1.0, -0.5},      {0.75, 0.875, 0.0, 0.875},
                 {0.5, 0.0, 0.0, 0.0},   {-1.0, 0.0, 1.375, -0.6875}, {2.0, 1.375, 0.0, 1.375},
                 {0.0, 0.0, 1.5, -0.75}, {1.25, 1.125, 0.375, 0.9375}};
    int ok    = 1;
    net m;
    net_init(&m);
    for (int n = 0; n < (int)(sizeof exact / sizeof exact[0]); ++n) {
        net_step(&m, exact[n].x);
        if (m.h[0][0][0] != exact[n].h1 || m.h[0][1][0] != exact[n].h2 || m.y[0] != exact[n].y) {
            printf("at %d: h [%g; %g], y %g; not [%g; %g], %g\n", n, m.h[0][0][0], m.h[0][1][0],
                   m.y[0], exact[n].h1, exact[n].h2, exact[n].y);
            ok = 0;
        }
    }
    return !ok;
}

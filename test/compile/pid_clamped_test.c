/* The clamped controller closing the loop of pid_test.c, against that loop's
 * exact values computed with Python's fractions: the output is at its limit
 * for the first two steps, so the held integral is what the later ones show. */
#include "pid_clamped.h"

#include <math.h>
#include <stdio.h>

int main(void) {
    static const struct {
        int    n;
        double y;
    } exact[]          = {{1, 0.15},
                          {3, 0.386},
                          {10, 0.7060721076553859},
                          {50, 0.9449500482805079},
                          {200, 0.9998127257257303}};
    const int   count  = (int)(sizeof exact / sizeof exact[0]);
    int         next   = 0;
    int         failed = 0;
    double      y      = 0.0;
    pid_clamped m;
    pid_clamped_init(&m);
    for (int n = 0; next < count; ++n) {
        if (n > 0) y = y + (m.u[0] - y) * m.dt;
        pid_clamped_step(&m, y);
        if (n < 2 && m.u[0] != 1.5) {
            printf("u_%d is %.17g, not the limit\n", n, m.u[0]);
            failed = 1;
        }
        if (n != exact[next].n) continue;
        if (fabs(y - exact[next].y) > 1e-14) {
            printf("y_%d is %.17g, not %.17g\n", n, y, exact[next].y);
            failed = 1;
        }
        ++next;
    }
    return failed;
}

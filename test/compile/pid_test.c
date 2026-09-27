/* The compiled controller closing the loop its model was written for,
 * y_n = y_(n-1) + (u_(n-1) - y_(n-1))*dt, against that loop's exact values
 * computed with Python's fractions (MODERNIZATION.md, phase 14, step 2). */
#include "pid.h"

#include <math.h>
#include <stdio.h>

int main(void) {
    static const struct {
        int    n;
        double y;
    } exact[]        = {{1, 0.2},
                        {3, 0.43152},
                        {10, 0.7477100211622512},
                        {50, 0.9544650319659908},
                        {200, 0.9998450953537439}};
    const int count  = (int)(sizeof exact / sizeof exact[0]);
    int       next   = 0;
    int       failed = 0;
    double    y      = 0.0;
    pid       m;
    pid_init(&m);
    for (int n = 0; next < count; ++n) {
        if (n > 0) y = y + (m.u[0] - y) * m.dt;
        pid_step(&m, y);
        if (n != exact[next].n) continue;
        if (fabs(y - exact[next].y) > 1e-14) {
            printf("y_%d is %.17g, not %.17g\n", n, y, exact[next].y);
            failed = 1;
        }
        ++next;
    }
    return failed;
}

/* The loop against its exact values, x: 0, 1, 1/2, 3/4 and u: 2, 0, 1, 1/2 as
 * the interpreter gives them, then with the controller's gain set to 1 from
 * the fifth step: x_4 = 5/8 and u_4 = 3/8 (DESIGN.md, phase 15). */
#include "loop.h"

#include <stdio.h>

int main(void) {
    static const double x[]    = {0.0, 1.0, 0.5, 0.75, 0.625};
    static const double u[]    = {2.0, 0.0, 1.0, 0.5, 0.375};
    int                 failed = 0;
    loop                m;
    loop_init(&m);
    for (int n = 0; n < 5; ++n) {
        if (n == 4) {
            m.ctl.kp = 1.0;
            loop_update(&m);
        }
        loop_step(&m);
        if (m.plt.x[0] != x[n] || m.ctl.u[0] != u[n]) {
            printf("at %d, x is %.17g and u %.17g, not %.17g and %.17g\n", n, m.plt.x[0],
                   m.ctl.u[0], x[n], u[n]);
            failed = 1;
        }
    }
    return failed;
}

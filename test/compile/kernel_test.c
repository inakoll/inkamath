/* The kernel against the interpreter's answers for x = 1/4, 3/2, -2, 3: y_n,
 * and the diffusion's corner and centre, all exact in a double. */
#include "kernel.h"

#include <stdio.h>

int main(void) {
    static const double x[]      = {0.25, 1.5, -2.0, 3.0};
    static const double y[]      = {33.0, 35.0, 30.0, 37.5};
    static const double corner[] = {0.0, 0.0, 0.5, 0.75};
    static const double centre[] = {16.0, 8.0, 5.0, 3.5};
    int                 failed   = 0;
    kernel              m;
    kernel_init(&m);
    for (int n = 0; n < 4; ++n) {
        kernel_step(&m, x[n]);
        if (m.y[0] != y[n] || m.u[0][0][0] != corner[n] || m.u[0][1][1] != centre[n]) {
            printf("at %d, y is %.17g, and u %.17g and %.17g\n", n, m.y[0], m.u[0][0][0],
                   m.u[0][1][1]);
            failed = 1;
        }
    }
    return failed;
}

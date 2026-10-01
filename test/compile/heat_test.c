/* The compiled rod heated at a constant rate, its step doubled after ten
 * steps, against the exact temperatures computed with Python's fractions
 * (DESIGN.md, phase 14, step 2). */
#include "heat.h"

#include <math.h>
#include <stdio.h>

static int near(int n, int node, double got, double exact) {
    if (fabs(got - exact) <= 1e-14 * fmax(1.0, fabs(exact))) return 1;
    printf("u[%d] at %d is %.17g, not %.17g\n", node, n, got, exact);
    return 0;
}

int main(void) {
    static const struct {
        int    n;
        double u[3];
    } exact[]       = {{1, {0.007690433187121929, 0.000946073793755913, 0.00011467561136435309}},
                       {10, {0.03495430215531027, 0.01593317226679509, 0.005650158208078555}},
                       {11, {0.0370835350988498, 0.01832092314626061, 0.00702003269200119}},
                       {20, {0.0449144465518458, 0.028487237543918187, 0.013678271777518819}}};
    const int count = (int)(sizeof exact / sizeof exact[0]);
    int       next  = 0;
    int       ok    = 1;
    heat      m;
    heat_init(&m);
    for (int n = 0; next < count; ++n) {
        if (n == 11) {
            m.dt = 0.02;
            heat_update(&m);
        }
        heat_step(&m, 1.0);
        if (n != exact[next].n) continue;
        for (int node = 0; node < 3; ++node)
            ok &= near(n, node, m.u[0][node][0], exact[next].u[node]);
        ++next;
    }
    return !ok;
}

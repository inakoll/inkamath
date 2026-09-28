/* The two-measurement tracker fed zp_n = 2n + (-1)^n/2 and zs_n = zp_n + 2 +
 * (-1)^n/4, against its exact estimates and first gain computed with
 * Python's fractions (MODERNIZATION.md, phase 14, step 2). */
#include "kalman2.h"

#include <math.h>
#include <stdio.h>

static int near(const char* what, int n, double got, double exact) {
    if (fabs(got - exact) <= 1e-14 * fmax(1.0, fabs(exact))) return 1;
    printf("%s at %d is %.17g, not %.17g\n", what, n, got, exact);
    return 0;
}

int main(void) {
    static const struct {
        int    n;
        double position, velocity;
    } exact[]       = {{1, 1.6422326296532936, 1.570377270804756},
                       {2, 3.8148547401157256, 2.6935762823789093},
                       {5, 9.845604007671273, 1.9586515934959736},
                       {20, 40.147150699506895, 2.0779756572997705},
                       {100, 200.1475176163173, 2.078004184213378}};
    const int count = (int)(sizeof exact / sizeof exact[0]);
    int       next  = 0;
    int       ok    = 1;
    kalman2   m;
    kalman2_init(&m);
    for (int n = 0; next < count; ++n) {
        const double sign = n % 2 ? -1.0 : 1.0;
        const double zp   = 2.0 * n + sign / 2.0;
        kalman2_step(&m, zp, zp + 2.0 + sign / 4.0);
        if (n == 1) {
            ok &= near("K[0][0]", n, m.K[0][0][0], 526513.0 / 777863.0);
            ok &= near("K[0][1]", n, m.K[0][0][1], 150050.0 / 777863.0);
            ok &= near("K[1][0]", n, m.K[0][1][0], -978001.0 / 1555726.0);
            ok &= near("K[1][1]", n, m.K[0][1][1], 1203101.0 / 1555726.0);
        }
        if (n != exact[next].n) continue;
        ok &= near("the position", n, m.x[0][0][0], exact[next].position);
        ok &= near("the velocity", n, m.x[0][1][0], exact[next].velocity);
        ++next;
    }
    return !ok;
}

/* The compiled tracker fed z_n = 2n + (-1)^n/2, against the exact estimates
 * computed with Python's fractions (MODERNIZATION.md, phase 14, step 2). */
#include "kalman.h"

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
    } exact[] = {{1, 1.4286054259876249, 0.7139457401237506},
                 {2, 4.21126301589948, 2.3673477120814828},
                 {5, 9.85452840856099, 1.9746993128061594},
                 {20, 40.10314146070398, 2.0242752613443757},
                 {100, 200.10334303178394, 2.0249610288193445}};
    const int count = (int)(sizeof exact / sizeof exact[0]);
    int       next  = 0;
    int       ok    = 1;
    kalman    m;
    kalman_init(&m);
    for (int n = 0; next < count; ++n) {
        kalman_step(&m, 2.0 * n + (n % 2 ? -0.5 : 0.5));
        if (n == 2) {
            ok &= near("K[0]", n, m.K[0][0][0], 752651.0 / 857701.0);
            ok &= near("K[1]", n, m.K[0][1][0], 1203101.0 / 1715402.0);
        }
        if (n != exact[next].n) continue;
        ok &= near("the position", n, m.x[0][0][0], exact[next].position);
        ok &= near("the velocity", n, m.x[0][1][0], exact[next].velocity);
        ++next;
    }
    return !ok;
}

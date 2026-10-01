/* The compiled reading fed a noisy ramp up and back down, against the exact
 * readings computed with Python's fractions (DESIGN.md, phase 14,
 * step 2). Every value is a quarter, so the comparison is exact. */
#include "adc.h"

#include <stdio.h>

int main(void) {
    static const struct {
        double x, v, held, alarm, rising;
    } exact[] = {{0.1, 0.0, 0.0, 0, 0},    {0.3, 0.25, 0.25, 0, 1},    {0.6, 0.5, 0.5, 0, 1},
                 {0.62, 0.5, 0.5, 0, 0},   {0.9, 0.75, 0.75, 0, 1},    {1.3, 1.25, 1.25, 1, 1},
                 {1.1, 1.0, 1.0, 0, 0},    {1.2, 1.0, 1.0, 0, 0},      {0.2, 0.0, 0.0, 0, 0},
                 {-0.3, -0.5, -0.5, 0, 0}, {-1.2, -1.25, -1.25, 1, 0}, {-1.4, -1.5, -1.5, 1, 0},
                 {0.0, 0.0, 0.0, 0, 1}};
    int ok    = 1;
    adc m;
    adc_init(&m);
    for (int n = 0; n < (int)(sizeof exact / sizeof exact[0]); ++n) {
        adc_step(&m, exact[n].x);
        if (m.v[0] != exact[n].v || m.held[0] != exact[n].held || m.alarm[0] != exact[n].alarm ||
            m.rising[0] != exact[n].rising) {
            printf("at %d: v %g, held %g, alarm %g, rising %g; not %g, %g, %g, %g\n", n, m.v[0],
                   m.held[0], m.alarm[0], m.rising[0], exact[n].v, exact[n].held, exact[n].alarm,
                   exact[n].rising);
            ok = 0;
        }
    }
    return !ok;
}

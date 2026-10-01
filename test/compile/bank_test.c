/* The bank fed 1 against the interpreter's answers: each row's filter with
 * its own a and memory, and y smoothing the last; then the first row's
 * filter retuned, which the others do not see (MODERNIZATION.md, phase 15). */
#include "bank.h"

#include <stdio.h>

int main(void) {
    static const double rows[4][3] = {{0.0, 0.0, 0.0},
                                      {0.25, 0.5, 0.75},
                                      {7.0 / 16, 0.75, 15.0 / 16},
                                      {37.0 / 64, 7.0 / 8, 63.0 / 64}};
    static const double y[4]       = {0.0, 3.0 / 8, 21.0 / 32, 105.0 / 128};
    int                 failed     = 0;
    bank                m;
    bank_init(&m);
    for (int n = 0; n < 4; ++n) {
        bank_step(&m, 1.0);
        for (int j = 0; j < 3; ++j)
            if (m.bank[0][j][0] != rows[n][j]) {
                printf("bank_%d row %d is %.17g, not %.17g\n", n, j + 1, m.bank[0][j][0],
                       rows[n][j]);
                failed = 1;
            }
        if (m.y[0] != y[n]) {
            printf("y_%d is %.17g, not %.17g\n", n, m.y[0], y[n]);
            failed = 1;
        }
    }
    m.bank_smooth_1.a = 1.0;
    bank_step(&m, 1.0);
    if (m.bank[0][0][0] != 1.0 || m.bank[0][1][0] != 15.0 / 16) {
        printf("retuned, bank_4 is %.17g and %.17g\n", m.bank[0][0][0], m.bank[0][1][0]);
        failed = 1;
    }
    return failed;
}

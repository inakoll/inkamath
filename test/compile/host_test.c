/* docs/compile.md's host file for pid.h, taken from it: ink_run over N
 * steps gives what N calls of ink_step give, and each offset is its field's. */
#include "pid.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

size_t ink_size(void);
void   ink_init(pid* m);
void   ink_step(pid* m, double y);
void   ink_run(pid* m, const double* y, long n, double* u);
extern const size_t ink_u, ink_kp;

int main(void) {
    enum { N = 50 };
    double y[N], u[N];
    pid    run, steps;
    int    failed = 0;
    for (int k = 0; k < N; ++k) y[k] = 0.02 * k;
    ink_init(&run);
    ink_init(&steps);
    ink_run(&run, y, N, u);
    for (int k = 0; k < N; ++k) {
        ink_step(&steps, y[k]);
        if (u[k] != steps.u[0]) {
            printf("u_%d is %.17g by ink_run, %.17g by ink_step\n", k, u[k], steps.u[0]);
            failed = 1;
        }
    }
    if (memcmp(&run, &steps, sizeof run) != 0 || ink_size() != sizeof(pid)) {
        puts("the states part, or ink_size is not their size");
        failed = 1;
    }
    if ((char*)&steps + ink_u != (char*)steps.u || (char*)&steps + ink_kp != (char*)&steps.kp) {
        puts("an offset is not its field's");
        failed = 1;
    }
    return failed;
}

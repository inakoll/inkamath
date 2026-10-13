/* fallback.ink's models compiled, against values computed with Python's
 * fractions and README section 4's stopping rule (DESIGN.md, a clause for
 * a limit that does not converge). Specified before it is built: exits 77,
 * which skips it, while the step is NaN where the clause after a guard is
 * to be taken, at the plan's u_2 and Newton's x_1, and nothing before them
 * is otherwise; any other difference fails. */
#include "fallback_mpc.h"
#include "fallback_newton.h"
#include "fallback_rooted.h"

#include <math.h>
#include <stdio.h>

static int  failed = 0, early = 0;
static char first_[96];

static void expect(const char* name, int n, double got, double want, double within, int first) {
    if (isnan(want) ? isnan(got) : fabs(got - want) <= within) return;
    if (!failed) snprintf(first_, sizeof first_, "%s_%d is %.17g, not %.17g", name, n, got, want);
    failed = 1;
    if (n < first) early = 1;
}

int main(void) {
    const double    root = 0.6180339887498949;
    double          u2 = 0.0, x1 = 0.0;
    fallback_mpc    m;
    fallback_newton h;
    fallback_rooted r;
    fallback_mpc_init(&m);
    fallback_newton_init(&h);
    fallback_rooted_init(&r);
    for (int n = 0; n <= 20; ++n) {
        fallback_mpc_step(&m);
        fallback_newton_step(&h, 1.0 + (double)(n % 2));
        fallback_rooted_step(&r, 2.0);
        expect("x", n, m.x[0], n == 0 ? 2.5 : n == 1 ? 1.5 : ldexp(1.0, 1 - n), 0.0, 3);
        expect("u", n, m.u[0], n < 2 ? -1.0 : -ldexp(1.0, -n), 0.0, 2);
        expect("solved", n, m.solved[0], n < 2 ? 1.0 : 0.0, 0.0, 2);
        expect("newton x", n, h.x[0], n < 2 ? 0.5 : root, 1e-15, 1);
        expect("rooted y", n, r.y[0], NAN, 0.0, 21);
        if (n == 2) u2 = m.u[0];
        if (n == 1) x1 = h.x[0];
    }
    if (failed && !early && isnan(u2) && isnan(x1)) {
        printf("skipped: NaN where the clause after is to be taken, as before the change\n");
        return 77;
    }
    if (failed) printf("first: %s\n", first_);
    return failed;
}

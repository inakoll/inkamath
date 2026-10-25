/* Headers built as shared libraries with -D<NAME>_SHARED (DESIGN.md, *A
 * header built as a shared library*), loaded as a host that reads no C loads
 * them, each output read at the offset the library's tables give, against
 * the interpreter's values: pid's u_2 and u_10 for y_n = n/10, as doubles
 * and, built with --float, as floats; track's x_3, x_15 and d_30 for
 * optional.ink's seen; and bank's y_3 and, by its dotted name, its instance
 * s_sharp's v_3, for an input of 1. Each library's tables are held to one
 * another: its count to its names, its types to its offsets, and its inputs
 * to what its step takes. */
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Opaque, and named so that each call has the type its function was
 * defined with. */
struct pid;
struct pidf;
struct track;
struct bank;

static void* load(const char* path) {
#ifdef _WIN32
    void* library = (void*)LoadLibraryA(path);
#else
    void* library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if (!library) printf("cannot load %s\n", path);
    return library;
}

/* The address of module_suffix, a function's copied into 'to' where 'to' is
 * given: ISO C converts no object pointer to a function pointer. */
static void* symbol(void* library, const char* module, const char* suffix, void* to, size_t size) {
    char  name[64];
    void* found = NULL;
    snprintf(name, sizeof name, "%s_%s", module, suffix);
#ifdef _WIN32
    FARPROC address = GetProcAddress((HMODULE)library, name);
    memcpy(&found, &address, sizeof found);
#else
    found = dlsym(library, name);
#endif
    if (!found) printf("%s is not exported\n", name);
    if (to && found) memcpy(to, &found, size);
    return found;
}

/* A member's offset, by its name in the library's tables. */
static size_t offset(void* library, const char* module, const char* member) {
    const char* const* names   = (const char* const*)symbol(library, module, "members_", NULL, 0);
    const size_t*      offsets = (const size_t*)symbol(library, module, "offsets_", NULL, 0);
    for (size_t i = 0; names && offsets && names[i]; ++i)
        if (strcmp(names[i], member) == 0) return offsets[i];
    printf("%s has no member %s\n", module, member);
    exit(1);
}

/* A model's state, as large as the library says. */
static char* state(void* library, const char* module) {
    const size_t* size = (const size_t*)symbol(library, module, "size_", NULL, 0);
    if (!size) exit(1);
    return (char*)calloc(1, *size);
}

/* A member's bytes, by its type in the tables: "double[2][3]" is 6 doubles. */
static size_t bytes(const char* type) {
    static const struct {
        const char* name;
        size_t      size;
    } bases[] = {{"double", sizeof(double)},
                 {"float", sizeof(float)},
                 {"long long", sizeof(long long)},
                 {"int", sizeof(int)}};
    const size_t length = strcspn(type, "[");
    size_t       size   = 0;
    for (size_t i = 0; i < sizeof bases / sizeof bases[0]; ++i)
        if (strlen(bases[i].name) == length && strncmp(type, bases[i].name, length) == 0)
            size = bases[i].size;
    for (const char* at = type + length; *at == '['; at = strchr(at, ']') + 1)
        size *= strtoul(at + 1, NULL, 10);
    return size;
}

/* Each member's type fits between its offset and the next, or the struct's
 * end; 'inputs' is the step's, separated by spaces. */
static int tables(void* library, const char* module, const char* inputs) {
    const size_t*      size    = (const size_t*)symbol(library, module, "size_", NULL, 0);
    const size_t*      count   = (const size_t*)symbol(library, module, "count_", NULL, 0);
    const size_t*      offsets = (const size_t*)symbol(library, module, "offsets_", NULL, 0);
    const char* const* names   = (const char* const*)symbol(library, module, "members_", NULL, 0);
    const char* const* types   = (const char* const*)symbol(library, module, "types_", NULL, 0);
    const char* const* given   = (const char* const*)symbol(library, module, "inputs_", NULL, 0);
    if (!size || !count || !offsets || !names || !types || !given) return 1;
    size_t n = 0;
    while (names[n]) ++n;
    int failed = n != *count;
    if (failed) printf("%s_count_ is %zu, not %zu\n", module, *count, n);
    for (size_t i = 0; i < n; ++i) {
        size_t next = *size;
        for (size_t j = 0; j < n; ++j)
            if (offsets[j] > offsets[i] && offsets[j] < next) next = offsets[j];
        if (bytes(types[i]) == 0 || offsets[i] + bytes(types[i]) > next) {
            printf("%s's %s, %s at %zu, does not fit before %zu\n", module, names[i], types[i],
                   offsets[i], next);
            failed = 1;
        }
    }
    char joined[64] = "";
    for (size_t i = 0; given[i]; ++i)
        snprintf(joined + strlen(joined), sizeof joined - strlen(joined), "%s%s", i ? " " : "",
                 given[i]);
    if (strcmp(joined, inputs) != 0) {
        printf("%s's inputs are '%s', not '%s'\n", module, joined, inputs);
        failed = 1;
    }
    return failed;
}

static int held(const char* what, double got, double exact, double within) {
    if (fabs(got - exact) <= within) return 0;
    printf("%s is %.17g, not %.17g\n", what, got, exact);
    return 1;
}

static int pid(void* library) {
    void (*init)(struct pid*)         = NULL;
    void (*step)(struct pid*, double) = NULL;
    symbol(library, "pid", "init", &init, sizeof init);
    symbol(library, "pid", "step", &step, sizeof step);
    if (!init || !step) return 1;
    const size_t u      = offset(library, "pid", "u");
    char*        m      = state(library, "pid");
    int          failed = 0;
    init((struct pid*)m);
    for (int n = 0; n <= 10; ++n) {
        step((struct pid*)m, n / 10.0);
        double got;
        memcpy(&got, m + u, sizeof got);
        if (n == 2) failed |= held("pid's u_2", got, 1.67, 1e-14);
        if (n == 10) failed |= held("pid's u_10", got, 0.35, 1e-14);
    }
    free(m);
    return failed;
}

static int pidf(void* library) {
    void (*init)(struct pidf*)        = NULL;
    void (*step)(struct pidf*, float) = NULL;
    symbol(library, "pidf", "init", &init, sizeof init);
    symbol(library, "pidf", "step", &step, sizeof step);
    if (!init || !step) return 1;
    const size_t u = offset(library, "pidf", "u");
    char*        m = state(library, "pidf");
    init((struct pidf*)m);
    for (int n = 0; n <= 2; ++n) step((struct pidf*)m, (float)n / 10.0f);
    float got;
    memcpy(&got, m + u, sizeof got);
    free(m);
    return held("pidf's u_2", got, 1.67, 1e-6);
}

static int track(void* library) {
    void (*init)(struct track*)              = NULL;
    void (*step)(struct track*, double, int) = NULL;
    symbol(library, "track", "init", &init, sizeof init);
    symbol(library, "track", "step", &step, sizeof step);
    if (!init || !step) return 1;
    const size_t x      = offset(library, "track", "x");
    const size_t d      = offset(library, "track", "d");
    char*        m      = state(library, "track");
    int          failed = 0;
    init((struct track*)m);
    for (int n = 0; n <= 30; ++n) {
        /* zd: 40 at 30, and n + 1/4 at every third step but from 9 to 14. */
        const int given = n == 30 || (n % 3 == 0 && (n < 9 || n > 14));
        step((struct track*)m, n == 30 ? 40.0 : n + 0.25, given);
        double xn, dn;
        memcpy(&xn, m + x, sizeof xn);
        memcpy(&dn, m + d, sizeof dn);
        if (n == 3) failed |= held("track's x_3", xn, 25.0 / 8, 0);
        if (n == 15) failed |= held("track's x_15", xn, 487.0 / 32, 0);
        if (n == 30) failed |= held("track's d_30", dn, 51.0 / 4, 0);
    }
    free(m);
    return failed;
}

static int bank(void* library) {
    void (*init)(struct bank*)         = NULL;
    void (*step)(struct bank*, double) = NULL;
    symbol(library, "bank", "init", &init, sizeof init);
    symbol(library, "bank", "step", &step, sizeof step);
    if (!init || !step) return 1;
    const size_t y = offset(library, "bank", "y");
    const size_t v = offset(library, "bank", "s_sharp.v");
    char*        m = state(library, "bank");
    init((struct bank*)m);
    for (int n = 0; n <= 3; ++n) step((struct bank*)m, 1.0);
    double yn, vn;
    memcpy(&yn, m + y, sizeof yn);
    memcpy(&vn, m + v, sizeof vn);
    free(m);
    return held("bank's y_3", yn, 105.0 / 128, 0) | held("bank's s_sharp.v_3", vn, 1.125, 0);
}

int main(int argc, char** argv) {
    void* libraries[4];
    if (argc != 5) {
        puts("usage: compile_shared libpid libtrack libpidf libbank");
        return 2;
    }
    for (int i = 0; i < 4; ++i) {
        libraries[i] = load(argv[i + 1]);
        if (!libraries[i]) return 1;
    }
    return pid(libraries[0]) | track(libraries[1]) | pidf(libraries[2]) | bank(libraries[3]) |
           tables(libraries[0], "pid", "y") | tables(libraries[1], "track", "z?") |
           tables(libraries[2], "pidf", "y") | tables(libraries[3], "bank", "x");
}

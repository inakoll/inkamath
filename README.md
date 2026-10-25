Inkamath
========

Inkamath is a small mathematical interpreter written in standard C++20, with a
compiler of its definitions to C (`--compile`) and a check of that C against
the interpreter (`--check`). It evaluates exact rational arithmetic, complex
numbers in doubles, matrices and rank-3 tensors of expressions, user-defined
functions and models, sequences defined by recurrence, series, and derivatives
(`grad`). First written offline around 2007–2008 and imported from Google Code
around 2014, it has since been rebuilt around exact numbers as the reference
semantics and generated C checked against them ([`DESIGN.md`](DESIGN.md)).

The idea it is built around is that **an identifier names an expression, not a
value**. The expression is re-evaluated every time the name is used, so a
definition keeps referring to whatever its free names mean *now*:

```
>> a = 1
a = 1

>> b = a+a
b = a+a

>> a = 2
a = 2

>> b
4
```

`b` is not `2`. It is the expression `a+a`, and `a` is `2`.

*Quick overview:*
```
>> [pi, e]
[~3.14159265, ~2.71828183]

>> 1+2*3^3*2+1
110

>> a=[1 2;3 4]
a=[1 2;3 4]

>> [a, a; a, a]
[1, 2, 1, 2;
 3, 4, 3, 4;
 1, 2, 1, 2;
 3, 4, 3, 4]
```

A sequence is written as a recurrence, the way it is on paper, and `lim`
follows it to its limit. Here is the exponential series, each term built on the
one before:

```
>> exp(x)_0=1
exp(x)_0=1

>> exp(x)_n=exp(x)_(n-1)+x^n/!n
exp(x)_n=exp(x)_(n-1)+x^n/!n

>> lim exp(1)
~2.71828183
```

A series also has the notation it has on paper, so the term is written once
and the previous one is never named:

```
>> exp(x)_n=sum_(k=0)^n x^k/!k
exp(x)_n=sum_(k=0)^n x^k/!k

>> exp(1)_10
~2.7182818
```

Without an upper bound the sum is the series itself, summed to its limit, and
`exp` needs neither a sequence nor `lim`: it is an ordinary function. A name
has one definition, so the sequence goes first:

```
>> clear exp
clear exp

>> exp(x)=sum_(k=0) x^k/!k
exp(x)=sum_(k=0) x^k/!k

>> cos(x)=(exp(i*x)+exp(-i*x))/2
cos(x)=(exp(i*x)+exp(-i*x))/2

>> cos(pi/3)
~0.5
```

| | |
|---|---|
| [README.md](README.md) | this page: getting started and building |
| [docs/language.md](docs/language.md) | the language, section by section |
| [docs/compile.md](docs/compile.md) | the compiler and the check, for reference |
| [test/compile/](test/compile/) | models compiled and checked by the tests, each a worked example |
| [MANIFESTO.md](MANIFESTO.md) | why it is built so, and where it could go |
| [DESIGN.md](DESIGN.md) | the decision record and the defect table, for maintainers |
| [CLAUDE.md](CLAUDE.md) | the rules for contributors and agents |

Contents: [Getting started](#getting-started) ·
[Building](#building) ·
[the language](docs/language.md) ·
[compiling to C](docs/compile.md).

Getting started
---------------

A controller, from its equations to C checked against them. Build it, then
work in `build/`:

```sh
cmake -S . -B build -G Ninja && cmake --build build
cd build && ./inkamath
```

A PID controller sampled every dt, as a textbook writes it, and the plant
x' = u - x it drives, discretised by Euler:

    e_n = r - y_n                     s_n = s_(n-1) + e_n dt
    d_n = (e_n - e_(n-1))/dt          u_n = kp e_n + ki s_n + kd d_n
    x_n = x_(n-1) + dt (u_(n-1) - x_(n-1))

Each is a model: its parameters have defaults, its input `y_n` or `u_n` has
none. Two instances close the loop, the plant reading the controller a step
late, which lets it start:

```
>> pid(dt = 1/10, r = 1, kp = 2, ki = 1, kd = 1/10, y_n) = {
..     e_n = r - y_n
..     s_0 = 0
..     s_n = s_(n-1) + e_n*dt
..     d_0 = 0
..     d_n = (e_n - e_(n-1))/dt
..     u_n = kp*e_n + ki*s_n + kd*d_n
.. }
pid(dt = 1/10, r = 1, kp = 2, ki = 1, kd = 1/10, y_n) = { ... }

>> plant(dt = 1/10, u_n) = {
..     x_0 = 0
..     x_n = x_(n-1) + dt*(u_(n-1) - x_(n-1))
.. }
plant(dt = 1/10, u_n) = { ... }

>> ctl = pid(y_n = plt.x_n)
ctl = pid(y_n = plt.x_n)

>> plt = plant(u_n = ctl.u_n)
plt = plant(u_n = ctl.u_n)
```

Asked at the prompt, every term is exact; a bare number is all of its value,
`~` marks one cut short for printing. The integral pulls the plant to r:

```
>> [ctl.u_0, ctl.u_1, ctl.u_2]
[2, 1.48, 1.3632]

>> frac ctl.u_2
852/625

>> plt.x_200
~0.999845095
```

The same lines without the prompts are
[`test/compile/pidloop.ink`](test/compile/pidloop.ink). Compiled
by the model's name, the controller is a C header for a host that measures y
and applies u; without a name, the file's instances are compiled as one
closed-loop step:

```sh
./inkamath --compile ../test/compile/pidloop.ink pid -o pid.h
```

Then the check: `ctl` compiled, stepped a hundred times on the y the
interpreter gives it, each term held to the exact one within a billionth of
one plus the term. "within" is the largest difference:

```sh
./inkamath --check ../test/compile/pidloop.ink ctl -o check.c
cc -O2 check.c -o check -lm && ./check
```

    ctl: 100 steps from 0, against exact values
    ctl.e: within 1.1e-16
    ctl.d: within 1.1e-15
    ctl.s: within 3.3e-16
    ctl.u: within 3.3e-16

For a target that computes in float, `--float` writes floats, holds each
term within a thousandth and counts the most it differs in units of a float
at the exact term. d settles to 0, where floats are dense, so a small error
there is many units:

```sh
./inkamath --check ../test/compile/pidloop.ink ctl --float -o checkf.c
cc -O2 checkf.c -o checkf -lm && ./checkf
```

    ctl: 100 steps from 0 in float, against exact values
    ctl.e: within 3e-08, 56 units of a float
    ctl.d: within 5e-07, 1.8e+03 units of a float
    ctl.s: within 5e-07, 8.3 units of a float
    ctl.u: within 5.9e-07, 9.9 units of a float

A check that fails names the first term that parts and exits with a failure.
`wild` in [`test/compile/drift.ink`](test/compile/drift.ink) computes a tenth
again at every step, and each step multiplies the double's rounding by ten:

```sh
./inkamath --check ../test/compile/drift.ink wild -o wild.c
cc -O2 wild.c -o wild -lm && ./wild
```

    wild: 100 steps from 0, against exact values
    wild.d: 0.099999997532837726 at 9, where the interpreter gives 0.10000000000000001

`pid.h` opens with how to call it:

    /* Using it:
     *
     *     pid m;
     *     pid_init(&m);
     *     pid_step(&m, y);  once for each index, the first 0
     *     m.u[0]  is then u_n
     *
     * A step takes y_n, the input at its index. After a step, m.name[k] is
     * name_(n-k) for each sequence: y, e (k <= 1), d, s (k <= 1) and u. The
     * parameters are fields holding the model's defaults once pid_init has run:
     * dt = 0.1, kd = 0.1, ki = 1.0, kp = 2.0 and r = 1.0. After assigning one,
     * call pid_update.
     */

A host in C includes it; the struct is named after the header:

```c
#include <stdio.h>
#include "pid.h"

int main(void) {
    pid m;
    pid_init(&m);
    m.kp = 3;
    pid_update(&m);
    for (int k = 0; k < 3; ++k) {
        pid_step(&m, k/10.0);
        printf("u_%d = %g\n", k, m.u[0]);
    }
}
```

`cc -O2 main.c -o main -lm && ./main` prints `u_0 = 3`, `u_1 = 2.69` and
`u_2 = 2.47`, as `pid(kp = 3, y_n = n/10)` gives them at the prompt. The
header is C99, warning-free under `-std=c99 -Wpedantic` with GCC and Clang,
and C++ as well; it includes `<math.h>` and `<string.h>`, and `<float.h>`
where a limit is compiled, and allocates nothing: the struct holds all the
step keeps. Python calls it through a shared library
([`docs/compile.md`](docs/compile.md#hosting-from-c-and-python)).

A training step takes the same path. `lsq` in
[`test/compile/grad.ink`](test/compile/grad.ink) is
least squares by gradient descent, `w_n = w_(n-1) - eta*grad_(v = w_(n-1))
loss(v)`, and `line` an instance of it: `--compile` of `lsq` writes its step,
and `--check` of `line`, built and run as above, reports `line.w: within
4.4e-16`.

Next: [`docs/language.md`](docs/language.md) for the notation;
[`docs/compile.md`](docs/compile.md) for what else compiles and what the check
reports.

Building
--------

Requires a C++20 compiler and CMake 3.20 or newer. No external dependencies:
doctest is vendored under `third_party/` and is used by the tests only.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/inkamath            # the REPL; 'q' or end of input quits
./build/inkamath --help     # files, transcripts, and the rest
ctest --test-dir build --output-on-failure
```

Useful options: `-DINKAMATH_WERROR=ON` (warnings are errors, as in CI) and
`-DINKAMATH_SANITIZE=address,undefined`.

At a terminal the line can be edited: the arrows, Home and End move, Up and
Down walk back through what was typed, Ctrl-C drops the line and Ctrl-D quits.

Given files, it runs them in order and exits; read from a pipe it prints bare
answers, as `bc` does, and `--echo` prints a transcript instead, in the format
of `test/data/*.ink`.

`--compile` writes the sequences a file defines as a C header and `--check` holds it
to the interpreter: [`docs/compile.md`](docs/compile.md) has the header, what
compiles and what is refused, how to build it, rates and optional inputs, the
check's report, `--float`, and hosting from C and Python.

`--check session.ink`, given a transcript alone, replays it and shows each
answer that is not the one recorded, as recorded (`-`) and as given now (`+`),
under the line it answers. It is the check the tests make of
`test/data/*.ink`, for any transcript.

Interpreter behaviour is pinned by golden transcripts in `test/data/*.ink`,
which are literal sessions. This file's sessions are replayed by a test of
their own, and docs/language.md's by another, each file as one session, so the
documentation and the interpreter check each other.
Regenerate the goldens with
`cmake --build build --target record_goldens` and read the diff: it is the
record of what your change did.

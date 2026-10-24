Inkamath
========

Inkamath is a small mathematical interpreter written in standard C++. It
evaluates complex arithmetic, matrices of expressions, user-defined functions,
sequences defined by recurrence, and series. It was written around 2014; see the
accompanying licence file.

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

`--compile model.ink -o model.h` writes the sequences a file defines as a C
header over doubles: a struct holding the parameters and each sequence's latest
terms, an `init` and a `step`, for a filter to run where the interpreter cannot.
What derives from the parameters alone is computed by `update`, which `init`
calls and the host calls again after assigning a parameter. The header opens
with how to call it: the step's inputs, what each field holds after a step,
and the parameters with their values. A parameter read where the step
needs a constant, a size or an exponent, is compiled in instead, as the
header says. A matrix product of 27 multiplications or more, and a term
whose cells are each written alike and call no function, are written as
loops, every value the same, bit for bit, as written out cell by cell (a
NaN's bits aside), unless the C compiler contracts a multiply and an add
(`-ffp-contract`).
A step too long for a compiler to optimise as one function is written as
several, and what one computes and a later one reads is held in the model
struct, its last member: about 300 KB for one training a transformer's 13
weights at once. A host places that struct statically or on the heap, not as a
local of a thread with a small stack; `sizeof` the struct counts it.
`test/compile/pid.ink` is a PID controller, and `test/compile/expected/pid.h`
is what it compiles to; `test/compile/kalman.ink` is a Kalman filter over
matrices.
`test/compile/pid_clamped.ink` limits its output with guards, and
`test/compile/fir.ink` is a filter written as a sum. A sum or a product up to
the index, or the index less a constant, `sum_(t=1)^n e_t`, is kept as a
running total, a sequence of the step's own, `s0_`, as a recurrence written by
hand would be: `test/compile/accum.ink` has a regret. Refused: terms that read
the index itself, as Adam's `b2^(t-i)` does, a last index other than the index
less a constant, and for now a maximum up to it. `test/compile/kalman2.ink`
inverts a matrix at every step, and `test/compile/heat.ink` once, in `update`;
`test/compile/adc.ink` rounds with `floor` and tests with `and` and `or`, and
`test/compile/net.ink` is a small network whose terms are defined by cells.
`--compile mix.ink mix -o mix.h` compiles the model `mix` that `mix.ink`
defines (section 5), read as `use` reads the file: its inputs are the step's
arguments, in the order of its signature, one whose size the signature states
a pointer to its cells, row by row, and its parameters, with their
defaults, the only fields; a name nothing defines is refused rather than
taken for one more input. A read of an input before the stream is a term the
model's history gives, which `init` folds into the input's window, or is
refused: `down` in `test/compile/decimate.ink` states `x_n | n < 0 = 0`.
An optional input, `z_n?`, is followed in the step by its flag, `int
z_present_in_`, 0 where it is absent, its value then not read; the struct
keeps the flags beside the input's terms, `z_present[k]`, and the input's
cells are NaN where it is absent, before the stream too. A guard whose truth
is NaN where an input it reads is absent does not hold, as interpreted:
`test/compile/optional.ink`. An inner instance's optional input takes its
argument's flag: an optional input's, forwarded; a sequence's, 1 where a
clause of its chain gave the term, NaN included, and 0 where none did; any
other's, 1; and 0 at every step where its instance
leaves it out (`test/compile/inner.ink`). An argument reading such a chain back
in time, or holding it, is refused, for now.
A file's named instances are compiled with it, into
one step that orders all their terms together, each instance a struct of its
own: `test/compile/loop.ink` closes a loop, `m.ctl.u[0]` and `m.plt.x[0]`,
and `test/compile/chain.ink` nests one. A function, and an instance of a model
without memory, are compiled where they are called, a constant folding
through and a recursion unrolling where its guards fold:
`test/compile/kernel.ink` has a convolution run at every step. A local is a
temporary of the step, read from its binding on in its clause as
interpreted: `test/compile/locals.ink` takes Runge-Kutta's stages as locals,
each computed once. One by cells, one bound on the right of `and` or `or`
and read after it, one bound in a term's index, and one a sum without an
upper bound carries past a term are refused for now. An instance
with memory written where it is read is one of its own, named after where it
is written and the cell it is written for: `test/compile/bank.ink` is a bank
of filters, `m.bank_smooth_1` to `_3`. The compiler refuses by name what it
cannot yet express, such as an instance with memory made anew at each step,
or a cell at an index the step computes, `T[k_n, 1]`, "a cell whose place is
not a constant";
without `-o` it writes nothing and lists every definition it would refuse,
and why. A limit of constants is folded, and any other is a
function that walks the terms with the interpreter's stopping rule, NaN where
the interpreter would say it does not converge: `test/compile/newton.ink`
solves each step of a stiff equation by Newton's method, a sequence with
parameters under `lim`, and `test/compile/logistic.ink` trains a logistic
regression, its `exp` a limit written in inkamath, as `test/compile/softmax.ink`
writes softmax and cross-entropy, `log` being Newton's method on `exp`. A
guard that reads a limit whose walk runs out does not hold, as interpreted,
whatever else it reads, so the action to take in doubt is the later clause:
`test/compile/fallback.ink` is an MPC whose plan falls back to a fixed gain
where projected gradient cycles. A
sequence with parameters read at a constant index, `f(x)_K`, is written out
as a sum with constant bounds is, each term below the one read a temporary of
the step: `test/compile/iterates.ink` takes a determinant by Faddeev-LeVerrier,
grad through it, and a few steps of projected gradient. One read at a
limit's index less a constant, `t(x)_k`, is walked beside it in its loop:
`test/compile/series.ink` writes e^x and a rotation's exponential as the
limits of their partial sums, as on paper, and `test/compile/sums.ink` as
unbounded sums, `sum_(q=0) x^q/!q`, each walked as such a limit is. A
limit of matrices fills an array, cell by cell: `test/compile/steady.ink`
finds a chain's steady state and, by power iteration, a matrix's dominant
direction at every step. A limit's iterate has temporaries as a step has, a
matrix inverse or a call nested in an argument computed once per iterate:
`test/compile/implicit.ink` takes each step of backward Euler on a stiff
chemical system by Newton's method under `lim`, its inverse written as the
paper writes it. A `grad` by a single value is compiled forward, each value
carrying its part beside it by the interpreter's rules, and one by a matrix in
reverse mode as the interpreter takes it, the body written once and swept
back, so that the step's gradient is the interpreter's to the bit where the
interpreter's values are doubles: `test/compile/grad.ink` trains a line and a
logistic regression by it, and `test/compile/cellguards.ink` a ReLU network
through its activation written by its cells, each cell's guards tested in the
step. A grad inside a grad is compiled forward over forward, each value
carrying a part by each, so a Hessian and a Jacobian by cells of a gradient
compile: `test/compile/hessian.ink` steps a double pendulum from its
Lagrangian. A nest three deep or holding a grad by several names, and a
derivative of a limit or of a matrix power, are refused for now. A `grad` by several names is a struct of its parts, as an instance
is, `m.g.W[0]` being `g.W_n`, the body written once and swept back once:
`test/compile/gradmany.ink` trains a two-layer network by one grad of its
four weights. A tensor is an array as C keeps one,
`double O[B][T][D]`, slice after slice, and is met slice by slice as the
interpreter meets it: `test/compile/tensor.ink` runs multi-head attention
over a batch and trains a layer on minibatches by `grad`, and
`test/compile/tensorgrad.ink` takes a `grad` with respect to a tensor; a
tensor in a limit is refused for now. A block literal,
`[A, 0; -C, 1]`, is laid out as the interpreter lays it out (section 2),
each block's cells written into their place: `test/compile/blocks.ink`
packs a doubling algorithm's iterates into one term, and refuses a block
that does not fill its band. Two matrices are compared whole, `R'*R == I3`
as the equalities of their cells: `test/compile/orthogonal.ink` has a
rational rotation the step takes out of SO(3) at its first step, which the
interpreter never does. A header that
writes NaN anywhere carries it to every term that reads it, as the
interpreter refuses them: a guard, a comparison and a power reading NaN
answer NaN, and a matrix or tensor term with a NaN cell is NaN in
every cell, which its first comment says. Such a header is not to be built
with `-ffinite-math-only`, which `-ffast-math` implies: GCC removes the tests
that carry NaN, and Clang warns of each NaN the header writes.
`test/compile/nan.ink` has an instance of each.
Clang before 23.1 can miscompile some headers' NaN tests at -O2 (llvm
925e284), and Clang 19 does on the tests; build with `-fno-slp-vectorize`.
Built at -O0, a header builds in seconds and runs about four times slower
than at -O2: a transformer step of 13 names builds in 14 s and takes 1.9 ms a
step, against 66 s and 0.45 ms at -O2, so -O0 pays for a short run. Loops
in the generated code cut the -O2 build by 40 per cent; what is left goes
mostly to what is still written cell by cell: the loads and stores between
the step's functions, the cells of layer norm, softmax, ReLU and the
residual sums, and the tests for NaN.

A sequence that reads another at `x_(2*m)` samples it, and is computed every
second step: its terms are the input's at another rate, and `y_(floor(n/2))`
holds its latest term at the input's rate again. The step is still one, the
input's, and a term of `y` is computed on the first step at which every sample
it reads exists; a hold of a term not computed yet is refused, saying to read
the term before it. `test/compile/rates.ink` is a decimator and two holds.
Such a sequence may be guarded, defined by its cells or without a base
clause; one read by another is refused, naming the hold at the input's rate
to sample instead. `test/compile/decimate.ink` adds an interpolator and a
strided convolution, and `test/compile/cascade.ink` a servo whose clamped
outer loop runs every fourth step.

`--check drift.ink calm -o calm.c` holds the compiled code to the
interpreter. It compiles `calm`, an instance the file defines, with the
parameters the instance gives, and writes a C program that steps it a hundred
times on the inputs the interpreter gives `calm`, replayed so that a difference
is the step's own, and compares each term with the interpreter's, from the
step that computes a sequence's first term, which it names where that is
after the first. Before it the interpreter must have no term either; from it,
a term the interpreter cannot give parts, unless the step's is NaN. Where every
term is within a billionth of one plus the interpreter's, it prints the largest
difference, and where a term below 1 was lost whole or of the other sign, 2^52
units of a double at the interpreter's term, its cell and step:
`gone.y: within 1e-200, 6.9e+15 units of a double at step 0`; otherwise the first term that parts and what the interpreter gives
there, and it exits with a failure. It also says whether the interpreter's
terms were exact throughout, and, where they were not, about how far they are
from the exact ones: each is asked three times more with every rounding taken
the other way at random and every limit moved by the remainder it estimated,
an estimate and not a bound, which names the first step it passes the
tolerance (`test/compile/estimate.ink`). `test/compile/drift.ink` has an instance that
holds and one that does not: a tenth computed again at every step, whose
rounding each step multiplies by ten. Before the values, it reports the first step
at which a compiled guard takes another clause than the interpreter's, and how
far that guard is from its threshold as the interpreter computes it, exactly
where both its sides are exact and in doubles where one is not: rounding
explains a flip at a margin near zero, and not one at a large margin. `brink`
in the same file sits exactly on its threshold, and `ledge` a trillionth from
it. Before that, it reports the first step at which a disturbed run takes
another clause than the interpreter, at a sequence's guard or a function's the
file writes, in the same words: there the estimate is a clause's, not a
rounding's, and the check does not fail for it (`test/compile/straddle.ink`,
where `crease`'s 0.30000000000000004 is 5.6e-17 from the double nearest its
threshold 3/10). After the values it lists, from the code and not the run,
every guard the step compiles that reads a limit and the clause tried where
that limit runs out, so that a reviewer can see which way each falls:
`braked.brake: where lim nv runs out, 'brake_n | lim nv(c_n) < 1 = 1' falls to
'brake_n = 0'` skips the brake where Newton fails (`test/compile/fallback.ink`).
A guard that reads an optional input is listed with it: `seen.x: where z is
absent, 'x_n | ...' falls to 'x_n = xp_n'`, after the limits if it reads
both, `where lim qp runs out or z is absent`; the check gives the step an
absent input as its flag 0 (`test/compile/optional.ink`).

`--steps n` beside an instance's `--check` steps it n times rather than a
hundred, n from 1 to 100000. n counts the step's calls, the one that computes the first
term among them, so a model of N steps after its initial term, `y_0` to
`y_N`, is checked whole with `--steps` N + 1 (`test/compile/steps.ink`).

`--float`, beside `--compile` or an instance's `--check`, writes floats where
the header writes doubles, every constant the nearest float, for a target that
computes in float; its first comment names what reads a constant folded from
inexact numbers, computed at the interpreter's precision, not a float's. Its check holds each term within a thousandth of one plus
the interpreter's and says the most any term came in units of a float, the
spacing of floats at the interpreter's term, a term of 0 counting none:
`test/compile/float.ink`. Where that most reaches 2^23 units, as a term a
normal float holds does when lost whole or of the other sign, it names the
cell and step: `outside.y: within 0.00061, 1e+10 units of a float in y[2,1]
at step 1101`. A limit
compiled in float also stops where a step is within twice `FLT_EPSILON` of its
term, so it may answer where the interpreter says it does not converge.

A header's functions are `static inline`, so a host that calls a shared
library, Python's say, needs a small C file that includes the header and
exports what the host calls (on Windows, each `__declspec(dllexport)`). For
`pid.ink`:

```c
#include <stddef.h>
#include "pid.h"

size_t ink_size(void) { return sizeof(pid); }
void ink_init(pid* m) { pid_init(m); }
void ink_update(pid* m) { pid_update(m); }
void ink_step(pid* m, double y) { pid_step(m, y); }
void ink_run(pid* m, const double* y, long n, double* u) {
    for (long k = 0; k < n; ++k) { pid_step(m, y[k]); u[k] = m->u[0]; }
}
/* An optional input z_n? takes its flag after it: track_step(m, z[k], ok[k]). */
const size_t ink_u = offsetof(pid, u), ink_kp = offsetof(pid, kp);
```

A field is read at its `offsetof`, so the host does not mirror the struct.
Built with `cc -O2 -shared -fPIC pid_host.c -o libpid.so`, Python's ctypes
drives it:

```python
import ctypes as C
lib = C.CDLL("./libpid.so")
lib.ink_size.restype = C.c_size_t
lib.ink_init.argtypes = [C.c_void_p]
lib.ink_run.argtypes = [C.c_void_p, C.c_void_p, C.c_long, C.c_void_p]
m = (C.c_double * ((lib.ink_size() + 7) // 8))()
u = C.c_double.from_buffer(m, C.c_size_t.in_dll(lib, "ink_u").value)
y, out = (C.c_double * 3)(0, 0.1, 0.2), (C.c_double * 3)()
lib.ink_init(m); lib.ink_run(m, y, 3, out)    # u.value is out[2]
```

An optional input's flag is one more `int`, a `C.c_int` in `argtypes`; a
driver that says absent by NaN converts in its own C, `track_step(m, z,
!isnan(z))`, where the choice is seen. Declare each function's `argtypes`:
without them an address held as a
Python int, as numpy's `a.ctypes.data` is, is passed as a C int and cut to
32 bits. A call from Python costs about 0.7 µs, twenty times a small
model's step, 31 to 34 ns for `test/compile/kalman.ink`: for such a model,
batch the steps in C, as `ink_run` does, 37 to 81 ns a step from Python.
`compile_host` builds this file as written here and holds `ink_run` to
`ink_step`.

inkamath solves no optimisation, but it writes what a solver is given: a
model's dynamics and their Jacobians by cells (section 4), compiled and held
to the interpreter by `--check`, are terms the host reads after each step,
`m.A[0]` and `m.B[0]`, to build the QP that OSQP or HPIPM solves for an MPC.
The solver, its tolerance and its failure stay the host's.

`--check session.ink`, given a transcript alone, replays it and shows each
answer that is not the one recorded, as recorded (`-`) and as given now (`+`),
under the line it answers. It is the check the tests make of
`test/data/*.ink`, for any transcript.

Interpreter behaviour is pinned by golden transcripts in `test/data/*.ink`,
which are literal sessions — every example in this file is one of them, so the
documentation and the tests check each other. Regenerate with
`cmake --build build --target record_goldens` and read the diff: it is the
record of what your change did.

`DESIGN.md` tracks the work in progress and the known defects.
`CLAUDE.md` has the working rules.

Language
--------

### 1. Numbers and operators

A literal is exact as written, and stays exact through `+`, `-`, `*`, `/` and
whole powers: `1/3+1/3+1/3` is `1`, and `0.1+0.2` is `0.3`. What can only be
approached — `pi`, `e`, a root that is not rational, a limit — is inexact, as
is anything written after `~`, and an inexact number makes inexact whatever it
touches: a comparison that reads one, since a truth read from a double may be
wrong, and the answer of a clause a guard reading one chose. An exact number
that outgrows a thousand digits becomes inexact rather than wrong, and an
answer that did ends in `# approximated past a thousand digits`, a comment, so
it still reads back; so does whatever such a number touches, a truth and a
clause chosen too. Dividing by an exact zero is an error.

Every number prints in decimal: an exact whole number in full, anything else
to nine significant digits, with `~` in front unless it is exact and what is
printed is all of it, so a bare number is exact and is what it shows. A
complex number has one, before its parts in parentheses. `frac` at the start of a line shows the answer as its exact
fraction, and `digits = n` sets how many digits are shown.

Numbers are complex; `i` is the imaginary unit, a name that a bound one (a
sum's index, a cell's row) shadows in its own scope and that nothing may
define again. A complex number is a pair of doubles, so whatever `i` touches
is inexact, even where it comes out real and whole. `e` and `pi` are the only
other built-in values, and `floor`, `re` and `im` the only built-in
functions, each cell by cell of a matrix: the largest whole number not above
its argument, exact of an exact number, and a complex number's real and
imaginary parts, the paper's Re and Im. `ceil(x) = -floor(-x)` and
`mod(a, b) = a - b*floor(a/b)` come with them, from a prelude (section 5),
and so do `exp`, `log`, `tanh`, `sin`, `cos`, `atan`, `atan2`, `asin`,
`acos`, `acosh`, `abs`, `max` and `min`, and, of matrices, `charpoly`,
`conv`, `polyval`, `roots`, `hurwitz`, `schurcohn`, `rho`, `abscissa`, `eig`, `chol`,
`smax`, `hinf` and `dhinf` (section 2). `abs` takes a complex number too, |z|, so a
frequency response's magnitude |T(e^{iw})| is written as on paper.
Any other rounding is a line of it, by the rule the model needs —
`round(x) = floor(x + 1/2)` — and, like `pi`, each of them can be defined
again, and given back by `clear`.

| | |
|---|---|
| `+expr` `-expr` | unary plus and minus |
| `!expr` | factorial, written as a prefix |
| `~expr` | the same value, inexact |
| `expr+expr` `expr-expr` | addition, subtraction |
| `expr*expr` `expr/expr` | multiplication, division |
| `expr^expr` | power; of a square matrix, a whole one, negative for the inverse |
| `expr<expr` `expr>expr` | comparison, answering 1 or 0, inexact where a side read is |
| `expr<=expr` `expr>=expr` | the same, or equal |
| `expr==expr` `expr<>expr` | equal, not equal, of numbers or whole matrices of one shape |
| `expr and expr` `expr or expr` | both, either: 1 or 0, inexact where a side read is, the right read only if needed |
| `name = expr` | definition (section 3) |
| `name \| cond = expr` | a definition in cases (section 3) |
| `clear name` | drop a definition (section 3) |
| `lim name` | the limit of a sequence (section 4) |
| `name.name` | a name of an instance or a file (section 5) |
| `use file` | read a file's definitions (section 5) |
| `?name` | print a definition back (section 6) |
| `tex ?name` | the same definition as LaTeX (section 6) |
| `frac expr` | the answer as an exact fraction |
| `digits` `digits = n` | the significant digits shown, 1 to 1000, 9 unless set |

`^` associates to the right, so `2^3^2` is `512`. `#` starts a comment and
runs to the end of the line.

```
>> 1/3+1/3+1/3
1

>> 10/4
2.5

>> 1/3
~0.333333333

>> frac 1/3
1/3

>> 2^(1/2)
~1.41421356

>> !5
120

>> 2+3*i
~(2+i*3)

>> (1+i)*(1-i)
~2

>> floor(-7/2)
-4

>> im(2+3*i)
~3

>> T(z) = 1/(z - 1/2)
T(z) = 1/(z - 1/2)

>> abs(T(e^(i*pi/2)))
~0.894427191
```

### 2. Matrices

`[a b; c d]` is a matrix literal: `,` or a space separates columns, `;`
separates rows. A short row is padded with zeros. A sign with a space before
it and none after it begins a column, so `[1 -1]` is two numbers and `[1 - 1]`
one; an argument list reads it alike.

```
>> [1 2;3 4]
[1, 2;
 3, 4]

>> [1 2 3;4 5 6]
[1, 2, 3;
 4, 5, 6]

>> [1; 2, 3]
[1, 0;
 2, 3]
```

The cells are expressions, and the literal expands to the size of what they
evaluate to. If `a` is the 2x2 matrix above, then `[a, a; a, a]` is 4x4:

```
>> [a, a; a, a]
[1, 2, 1, 2;
 3, 4, 3, 4;
 1, 2, 1, 2;
 3, 4, 3, 4]
```

Each row of blocks is a band as tall as its tallest block, each column of
blocks a band as wide as its widest, and a block's place is where its two
bands meet. A single value is stretched over its place, so `[a, 0]` borders
`a` with zeros; any other block must fill its place, and one that does not
is refused rather than continued by its corner:

```
>> [a, 0]
[1, 2, 0;
 3, 4, 0]

>> [a, [3 4]]
error: a block that does not fill its band
```

A matrix prints as the literal that would produce it, with its columns
aligned, so what is printed can be typed back. At the prompt a line with an
unclosed bracket is continued — `..` asks for the rest — which is what lets a
matrix be pasted back over the lines it printed on.

`m[i,j]` is the cell in row `i`, column `j`, counted from one as the rows and
columns are written:

```
>> a[2,1]
3

>> [1 2;3 4][2,1]
3

>> a[3,1]
error: row 3, column 1 is outside a 2x2 matrix
```

`m[i]` is row `i`, a matrix of one row, so a vector, which is a column as on
paper, gives its element, and a row vector is read through its transpose, as
a paper writes one. Brackets index only what they touch; a space before them
separates blocks in a literal, as everywhere in one.

```
>> a[2]
[3, 4]

>> [1; 4; 9][2]
4

>> [1 2 3]'[2]
2
```

`m'` is the transpose of `m`, its rows made columns. The quote belongs to what
it follows, subscript and cell brackets included, before any operator: `2*a'`
is `2*(a')`, and `x_(n-1)'` transposes the term `x_(n-1)`. It does not
conjugate a complex cell, as MATLAB's and Julia's quote does.

```
>> a'
[1, 3;
 2, 4]

>> a'*[1; 1]
[4;
 6]
```

A matrix can be defined by its cells, as a sequence is by its terms. On the
left the brackets name the row and the column and bound them, which is the
size; the right-hand side is any cell, and a guard says which cells a clause
gives. A clause for one cell, `M[1,2] = 5`, beats the others. A cell no clause
gives is 0, or, where the matrix was also written whole, that matrix's: after
`T = [1 2; 3 4]`, `T[1,1] = 9` changes that cell and nothing else. One index
on the left defines a column, `w[j<=3] = j^2`, whose clauses then name one
index each. The names are any, `i` among them, which shadows the imaginary
unit within the clause:

```
>> I[j<=2, k<=2] = j == k
I[j<=2, k<=2] = j == k

>> I
[1, 0;
 0, 1]

>> U[j<=3, k<=3] | j <= k = 1
U[j<=3, k<=3] | j <= k = 1

>> U
[1, 1, 1;
 0, 1, 1;
 0, 0, 1]
```

A size can come from a parameter: `H(n)[j<=n, k<=n] = 1/(j+k-1)` is the
Hilbert matrix of any size.

A bound left out is the size of the matrix the index reads, so a ReLU is one
line, of a matrix of any size:

```
>> rl(z)[i,j] = z[i,j]*(z[i,j] > 0)
rl(z)[i,j] = z[i,j]*(z[i,j] > 0)

>> rl([1 -2; 0 3])
[1, 0;
 0, 3]
```

Blocks are written as a paper writes them, by name, `[A, B; C, D]`, with a
comma between them. Brackets index whatever they touch, inside a literal as
anywhere, so a block must not touch the one before it: with a space or a
comma, `[a [5; 6]]` is `a` beside a column, while `[[1 2][1]]` is row 1 of
`[1 2]`, not two blocks.

```
>> [a, [5; 6]]
[1, 2, 5;
 3, 4, 6]

>> [[1 2][1]]
[1, 2]
```

`*` is matrix multiplication; `+`, `-` and `/` work cell by cell. A single
value stretches to the other side's size, and the order is kept:

```
>> a/2
[0.5, 1;
 1.5, 2]

>> 1-a
[ 0, -1;
 -2, -3]
```

A tensor of rank 3 is a stack of matrices of one size, its slices, along its
first index, where a paper puts the batch. A touching `;;` separates its
slices, one semicolon more than separates rows, after Julia's convention of
counting semicolons, and it prints so; a tensor of one slice keeps its `;;`,
as `[1 2;;]`, since it is not its slice. One index reads a slice and three a
cell, and a definition by three indices, `P[b<=2, j<=2, k<=2] = b*j*k`, is a
tensor. Whatever meets a tensor meets it slice by slice, a matrix or a single
value every slice, so `*` is a product batched over the first index and `'`
transposes each slice:

```
>> B = [1 2; 3 4;; 5 6; 7 8]
B = [1 2; 3 4;; 5 6; 7 8]

>> B[2]
[5, 6;
 7, 8]

>> B[2,1,2]
6

>> B*[1; 1]
[ 3;
  7;;
 11;
 15]
```

The prelude gives the characteristic polynomial and the stability tests.
`charpoly(A)` is det(lambda I - A) as a column, highest power first,
exactly. Of a polynomial so written, `hurwitz(p, s = 0)` is 1 where every
root has its real part below s, and `schurcohn(p, r = 1)` where every root
is inside |z| < r, both strictly. `rho(A)`, the spectral radius, and
`abscissa(A)`, the largest real part of an eigenvalue, bisect on those
tests: `rhob(A)_m` and `abscissab(A)_m` are the brackets [lo; hi] after m
halvings, lo at most the value and hi above it, and the answer is lo,
inexact, once the bracket is within 2^-53 of its end
nearer 0, or its ends are adjacent doubles, after 64 halvings and at most 256. It is certified while the tests
are exact; their numbers grow with the matrix and the halvings, and past a
thousand digits they are approximated and the answer is marked, within a
double's accuracy but no longer proved. An answer the 256th halving leaves
wider is marked too: one below about 2^-202 of the sum of |A[j,k]|, and
every 0, which no bracket is within 2^-53 of, so the tests cannot tell a
marginal system's abscissa from a small one. A nilpotent matrix's rho, its
polynomial lambda^n, is 0 and certified, and so is an abscissa whose
polynomial is lambda^k q, q's roots left of the axis, but not one with a
pair on it. Of a matrix of tenths, `rho` is
certified up to about 6x6 and `abscissa` 8x8. An inexact cell is read as
the exact rational its double is, as by `hinf` below, so the answer is
right for the data as stored, certified or marked as of an exact matrix,
and inexact, a bracket too, marked where a cell was approximated past a
thousand digits: of full doubles, `rho` is certified up to 6x6
and `abscissa` 8x8 as well. `grad` refuses both, a bisection being a
staircase:

```
>> charpoly([1 2; 3 4])
[ 1;
 -5;
 -2]

>> hurwitz(charpoly([0 1; -1 -1/5]))
1

>> rho([1/2 1; -1 1/2])
~1.11803399

>> abscissa([0 1; -1 -1/5])
~-0.1
```

`conv(p, q)` is the product of two polynomials and `polyval(p, x)` the
value of one at x, each a column, highest power first, as `charpoly`
writes it. A single value is a constant polynomial; a row is refused, not
read as MATLAB's. conv's length is the sum of the two less 1, a leading
zero kept, and polyval is Horner's rule, exact at an exact x. Of a square
matrix, polyval is the matrix polynomial p(A), its constant term times A^0,
so a matrix is a root of its characteristic polynomial; MATLAB's and
numpy's `polyval` work cell by cell instead, which is written by cells
here, `Y[j<=n, k<=m] = polyval(p, X[j,k])`. `grad` and the compiler take
both as any definition. A PI controller (2s + 1)/s around 1/((s + 1)(s + 2)) closes
the loop on s(s + 1)(s + 2) + 2s + 1:

```
>> conv(conv([1; 1], [1; 2]), [1; 0]) + [0; 0; 2; 1]
[1;
 3;
 4;
 1]

>> hurwitz(conv(conv([1; 1], [1; 2]), [1; 0]) + [0; 0; 2; 1])
1

>> polyval(charpoly([1 2; 3 4]), [1 2; 3 4])
[0, 0;
 0, 0]
```

`roots(p)` is the roots of a polynomial in the same column, as a column,
each as often as its multiplicity, by real part then imaginary part, each
ascending. It is Durand and Kerner's iteration, a `lim`, on the squarefree
part p/gcd(p, p'), so a repeated root converges as a simple one does.
Where that leaves only factors of degree 1, as of (s + 1)^2 or z^3, every
root is exact; otherwise every root is inexact, its place found by
comparing doubles. Equal roots are adjacent only where they come out equal
to the bit: (s^2 + 2s + 2)(s^2 + 2s + 5) gives -1 - i, -1 + i, -1 - 2i,
-1 + 2i, their real parts rounding apart. Of a real polynomial, a real
root is real and a pair exactly conjugate; of a complex one, a repeated
root is unreliable, its gcd being in doubles. Near roots cost digits, as
in any root finder in doubles: a cluster of k roots d apart loses about
(k - 1) log10(1/d) of a double's 16, so (s - 1)(s - 1.00001)(s - 1.00002)
gives 1, 1.00000789 and 1.00002073, and (s + 1)^2 - 10^-16 gives
-1.00000001 and -0.999999989 for -1 +- 10^-8; a near double root may print
as a complex pair, (s + 1)^2 - 10^-20 as -1 +- 8.18554879e-9i for -1 +-
10^-10. The iteration reaches a degree of about 30, and at 40 `lim` runs
out and refuses; so it does for roots spread over many decades, as of
(s + 10^-7)(s + 2*10^-7)(s + 10^7)(s + 2*10^7), of degree 4. A root on the imaginary axis keeps a real part of
rounding size: stability is `hurwitz`'s test, exact. `grad` refuses roots,
and so does the compiler, in the words of a definition it calls. The PI
loop above:

```
>> roots(conv(conv([1; 1], [1; 2]), [1; 0]) + [0; 0; 2; 1])
[~(-1.3411639-i*1.1615414);
 ~(-1.3411639+i*1.1615414);
             ~-0.317672196]

>> roots(conv([1; 1], [1; 1]))
[-1;
 -1]
```

`eig(A)` is the eigenvalues of a square matrix as a column, smallest first,
each as often as its multiplicity, and `smax(A)` the largest singular value
of any matrix, the root of the largest eigenvalue of A'A. Each eigenvalue
is bisected as `rho` is, on a count of those below the midpoint, certified
and marked as rho's: `eigb(A, k)_m` is the k-th one's bracket, and a 0 is
certified without one. The count holds only where every eigenvalue is
real, so `eig` refuses a matrix with a complex one rather than give its
real ones alone; a symmetric matrix has none, and any other is asked by
Hermite's test. An inexact matrix is read as `rho` reads it, so its
eigenvalues are those of the data as stored: a k-fold one of the matrix
meant may be split by the rounding of its cells, and `eig` gives the split
ones, each right. Hermite's test of doubles is exact too, and passes past
a thousand digits from about 8x8, as the symmetric test does not: `Q*D*Q'`
rounded, symmetric but for rounding, is answered up to 7x7 and refused
from 8x8, saying so, which `eig((A + A')/2)` asks as symmetric. An exact
matrix whose test passes a thousand digits may be refused, saying so, or
answered, marked. Of a symmetric matrix of tenths, `eig` is certified up
to about 36x36, and of full doubles up to 24x24 at least, in 200 s.
`grad` refuses both:

```
>> eig([2 1; 1 3])
[~1.38196601;
 ~3.61803399]

>> eig([0 1; -1 0])
error: eig needs a matrix whose eigenvalues are all real

>> smax([1 2; 3 4; 5 6])
~9.52551809
```

`eig(A, B)` is the generalized problem's: every lambda of A phi = lambda B
phi, a structure's frequencies K phi = omega^2 M phi or its buckling loads,
as a column, smallest first; `eig(A)` is `eig(A, A^0)`. B must be
symmetric positive definite, and a B indefinite, singular or not symmetric
is refused before A is looked at. The eigenvalues are B^-1 A's, bisected,
certified and marked as eig's; beside such a B a symmetric A has every one
real, without Hermite's test, and any other A is asked it of B^-1 A, as
`eig(B^-1*A)` is. So a cantilever's M^-1 K, refused past a thousand digits
from 12 unknowns, is certified as `eig(K, M)` at 24. Doubles in A and B are
read as the rationals they are, so the answer is right for the data as
stored, and inexact. A B of doubles symmetric only up to rounding, as
X'*D*X, is refused: write (B + B')/2. An A not symmetric then has B^-1 A
exact and larger than the rounded product, marked from about 5x5 and
refused from 6x6 where `eig(B^-1*A)` of the rounded product still answers:

```
>> eig([2 -1; -1 2], [2 1; 1 2])
[~0.333333333;
           ~3]

>> eig([2 1; 1 2], [1 0; 0 -1])
error: eig needs B symmetric positive definite
```

`chol(A)` is the Cholesky factor, L lower triangular with a positive
diagonal and L*L' = A, as a square-root filter writes `L = chol(P)`. It is
Cholesky's recurrence written in the prelude, each cell computed once, so
it is exact where every pivot is the square of a rational, and `grad` and
the compiler take it as any definition. chol refuses a matrix that is not
symmetric, as eig refuses such a B; compiled, where a covariance symmetric
exactly is not in doubles, it reads only the lower triangle of A. Of
complex entries it is L L' = A with ' the plain transpose, not the
Hermitian factor L L^H. A pivot not above 0, of a matrix indefinite or
singular, is refused:

```
>> chol([4 2 -2; 2 10 1/2; -2 1/2 9/4])
[ 2,   0, 0;
  1,   3, 0;
 -1, 0.5, 1]

>> chol([4 2; 2 3])
[2,           0;
 1, ~1.41421356]

>> chol([1 2; 2 1])
error: chol needs a positive definite matrix

>> chol([4 1; 2 3])
error: chol needs a symmetric matrix
```

`hinf(A, B, C, D)` is the H-infinity norm of the stable system x' = Ax +
Bu, y = Cx + Du: the largest singular value of its transfer function
C(iwI - A)^-1 B + D over every frequency w, the peak of |T(iw)| for one
input and one output, D left out being 0. `dhinf(A, B, C, D)` is the same
of x_(n+1) = Ax_n + Bu_n over the unit circle, so `dhinf(1/2, 1, 1)` is the
peak of section 1's T, at z = 1. Each bisects the norm's square, certified
as rho's, on whether the Hamiltonian has an eigenvalue on the imaginary
axis, which an exact count of real roots tells, and refuses a test whose
numbers would pass a thousand digits rather than read it from doubles, so
an answer is marked only where it leaves a double's range or a cell was
approximated past a thousand digits. The system is
divided by its largest cells first, exactly, so the units of its input,
output and time cost nothing, though not its state's: a 3x3 whose states
are scaled 10^15 apart, or 10^5 apart in doubles, is refused. An inexact
cell is read as the exact rational its double is, so the answer is
certified for the data as stored. Certified of small whole numbers up to
10x10 with one input and 9x9 with two inputs and two outputs, the largest
measured, and of full doubles up to 3x3, or 4x4 of one scale such as k/7;
refused past that, and where time scales are 10^95 apart or a mode B
cannot reach is 10^160 slower. The norm is of a stable A: `hinf` refuses an eigenvalue of A on or
right of the imaginary axis and `dhinf` one on or outside the unit circle,
even where it cancels in the transfer function. `grad` refuses both:

```
>> hinf([0 1; -1 -1/2], [0; 1], [1 0])
~2.06559112

>> dhinf(1/2, 1, 1)
~2

>> hinf(1, 1, 1)
error: hinf needs every eigenvalue of A left of the imaginary axis
```

### 3. Definitions

A definition binds a name and echoes what was written. It evaluates nothing —
neither side — so defining a series does not run it.

```
>> g=1+2
g=1+2

>> g
3
```

A definition may take parameters, supplied by position or by name:

```
>> f(x, y)=x^2+y
f(x, y)=x^2+y

>> f(2, 1)
5

>> f(x=3, y=2)
11
```

Parameters are lexically scoped. A name inside a definition resolves to that
definition's own parameters, and then to the names defined at the prompt — never
to the parameters of whatever call is in progress. Supplying the wrong number
of arguments is an error, not a search for something that might fit:

```
>> f(1)
error: f expects 2 arguments, got 1

>> x
error: x is not defined
```

A definition written *inside* an expression binds a **local**: it is visible
for the rest of the line — where it can see the parameters around it — and
gone on the next one. Its value is the value of the binding.

```
>> (t = 3) + t
6

>> t
error: t is not defined

>> sq(x) = (s = x+1) * s
sq(x) = (s = x+1) * s

>> sq(4)
25
```

A definition may be written **in cases**, one line each, by guarding a clause
with the condition it applies under — the `|` of set-builder notation, read
"such that". Clauses are tried in the order they were written and the first
whose guard holds is the one evaluated; the others are not:

```
>> abs(x) | x < 0 = 0-x
abs(x) | x < 0 = 0-x

>> abs(x) | x >= 0 = x
abs(x) | x >= 0 = x

>> abs(0-3)
3
```

A clause with no guard would answer every call, so it is the **default**: it
is tried after the guarded ones wherever it was written. That is what lets a
definition be patched up with the cases it turns out to need —

```
>> ramp(x) = x
ramp(x) = x

>> ramp(x) | x < 0 = 0
ramp(x) | x < 0 = 0

>> ramp(0-2)
0

>> ramp(2)
2
```

— and a clause for one index is not a default, so a guard added after one
could never apply and is refused rather than ignored:

```
>> step_0 = 1
step_0 = 1

>> step_0 | 1 = 7
error: step_0 is already defined without a guard, so this clause can never apply
```

Only a definition of one's own is patched up so: a clause on a built-in or a
name of the prelude starts a definition, and after `floor(x) | x > 10 = 0`,
`floor(2.5)` finds no clause.

A comparison is a number — `1` or `0` — so a guard is simply an expression
that is not zero, and `sgn(x) = (x>0) - (x<0)` needs no guard at all. Ordering
needs real numbers; equality does not. Neither takes NaN, which `0/~0` is, nor
does a guard: a comparison or a truth of NaN has no answer, and is refused.
Conditions combine with `and` and `or`, which answer `1` or `0` as a
comparison does, bind looser than it, and read their right side only when the
left has not decided — so a guard such as `n > 0 and s_(n-1) > 1` never asks
for `s_(-1)`:

```
>> inside(x) = 0 < x and x < 1
inside(x) = 0 < x and x < 1

>> inside(1/2)
1
```

If no clause applies, the interpreter says so rather than inventing a value:

```
>> abs(i)
error: a comparison needs real numbers, not ~(i)
```

A default written last is a paper's "otherwise", and means the same:

```
>> clamp(x) | x > 1 = 1
clamp(x) | x > 1 = 1

>> clamp(x) = x
clamp(x) = x

>> clamp(5)
1
```

A name has one definition, and a clause written again replaces itself where it
stands. A plain definition alone is replaced parameters and all, as nothing
else goes with it; beside other clauses, or in a sequence, a clause takes the
definition's parameters, defaults included, and one that cannot join is
refused. `clear` drops a whole definition, and the name is then what it is
beneath — a built-in, the prelude's, or nothing:

```
>> clamp(x, y) = x
error: clamp takes (x), so a clause cannot take (x, y); write 'clear clamp' first

>> clear clamp
clear clamp

>> clamp(x, y) = x
clamp(x, y) = x
```

A parameter may state its size by bounds, as a definition by cells does:
`v[j<=n]` is a column, `M[j<=n, k<=n]` a square matrix and `T[b<=p, j<=m,
k<=n]` a tensor, slices first. A bound that is a name is bound by the call to
the argument's size, a whole number, so the body can loop over it; a name
written twice is one size, and a single value is a 1x1 matrix. An argument of
another size is refused before the body is evaluated:

```
>> tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]
tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]

>> tr([1 2; 3 4])
5

>> tr([1 2 3; 4 5 6])
error: tr takes M[j<=n, k<=n], not a 2x3 matrix
```

A size is bound as a parameter is, so inside the definition it hides a name
defined at the prompt: after `n = 10`, `tr` still reads its argument's size.
A model's input reads the names in its bounds instead (section 5).

### 4. Sequences

A name indexed with `_` is a sequence. It is one definition with several
*clauses*: any number of base cases at constant indices, plus at most one
general clause indexed by an identifier. This is how a recurrence is written on
paper.

```
>> s_0=1
s_0=1

>> s_n=s_(n-1)/2
s_n=s_(n-1)/2

>> s_20
~9.53674316e-07
```

A recurrence may be written ahead of its index, as papers write
`x_(t+1) = f(x_t)`: the clause is read as its shift, the index read as one
step back wherever it appears, so `fib_(n+2) = fib_(n+1) + fib_n` says
`fib_n = fib_(n-1) + fib_(n-2)`. The index is a name plus a whole number
from 1; any other index that reads a name nothing defines is refused, and
`?` shows the clause as it was written:

```
>> fib_0 = 0
fib_0 = 0

>> fib_1 = 1
fib_1 = 1

>> fib_(n+2) = fib_(n+1) + fib_n
fib_(n+2) = fib_(n+1) + fib_n

>> fib_10
55

>> fib_(n-1) = 1
error: a clause is written at a whole number, as fib_0, at its index, as fib_n, or ahead of it, as fib_(n+1), not at fib_(n-1)

>> ?fib
fib_0 = 0
fib_1 = 1
fib_(n+2) = fib_(n+1) + fib_n
```

A base case wins over the general clause, whatever the order of definition.
Below the lowest base case the sequence is simply not defined — there is no
implicit value:

```
>> fact_n=fact_(n-1)*n
fact_n=fact_(n-1)*n

>> fact_5
error: evaluation nests more than 256 references deep

>> fact_0=1
fact_0=1

>> fact_5
120
```

A bare sequence name has no value, because there is no single term to give:

```
>> s
error: s is a sequence; index it (s_0) or take its limit (lim s)
```

`lim` iterates the general clause until two successive terms agree to within
1e-10 of their own size, every cell of a matrix against its own, and reports
non-convergence rather than handing back the term it stopped on:

```
>> lim s
~5.82076609e-11

>> u_n=2*n
u_n=2*n

>> lim u
error: u did not converge within 100 terms (last term 200, its steps in a ratio of 0.99); for a fixed count, read a term, as u_100
```

A limit of 0 has no size to agree within: a walk that does not within 100
terms answers the first term that agreed to within 1e-10 absolute below 1,
as `lim s` does, halving to 0, and so does one that meets a term refused or
NaN past that one. A limit far below 1 reached from far, `10^-30 + 2^-k`,
is such a walk within 100 terms, and so is every cell of a matrix beside a
cell walking to 0; there a read at a count known to suffice, `f(x)_K`, gives
the exact discrete value instead. So for a geometric integrator, where the
tolerance is a drift: solved by `lim` with the 1e-10 absolute below 1, the
implicit midpoint rule drifted a free rigid body's invariants by about 4e-13
a step, and a fixed count holds them.

`lim` stops when the last step is under the tolerance *and* the remainder the
steps imply is too. A series can converge too slowly to be summed this way —
after `n` terms of `1/n^2` the sum has moved by `1e-10` while it still has
`1e-5` to go — and then `lim` says so rather than answering:

```
>> w_1=1
w_1=1

>> w_n=w_(n-1)+1/n^2
w_n=w_(n-1)+1/n^2

>> lim w
error: w did not converge within 100 terms (last term ~1.63508193, its steps in a ratio of 0.98); for a fixed count, read a term, as w_101
```

That is a mathematical problem, not a limitation of `lim`, and the language is
enough to solve it. What remains after `n` terms is `1/n - 1/(2n^2) +
1/(6n^3) - ...`, and a clause may add it:

```
>> y_n=w_n+1/n-1/(2*n^2)+1/(6*n^3)-1/(30*n^5)
y_n=w_n+1/n-1/(2*n^2)+1/(6*n^3)-1/(30*n^5)

>> y_20
~1.64493407

>> pi^2/6
~1.64493407
```

Twenty corrected terms give every digit that is printed. An acceleration is a
sequence like any other, so which one to use stays the user's decision — it
is the mathematics, and no built-in choice would be right for every series.

A guard that reads a limit that does not converge does not hold, and the
clause after it is tried, so a solver's answer is taken where it passes the
paper's own test of it, a residual or a bound, and a fallback elsewhere.
The action to take in doubt is therefore the later clause:
`brake(x) | lim d(x) < margin = 1` before `brake(x) = 0` does not brake where
the walk runs out, and `brake(x) | lim d(x) >= margin = 0` before
`brake(x) = 1` does.
Newton's method on x^3 - 2x + 2 settles from -2, and from 0 cycles 0, 1, 0,
... without end:

```
>> F(x) = x^3 - 2*x + 2
F(x) = x^3 - 2*x + 2

>> nw(x)_0 = x
nw(x)_0 = x

>> nw(x)_k = nw(x)_(k-1) - F(nw(x)_(k-1))/(3*nw(x)_(k-1)^2 - 2)
nw(x)_k = nw(x)_(k-1) - F(nw(x)_(k-1))/(3*nw(x)_(k-1)^2 - 2)

>> root(x) | abs(F(lim nw(x))) < 1/10^6 = lim nw(x)
root(x) | abs(F(lim nw(x))) < 1/10^6 = lim nw(x)

>> root(x) = x
root(x) = x

>> root(-2)
~-1.76929235

>> root(0)
~0
```

An optional input that is absent makes a guard fall as well, and exactly
(section 5).

The fallback is inexact: whether a walk settles is read from its steps as
doubles. A guard that cannot be read whole does not hold, so `lim nw(x) < 0
or x < 1` does not hold at 0; a term the guard reads, and a limit's own
terms, refuse as they always did, and where no clause comes after, the
limit's refusal stands. A term that is NaN refuses the limit outright,
unless a term before it was kept, as above.

A sum or a product over an index is written as on paper, with `_` for the
index and its first value and `^` for its last. The body is a term — it runs
to the next `+` or `-` — and sees every name around it; the index is bound for
the body alone.

```
>> sum_(k=1)^10 k
55

>> prod_(k=1)^5 k
120

>> harm_n=sum_(k=1)^n 1/k
harm_n=sum_(k=1)^n 1/k

>> harm_10
~2.92896825

>> frac harm_10
7381/2520
```

Without an upper bound the series is summed to its limit, by the rule `lim`
follows and with its report, so a series needs no sequence at all. The second
of these is the series above, converging too slowly for a hundred terms to show
it:

```
>> sum_(k=0) 1/!k
~2.71828183

>> sum_(k=1) 1/k^2
error: the sum did not converge within 100 terms (last partial sum ~1.6349839)
```

A maximum or a minimum over an index is written as a sum is, `max_(k=1)^n`
and `min_(k=1)^n`, and so is the index of the first largest or smallest term,
`argmax_` and `argmin_`, as a classifier's prediction is read from its logits.
Of equal terms, the first: a tied target is counted as predicted only where it
comes first.

```
>> acc(Z[b<=m, j<=c], y) = sum_(b=1)^m (argmax_(v=1)^c Z[b,v] == y[b])/m
acc(Z[b<=m, j<=c], y) = sum_(b=1)^m (argmax_(v=1)^c Z[b,v] == y[b])/m

>> Z = [1/2 3 -1 3; 2 0 2 1; -1 -2 -3 -1/2]
Z = [1/2 3 -1 3; 2 0 2 1; -1 -2 -3 -1/2]

>> frac acc(Z, [2; 3; 4])
2/3

>> frac acc(Z, [2; 1; 4])
1
```

The derivative of an expression with respect to a name at a point is written
as a paper writes it there, and binds the name as a sum binds its index: the
point is read around it, the name only in the body, which is a term as a sum's
is. What the body reads through definitions is differentiated with it, exactly
where its parts are exact. A derivative at a parameter is the derivative as a
function, and a single value's gradient with respect to a matrix has the
matrix's shape and costs one evaluation and a sweep back, whatever its cells:

```
>> grad_(x = 2) x^3
12

>> cube(t) = t^3
cube(t) = t^3

>> dcube(t) = grad_(u = t) cube(u)
dcube(t) = grad_(u = t) cube(u)

>> frac dcube(1/2)
3/4

>> grad_(v = [1; 2]) v'*v
[2;
 4]
```

A gradient with respect to several names at once, the nabla's `∇_θ`, names
each at its point in the one subscript. It has a part for each name and no
value of its own. Where `grad` answers by each name alone, the others held
at their points, a part is what it gives by that name; where it refuses one
name, it refuses the whole. A definition holds it, read by its parts after a
point as an instance's names are, so a training step reads as a paper
writes it, `g_n = grad_(W = W_(n-1), b = b_(n-1)) L(W, b, x_n)` and `W_n =
W_(n-1) - eta*g.W_n`. At a real point it costs one evaluation and one sweep
back, however many names it has:

```
>> dxy = grad_(x = 2, y = 3) x^2*y
dxy = grad_(x = 2, y = 3) x^2*y

>> dxy.x
12

>> dxy.y
4
```

A definition in cases takes the slope of the clause that holds at the point,
a definition by cells each cell's, and a limit the limit of its terms'
slopes:

```
>> relu(v)[j<=2] = v[j]
relu(v)[j<=2] = v[j]

>> relu(v)[j<=2] | v[j] < 0 = 0
relu(v)[j<=2] | v[j] < 0 = 0

>> grad_(v = [3; -1]) [1 1]*relu(v)
[1;
 0]
```

Where a derivative does not exist or would mislead, `grad` says why rather
than answer: at a jump of `floor` or of a comparison, at a clause that holds
only at the point, for an exponent that changes with the name over any base
but `e`, for a Jacobian, for a body that does not read the name, and through a
definition that reads the global of that name, which the bound name does not
reach.

A Jacobian is written by its cells, row i the gradient of output i, as a
paper defines J_ij, and compiles so:

```
>> f(v) = [v[1]*v[2]; sin(v[1]) + v[2]^2; v[1]]
f(v) = [v[1]*v[2]; sin(v[1]) + v[2]^2; v[1]]

>> J(x)[i<=3, j<=2] = (grad_(v = x) f(v)[i])[j]
J(x)[i<=3, j<=2] = (grad_(v = x) f(v)[i])[j]

>> J([1/2; 3])
[           3, 0.5;
 ~0.877582562,  ~6;
            1,   0]
```

A derivative of a derivative is a grad inside a grad, and grad's order,
after the subscript as a sum's bound is, writes n of them by one name: a
whole number, 2 or more. With respect to a vector, order 2 is the Hessian,
its cell (i, j) the derivative by v[i] of the derivative by v[j] and (j, i)
the same number, so it is symmetric exactly; a Lagrangian's, at l = 1/2:

```
>> grad_(x = 1/2)^4 x^5
60

>> Lg(v, l) = (v[1] - 2)^4 + (v[1] - 2*v[2])^2 + l*(v[1]^2 - v[2])
Lg(v, l) = (v[1] - 2)^4 + (v[1] - 2*v[2])^2 + l*(v[1]^2 - v[2])

>> grad_(v = [3; 1])^2 Lg(v, 1/2)
[15, -4;
 -4,  8]
```

The Jacobian by cells of the gradient is the Hessian too, its cell (i, j)
by v[j] outside v[i], so on inexact values the two may part in the last
bit. A Hessian times a vector needs none of its cells: it is the derivative
in that direction, one pass by the vector's cells, as a pendulum's Coriolis
term C*w is `grad_(e = 0) grad_(v = w) L(q + e*w, v)`:

```
>> Lp(q, w) = 2*w[1]^2/2 + w[2]^2/2 + w[1]*w[2]*cos(q[1] - q[2]) + 20*cos(q[1]) + 10*cos(q[2])
Lp(q, w) = 2*w[1]^2/2 + w[2]^2/2 + w[1]*w[2]*cos(q[1] - q[2]) + 20*cos(q[1]) + 10*cos(q[2])

>> grad_(e = 0) grad_(v = [3/10; -7/10]) Lp([1; 1/2] + e*[3/10; -7/10], v)
[ ~0.335597877;
 ~-0.143827662]
```

A term can be defined by its cells, as a matrix is (section 2): the brackets
after the index name the row and the column and bound them, or one index a
column's, every cell sees the index, and a guard says which cells a clause
gives. A delay line keeps the newest sample on top and shifts the others
down:

```
>> tap_0[j<=3] = 0
tap_0[j<=3] = 0

>> tap_n[j<=3] = tap_(n-1)[j-1]
tap_n[j<=3] = tap_(n-1)[j-1]

>> tap_n[j<=3] | j == 1 = n^2
tap_n[j<=3] | j == 1 = n^2

>> tap_4
[16;
  9;
  4]
```

One cell, of every term (`tap_n[2] = 0`) or of one (`tap_2[2] = 0`),
overrides the rest, as `M[1,2] = 5` does. A clause wins where it is at least
as specific in both the index and the cell; a base term and a cell of every
term are each more specific in one, so where both give a cell the
interpreter asks which is meant rather than choosing.

An index must be an exact whole number, and `lim`, `sum`, `prod`, `grad`,
`frac`, `digits`, `tex`, `and` and `or` are reserved words.

A term is evaluated once per *context* — which definition, which index, which
argument values — and the answer is remembered until a definition changes. It
makes no difference to what an expression means; it makes the difference
between a chain of terms and a tree of them for a recurrence whose general
clause names itself twice, as a pair of mutually recursive sequences does.

### 5. Models and files

A model is defined as a function is, its value a group of definitions in
braces, one a line; at the prompt an open brace continues the line, as an open
bracket does. Its signature is its interface: a parameter has a default, and
an input has none. An instance is a definition, the model applied to
arguments, and its names are read after a point:

```
>> lowpass(a = 1/10, u_n) = {
..     v_0 = 0
..     v_n = a*u_n + (1-a)*v_(n-1)
.. }
lowpass(a = 1/10, u_n) = { ... }

>> fast = lowpass(a = 1/2, u_n = 1)
fast = lowpass(a = 1/2, u_n = 1)

>> slow = lowpass(a = 1/4, u_n = fast.v_n)
slow = lowpass(a = 1/4, u_n = fast.v_n)

>> frac slow.v_3
55/128
```

A model's body reads its parameters, its inputs and its own names, then those
of the scope it is written in: a file's, or, at the prompt, the session's, so
that a model there may use another, a function, or itself. An argument reads the
session's, where it is written, and follows them as any definition does. A
model used once needs no name, and an input left out says so when it is read:

```
>> frac lowpass(u_n = 1).v_1
1/10

>> lowpass().v_1
error: lowpass(...).u_1 is an input, and nothing defines it
```

A model that reads its input before the stream says what the input was there,
by clauses of its own on it, terms and guarded clauses. The argument comes
after them, as a definition's default comes after its other clauses, so a
clause that always applies is refused:

```
>> delay(x_n) = {
..     x_n | n < 0 = 0
..     c_n = x_(n-1)
.. }
delay(x_n) = { ... }

>> delay(x_n = n^2).c_0
0

>> delay(x_n = n^2).c_3
4
```

An input of more than one cell states its size in the signature, by bounds,
as a definition by cells does, and each of its terms is held to it where it is
read:

```
>> dot(x_n[j<=2]) = {
..     y_n = [1 2]*x_n
.. }
dot(x_n[j<=2]) = { ... }

>> dot(x_n = [n; 1]).y_3
5

>> dot(x_n = n).y_3
error: dot(...).x_3 is a single value, where dot takes a 2x1 matrix
```

A bound may be a name, which the model reads where it is defined, as it reads
any other: the model is compiled before any argument exists, so its input
binds no size, where a function's parameter does (section 3).

An input marked `?`, `z_n?` or `z_n[j<=2]?`, is optional: present on some
steps only. It is absent where its argument reads a term that is not there,
one no clause gives or one below a sequence's base clauses, where the
instance leaves it out, and before the stream unless a clause of its own
gives it there, where an input not marked reads its argument below 0. Read
through another definition, a term not there is that definition's refusal,
as ever. A guard that reads it where it is absent does not hold, and the
clause after is tried, exactly; here the last value seen is held:

```
>> kept(z_n?) = {
..     x_0 = 0
..     x_n | z_n >= 0 = z_n
..     x_n = x_(n-1)
.. }
kept(z_n?) = { ... }

>> every3_n | mod(n, 3) == 0 = n/3
every3_n | mod(n, 3) == 0 = n/3

>> [kept(z_n = every3_n).x_4, kept(z_n = every3_n).x_6, kept().x_6]
[1, 2, 0]

>> kept(z_n = every3_n).z_4
error: kept(...).z_4 is absent
```

What reads an optional input is checked where the model is defined, and
again where an instance is made, an inner model being sought then: each
read sits in a guard that an unguarded clause follows, or in the value of a
clause whose guard reads the same term wherever it holds, not on the right
of an `or` nor in a sum's body, or is the argument of an inner instance's
optional input. So no term of a model is ever absent, and what reads it
need not say what to do there:

```
>> bad(z_n?) = {
..     y_n | z_n > 0 = z_(n-1)
..     y_n = 0
.. }
error: y reads z_(n-1), an optional input of bad, in a value whose guard may hold without reading it
```

Outside the model, an absent term is refused, as above, and a guard reading
it does not hold. NaN is not absence: given NaN, the input is present, and a
comparison of it is refused, as for any input. An input with no range to
test, a setpoint or a count, is tested for presence by `z_n == z_n`, which
holds where it is present and refuses NaN as any comparison does.

Defining an instance evaluates nothing, so two that read each other's terms,
a controller and the plant it drives, are written in either order:

```
>> controller(kp = 2, r = 1, y_n) = { u_n = kp*(r - y_n) }
controller(kp = 2, r = 1, y_n) = { ... }

>> plant(dt = 1/2, u_n) = {
..     x_0 = 0
..     x_n = x_(n-1) + dt*(u_(n-1) - x_(n-1))
.. }
plant(dt = 1/2, u_n) = { ... }

>> ctl = controller(y_n = plt.x_n)
ctl = controller(y_n = plt.x_n)

>> plt = plant(u_n = ctl.u_n)
plt = plant(u_n = ctl.u_n)

>> frac plt.x_3
3/4
```

The plant reads the controller's output a step late, which is what lets the
loop start. Without such a delay each would read the other at the same index,
and the definition that closes the loop is refused, naming it.

An instance is changed where it is defined: `fast.a = 1` is an error, and
`fast = lowpass(a = 1, u_n = 1)` the way to say it. Redefined, a model
changes its instances, as a function redefined changes what calls it.

`use filters` reads `filters.ink`, beside the file that names it, into a scope
of its own: the session reaches its names qualified, `filters.lowpass`, and
`use filters (lowpass)` brings in unqualified those listed. A file is read
once, holds definitions only, and one that cannot be read or parsed loads
nothing and says where. The prelude that defines `ceil`, `mod`, `exp`, `log`,
`tanh`, `sin`, `cos`, `atan`, `atan2`, `asin`, `acos`, `acosh`, `abs`,
`max`, `min`, `charpoly`, `conv`, `polyval`, `roots`, `hurwitz`,
`schurcohn`, `rho`, `rhob`, `abscissa`, `abscissab`, `eig`, `eigb`, `chol`,
`smax`, `hinf` and `dhinf`, and what they call, is included bare beneath the
session, as the built-ins are: every scope sees it, and a session
that defines one of its names again does so for itself
alone, and clearing it gives the prelude's back. `exp`, `log`, `tanh`,
`sin` and `cos` are written in it, accurate to under 3 units in the last
place of a double, and `atan`, `atan2`, `asin` and `acos` to under 2.7 and
`acosh` to under 3.6, by the operations a compiled step performs:
`exp(x)` is 2^k e^r, r = x - k ln 2 and e^r a polynomial, `log` reduces by
`ilogb`, the power of two at or below its argument, and `sin` and `cos` by the
multiple of pi/2 nearest theirs. An exact argument is reduced exactly, save
within about 1e-31 of a multiple of pi/2 (C123), nothing they give is exact, and `log(0)` is refused in log's own name; so are `sin` and
`cos` past 2^20 either way, where the reduction would round, so a growing
phase such as `sin(w*t)` stops at 2^20. `atan` reduces by one of 0, 1/2, 1,
2 and infinity, `atan2(y, x)` is the angle of (x, y) in (-pi, pi], `asin` is
`atan2` of x and the root of (1 - x)(1 + x), `acos` of the root and x, and
`acosh` is log(x + root(x^2 - 1)), what the sum rounded added back. `asin` and `acos`
refuse past 1 either way, `acosh` below 1 and `atan2` the origin, each in its
own words; a double's -0 is 0, so `atan2(~0*(-1), -1)` is pi, where C's is
-pi. Compiled, each is a C function of the
header's own; the interpreter calls the same functions, and under `grad` their
parts, checked in as `include/inkamath/inkamath_prelude.h`, on a double, where
they answer what the definitions answer. Measured against mpmath, at worst
`exp` is 1.31 units off, `log` 2.94, `tanh` 2.97, `sin` 2.43, `cos` 2.45,
`atan` 1.44, `atan2` 1.89, `asin` 2.61, `acos` 2.61 and `acosh` 3.57, log's
own error near its fold (`test/prelude_test.cpp`).
`test/data/models.ink` is the whole of it.

Data comes in as a file of definitions like any other, written by whatever
holds it. From Python, a matrix is one literal, its decimals read exactly as
written:

```python
def ink(name, rows):
    cells = ";\n    ".join(", ".join(repr(float(v)) for v in row) for row in rows)
    return name + " = [" + cells + "]\n"

with open("data.ink", "w") as f:
    f.write(ink("X", rows))
```

and `use data` reads it, `data.X` the matrix. A line is bounded by how deep
it nests, not how long it is, so a literal of thousands of cells is one line
(DESIGN.md, C20).

### 6. Printing a definition back

`?name` shows what a name is bound to, as it was written, without evaluating
it.

```
>> ?b
b = a+a
```

`b` was defined at the top of this file, when `a` was `1`. Section 2 redefined
`a` as a matrix, so `b` is a matrix now — the binding is to the expression, not
to the number it once produced:

```
>> b
[2, 4;
 6, 8]
```

On a sequence `?` shows every clause, in the order they are tried, and
`?name_0` shows one of them:

```
>> ?s
s_0=1
s_n=s_(n-1)/2

>> ?s_0
s_0=1
```

`tex ?name` sets the definition as a paper would, in LaTeX, so that a
transcription can be read against the page it came from: the clauses for one
index a line each, and the others, guarded or not, as one definition in cases.

```
>> tex ?s
s_0 = 1
s_n = \frac{s_{n-1}}{2}
```

A definition by cells is set by its entry, with the range of its row and
column after it, and a model by its head, then its definitions a line each:

```
>> E[j<=2, k<=2] = j*k
E[j<=2, k<=2] = j*k

>> tex ?E
E_{j,k} = j\,k, \quad 1 \le j \le 2,\ 1 \le k \le 2
```

### 7. Diagnostics

An expression either produces a value or says why it cannot. Nothing evaluates
to zero by default.

```
>> undefined
error: undefined is not defined

>> 1+
error: unexpected end of input after '+'

>> [1 2)
error: missing ']' after '2'
```

Evaluation is bounded: a runaway recursion is reported rather than crashing the
process.

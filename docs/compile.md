Compiling to C
==============

`--compile` writes the sequences a file defines as a C header, for a filter to
run where the interpreter cannot; `--check` writes a C program that holds that
header to the interpreter. [README.md](../README.md#getting-started) walks
through both on a PID controller. This page is the reference.

Contents: [The header](#the-header) · [What compiles](#what-compiles) ·
[Refused for now](#refused-for-now) ·
[Building a header](#building-a-header) ·
[Rates and optional inputs](#rates-and-optional-inputs) ·
[The check](#the-check) · [In float](#in-float) ·
[Hosting from C and Python](#hosting-from-c-and-python).

The header
----------

`--compile model.ink -o model.h` writes the sequences a file defines as a C
header over doubles. `--compile mix.ink mix -o mix.h` compiles the model `mix`
that `mix.ink` defines ([section 5](language.md#5-models-and-files)), read as
`use` reads the file: its inputs are the step's arguments, in the order of its
signature, one whose size the signature states a pointer to its cells, row by
row, and its parameters, with their defaults, the only fields; a name nothing
defines is refused rather than taken for one more input.

The header holds, in order:

- **A first comment**, on how to build it
  ([Building a header](#building-a-header)).
- **"Using it"**: how to call it: the step's inputs, what each field holds
  after a step, and the parameters with their values. A parameter read where
  the step needs a constant, a size or an exponent is compiled in instead, as
  the header says.
- **The struct**, named after the header: the parameters and each sequence's
  latest terms, `m.name[k]` being `name_(n-k)`.
- **`update`**, which computes what derives from the parameters alone. `init`
  calls it, and the host calls it again after assigning a parameter. A
  default reading another parameter, `Kc = gain(1, R)`, is a value derived
  from it, recomputed by `update`, so a host that assigns it directly has it
  overwritten.
- **`init`**, which sets the parameters to their values.
- **`step`**, which advances to the next index, the first 0, and computes its
  terms.

[`pid.ink`](../test/compile/pid.ink) is a PID controller, and
[`expected/pid.h`](../test/compile/expected/pid.h) is what it compiles to.

A matrix product of 27 multiplications or more, and a term whose cells are each
written alike and call no function, are written as loops, every value the same,
bit for bit, as written out cell by cell (a NaN's bits aside), unless the C
compiler contracts a multiply and an add (`-ffp-contract`).

A step too long for a compiler to optimise as one function is written as
several, and what one computes and a later one reads is held in the model
struct, its last member: about 300 KB for one training a transformer's 13
weights at once, over a batch of four. A host places that struct statically or
on the heap, not as a local of a thread with a small stack; `sizeof` the struct
counts it.

A header that writes NaN anywhere carries it to every term that reads it, as
the interpreter refuses them: a guard, a comparison and a power reading NaN
answer NaN, and a matrix or tensor term with a NaN cell is NaN in every cell,
which its first comment says. [`nan.ink`](../test/compile/nan.ink) has an
instance of each.

What compiles
-------------

| What | How it compiles | Example |
|---|---|---|
| Matrices | — | [`kalman.ink`](../test/compile/kalman.ink), a Kalman filter |
| Guards | — | [`pid_clamped.ink`](../test/compile/pid_clamped.ink) limits its output |
| A sum | — | [`fir.ink`](../test/compile/fir.ink), a filter written as a sum |
| A sum or a product up to the index, or the index less a constant, `sum_(t=1)^n e_t` | kept as a running total, a sequence of the step's own, `s0_`, as a recurrence written by hand would be | [`accum.ink`](../test/compile/accum.ink) has a regret |
| A matrix inverse | computed at every step | [`kalman2.ink`](../test/compile/kalman2.ink) |
| A matrix inverse of the parameters alone | computed once, in `update` | [`heat.ink`](../test/compile/heat.ink) |
| `floor`, `and`, `or` | — | [`adc.ink`](../test/compile/adc.ink) rounds and tests |
| Terms defined by cells | — | [`net.ink`](../test/compile/net.ink), a small network |
| A named model, `--compile mix.ink mix` | its inputs the step's arguments and its parameters the only fields ([The header](#the-header)) | [`mix.ink`](../test/compile/mix.ink) |
| A read of an input before the stream | a term the model's history gives, which `init` folds into the input's window | [`decimate.ink`](../test/compile/decimate.ink): `down` states `x_n \| n < 0 = 0` |
| A file's named instances | one step that orders all their terms together, each instance a struct of its own: `m.ctl.u[0]` and `m.plt.x[0]` | [`loop.ink`](../test/compile/loop.ink) closes a loop |
| An instance in an instance | — | [`chain.ink`](../test/compile/chain.ink) nests one |
| A function, and an instance of a model without memory | where they are called, a constant folding through and a recursion unrolling where its guards fold | [`kernel.ink`](../test/compile/kernel.ink) has a convolution run at every step |
| A local | a temporary of the step, read from its binding on in its clause as interpreted | [`locals.ink`](../test/compile/locals.ink) takes Runge-Kutta's stages as locals, each computed once |
| An instance with memory written where it is read | one of its own, named after where it is written and the cell it is written for | [`bank.ink`](../test/compile/bank.ink), a bank of filters, `m.bank_smooth_1` to `_3` |
| A limit of constants | folded | — |
| Any other limit | a function that walks the terms with the interpreter's stopping rule, NaN where the interpreter would say it does not converge | [`newton.ink`](../test/compile/newton.ink) solves each step of a stiff equation by Newton's method, a sequence with parameters under `lim` |
| A function written as a limit | — | [`logistic.ink`](../test/compile/logistic.ink) trains a logistic regression, its `exp` a limit written in inkamath |
| A limit inside a limit | — | [`softmax.ink`](../test/compile/softmax.ink) writes softmax and cross-entropy, `log` being Newton's method on `exp` |
| A guard that reads a limit | where the limit's walk runs out, it does not hold, as interpreted, whatever else it reads, so the action to take in doubt is the later clause | [`fallback.ink`](../test/compile/fallback.ink), an MPC whose plan falls back to a fixed gain where projected gradient cycles |
| A sequence with parameters read at a constant index, `f(x)_K` | written out as a sum with constant bounds is, each term below the one read a temporary of the step | [`iterates.ink`](../test/compile/iterates.ink) takes a determinant by Faddeev-LeVerrier, grad through it, and a few steps of projected gradient |
| A sequence with parameters read at a limit's index less a constant, `t(x)_k` | walked beside the limit in its loop | [`series.ink`](../test/compile/series.ink) writes e^x and a rotation's exponential as the limits of their partial sums, as on paper |
| A sum without an upper bound, `sum_(q=0) x^q/!q` | walked as such a limit is | [`sums.ink`](../test/compile/sums.ink) |
| A limit of matrices | fills an array, cell by cell | [`steady.ink`](../test/compile/steady.ink) finds a chain's steady state and, by power iteration, a matrix's dominant direction at every step |
| A limit's iterate | has temporaries as a step has, a matrix inverse or a call nested in an argument computed once per iterate | [`implicit.ink`](../test/compile/implicit.ink) takes each step of backward Euler on a stiff chemical system by Newton's method under `lim`, its inverse written as the paper writes it |
| `grad` by a single value, and by a matrix | by a value forward, each value carrying its part beside it by the interpreter's rules; by a matrix in reverse mode as the interpreter takes it, the body written once and swept back; so the step's gradient is the interpreter's to the bit where the interpreter's values are doubles | [`grad.ink`](../test/compile/grad.ink) trains a line and a logistic regression |
| `grad` through an activation written by its cells | each cell's guards tested in the step | [`cellguards.ink`](../test/compile/cellguards.ink), a ReLU network |
| A grad inside a grad | forward over forward, each value carrying a part by each, so a Hessian and a Jacobian by cells of a gradient compile | [`hessian.ink`](../test/compile/hessian.ink) steps a double pendulum from its Lagrangian |
| A `grad` by several names | a struct of its parts, as an instance is, `m.g.W[0]` being `g.W_n`, the body written once and swept back once | [`gradmany.ink`](../test/compile/gradmany.ink) trains a two-layer network by one grad of its four weights |
| A tensor | an array as C keeps one, `double O[B][T][D]`, slice after slice, met slice by slice as the interpreter meets it | [`tensor.ink`](../test/compile/tensor.ink) runs multi-head attention over a batch and trains a layer on minibatches by `grad` |
| A `grad` with respect to a tensor | — | [`tensorgrad.ink`](../test/compile/tensorgrad.ink) |
| A block literal, `[A, 0; -C, 1]` | laid out as the interpreter lays it out ([section 2](language.md#2-matrices)), each block's cells written into their place | [`blocks.ink`](../test/compile/blocks.ink) packs a doubling algorithm's iterates into one term |
| Two matrices compared whole, `R'*R == I3` | as the equalities of their cells | [`orthogonal.ink`](../test/compile/orthogonal.ink) has a rational rotation the step takes out of SO(3) at its first step, which the interpreter never does |
| A sequence read at `x_(2*m)`, or held at `y_(floor(n/2))` | at another rate ([Rates](#rates-and-optional-inputs)) | [`rates.ink`](../test/compile/rates.ink) |
| An optional input, `z_n?` | followed in the step by its flag ([Optional inputs](#optional-inputs)) | [`optional.ink`](../test/compile/optional.ink) |

Refused for now
---------------

The compiler refuses by name what it cannot yet express; without `-o` it
writes nothing and lists every definition it would refuse, and why, or says
that none is, `pid.ink compiles`. Refused:

- In a sum or a product up to the index: a term that reads the index itself,
  as Adam's `b2^(t-i)` does, and a last index other than the index less a
  constant; and, for now, a maximum up to it.
- In a named model, a name nothing defines, rather than taken for one more
  input.
- A read of an input before the stream that the model's history does not give.
- An instance with memory made anew at each step.
- A cell at an index the step computes, `T[k_n, 1]`: "a cell whose place is
  not a constant".
- For now, a local by cells, one bound on the right of `and` or `or` and read
  after it, one bound in a term's index, and one a sum without an upper bound
  carries past a term.
- For now, a nest of grads three deep or holding a grad by several names, and
  a derivative of a limit or of a matrix power.
- For now, a tensor in a limit.
- A block that does not fill its band
  ([`blocks.ink`](../test/compile/blocks.ink)).
- At another rate: a hold of a term not computed yet, saying to read the term
  before it; and a sampled sequence read by another, naming the hold at the
  input's rate to sample instead.
- For now, an argument of an inner instance's optional input that reads a
  sequence's chain back in time, or holds it.

Building a header
-----------------

The header is C99, warning-free under `-std=c99 -Wpedantic` with GCC and
Clang, and C++ as well; it includes `<math.h>` and `<string.h>`, and
`<float.h>` where a limit is compiled, and allocates nothing: the struct holds
all the step keeps.

| Flag | Use |
|---|---|
| `-ffast-math` | Not to be used, as the header's first comment says: the arithmetic is in the order the definitions give it, and it reorders. |
| `-ffinite-math-only`, which `-ffast-math` implies | Not for a header that writes NaN: GCC removes the tests that carry NaN, and Clang warns of each NaN the header writes. |
| `-ffp-contract` | Where the compiler fuses a multiply and an add, it rounds once where the definitions round twice: often closer to exact, but the step then depends on how it is built, so build what `--check` writes the way the step itself is built. Loops are bit for bit with cells written out only without it. |
| `-fno-slp-vectorize` | Under Clang before 23.1: it can miscompile some headers' NaN tests at -O2 (llvm 925e284), and Clang 19 does on the tests. |
| `-O0` | Builds faster than at -O2 and runs slower. |

Loops in the generated code cut the -O2 build by 40 per cent; what is left
goes mostly to what is still written cell by cell: the loads and stores between
the step's functions, the cells of layer norm, softmax, ReLU and the residual
sums, and the tests for NaN.

README's figures are those of a compiler that does not fuse, as GCC and Clang
on x86-64 do not by default. Where they fuse, on AArch64 or with `-mfma` or
`-march=native` on x86-64, the last digits move: with `-mfma`, GCC 13 and
Clang 18 give the getting started's `ctl.s` and `ctl.u` within 7.8e-16, and
its float check 4.7 and 5.2 units. `-ffp-contract=off` gives README's again.

Rates and optional inputs
-------------------------

### Rates

A sequence that reads another at `x_(2*m)` samples it, and is computed every
second step: its terms are the input's at another rate, and `y_(floor(n/2))`
holds its latest term at the input's rate again. The step is still one, the
input's, and a term of `y` is computed on the first step at which every sample
it reads exists; a hold of a term not computed yet is refused, saying to read
the term before it. [`rates.ink`](../test/compile/rates.ink) is a decimator and
two holds.

Such a sequence may be guarded, defined by its cells or without a base clause;
one read by another is refused, naming the hold at the input's rate to sample
instead. [`decimate.ink`](../test/compile/decimate.ink) adds an interpolator
and a strided convolution, and [`cascade.ink`](../test/compile/cascade.ink) a
servo whose clamped outer loop runs every fourth step.

### Optional inputs

An optional input, `z_n?`, is followed in the step by its flag, `int
z_present_in_`, 0 where it is absent, its value then not read; the struct keeps
the flags beside the input's terms, `z_present[k]`, and the input's cells are
NaN where it is absent, before the stream too. A guard whose truth is NaN
where an input it reads is absent does not hold, as interpreted:
[`optional.ink`](../test/compile/optional.ink).

An inner instance's optional input takes its argument's flag:

| Argument | Flag |
|---|---|
| an optional input | its flag, forwarded |
| a sequence | 1 where a clause of its chain gave the term, NaN included, and 0 where none did |
| any other | 1 |
| left out by its instance | 0 at every step |

[`inner.ink`](../test/compile/inner.ink). An argument reading such a chain back
in time, or holding it, is refused, for now.

The check
---------

`--check drift.ink calm -o calm.c` holds the compiled code to the interpreter.
It compiles `calm`, an instance the file defines, with the parameters the
instance gives, and writes a C program that steps it a hundred times on the
inputs the interpreter gives `calm`, replayed so that a difference is the
step's own, and compares each term with the interpreter's.

- **From** the step that computes a sequence's first term, which it names
  where that is after the first. Before it the interpreter must have no term
  either; from it, a term the interpreter cannot give parts, unless the step's
  is NaN.
- **Holds** where every term is within a billionth of one plus the
  interpreter's. Otherwise it prints the first term that parts and what the
  interpreter gives there, and exits with a failure.

[`drift.ink`](../test/compile/drift.ink) has an instance that holds and one
that does not: a tenth computed again at every step, whose rounding each step
multiplies by ten.

### The report

A report reads, in order:

1. **What was run**: the instance, its steps, `in float` under `--float`, and
   whether the interpreter's terms were exact throughout: `against exact
   values`, or `against exact values until 6 and inexact ones from there`.
2. **A disturbed run's other clause**, if any: the first step at which a
   disturbed run takes another clause than the interpreter, at a sequence's
   guard or a function's the file writes, in the words of 3. There the
   estimate is a clause's, not a rounding's, and the check does not fail for
   it ([`straddle.ink`](../test/compile/straddle.ink)).
3. **A compiled guard's other clause**, if any: the first step at which a
   compiled guard takes another clause than the interpreter's, and how far
   that guard is from its threshold as the interpreter computes it, exactly
   where both its sides are exact and in doubles where one is not: rounding
   explains a flip at a margin near zero, and not one at a large margin.
   `brink` in `drift.ink` sits exactly on its threshold, and `ledge` a
   trillionth from it.
4. **Each term**: the largest difference, `within 1.8e-15`; where a term below
   1 was lost whole or of the other sign, 2^52 units of a double at the
   interpreter's term, its cell and step,
   `gone.y: within 1e-200, 6.9e+15 units of a double at step 0`; or the first
   term that parts. Where the interpreter's terms were not exact, about how far
   they are from the exact ones: each is asked three times more with every
   rounding taken the other way at random and every limit moved by the
   remainder it estimated, an estimate and not a bound, which names the first
   step it passes the tolerance
   ([`estimate.ink`](../test/compile/estimate.ink)).
5. **Guards in doubt**, from the code and not the run: every guard the step
   compiles that reads a limit, and the clause tried where that limit runs out,
   so that a reviewer can see which way each falls
   ([`fallback.ink`](../test/compile/fallback.ink)). A guard that reads an
   optional input is listed with it, after the limits if it reads both,
   `where lim qp runs out or z is absent`; the check gives the step an absent
   input as its flag 0 ([`optional.ink`](../test/compile/optional.ink)).

```
crease: 100 steps from 0, against exact values until 0 and inexact ones from there
crease.r: at 1 a disturbed run takes 'r_n = 0' and the interpreter 'r_n | s_n > c = s_n - c'; the guard of the second is 5.6e-17 from its threshold
crease.s: within 0; the interpreter's terms about 1.1e-16 from the exact ones
crease.r: within 0; the interpreter's terms about 1.1e-16 from the exact ones
```

`crease`'s 0.30000000000000004 is 5.6e-17 from the double nearest its
threshold 3/10.

```
brink: 100 steps from 0, against exact values
brink.g: at 1 the compiled step takes 'g_n | d_n < c - w = 1' and the interpreter 'g_n = 0'; the guard of the first is exactly on its threshold
brink.d: 0.099999997532837726 at 9, where the interpreter gives 0.10000000000000001
brink.g: 1 at 1, where the interpreter gives 0
```

```
braked: 100 steps from 0, against exact values until 0 and inexact ones from there
braked.brake: within 0
braked.careful: within 0
braked.wheels: within 0
braked.brake: where lim nv runs out, 'brake_n | lim nv(c_n) < 1 = 1' falls to 'brake_n = 0'
braked.careful: where lim nv runs out, 'careful_n | lim nv(c_n) >= 1 = 0' falls to 'careful_n = 1'
braked.wheels: where lim nv runs out, 'wheels_n[j<=2] | lim nv(j*c_n/2) < 1 = 1' falls to 'wheels_n[j<=2] = 0'
```

`braked.brake` skips the brake where Newton fails. From `optional.ink`:

    seen.x: where z is absent, 'x_n | (z_n - xp_n)^2 <= gate^2 = xp_n + a*(z_n - xp_n)' falls to 'x_n = xp_n'

### Steps

`--steps n` beside an instance's `--check` steps it n times rather than a
hundred, n from 1 to 100000. n counts the step's calls, the one that computes
the first term among them, so a model of N steps after its initial term, `y_0`
to `y_N`, is checked whole with `--steps` N + 1
([`steps.ink`](../test/compile/steps.ink)).

Given a transcript alone, `--check` replays it instead
([README.md](../README.md#building)).

In float
--------

`--float`, beside `--compile` or an instance's `--check`, writes floats where
the header writes doubles, every constant the nearest float, for a target that
computes in float; its first comment names what reads a constant folded from
inexact numbers, computed at the interpreter's precision, not a float's.

Its check holds each term within a thousandth of one plus the interpreter's
and says the most any term came in units of a float, the spacing of floats at
the interpreter's term, a term of 0 counting none:
[`float.ink`](../test/compile/float.ink). Where that most reaches 2^23 units,
as a term a normal float holds does when lost whole or of the other sign, it
names the cell and step:
`outside.y: within 0.00061, 1e+10 units of a float in y[2,1] at step 1101`
(`outside` in [`robertson.ink`](../test/compile/robertson.ink), with
`--steps 2000`).

A limit compiled in float also stops where a step is within twice
`FLT_EPSILON` of its term, so it may answer where the interpreter says it does
not converge.

Hosting from C and Python
-------------------------

A host in C includes the header, as README's
[getting started](../README.md#getting-started) does. A header's functions are
`static inline`, so a host that calls a shared library, Python's say, needs a
small C file that includes the header and exports what the host calls (on
Windows, each `__declspec(dllexport)`). For the getting started's `pid.h`:

```c
/* pid_host.c: what a shared library of pid.h exports. */
#include <stddef.h>
#include "pid.h"

size_t ink_size(void) { return sizeof(pid); }
void ink_init(pid* m) { pid_init(m); }
void ink_update(pid* m) { pid_update(m); }
void ink_step(pid* m, double y) { pid_step(m, y); }
void ink_run(pid* m, const double* y, long n, double* u) {
    for (long k = 0; k < n; ++k) { pid_step(m, y[k]); u[k] = m->u[0]; }
}
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

- **An optional input.** The step of a model `track` with an optional input
  `z_n?` takes the input's flag after it, `track_step(m, z, ok)`: one more
  `int`, a `C.c_int` in `argtypes`. A driver that says absent by NaN converts
  in its own C, `track_step(m, z, !isnan(z))`, where the choice is seen.
- **`argtypes`.** Declare each function's: without them an address held as a
  Python int, as numpy's `a.ctypes.data` is, is passed as a C int and cut to
  32 bits.
- **Cost.** A call from Python costs about 0.7 µs, twenty times a small
  model's step, 31 to 34 ns for [`kalman.ink`](../test/compile/kalman.ink): for
  such a model, batch the steps in C, as `ink_run` does, 37 to 81 ns a step
  from Python.

inkamath solves no optimisation, but it writes what a solver is given: a
model's dynamics and their Jacobians by cells
([section 4](language.md#4-sequences)), compiled and held to the interpreter by
`--check`, are terms the host reads after each step, `m.A[0]` and `m.B[0]`, to
build the QP that OSQP or HPIPM solves for an MPC. The solver, its tolerance
and its failure stay the host's.

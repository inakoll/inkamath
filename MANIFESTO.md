# Manifesto

This file is not a roadmap. Nothing in it is scheduled, and nothing in it is
promised. It records where inkamath could go and why, with the technical
solutions that look worth exploring, so that a later decision starts from an
argument instead of from nothing. `DESIGN.md` remains the record of what
was done and measured; when a direction here is taken up, it becomes a phase
there, specified as transcripts first, and the hint written here is what it is
measured against, not what it is bound to.

## What inkamath is for

A language in which an equation is written as it is on paper, means exactly what
it says, and can be followed down to the loops a machine runs.

Three commitments follow, and every direction below is judged against them:

- **The notation is the paper's.** A recurrence is `x_n = f(x_(n-1))`, a sum is
  `sum_(k=1)^n`, a matrix is defined by its cells, a function by its cases. When
  the language and a paper disagree, the paper is the default and the language
  must justify the difference.
- **The meaning is exact.** The interpreter computes with exact numbers wherever
  the mathematics allows, refuses rather than guesses, and is therefore a
  reference semantics, not merely an implementation.
- **The machine is visible.** Generated code is meant to be read: it is the
  bridge from the equation to the hardware, and someone learning the mathematics
  should be able to see what their definition became.

No domain is the centre. Control and signal processing are where the language
started, machine learning is where most papers are written today, and both are
tests of the same thing: **how much of a paper transcribes faithfully**, and
whether what it transcribes into can be interpreted exactly and compiled into
reasonably efficient code. Robotics, where rigid bodies, rotations and filters
on manifolds meet both, is the third. The limit of what is within reach
is not written in advance. It is found by transcribing, and each place the
language has to grow is argued from an equation that needed it. Whoever reads
the result, a student learning the mathematics or an engineer checking a paper
or a backward pass, is served by the same three commitments. The large
frameworks do not aim at that: their value is in hiding the machine.

## What it is not for

**Not a race on speed.** Generated code should be reasonable, and nothing here
is a reason to make it slow, but a compiler that tries to beat vendor libraries
has lost before it starts. Tensor Comprehensions is the precedent (see
*Lineage*).

**Not continuous time.** No solver for differential equations, no plant
modelling. A model that needs one is discretised by hand, as `plant` in
`docs/language.md` section 5 is, and what the language offers is to say that
discretisation exactly.

**Not a computer algebra system.** The reasons are in `DESIGN.md`,
*Deferred*, under symbolic simplification, and nothing here weakens them.

**Not a framework.** No model zoo, no data pipeline, no training
infrastructure. A model is a file of definitions.

**Not yet a library.** A compiled header is hosted from Python already, by a
C file of a few lines and ctypes (`docs/compile.md`, *Hosting from C and
Python*), with no API of inkamath's. A C API, and bindings over it, would let
the oracle's harness run interactively. They are not the end, and they freeze
an interface; they wait until the language has stopped moving.

Taken: a header builds as a shared library under a macro, `-DPID_SHARED`,
exporting its functions and its struct's size, member names and offsets
(`DESIGN.md`, *A header built as a shared library*). Its symbols are not a
stable interface until the language stops moving.

Taken: `tools/inkamath.py` hosts the interpreter and a compiled model from
Python, a process a call and a library read from its tables, `--names` the
binary's one addition (`DESIGN.md`, *Hosting from Python*). It is no more
stable than they are.

The size constraint at the top of `DESIGN.md` governs everything below.
Several of these directions could each double the program. Where one does, the
version worth having is the one that adds a capability without adding a second
way to say something already sayable.

## The oracle

The idea that ties the directions together: **an exact interpreter during
design, and every piece of generated code tested against it.** Not "tested
until it agrees bit for bit", which no floating-point target can do, but
checked for non-divergence, with the difference measured and explained.

What that asks for, and what it should be careful about:

- **Values diverge continuously.** The comparison is against an error bound,
  not against equality, and what matters is how the difference grows with the
  step index. A stable filter keeps it bounded; an unstable one shows it
  growing, which is a finding about the model as much as about the code.
- **Decisions diverge discretely.** A guard whose condition sits exactly on its
  threshold in rationals can fall on the other side in floating point. The
  generated code then takes a different clause or activates on a different
  tick, and the outputs part structurally. Such a divergence should be reported
  as its own event, the first one in time, and the interpreter can compute what
  floating point cannot: the guard's *margin*, its exact distance from the
  threshold. A flip at a near-zero margin is explained by rounding; a flip at a
  large margin is a bug. A model full of near-zero margins is fragile, and
  worth knowing about before anything is compiled.
- **Exactness has a horizon.** Every step of a rational recurrence compounds
  denominators, and the thousand-digit limit arrives. A transcendental makes
  the reference inexact immediately. The oracle is exact for the first steps
  and for the models that stay rational, and should say which it is giving.
- **Parallel targets reorder.** A parallel reduction adds in a different order,
  atomics make that order vary between runs, and compilers contract into fused
  multiply-adds. Agreement between two floating-point targets is not a goal; a
  bound against the exact reference is. A deterministic-reduction mode, a tree
  in a fixed order, is worth having for debugging.

Hint: the harness belongs outside the compiler, and needs nothing the project
does not already have. `test/compile` pairs each model with a C harness that
replays inputs and checks the step against values computed exactly; the oracle
generalises that, generating the harness from a transcript rather than writing
it by hand, and running it under CTest. Each new target becomes testable the
day it exists, which is the condition for having a second one at all.

Taken: `--check file instance -o check.c` writes that harness from an
instance, run under CTest (`DESIGN.md`, *The first check*). It steps the
compiled code a hundred times, or `--steps n`, on the inputs the interpreter
gives the instance, and holds every term within a billionth of one plus the
interpreter's: a tolerance, not the bound asked above. It says whether the
interpreter's terms were exact throughout, and where they were not estimates
how far, by three runs more with each rounding an error-free transformation
detects taken the other way at random and each limit moved by the remainder
it estimated: an estimate, not a bound. It names the cell and step where a
term below 1 is lost whole or of the other sign, 2^52 units of a double, and
reports the first step at which a compiled or a disturbed guard takes another
clause, with that guard's distance from its threshold. It lists every
compiled guard that reads a limit or an optional input, with the clause tried
where the walk runs out or the input is absent (`docs/compile.md`, *The
check*).

## Directions

### Number types for targets

The compiler writes doubles, and floats under `--float`. For most of what
follows, double is the *least* interesting target: the oracle earns its keep
where precision is what is at risk.

- **float32**, because it is what CPUs vectorise widely and what GPUs are fast
  at, and because large coordinates and long sums go wrong in it in ways worth
  showing.
- **Fixed point**, for small processors, FPGAs and anything that is specified
  bit-exactly. The scaling of each quantity becomes part of the model, and the
  exact interpreter is the obvious way to check that a chosen scaling never
  overflows over a test input.
- **int8 and other quantised types**, for neural network inference, where
  quantisation error is exactly the comparison the oracle makes.

The interpreter's side is settled: phase 13 made the kind of a number part of
the number, so a `Number` is exact or not and says which. What is open is the
target's side. A parameter or a sequence would be annotated with the type its
compiled form should have, while the interpreter keeps computing exactly beside
it, and the question is what carries that annotation.

Taken for float32, the whole compilation's: `--float` writes the step in
floats and checks it in units of a float, naming the cell and step where
they reach 2^23, as a term a normal float holds lost whole or of the other
sign does; the header names what reads a constant folded at the
interpreter's precision (`docs/compile.md`, *In float*). Fixed point, int8
and a precision per definition are open.

### Time: several rates, and events

A control cascade, a sensor fusion filter and a resampling chain each run
parts of themselves at different rates. The language has one implicit index,
`n`, and the cheapest way to have several is to make rate relations index
arithmetic rather than a new kind of thing:

```
pos_m = kp*(r - enc.x_(10*m))           # one fast sample in ten
cur_n = kc*(pos.u_(floor(n/10)) - y_n)  # held between slow ticks
```

Reading at `10*m` is sampling; reading at `floor(n/10)` is holding the last
value. The compiler's new task would be to recognise index expressions of the
form `a*n+b` and `floor((n-b)/a)`, infer a period for each sequence, and refuse
a read whose index is neither, which is the clock calculus of synchronous
languages reduced to divisibility of integers.

Taken for periodic rates: samples `x_(a*m + b)` and holds `y_(floor((n -
b)/a) - d)` in one step at the input's rate (`docs/compile.md`, *Rates*;
`test/compile/rates.ink`, `decimate.ink`, `cascade.ink`). Irregular arrival
is an optional input, `z_n?`, at the fast rate, interpreted and compiled: a
static check refuses a read of it outside a guard with a clause after it, and
the step takes its flag, `z_present_in_` (`test/data/optinputs.ink`;
`test/compile/optional.ink`, `inner.ink`). Event clocks and a step per rate
are open.

Aperiodic events do not have closed forms, but the same two maps generalise.
A clock is a monotone correspondence between two indices: `t_m`, the base
instant of the m-th activation, and `count_n`, how many activations have
happened by `n`. A periodic clock has both in closed form; an event clock,
activated where a condition holds, has them computed. Then Lustre's three
constructs fall onto what the language already has: `when` is reading at
`t_m`, `current` is reading at `count_n`, and `merge` is a definition in cases
with complementary guards. A sketch, not a syntax:

```
gps = gpsfilter(z_m = raw.z_(t_m)) on valid_n
```

where the model's own index counts activations, so a recurrence inside it
advances only when it runs. Lustre's `current` had a known hole: no value
before the first activation. The rule this language already has, that there
is no implicit value below the lowest base case, closes it: `count_n` is 0
before the first activation, so the model's term at 0 must be defined.

For code generation, two shapes, and the choice matters:

- **One step**, with a counter, computing a slow term only on its ticks.
  Deterministic, and the existing causality check needs no change.
- **One step per rate**, for a rate-monotonic scheduler. A slow rate reading a
  fast one within the same tick is then a race, and the language should
  require a delay there, `pos.u_(floor(n/10)-1)`, refusing the version without
  it by name, as it already refuses a loop without a delay.

### A solver within a step

Not for differential equations: for the implicit equations a discretised model
produces. Backward Euler on a stiff model, an actuator that saturates with
feedback around it, a constraint that has to hold at each step, an initial
condition found by asking for a steady state.

The interpreter can already say this, because `lim` over an inner sequence is
a fixed-point or Newton iteration:

```
nw(xp, u)_0 = xp
nw(xp, u)_k = nw(xp, u)_(k-1) - F(nw(xp, u)_(k-1), xp, u)/dF(nw(xp, u)_(k-1), xp, u)
x_n = lim nw(x_(n-1), u_n)
```

Compiled, it is a bounded loop with the same stopping rule, and whether
the step converged is a guard: one that reads a limit that runs out does not
hold, so the clause after it is the fallback, and a status the host wants is a
sequence the model writes with the solver's own test, `solved_n | abs(x_n +
lim qp(x_n)) <= abs(x_n) = 1`, `solved_n = 0`. A fixed number of iterations,
for code whose execution time has to be bounded, is the term at a constant
index, `nw(xp, u)_5`, written out (`test/compile/iterates.ink`).
The derivative is written by hand or taken by `grad`.

Taken: the walk compiled with the interpreter's stopping rule
(`test/compile/newton.ink`, and backward Euler on a stiff chemical system in
`implicit.ink`), and the fallback (`fallback.ink`), which `--check` lists for
every such guard.

### Tensors

The largest change, and the one most papers will force first. Machine learning
is written in rank 3 and rank 4: batch, channel, height, width, heads. Tensors
stop at rank 3, matrices stacked by `;;` along the first index, interpreted,
differentiated and compiled (`docs/language.md` section 2;
`test/compile/tensor.ink`, `tensorgrad.ink`), and a column is read by one
index. The way forward should be **indexed tensors of any rank**, defined by
their cells exactly as matrices are now, and not matrices of matrices.

What the language already has is the right foundation. A definition by cells
with its bounds, `W[j<=n, k<=m]`, is what Dex calls an index set, so shapes are
known from the definition without annotations a paper never writes. Inferring
them rather than declaring them is how to have Dex's shape safety without its
ceremony.

Three notational decisions have to be made out loud before anything else:

- **How a function meets a tensor.** Papers write `Wx` for a product, `⊙` for
  the cellwise product, and `sigma(z)` meaning sigma applied to each cell. Here
  `*` is a matrix product and `/` is cellwise, so a user's `f(x) = x^2` applied
  to a matrix squares the matrix, and `sigma(W*x + b)` quietly means something
  other than what the paper means. The language already has a way to say the
  paper's meaning without a new rule: define by cells,
  `A[j<=n, k<=m] = sigma(Z[j,k])`, which is also how a careful paper defines it.
  That should be the default. An implicit map over cells changes what existing
  sessions mean, and needs a type the language does not have to tell a
  function written over numbers from one written over matrices; it is worth
  reconsidering only if the cell form proves too heavy on real transcriptions.
  Attention will be the test. Taken: the cell form, a bound left out read
  from the matrix its index reads, so a ReLU is one line of any size
  (`docs/language.md` section 2); attention's softmax is written so
  (`test/compile/attention.ink`).
- **Slices.** `a[1]` as a whole row, which would make a reduction natural
  rather than a recurrence over cells, and give chained indexing a reason to
  exist. At rank 3 they stop being a convenience, since a head, a batch
  element and a channel are each a slice. Taken: one index reads a tensor's
  slice, so `B[2][1,2]` chains (`docs/language.md` section 2).
- **Subscripts.** Papers use one notation for time and for components, `x_t`
  and `W_jk`. This language separates them, `_` and `[,]`. The separation is
  defensible and should probably stay, but it is the first thing a student will
  trip over, and the documentation should say why it is there.

### Differentiation

Training is gradient descent, and gradient descent is a recurrence:
`w_k = w_(k-1) - eta*g_k`. Momentum and the moments of Adam are more sequences.
Sums over a batch exist, and so does the gradient.

Hints, in the order they cost:

- **Forward mode as a number type.** A dual number, `a + b*eps` with
  `eps^2 = 0`, carries a derivative alongside a value through every operation.
  A dual over `Number` gives exact derivatives without touching the language,
  though perhaps not without touching the templates: C44 and C65 are what a new
  class number type has met before. Guards differentiate piecewise, which is
  exactly right for a ReLU, and `floor` and comparisons have zero derivative.
  The cost is one pass per parameter, or a vector of duals: fine for tens or
  hundreds of weights.
- **A gradient construct.** Something like `grad(L, w)`, the first construct to
  take a definition rather than a value. It is the one piece of paper notation,
  the nabla, that every training equation uses.
- **Reverse mode, deferred while the bottleneck is elsewhere.** Wanted:
  forward mode's cost grows with the number of parameters, and no target size
  caps what the language should handle. It waits until gradients are what
  limits the models being written. In the interpreter, a tape recorded during
  evaluation; in the compiler, an
  adjoint step emitted by transformation. Through time, the history it needs
  conflicts with a step that keeps only its latest terms; a fixed window of
  truncation is a lag, and lags are already compiled in as sizes.

Taken: `grad_(w = point) L`, an expression by a name at a point, or by
several names at once, read by its parts, `g.W` (`docs/language.md` section
4); its order, `grad_(x = a)^n`, to 8 interpreted and 2 compiled, a vector's
Hessian symmetric exactly; a derivative in a direction by a grad inside a grad;
a Jacobian by its cells. Forward mode as parts carried beside each value, not a
dual type; reverse mode for a matrix or several names, a tape in the
interpreter and an adjoint step compiled (`DESIGN.md`, *Differentiation*,
*Reverse mode in the interpreter*, *The compiler's adjoint step*;
`test/data/reverse.ink`, `test/compile/adjoint.ink`, `gradmany.ink`,
`hessian.ink`). Open: an order past 2 compiled, and a compiled derivative of
a limit or of a matrix power, which are refused.

The objection that sank symbolic simplification, that the identities are
unsound over matrices whose extents are unknown before evaluation, does not
apply to forward mode: it is mechanical per node and happens during
evaluation. It does apply to the compiler's adjoint, which is a transformation
and needs extents. The compiler has them, because its shapes are static; that
is why reverse mode belongs there and not in the interpreter's tree.

A use that needs no training at all: exact gradients are a better check for a
backward pass, written by hand or produced by a framework, than the finite
differences normally used, whose accuracy is limited by the very rounding they
are trying to rule out.

### Data

Compiled code already receives inputs as the arguments of its step, so a
stream of samples is an input sequence indexed by sample and a minibatch is
index arithmetic over it. The interpreter reads such a sequence from a file
of definitions, a matrix literal written by whatever holds the samples and
read by `use` (`docs/language.md` section 5), which keeps files holding
definitions only; anything more, a stream or a sensor, would be the
language's first real input from outside, and deserves its own decision.

A deterministic shuffle needs no new construct: a linear congruential
generator is a recurrence over `mod`, which the prelude has. Its terms are
exact interpreted, and compiled only while its products stay within the
target's integers, 2^53 in double and 2^24 in float: glibc's constants part at
the second step in double, Park and Miller's hold (`test/compile/lcg.ink`).

### Functions a paper takes for granted

Softmax, cross-entropy, sigmoid and tanh need `exp` and `log`. Embedded
deployments approximate them anyway, by polynomial or by pieces, which the
language can express. Taken: a prelude written in inkamath, no built-in.
`exp`, `log`, `tanh`, `sin`, `cos`, `atan`, `atan2`, `asin`, `acos` and
`acosh` are written by the operations a compiled step performs, within 3.6
units in the last place of a double, and compiled as the header's own C
functions. With them come `ceil`, `mod`, `abs`, `max` and `min`, and of
matrices `charpoly`, `conv`, `polyval`, `roots`, `hurwitz`, `schurcohn`, `rho`
and `abscissa` with their brackets `rhob` and `abscissab`, `eig` with `eigb`,
`chol`, `smax`, `hinf` and `dhinf` (`docs/language.md` sections 1, 2 and 5;
`DESIGN.md`, *`exp`, `log` and `tanh` accurate, bounded and compiled*).

### One compiler, several printers

The generated code is written from the definitions directly. Before a
second target, an intermediate form is worth having, small and of the
language's own making: loops marked as **parallel** (over cells, over
instances), **reductions** (sums, products, gradients over a batch) or
**sequential** (recurrences), each value with its extent. A target is then a
printer of that form, and most targets turn out to be modes of one printer.

Taken at its smallest: a product, the sweep and same-shaped terms are loops,
each written by the operation that knows its kind, parallel over cells or a
reduction (`DESIGN.md`, *Loops in generated code*), and `--float` is a mode of
the one printer. A tree of loops is not written: for C it would remove less
than it adds. Increments 4 and 5, a definition by cells as one loop body and
the reverse sweep through it, wait for a model that needs them.

None of these is written until an entry of the conformance suite needs it: a
printer is justified by a model that cannot be served without it, and arrives
as a mode of the one form, not as a second compiler.

- **C**, the reference printer.
- **C with OpenMP**: the same loops, with `parallel for` and `reduction` where
  the form says so. Threads on the hardware already targeted, and the cheapest
  way to find out whether the parallel marks are right.
- **C with SIMD**: `#pragma omp simd` over instances, with structure-of-arrays
  layout and `restrict`. ISPC is the one CPU target that would justify a
  printer of its own; it is unlikely to be needed.
- **C for high-level synthesis** (Vitis HLS and its kind): C with pipelining and
  partitioning pragmas, natural for fixed shapes and fixed point, and a place
  where the oracle is unusually valuable because simulating hardware is slow.
- **CUDA**, the first GPU printer, with HIP almost for free, since it is close
  to a renaming of the same API.
- **GLSL compute, compiled to SPIR-V by the host**, for Vulkan: Intel, mobile
  and embedded GPUs, and no vendor runtime. WGSL for WebGPU would be its sibling.
- **Structured Text** (IEC 61131-3), only if programmable logic controllers ever
  matter. It fits the control-loop core better than any GPU.

Two things are worth deciding before any of this is written:

- **Where the parallelism comes from.** A step of a control loop is smaller
  than the cost of launching a GPU kernel. The width a GPU needs comes from
  cells, from reductions, and above all from **instances**: a bank of filters,
  a thousand tracks each with its own Kalman filter, an ensemble, a sweep of
  parameters, one training sample per thread. The step loop stays on the host,
  and each step is spread across that width. The bank of filters the compiler
  already makes, one instance per cell, is the shape to grow from.
- **Layout.** One struct per instance is the natural C and the wrong GPU
  layout. It stays: its fields' offsets are the host's contract, read by
  ctypes (`docs/compile.md`), and structures of arrays across the instance
  dimension are a GPU printer's to decide. A step split into functions keeps
  what its parts share as the struct's last member, instances stepped in
  turn sharing a union, so the offsets hold (`DESIGN.md`, *A split step's
  scratch in the model struct*).

And one thing to keep: the compiler emits source, and the host's toolchain
builds it. That is how inkamath stays free of a C compiler today, and it is how
it can stay free of a GPU toolkit.

### Reading

Two features that would do more for a student than any target:

- **Definitions rendered as typeset mathematics.** `?name` already prints a
  definition as it was written. Printing it as LaTeX closes the loop with the
  paper: the source is typed in plain text and read as the equation it
  transcribes, and a transcription mistake becomes a visible difference from
  the page. This gives what Fortress wanted, source that looks like
  mathematics, without paying for it in the parser. Taken: `tex ?name`, of
  clauses, cells, grads and models (`docs/language.md` section 6;
  `test/data/tex.ink`, `grad.ink`).
- **Numerical lessons the oracle can teach.** Softmax written naively
  overflows in float32 where the exact result is a perfectly good number, and
  the log-sum-exp form stays within bound of it. A variance computed by the
  textbook formula cancels; a gradient through a deep chain of tanh vanishes.
  A student who sees their own transcription diverge from exact arithmetic,
  and then sees the textbook remedy converge, learns what a framework hides
  behind a library function.

## A paper conformance suite

The method `DESIGN.md` uses for the language itself carries over:
specify by transcripts before implementing. The specification should be a set
of canonical equations, transcribed as closely to the page as the notation
allows, drawn from more than one field because no field is the centre.
Each entry below but the perceptron now has a model in `test/compile`, held
to the interpreter by `--check`, or for `net`, `pid_clamped`, `kalman`, `adc`
and `bank`, which came before it, by a harness written by hand.

From machine learning:

- a perceptron and a multi-layer forward pass;
- softmax, and cross-entropy over it;
- backpropagation written out by hand, beside the same gradient from the
  gradient construct;
- gradient descent, then with momentum, then Adam;
- a recurrent cell, `h_t = tanh(W*h_(t-1) + U*x_t + b)`;
- a two-dimensional convolution;
- scaled dot-product attention, per head and per batch.

From control and signal processing:

- a PID with anti-windup (`pid_clamped.ink`);
- a Kalman filter (`kalman.ink`), the bridge between the two lists;
- a quantiser (`adc.ink`) and a filter bank (`bank.ink`);
- an LQR gain by its Riccati recurrence;
- a decimator and an interpolator, the first test of several rates;
- a Newton step inside a step, the first test of a compiled `lim`.

Every place a transcription has to depart from the paper is a design decision
made visible. Attention will force the rank question on its first line.

## Lineage

What came before, what each was for, and what this file takes or avoids:

- **APL** (Iverson, 1962) began as a notation for describing algorithms and
  became an array language for analysts; its descendants live in finance. Its
  lesson is that notation shapes thought, and its warning is that an invented
  alphabet loses the people who already read another one.
- **Lustre and SCADE** gave the synchronous semantics: streams, delays, clocks,
  static causality, and a qualified code generator in safety-critical
  industries. The models, instances and clocks above are theirs in this
  language's notation.
- **Simulink** gave model-in-the-loop then software-in-the-loop: the same model
  simulated and compiled, and the two compared. The oracle is that discipline
  with an exact reference instead of a floating-point one.
- **Faust** (GRAME, 2002–) compiles a functional language for audio signal
  processing to readable C++, and has worked on several rates. It is the
  nearest precedent for the rates and the filter banks above; its notation is
  the block diagram's, not the paper's.
- **Fortress** (Sun, 2006–2012) wanted scientific code that rendered as
  mathematics, and paid for it in the complexity of its parser and type system.
  Rendering to LaTeX takes the benefit without the cost; refusing
  juxtaposition, which this language already does, avoids the main trap.
- **Julia** (2012–) made code generic over its number type idiomatic: one
  function runs on `Rational{BigInt}`, on `Float32` and on ForwardDiff's dual
  numbers. It is the oracle in practice, assembled by a user. Its lesson is
  that the number type should be orthogonal to the code; what it does not
  offer is the paper's notation, time as an index, or an exact semantics the
  language commits to.
- **Halide** separated an algorithm from the schedule that runs it. This
  language has the algorithm half; the intermediate form above is where a
  schedule would attach, if one were ever wanted.
- **Tensor Comprehensions** (Facebook, 2018) compiled index notation to GPU
  kernels for researchers, and lost to vendor libraries on speed. The warning
  that justifies the first non-goal.
- **TACO** (MIT, 2017) compiles index notation over sparse formats to C and
  CUDA. Sparse tensors are not a direction here, but its separation of the
  notation from the storage format is the model to follow if they ever become
  one.
- **Dex** (Google Research, 2019–) made index sets into types, so that shape
  errors are type errors, and built in differentiation. Its lesson is that
  shape safety is what catches most mistakes; its warning is that rigour can
  make a language correct and forbidding at once. Inferring index sets from
  cell bounds is the attempt to have the first without the second.

None of them made an exact semantics the reference its generated code is
tested against, none of them took the paper's notation as the default to
depart from, and none of them was written for someone learning the
mathematics. That combination, together with time as a first-class index
rather than one more axis of an array, is what this language would add.

## Open questions

Recorded so that they are asked before they are answered by accident.

- Is a target's kind of number, fixed point or int8, a property of a
  definition, of a sequence, or of the compilation, as `--float` makes
  float32?
- Is a clock a property of an instance, of a sequence, or of an index?
- Where does data come from beyond files of definitions, a stream or a
  sensor?
- Do the loops' writers serve OpenMP, SIMD and CUDA as modes, as they serve
  C, once increments 4 and 5 make a definition by cells and its sweep one
  loop each, or does a second target need a tree of loops, and would it
  remove more than it adds?
- Is a definition the host implements, a QP solver called from the step,
  named when compiling and held by `--check` to the `.ink` definition it
  replaces?
- How much of a paper can be transcribed before the language has to grow, and
  is the answer to that a good measure of whether it should?
- Is an index a kind of number of its own, an input refused where it is not
  whole, and is its base 1 at every boundary with a host?
- Where do frames belong: in a name, as a paper's superscript, in something
  the check enforces, or with the reader?
- What would carry the epistemic level, exact, certified or estimated and
  what demoted it, and what is the smallest machine-readable format to show
  it? Carried before a C API or such a format reads it, it is an abstraction
  with nothing to serve (`CLAUDE.md` §2), so only the question is recorded.

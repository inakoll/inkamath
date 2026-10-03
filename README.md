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
`exp` needs neither a sequence nor `lim`: it is an ordinary function.

```
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
and the parameters with their values. A parameter that
gives a size, a bound or a lag is compiled in instead, as the header says.
`test/compile/pid.ink` is a PID controller, and `test/compile/expected/pid.h`
is what it compiles to; `test/compile/kalman.ink` is a Kalman filter over
matrices.
`test/compile/pid_clamped.ink` limits its output with guards, and
`test/compile/fir.ink` is a filter written as a sum; `test/compile/kalman2.ink`
inverts a matrix at every step, and `test/compile/heat.ink` once, in `update`;
`test/compile/adc.ink` rounds with `floor` and tests with `and` and `or`, and
`test/compile/net.ink` is a small network whose terms are defined by cells.
`--compile mix.ink mix -o mix.h` compiles the model `mix` that `mix.ink`
defines (section 5), read as `use` reads the file: its inputs are the step's
arguments, in the order of its signature, and its parameters, with their
defaults, the only fields; a name nothing defines is refused rather than
taken for one more input. A file's named instances are compiled with it, into
one step that orders all their terms together, each instance a struct of its
own: `test/compile/loop.ink` closes a loop, `m.ctl.u[0]` and `m.plt.x[0]`,
and `test/compile/chain.ink` nests one. A function, and an instance of a model
without memory, are compiled where they are called, a constant folding
through and a recursion unrolling where its guards fold:
`test/compile/kernel.ink` has a convolution run at every step. An instance
with memory written where it is read is one of its own, named after where it
is written and the cell it is written for: `test/compile/bank.ink` is a bank
of filters, `m.bank_smooth_1` to `_3`. The compiler refuses by name what it
cannot yet express, such as infinite series and an instance with memory made
anew at each step; without `-o` it writes nothing and lists every definition
it would refuse, and why. A limit of constants is folded, and any other is a
function that walks the terms with the interpreter's stopping rule, NaN where
the interpreter would say it does not converge: `test/compile/newton.ink`
solves each step of a stiff equation by Newton's method, a sequence with
parameters under `lim`, and `test/compile/logistic.ink` trains a logistic
regression, its `exp` a limit written in inkamath, as `test/compile/softmax.ink`
writes softmax and cross-entropy, `log` being Newton's method on `exp`. A
limit of matrices fills an array, cell by cell: `test/compile/steady.ink`
finds a chain's steady state and, by power iteration, a matrix's dominant
direction at every step.

A sequence that reads another at `x_(2*m)` samples it, and is computed every
second step: its terms are the input's at another rate, and `y_(floor(n/2))`
holds its latest term at the input's rate again. The step is still one, the
input's, and a term of `y` is computed on the first step at which every sample
it reads exists; a hold of a term not computed yet is refused, saying to read
the term before it. `test/compile/rates.ink` is a decimator and two holds.

`--check drift.ink calm -o calm.c` holds the compiled code to the
interpreter. It compiles `calm`, an instance the file defines, with the
parameters the instance gives, and writes a C program that steps it a hundred
times on the inputs the interpreter gives `calm`, replayed so that a difference
is the step's own, and compares each term with the interpreter's. Where every
term is within a billionth of one plus the interpreter's, it prints the largest
difference; otherwise the first term that parts and what the interpreter gives
there, and it exits with a failure. It also says whether the interpreter's
terms were exact throughout. `test/compile/drift.ink` has an instance that
holds and one that does not: a tenth computed again at every step, whose
rounding each step multiplies by ten. Before the values, it reports the first step
at which a compiled guard takes another clause than the interpreter's, and how
far that guard is from its threshold in exact arithmetic: rounding explains a
flip at a margin near zero, and not one at a large margin. `brink` in the same
file sits exactly on its threshold, and `ledge` a trillionth from it.

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
approached — `pi`, `e`, a root, a limit — is inexact, as is anything written
after `~`, and an inexact number makes inexact whatever it touches. An exact
number that outgrows a thousand digits becomes inexact rather than wrong, and
an answer that did ends in `# approximated past a thousand digits`, a comment,
so it still reads back. Dividing by an exact zero is an error.

Every number prints in decimal: an exact whole number in full, anything else
to nine significant digits, with `~` in front unless what is printed is all of
the value. `frac` at the start of a line shows the answer as its exact
fraction, and `digits = n` sets how many digits are shown.

Numbers are complex; `i` is the imaginary unit, a name that a bound one (a
sum's index, a cell's row) shadows in its own scope and that nothing may
define again. `e` and `pi` are the only
other built-in values, and `floor` the only built-in function: the largest
whole number not above its argument, exact of an exact number and cell by
cell of a matrix. `ceil(x) = -floor(-x)` and `mod(a, b) = a - b*floor(a/b)`
come with it, from a prelude (section 5), and so do `exp`, `log` and `tanh`. Any other rounding is a line of it,
by the rule the model needs — `round(x) = floor(x + 1/2)` — and, like `pi`,
each of them can be defined again.

| | |
|---|---|
| `+expr` `-expr` | unary plus and minus |
| `!expr` | factorial, written as a prefix |
| `~expr` | the same value, inexact |
| `expr+expr` `expr-expr` | addition, subtraction |
| `expr*expr` `expr/expr` | multiplication, division |
| `expr^expr` | power; of a square matrix, a whole one, negative for the inverse |
| `expr<expr` `expr>expr` | comparison, answering 1 or 0 |
| `expr<=expr` `expr>=expr` | the same, or equal |
| `expr==expr` `expr<>expr` | equal, not equal, of numbers or whole matrices |
| `expr and expr` `expr or expr` | both, either: 1 or 0, the right read only if needed |
| `name = expr` | definition (section 3) |
| `name \| cond = expr` | a definition in cases (section 3) |
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
2+i*3

>> (1+i)*(1-i)
2

>> floor(-7/2)
-4
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

A comparison is a number — `1` or `0` — so a guard is simply an expression
that is not zero, and `sgn(x) = (x>0) - (x<0)` needs no guard at all. Ordering
needs real numbers; equality does not. Conditions combine with `and` and
`or`, which answer `1` or `0` as a comparison does, bind looser than it, and
read their right side only when the left has not decided — so a guard such as
`n > 0 and s_(n-1) > 1` never asks for `s_(-1)`:

```
>> inside(x) = 0 < x and x < 1
inside(x) = 0 < x and x < 1

>> inside(1/2)
1
```

If no clause applies, the interpreter says so rather than inventing a value:

```
>> abs(i)
error: a comparison needs real numbers, not i
```

A name has one definition. Defining it again replaces what was there — a
plain definition clears the guarded clauses with it.

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
1e-10, every cell of a matrix, and reports non-convergence rather than handing
back the term it stopped on:

```
>> lim s
~5.82076609e-11

>> u_n=2*n
u_n=2*n

>> lim u
error: u did not converge within 100 terms (last term 200)
```

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
error: w did not converge within 100 terms (last term ~1.63508193)
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

The derivative of an expression with respect to a name at a point is written
as a paper writes it there, and binds the name as a sum binds its index: the
point is read around it, the name only in the body, which is a term as a sum's
is. What the body reads through definitions is differentiated with it, exactly
where its parts are exact. A derivative at a parameter is the derivative as a
function, and a single value's gradient with respect to a matrix has the
matrix's shape:

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

A definition in cases takes the slope of the clause that holds at the point,
and a limit the limit of its terms' slopes. Where a derivative does not exist
or would mislead, `grad` says why rather than answer: at a jump of `floor` or
of a comparison, at a clause that holds only at the point, for an exponent
that changes with the name over any base but `e`, for a Jacobian, for a body
that does not read the name, and through a definition that reads the global
of that name, which the bound name does not reach.

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
nothing and says where. The prelude that defines `ceil`, `mod`, `exp`, `log`
and `tanh` is included bare beneath the session, as the built-ins are: every
scope sees it, and a session that defines one of its names again does so for
itself alone. `exp(x)` is `e^x`, `log` a series reached by halving or
doubling, and `log(0)` says so as `1/0` does.
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

On a sequence `?` shows every clause, in the order they were written, and
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

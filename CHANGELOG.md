# Changes

What changed for someone using inkamath, release by release. The reasons, and
every defect by its number, are in [`DESIGN.md`](DESIGN.md).

## 2.0.0 — 2026-10-26

### Added

- **A compiler to C**, `--compile file [model] [-o header.h]`: a model's
  sequences become a header over doubles, a struct, an init and a step, with
  `--float` for floats. Without `-o` it lists what would not compile, or says
  that it all compiles. `-D<NAME>_SHARED` builds the header as a shared
  library whose tables describe it: its size in `<name>_size_`, its number of
  members in `<name>_count_`, each one's name, offset and type in
  `<name>_members_`, `<name>_offsets_` and `<name>_types_`, and the step's
  inputs in `<name>_inputs_`. See [`docs/compile.md`](docs/compile.md).
- **An oracle**, `--check file [instance or model] -o check.c`: a C program
  that steps the compiled instance, a model that needs no argument, or the
  file's own sequences, and holds each term to the interpreter's exact one,
  with `--steps n`. `--check transcript.ink` replays a transcript and reports
  each answer that is not the one recorded.
- **Python**, [`tools/inkamath.py`](tools/inkamath.py), installed by
  `pip install -e .` through [`pyproject.toml`](pyproject.toml), standard
  library only. The interpreter: `eval`, exact results as `int` or
  `Fraction`, `describe` (`?name`), `tex` and the prelude as functions.
  Compiled models: `Model`, built as a shared library and cached, stepped
  and read by name, `check`, and `load` or `import` of an `.ink` file as a
  module of its models, functions and values. `--names file` lists what a
  file defines, and what each is.
- **Models and files**: a model's signature is its interface, its instances
  are read after a point (`fast.v_3`), and `use filters` reads `filters.ink`
  into a scope of its own.
- **Exact integers of any size**: `2^63` and `!21` are exact; a result past a
  thousand digits is approximated and says so.
- **Derivatives**, `grad_(x = a) e`, by several names at once, of any order
  with `grad_(x = a)^n`, through definitions by cells, forward or in reverse.
- **Tensors of rank 3**, definitions by cells with inferred sizes, `a[1]` as
  a row, a negative matrix power as the inverse, `A^-1*b` as a solve, `==` and
  `<>` on whole matrices, and the limit of a sequence of matrices.
- **In sequences**: several rates, optional inputs `z_n?`, the forward form
  `x_(t+1) = f(x_t)`, `max_`, `min_` and `argmax_` over an index, a clause for
  a limit that does not converge, and a `lim` tolerance relative to the term.
- **In guards**: `and` and `or`. **At the prompt**: `clear`, and `tex` to
  write a definition as LaTeX.
- **A prelude**: `floor`, `ceil`, `mod`, `exp`, `log`, `tanh`, `sin`, `cos`,
  `atan`, `atan2`, `asin`, `acos`, `acosh`, `abs`, `max`, `min`, `chol`,
  `eig`, `rho`, `abscissa`, `hinf`, `charpoly`, `conv`, `polyval`, `roots` and
  their kin. The elementary functions are accurate to under 3.6 units in the
  last place and compile to the same C.

### Changed

Replayed on 2.0.0, 80 of the 649 answers recorded by 1.0.0's transcripts
differ. Those that a 1.0.0 session may meet:

- A complex number is a double, and prints with its `~`: `i*i` is `~-1`.
  Every approximated value prints its `~`, as `half(~1)` is `~0.5`.
- A name is a sequence or it is not; turning one into the other takes
  `clear`. A parameter and an index cannot share a name.
- A local takes no index nor parameters, and cannot bind a parameter again.
- `a[1]` is the first row of `a`; it was refused. A block in a matrix literal
  must fill its band. Inside a literal, a sign after a space begins a cell:
  `[sum_(k=1)^3 k -1]` is `[6, -1]`.
- A guard that meets NaN is refused rather than taken.
- A recurrence is no longer cut off at 256 references deep.
- `?u` lists a definition's cases before its general clause.
- Many errors now name the remedy, such as `write 'clear pick' first`.

### Fixed

306 defects, from C65 to C428, each in [`DESIGN.md`](DESIGN.md#defects) with
its test. Four are kept as documented limits, and C394 and C416 are open.

## 1.0.0 — 2026-09-27

The 2014 interpreter rebuilt in standard C++20 without Boost: exact rational
arithmetic, failure reported instead of printed, locals, memoised sequences,
series, and the golden transcripts that define its behaviour.

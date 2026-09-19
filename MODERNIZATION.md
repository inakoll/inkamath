# Modernization plan

Inkamath was written around 2014 in the idiom of the preceding decade. It is a
small, genuinely interesting program: a lazy mathematical expression
interpreter where every identifier names an *expression* rather than a value,
with user-defined functions, complex numbers, matrices of expressions, and
sequences defined by recurrence. That design is worth keeping. The plan below
changes almost none of it — it removes the scaffolding that has rotted around
it and fixes the defects the scaffolding was hiding.

Target: **C++20**, standard library only, tested and CI-verified at every step.

Two constraints govern every phase:

- **Size.** The project's value is that it is small. No phase may make it
  larger without removing at least as much.
- **Recognisability.** The author must still recognise this as his code. We
  refactor heavily, but we do not replace the design with a different one.
  The ideas that are *his* stay: references naming expressions rather than
  values, lazy re-evaluation, matrices of expressions that expand to the size
  of what their cells evaluate to, a scoped stack of definitions, and an
  explicit visitor over an expression tree. What goes is the scaffolding
  around them.

`CLAUDE.md` has the working rules.

---

## Verified defects

Everything below was reproduced against the code as it stood at the start of
this work, not inferred from reading. `[fixed]` items were resolved in
phase 0; the rest are open and are scheduled into the phases that follow.

### Build and packaging

| # | Defect |
|---|--------|
| B1 `[fixed]` | The headers could not be used from more than one translation unit. Three explicit specializations of `numeric_interface_imp<...>::parse` and a non-`inline` free function `print()` in `interpreter.hpp` produced multiple-definition link errors as soon as a second TU included them. The single-TU `main.cpp` hid this for eleven years. |
| B2 `[fixed]` | The qmake projects hardcoded `D:\boost\boost_1_55_0` and the MinGW 32-bit Boost library name `boost_unit_test_framework-mgw48-mt-d-1_55`. The project was buildable on exactly one machine, which no longer exists. |
| B3 `[fixed]` | The tests used `boost::test_toolbox::output_test_stream`, an API removed from Boost years ago, and loaded data through the hardcoded relative path `../inkamath/test/data/`. They had not compiled in a long time. |
| B4 `[fixed]` | `EvaluationVisitor` declared its constructor as `EvaluationVisitor<T>(...)`, which C++20 rejects (injected-class-name). This was the *only* thing standing between the codebase and C++20. |

### Correctness

| # | Defect |
|---|--------|
| C1 | **Unbounded recursion crashes the process.** The mutually recursive arithmetic-geometric mean from the project's own test data (`am`/`gm`) overflows the stack. Confirmed under ASan. There is no evaluation depth or step budget anywhere; `Reference::SafeRecursiveEval` guards one shape of recursion and nothing guards the rest. The original test suite worked around this by running each evaluation in a thread with a 1-second timeout and calling `std::terminate()` on expiry. |
| C2 | **Identifiers cannot start with `i`.** `Interpreter::Lexer` routes `'i'` to `Number_Lexer` alongside the digits, so `ii=3` fails with `Syntax error`. Any name beginning with `i` is unusable, and `i` itself can never be shadowed. |
| C3 | `ReferenceStack::WrapRecursiveExpression` never increments its index `i` when filling `recursive_placeholders`, so every slot after the first stays null. `EvaluationVisitor::visit(RecursiveExpression*)` then `dynamic_cast`s each child and silently ignores the nulls — the `else` branch is an empty block with a `// TODO` in it. Recursive expressions with more than one self-reference are quietly mis-evaluated. |
| C4 | `ParametersCall`'s constructor contains `return; subexpr_ = subexpr;` — the assignment is unreachable. `subexpr_` is therefore never set, and the `if(subexpr_)` branch in `TryEvalIndex` is dead code. |
| C5 | **`0^0` is NaN, so `exp(0)` is NaN.** Complex `pow` is specified as `exp(b*log(a))`, and `log(0)` is `-inf`, so `std::pow(complex(0,0), complex(0,0))` is NaN — where real `std::pow(0.0, 0.0)` is `1` by IEEE 754. Every series whose first term is `x^0` is poisoned at `x == 0`. `numeric_interface_imp<std::complex<T>>` already declares a `pow(complex, int)` overload that returns `1` for this; nothing ever calls it. The sign of the NaN is unspecified, so GCC and Clang disagree — caught by the cross-compiler CI. The `nan*nan` recorded against three definitions in `sequences.ink` is this bug, not a rule about what a definition returns (that is C10). |
| C10 | **A definition's value is undocumented and disagrees with the README.** `EvaluationVisitor::visit(EqualExpression*)` binds the name and then returns *the left-hand side evaluated after binding*, not the right-hand side. The two are not interchangeable: the left-hand side is a **lookup**, so it can resolve to a definition other than the one just installed. `f_n=2*n` followed by `f=5` prints `60`, not `5` — lookup order is indexed, then general, then simple, so the pre-existing general term wins and its convergence loop runs to the 30-iteration cap. README §4.3 shows `0` for parameterised and general-term definitions, which is merely what the rule produces when the parameters happen to be undefined. Defining a series also runs the convergence loop immediately, with every parameter defaulted to zero. |
| C11 | **Lookup precedence is undocumented, and a simple definition cannot shadow a general term.** A reference holds three definitions at once — singular terms (`f_0=`), a general term (`f_n=`) and a simple value (`f=`) — and `Reference::EvalImp` tries them in that order. A bare name on a sequence therefore means *the limit of the general term*, which is exactly how `exp(1)` works. It also means that once `f_n` exists, `f=5` is unreachable: bare `f` keeps iterating the series. README §4.3 states only that a singular definition beats the general term; it says nothing about the simple case. Decide the intended precedence and write it down — C10's fix changes what `f=5` *prints*, not what `f` *means*. |
| C12 | **Asking a recurrence for its limit segfaults.** `reference.hpp:187` dereferences `memoized_index_.rbegin()` unguarded, but the enclosing condition at line 180 is `!memoized_index_.empty() \|\| !indexed_expr_.empty()` — so the body is entered with `memoized_index_` empty whenever only a singular term exists. `ReferenceStack::Eval` evaluates a *copy* of the `Reference`, so memoisation never survives and that map is in practice always empty on entry. Three lines reproduce it: `f_n=2*n`, `f_0=7`, `f`. This is the textbook way to write a recurrence — an initial value plus a general term — and asking for its limit kills the process. `exp(1)` escapes only because `exp` has no singular term. Distinct from C1: an invalid dereference, not a stack overflow. Adding `!memoized_index_.empty() &&` to line 187 fixes it; verified, and no golden moves. |
| C6 | Parser errors print the raw token value through `std::complex`'s stream operator: `Missing operator ']' after '(2,0)'` where the user typed `2`. Error text leaks the interpreter's internal numeric type. |
| C7 | **Failure is not expressible.** `Interpreter::Eval` wraps everything in `try`/`catch(...)`, writes the message to `std::cout`, and returns a default-constructed value. A caller cannot distinguish a successful `0` from a failure, cannot redirect the message, and cannot test error behaviour without capturing `std::cout` — which is exactly what the new test harness has to do. |
| C8 | `Reference::TryEvaluateGeneralExpression` iterates a series until `diff > 1E-10 && iter_count < 30`, with both the epsilon and the cap hardcoded. It also computes `size_t index` from a subtraction of `int`s, which wraps for a negative result. |
| C9 | `numeric_interface_imp<std::complex<T>, false>` provides no `sqrt`, yet its own `abs` calls `numeric_interface<T>::sqrt`. `Matrix<T>::sqrt` throws unconditionally. The numeric interface is only accidentally complete for the one type actually instantiated. |

### Design and dead weight

| # | Defect |
|---|--------|
| D1 `[fixed]` | `template <typename T> using PExpression = std::shared_ptr<Expression<T>>` is written out identically in six headers. |
| D2 `[fixed]` | `sequence.hpp` is included by nothing. `pmath.hpp`/`pmath.cpp` define a `fact()` that nothing calls — superseded by `numeric_interface<T>::fact`, which is a verbatim copy of it. `main.cpp` carries a dead `inkamath_test()` function that duplicates the test data. |
| D3 `[fixed]` | `interpreter.hpp` includes `expression_visitor.hpp` *in the middle of the file*, after the class definition, to break a circular dependency. `make_matrix_array_from_vector` is called four lines before it is declared and resolves only through ADL at instantiation. |
| D4 | `Matrix<T>` owns a raw `T*` with `new[]`/`delete[]`, copies it with `memcpy` (undefined for any `T` that is not trivially copyable), has no move constructor or move assignment, and exposes `Matrix(const T&)` as an implicit converting constructor. Its `std::vector` constructor can leak on exception and carries the author's own note: `// todo : reimplement this matrix class...`. |
| D5 | `dynarray` is a hand-rolled container written while waiting for a `std::dynarray` that C++14 never shipped. `std::vector` covers every use here. |
| D6 | `Expression` exposes `dynarray<PExpression<T>> children` as a public mutable member while subclasses also offer `m_e1()`/`m_e()` accessors over the same storage; the two views are not kept consistent by anything but convention. `Clone()` deep-copies subtrees that `shared_ptr` already lets us share. |
| D7 | `ExpressionVisitor` has eleven pure virtual `visit` overloads plus two that default to returning `{}`. Adding a node type is a change to every visitor; forgetting one is silent. |
| D8 `[fixed]` | Reserved identifiers: `_EXPRESION_EPSILON` (misspelled, and unused) and `_NUMERIC_INTERFACE_PRECISION`. A leading underscore followed by a capital is reserved to the implementation. |
| D9 | `Interpreter<T, U = Matrix<T>>` templates on the token scalar `T`, but every AST node is instantiated on `U`. Consequently every literal in every expression is a heap-allocated 1×1 `Matrix<complex<double>>` — one `new T[1]` per number. Scalars and matrices are not separable. |
| D10 `[fixed]` | `getlines.hpp` reimplements line iteration on top of `std::iterator`, deprecated since C++17. Its only user was the Boost test file. |
| D11 | Comments and commit history are in French, the README is half French and half English, and the public documentation describes behaviour (C5) that the code does not have. |

---

## Phase 0 — Make it buildable and verifiable `[done]`

Nothing else can be trusted until a change can be checked. This phase changed
no interpreter behaviour; the transcripts recorded in it are the baseline every
later phase is measured against.

- CMake ≥ 3.20 replacing the two qmake projects. Targets: `inkamath` (the
  library), `inkamath_cli` (the REPL), `inkamath_tests`.
- C++20, `-Wall -Wextra -Wpedantic`, opt-in `-Werror` and sanitizers.
- Boost.Test replaced by vendored doctest (`third_party/doctest`, header-only).
  `mapstack_test` and `dynarray_test` ported mechanically.
- New golden-transcript harness (`test/transcript.hpp`): `.ink` files are
  literal interpreter sessions, one interpreter per file, capturing both the
  result and whatever the interpreter wrote to `std::cout`. Regenerate with
  `cmake --build build --target record_goldens`.
- Five transcripts drawn from `README.md`: `basics`, `matrices`, `references`,
  `sequences`, `errors`.
- GitHub Actions: GCC and Clang × Debug and RelWithDebInfo with `-Werror`,
  an ASan+UBSan job, and a clang-format check limited to changed lines.
- Fixed B1–B4 and the four warnings that stood in the way of `-Werror`
  (two `std::bind2nd` uses removed in C++17 by libc++ and deprecated
  everywhere; `EqualExpression::Name() const` silently *hiding* rather than
  overriding `Expression::Name()`, so the override was reachable only through
  a statically-typed `EqualExpression*`).

The goldens record current behaviour **including its bugs** — C5 and C6 are
visible in `sequences.ink` and `errors.ink`. That is deliberate: a bug that is
pinned by a test cannot regress unnoticed, and the diff when it is fixed is the
proof that it was fixed.

## Phase 1 — Clear the ground `[done]`

No behaviour change. Every `.ink` file came out byte-identical, which was the
acceptance criterion for the whole phase.

1. Delete `sequence.hpp`, `pmath.hpp`, `pmath.cpp`, and `inkamath_test()` from
   `main.cpp` (D2). `inkamath` becomes a header-only `INTERFACE` target.
2. Hoist `PExpression` into a single header and delete the other five
   definitions (D1).
3. Fix the include graph: forward-declaration header for the visitors, so
   `interpreter.hpp` can include `expression_visitor.hpp` at the top like a
   normal file; declare `make_matrix_array_from_vector` before use (D3).
4. Rename `_EXPRESION_EPSILON` and `_NUMERIC_INTERFACE_PRECISION`; the first is
   unused and should simply go (D8).
5. `override` on every override, `= default`/`= delete` on special members.
6. Move to a conventional layout — `include/inkamath/`, `src/`, `test/` — as
   its own commit, containing nothing but the moves.
7. Delete `getlines.hpp` (D10); nothing uses it any more.

## Phase 2 — Make failure expressible

The first phase that deliberately changes behaviour. Goldens move, and the
commits say exactly how.

1. **Evaluation budget** (C1). A per-`Eval` limit on recursion depth and total
   steps, threaded through the evaluator, reported as a normal error. This is
   the difference between "the interpreter rejects your input" and "the process
   dies", and it is what lets the AGM example from the project's own test data
   go back into the suite.
2. **An error channel** (C7). `Eval` returns a result *or* a diagnostic —
   `std::expected<Matrix<...>, Diagnostic>` — instead of printing and returning
   zero. The REPL keeps printing to `std::cout`; it just does it at the edge,
   where it belongs. The test harness stops having to hijack `std::cout`.
3. **Diagnostics carry a source position and the text the user typed**, not a
   stringified `std::complex` (C6).
4. **`0^0`** (C5). Route an integral exponent to the `pow(complex, int)`
   overload that already exists. Verified: fixes `0^0`, `exp(0)`, `cos(0)` and
   every series whose first term is `x^0`, moves exactly the three recorded
   `nan*nan` lines, and leaves `exp(1)-e` unchanged. A correctness bug, not a
   cosmetic one — do this before item 5, which it partly obscures.
5. **Decide what a definition evaluates to** (C10). Two independent choices:
   *when* to evaluate at all, and *what* to evaluate.

   Recommended: **the value of an assignment is the value of its right-hand
   side, evaluated only when the left-hand side binds no names.** Otherwise
   the assignment evaluates to `0`.

   - Not evaluating a binding left-hand side gives `f(x,y)=...`, `u_n=...`
     and `exp(x)_n=...` the `0` that README §4.3 documents, by rule instead
     of by accident, and stops the interpreter running a freshly typed
     recursive definition with zeroed parameters — one of the routes into C1.
   - Taking the right-hand side rather than the left-hand side lookup is one
     sentence to specify, costs no reference resolution, and cannot be
     hijacked by a definition of higher precedence. `f_n=2*n` then `f=5`
     prints `5`, not `60`.
   - Unchanged: `a=1` -> `1`, `b=a+a` -> `2`, `g=1+2` -> `3`, `x=5` -> `5`,
     `f_0=1+2` -> `3`, `u_0=2` -> `2` (README §4.1, §4.2). In every recorded
     transcript the installed definition is also the one lookup finds, so the
     left/right choice moves no golden on its own; `f_n=2*n; f=5` is not
     covered today and goes in as a new test with the change.

## Phase 3 — The front end

1. Fix the `i` lexing rule (C2): `i` is a numeric literal only when it is not
   part of a longer identifier.
2. Give `Token` a `std::variant` payload and a `Print` that renders what the
   user wrote.
3. Positions on tokens, carried into diagnostics.
4. Replace `std::list<Token>` + a member iterator with a `std::vector<Token>`
   and an index — the parser's backtracking (`m_i = m_s`) becomes obvious
   rather than incidental.

## Phase 4 — Value types

1. Rewrite `Matrix<T>` (D4): `std::vector<T>` storage, rule of zero, explicit
   constructors, `operator()` taking 0-based indices, dimensions as a single
   `Extent` type. The author already asked for this in a comment.
2. Delete `dynarray` (D5) in favour of `std::vector`, and delete its test.
3. Separate the scalar type from the matrix type in `Interpreter` (D9) so a
   scalar expression does not allocate a 1×1 matrix per literal. This is the
   largest single win available and should be measured, not assumed.
4. Replace the `numeric_interface`/`best_promotion`/`numeric_interface_imp_types`
   trio with C++20 concepts (C9), which turns "this type is missing `sqrt`"
   from a link-time surprise into a compile error at the point of use.

## Phase 5 — The core

1. Make `Expression::children` private with a narrow accessor, and drop the
   redundant `m_e1()`/`m_e()` views or express them in terms of it (D6).
2. Collapse the visitor interface (D7) by giving `ExpressionVisitor` a default
   implementation that recurses over `children`, so a visitor overrides only
   the nodes it cares about. The `std::variant` + `std::visit` alternative
   would delete the `accept`/`visit` double dispatch outright, and it is the
   more modern design — but the double dispatch is a deliberate choice the
   author documented in `expression_visitor.hpp`, and replacing it would make
   the core unrecognisable. Ruled out on the recognisability constraint, not
   on the merits.
3. Fix C3 and C4 — both are one-line bugs, but both need a test that would have
   caught them, and the second needs the dead branch's intent recovered first.
4. Revisit the series convergence loop (C8): epsilon and iteration cap become
   named constants or interpreter settings, and the index arithmetic stops
   wrapping.

## Phase 6 — Documentation

1. `README.md` in English, matching actual behaviour, with its examples drawn
   from the `.ink` transcripts so the two cannot drift.
2. Keep the French history — it is the project's provenance — but write new
   comments in English (D11).
3. A short note on the interpreter's model: references name expressions, not
   values. It is the idea the whole program is built around and the README
   currently explains it halfway down section 4.

---

## Sequencing

Phases 1–3 are independent of 4–5 and can be done in any order within
themselves. Phase 2 should not wait: C1 is a crash, and until there is an error
channel every later fix has to keep working around `std::cout`.

The honest risk is phase 5. The visitor rework touches every file and cannot be
verified by the goldens alone if it lands with a behaviour change — so it must
land without one, in its own commits, with the transcripts unchanged.

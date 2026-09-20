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
- **Recognisability.** The author must still recognise this as his project.
  Three ideas are his and are not up for renegotiation: names bind
  *expressions* rather than values and are re-evaluated lazily; matrices of
  expressions expand to the size of what their cells evaluate to; sequences
  are written as `f_0 = ...` and `f_n = f_(n-1) + ...`, the way mathematics
  writes a recurrence.

  This constraint used to cover the *semantics* as well. It no longer does.
  The author, reviewing the defect list below, judged the surrounding
  semantics broken — multiple bindings per identifier, implicit convergence,
  and left-hand-side evaluation — and asked for them to be redesigned rather
  than patched. Recognisability now attaches to the three ideas above and to
  the syntax that expresses them, not to the rules the 2014 interpreter
  happened to implement.

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
| C1 `[fixed]` | **Unbounded recursion crashes the process.** The mutually recursive arithmetic-geometric mean from the project's own test data (`am`/`gm`) overflows the stack. Confirmed under ASan. There is no evaluation depth or step budget anywhere; `Reference::SafeRecursiveEval` guards one shape of recursion and nothing guards the rest. The original test suite worked around this by running each evaluation in a thread with a 1-second timeout and calling `std::terminate()` on expiry. Fixed by a depth and step budget on `ReferenceStack` — 256 and 1000000, against a measured worst case of 33 and 6444 across every golden input. Steps as well as depth because the AGM nests shallowly but branches twice per level, so depth alone does not bound time. `f=f` was **not** an instance of this: see C16. |
| C2 `[fixed]` | **Identifiers cannot start with `i`.** `Interpreter::Lexer` routes `'i'` to `Number_Lexer` alongside the digits, so `ii=3` fails with `Syntax error`. Any name beginning with `i` is unusable, and `i` itself can never be shadowed. Fixed: `i` is the imaginary unit only when the next character cannot continue a name. No golden moved — every `i` in them is followed by an operator or a bracket. |
| C3 `[fixed]` | `ReferenceStack::WrapRecursiveExpression` never incremented its index `i` when filling `recursive_placeholders`, so every slot after the first stayed null. `EvaluationVisitor::visit(RecursiveExpression*)` then `dynamic_cast`ed each child and silently ignored the nulls — the `else` branch was an empty block with a `// TODO` in it. Recursive expressions with more than one self-reference were quietly mis-evaluated. Fixed by deleting the substitution machinery outright (phase 4 item 4). |
| C4 `[fixed]` | `ParametersCall`'s constructor contained `return; subexpr_ = subexpr;` — the assignment was unreachable. `subexpr_` was therefore never set, and the `if(subexpr_)` branch in `TryEvalIndex` was dead code; every index went through `SubVisitor`'s `a*name + b` reconstruction instead. Fixed by deleting `SubVisitor` and evaluating the index expression, which is what that dead branch had meant to do. |
| C5 `[fixed]` | **`0^0` is NaN, so `exp(0)` is NaN.** Complex `pow` is specified as `exp(b*log(a))`, and `log(0)` is `-inf`, so `std::pow(complex(0,0), complex(0,0))` is NaN — where real `std::pow(0.0, 0.0)` is `1` by IEEE 754. Every series whose first term is `x^0` is poisoned at `x == 0`. `numeric_interface_imp<std::complex<T>>` already declares a `pow(complex, int)` overload that returns `1` for this; nothing ever calls it. The sign of the NaN is unspecified, so GCC and Clang disagree — caught by the cross-compiler CI. The `nan*nan` recorded against three definitions in `sequences.ink` was this bug, not a rule about what a definition returns (that is C10). Fixed by routing an integral exponent to that unused overload; the goldens lose their last three NaNs, and the harness lost the workaround that normalised the NaN sign across compilers. |
| C10 `[fixed]` | **A definition's value is undocumented and disagrees with the README.** `EvaluationVisitor::visit(EqualExpression*)` binds the name and then returns *the left-hand side evaluated after binding*, not the right-hand side. The two are not interchangeable: the left-hand side is a **lookup**, so it can resolve to a definition other than the one just installed. `f_n=2*n` followed by `f=5` prints `60`, not `5` — lookup order is indexed, then general, then simple, so the pre-existing general term wins and its convergence loop runs to the 30-iteration cap. README §4.3 shows `0` for parameterised and general-term definitions, which is merely what the rule produces when the parameters happen to be undefined. Defining a series also runs the convergence loop immediately, with every parameter defaulted to zero. Fixed: a top-level definition is a statement handled before evaluation. It binds and echoes what was written, evaluating nothing. |
| C11 `[fixed]` | **Lookup precedence is undocumented, and a simple definition cannot shadow a general term.** A reference holds three definitions at once — singular terms (`f_0=`), a general term (`f_n=`) and a simple value (`f=`) — and `Reference::EvalImp` tries them in that order. A bare name on a sequence therefore means *the limit of the general term*, which is exactly how `exp(1)` works. It also means that once `f_n` exists, `f=5` is unreachable: bare `f` keeps iterating the series. README §4.3 states only that a singular definition beats the general term; it says nothing about the simple case. Fixed by making the two kinds mutually exclusive: an indexed clause extends a sequence, a plain definition replaces it. There is no precedence left to document. A bare sequence name and an uncovered index are now diagnostics rather than zero. |
| C12 `[fixed]` | **Asking a recurrence for its limit segfaults.** `reference.hpp:187` dereferences `memoized_index_.rbegin()` unguarded, but the enclosing condition at line 180 is `!memoized_index_.empty() \|\| !indexed_expr_.empty()` — so the body is entered with `memoized_index_` empty whenever only a singular term exists. `ReferenceStack::Eval` evaluates a *copy* of the `Reference`, so memoisation never survives and that map is in practice always empty on entry. Three lines reproduce it: `f_n=2*n`, `f_0=7`, `f`. This is the textbook way to write a recurrence — an initial value plus a general term — and asking for its limit kills the process. `exp(1)` escapes only because `exp` has no singular term. Distinct from C1: an invalid dereference, not a stack overflow. Fixed by guarding that dereference; `sequences.ink` gained the repro as a regression test. |
| C6 `[fixed]` | Parser errors printed the raw token value through `std::complex`'s stream operator: `Missing operator ']' after '(2,0)'` where the user typed `2`. Fixed by giving each token its lexeme, which also deleted `Token::Print` and its switch. |
| C7 `[fixed]` | **Failure is not expressible.** `Interpreter::Eval` wraps everything in `try`/`catch(...)`, writes the message to `std::cout`, and returns a default-constructed value. A caller cannot distinguish a successful `0` from a failure, cannot redirect the message, and cannot test error behaviour without capturing `std::cout` — which is exactly what the new test harness had to do. Fixed: `Eval` returns `std::variant<U, Diagnostic>`, the REPL formats at the edge, and `catch(...)` is gone. |
| C8 `[fixed]` | `Reference::TryEvaluateGeneralExpression` iterated a series until `diff > 1E-10 && iter_count < 30`, with both the epsilon and the cap hardcoded and neither reachable by the user, who saw only a number. They are now named constants that the non-convergence diagnostic quotes. The cap is 100 rather than 30 so that a geometric sequence reaches the tolerance, and it cannot go much higher: each term of a recurrence nests one more reference, so the term budget is bounded by `ReferenceStack::max_depth`. Making either settable is deferred — no requirement asks for it. Its other half — a `size_t index` computed from a subtraction of `int`s, which wrapped for a negative result — went with C17. |
| C9 | `numeric_interface_imp<std::complex<T>, false>` provides no `sqrt`, yet its own `abs` calls `numeric_interface<T>::sqrt`. `Matrix<T>::sqrt` throws unconditionally. The numeric interface is only accidentally complete for the one type actually instantiated. |
| C14 `[fixed]` | **Four `catch` blocks discarded errors inside a constructor.** `parameters.hpp:28, 38, 105, 115` caught `SubVisitor`/`ParametersVisitor` exceptions in the `ParametersDefinition` and `ParametersCall` constructors and returned, leaving the object half-built — `parameters_names_`, `index_name_`, `a_` and `b_` never assigned. The errors never reached `Eval`, so the phase 3 error channel did not surface them; they were invisible by construction. A sibling of C13 rather than an instance of it: there the interpreter has nothing to report, here it had something and threw it away. Fixed with C4: two of the four catches guarded `SubVisitor` and went with it, the other two are gone and `f(x=1, y)=x+y` is a golden. |
| C15 `[fixed]` | **The parser read one token past the end.** `ParseParameters` tested `Peek().type` after `ParseMatrix` had consumed the input — an `end()` dereference before the token cursor became an index, `vector::operator[](size())` after. Neither ASan nor UBSan catches it while the index lands inside the allocation, which for a vector with spare capacity is most of the time; `_GLIBCXX_ASSERTIONS` does, and sanitizer builds now define it. `f(1+2` is the repro and is a golden. A sweep of all 117 prefixes of ten representative inputs found no other instance — only C1. |
| C16 `[fixed]` | **`f=f` crashed on an uninitialised pointer, before recursing at all.** `RecursiveExprVisitor::to_transform_` was assigned only when descending into a child, but the constructor visits the root directly — so a self-reference at the root of a definition read an indeterminate `PExpression<T>*` and then wrote through it. The stack was ten frames deep, not overflowing; ASan does not flag a read of an uninitialised pointer, which is why this looked like C1 for several rounds. Fixed by initialising the member and not wrapping a root self-reference, which has no enclosing slot to substitute into. `f=f` and `w_n=w` are goldens and now reach the budget. |
| C17 `[fixed]` | **Indexed clauses are keyed by `size_t`, so a negative index destroys their ordering.** `reference.hpp` declares `std::map<size_t, ExpressionDefinition<T>>` while `ParametersDefinition::b()` is `int`, so `f_(-1)=0` stores at key 18446744073709551615. Lookup round-trips through the same conversion and appears to work, but `TryEvaluateGeneralExpression` picks its starting term with `indexed_expr_.rbegin()`, which then finds the negative clause as the *largest*. With `g_(-1)=0`, `g_0=5`, `g_n=g_(n-1)+1`, `g_1` is a correct `6` while bare `g` is `34`. The same signedness confusion is half of C8: `size_t index = ai_parameters.b() - gen_params_def.b()` wraps for a negative result. Matters now because an explicit base case at a negative index is exactly how the implicit zero would be written out. Fixed by keying the clause maps `long long`, which also stops the subtraction wrapping. |
| C13 `[fixed]` | **Nothing ever fails, which is why everything else survived.** An unknown identifier evaluates to `0`. Surplus arguments are dropped. A missing parameter is looked up by name in the enclosing scope — `h(x)=x^2` with `x=5` in scope makes bare `h` evaluate to `25`, which is dynamic scoping arrived at by accident. A non-integer index is truncated. None of these reports anything. The interpreter always returns a number, so a typo and a correct series are indistinguishable from the outside; that is how a decade of defects stayed invisible. Root cause of the plausibility of C1, C2, C5, C10 and C11 alike. Arity, the undefined name and the truncated index are all diagnostics now. The dynamic-scoping half is phase 4 item 5. |

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

## Phase 2 — Write the language down `[in progress]`

The redesign is specified as transcripts before it is implemented, in
`test/data/spec/*.ink`. They use the phase 0 harness, so the specification is
executable: running them prints a diff between the language we have and the
language we want. They are registered under the `spec` doctest suite and
marked `may_fail`, so they report without gating CI, and `record_goldens`
never rewrites them — recording a specification from current behaviour would
defeat its purpose.

As each part of the design lands, its entries move from `test/data/spec/` into
`test/data/` and become ordinary goldens.

This ordering is deliberate. The README and the code disagree today (C5, C10,
C11) because the prose was written once and then drifted. A specification that
is run on every push cannot drift.

## Phase 3 — Failure exists `[done except where phase 4 blocks it]`

C13 first, because everything else is easier to see once the interpreter stops
answering every question with a number.

1. **An error channel** (C7) `[done]`. `Eval` returns `std::variant<U,
   Diagnostic>` instead of printing to `std::cout` and returning a
   default-constructed value. The REPL prints at the edge, where it belongs;
   the test harness stopped hijacking `std::cout`. `std::expected` would be
   the natural type and is C++23, so this becomes one mechanically if the
   project ever moves.
2. **Diagnostics quote the text the user typed** (C6) `[done]`, and the token
   rework that enables it: `std::vector<Token>` with an index in place of
   `std::list` with a member iterator, so the parser's backtracking is visible
   rather than incidental.

   Two parts of this item were dropped after looking at the code. A
   `std::variant` payload is unnecessary: once a token carries its lexeme,
   `name` *is* the lexeme, `Print()` is `return text;`, and the whole switch
   goes — smaller than a variant and it fixes C6 outright. Source *positions*
   are not needed either: nothing renders one. The spec transcripts quote the
   offending token rather than pointing at a column, which reads better for
   single-line input, and inkamath has no other kind. Adding `offset` to
   `Token` is three lines whenever something wants a caret.
3. **Arity mismatch becomes a diagnostic** (C13) `[done]`. The check sits at
   the user's call, not at every parameter binding: the recursion machinery
   builds synthetic `ParametersCall`s carrying no arguments on purpose, and
   checking those breaks `exp(1)`.

   The other two thirds of this item are **blocked on phase 4**, which is not
   what the plan assumed. Both were tried and reverted:

   - **Unknown name** `[done]`. Was blocked on C10: reporting an undefined
     name broke *defining* a function, because a definition evaluated its own
     left-hand side with its parameters unbound. Unblocked the moment
     definitions became statements, and landed with four goldens moving from
     a silent `0` to a diagnostic.
   - **Non-integer index.** A throw from `SubVisitor` never reaches the user:
     the `ParametersDefinition` and `ParametersCall` constructors catch it and
     `return` (C14). Verified with a probe that threw unconditionally and
     changed nothing an interpreter session could see. Those catches are
     load-bearing — they are how the code decides an expression is not an
     index — so they cannot simply be deleted; the clause model in phase 4
     replaces them.
4. **Evaluation budget** (C1) `[done]`. A per-`Eval` limit on recursion depth
   and total steps, reported as a diagnostic. `spec/recursion.ink` is no
   longer skipped, and a sweep of all 117 prefixes of ten representative
   inputs now runs clean under ASan, UBSan and `_GLIBCXX_ASSERTIONS` — it
   previously overflowed the stack.
5. **Fix the `i` lexing rule** (C2) `[done]`.
6. **`0^0`** (C5) `[done]`. Moved exactly the three predicted lines and left
   `exp(1)-e` unchanged, as forecast when the fix was first measured.

## Phase 4 — The definition model

The redesign proper. Replaces C10 and C11 rather than deciding them.

1. **One definition per name.** `Reference`'s three parallel slots
   (`single_expr_`, `indexed_expr_`, `general_expr_`) collapse into one
   definition that may have several *clauses*: constant-index base cases plus
   at most one general clause. `f_0 = 1` and `f_n = f_(n-1)/2` are two clauses
   of one sequence, the way a recurrence is written on paper. A later `f = 5`
   *replaces* the definition instead of hiding underneath it, and the
   undocumented indexed/general/simple precedence disappears with the slots.
2. **No implicit limit** `[done]`. A bare name never means "iterate until it
   stops changing"; `lim` is a reserved word taking a sequence name, and a
   bare sequence name is a diagnostic that names both ways out. Non-
   convergence is reported with the budget and the last term rather than
   silently returning the term the loop stopped on, so `lim k` on a divergent
   recurrence says so where bare `k` used to answer `35`. `exp(1)-e` is
   `-7.7e-13` not from floating point but from a series stopped as soon as
   two terms agree to 1e-10 — measured, not the 30-term cap this item
   originally blamed: raising the cap to 200 leaves the figure unchanged.
3. **A definition is a statement.** It binds and echoes what it bound; it
   evaluates nothing. No left-hand-side lookup, no right-hand-side evaluation,
   no convergence loop triggered by typing a definition.
4. **A recurrence needs its base case; there is no implicit value at an
   undefined index** `[done]`. `SafeRecursiveEval` returned `0` below the
   lowest defined index, an unannounced choice of the *additive* identity.
   It is right for a series and wrong for a product: `p_n = p_(n-1)*n` with no
   base evaluates to `0` at every index, while `p_0 = 1` gives `1, 2, 6, 24,
   120`. The 2014 test data makes the point by itself — `exp`, `ln` and `atan`
   are additive and carry no base, the arithmetic-geometric mean is
   multiplicative and carries `am(x,y)_0` and `gm(x,y)_0`. Base cases were
   already written wherever the hidden zero would have been wrong. Done by
   deleting `WrapRecursiveExpression`, `SafeRecursiveEval`,
   `RecursiveExprVisitor` and the two recursive expression nodes: an indexed
   call is now evaluated at the index it asks for, against the clause that
   covers it. `sequences.ink` gained `exp(x)_0=1`, which the hidden zero had
   been supplying, and `fact_n=fact_(n-1)*n` as the case it got wrong. C3 and
   C16 go with the machinery.
5. **Parameters are lexically scoped** (C13) `[done]`. A missing argument is a
   diagnostic, not a search of the enclosing scope for a name that matches.
   A name resolves in the innermost call's parameters and then in the global
   scope, and nowhere else. `Mapstack` went with the change: it copied every
   live name forward on `Push`, which is what made a caller's parameters
   visible to everything it called. Two plain maps replace it.
6. **`?name` prints a definition back**, as written, without evaluating it.
   On a sequence it prints every clause, so the whole definition is visible
   at once — which the three parallel slots made impossible. Together with
   item 3 this is what makes the core idea legible: after `b = a+a` and
   `a = 2`, `?b` is `b = a+a` while `b` is `4`. `?` is currently an
   unrecognised character, so the syntax is free.
7. C3, C4, C14 and C17 were small bugs in machinery this phase rewrites; they
   went away with it rather than being patched first.

## Phase 5 — Value types and the core

1. Rewrite `Matrix<T>` (D4): `std::vector<T>` storage, rule of zero, explicit
   constructors, dimensions as one `Extent` type. The author asked for this in
   a comment in 2014.
2. Delete `dynarray` (D5) in favour of `std::vector`, and delete its test.
3. Separate the scalar type from the matrix type in `Interpreter` (D9) so a
   scalar expression stops allocating a 1x1 matrix per literal. The largest
   single win available; measure it rather than assuming it.
4. Replace `numeric_interface`/`best_promotion`/`numeric_interface_imp_types`
   with C++20 concepts (C9): a missing `sqrt` becomes a compile error at the
   point of use instead of a link-time surprise.
5. Make `Expression::children` private with a narrow accessor and drop the
   redundant `m_e1()`/`m_e()` views (D6).
6. Collapse the visitor interface (D7) by giving `ExpressionVisitor` a default
   implementation that recurses over `children`, so a visitor overrides only
   the nodes it cares about. The `std::variant` + `std::visit` alternative
   would delete the `accept`/`visit` double dispatch outright and is the more
   modern design, but the double dispatch is a choice the author documented in
   `expression_visitor.hpp`. Ruled out on recognisability, not on the merits.

## Phase 6 — Documentation

1. `README.md` in English, matching actual behaviour, with its examples drawn
   from the `.ink` transcripts so the two cannot drift.
2. Keep the French history — it is the project's provenance — but write new
   comments in English (D11).
3. A short note on the interpreter's model: references name expressions, not
   values. It is the idea the whole program is built around and the README
   currently explains it halfway down section 4.

---

## Deferred

Recorded so they are not re-litigated later, or drifted into by accident.

- **Substitution and partial expansion.** `?b` showing `2+2` rather than
  `a+a`: evaluating some references while leaving others symbolic. This is a
  different feature from printing a definition back, not an option on it. It
  needs its own syntax, a rule for how far expansion goes, and an answer for
  what a partially evaluated sequence or matrix of expressions even means.
  `?name` (phase 4, item 5) prints what was written and nothing more.

## Sequencing

Phase 2 gates everything: no implementation work starts before the transcripts
say what it should do. Phase 3 comes next because C13 makes every later change
observable. Phase 4 is the redesign the rest of the plan exists to serve.
Phase 5 is independent of 3 and 4 and can be interleaved when convenient.

The honest risk is phase 4. It changes what existing sessions mean, so it
cannot hide behind unchanged goldens — every moved line has to be justified
against a spec transcript, in the commit that moves it.

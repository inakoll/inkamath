# Design record

This began as the plan to modernise the 2014 sources, which was done by
phase 7; phase 8 was the first to change the language. It is now the record
of what was decided, measured and fixed, in the order it happened. Where the
language could go next is in `MANIFESTO.md`.

Inkamath was written around 2014 in the idiom of the preceding decade. It is a
small, genuinely interesting program: a lazy mathematical expression
interpreter where every identifier names an *expression* rather than a value,
with user-defined functions, complex numbers, matrices of expressions, and
sequences defined by recurrence. That design is worth keeping. The plan below
changes almost none of it — it removes the scaffolding that has rotted around
it and fixes the defects the scaffolding was hiding.

Target: **C++20**, standard library only, tested and CI-verified at every step.

Two constraints govern every phase:

- **Size.** The project's value is that it is small. A phase that repairs may
  not make it larger without removing at least as much.

  Phases 9 and 10 did not repair, and they broke that rule as it was first
  written: the headers went from **2,406 lines to 2,964** across memoisation,
  locals, cell indexing, comparisons and guards -- a fifth larger for five
  capabilities. The rule is restated rather than quietly missed, because the
  first version made an honest feature phase impossible to pass. What it asks
  now: a phase that adds a capability says in its own section what it cost,
  and looks for what it can remove. Looking, at the end of phase 10, found
  nineteen lines -- `PrintTokens`, the `Space` token, a virtual `Size`, a
  stored reference nothing read, and nine forwarding methods in
  `ParametersVisitor`. Nineteen against five hundred and fifty-eight is the
  measurement, not an argument that the five hundred and fifty-eight were
  wrong.

  Each entry since stated its own cost and none the sum: `include/` went
  from 8,896 lines to 12,790 in three days. A removal pass at ba91fcf,
  sharing what had been written twice, took `include/` and `src/` together
  from **13,463 lines to 13,275**; the rest of the growth is capability.
  From here an entry states the total after it as well as its own delta.
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

## Verified defects — first pass

Everything in this section was reproduced against the code as it stood at the
start of this work, not inferred from reading. A second pass, after phase 6,
found more; it has its own section below.

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
| C1 `[fixed]` | **Unbounded recursion crashes the process.** The mutually recursive arithmetic-geometric mean from the project's own test data (`am`/`gm`) overflows the stack. Confirmed under ASan. There is no evaluation depth or step budget anywhere; `Reference::SafeRecursiveEval` guards one shape of recursion and nothing guards the rest. The original test suite worked around this by running each evaluation in a thread with a 1-second timeout and calling `std::terminate()` on expiry. Fixed by a depth and step budget on `ReferenceStack` — 256 and 1000000, against a measured worst case of 33 and 6444 across every golden input. Steps as well as depth because the AGM nests shallowly but branches twice per level, so depth alone does not bound time. Those figures are stale: measured against the corpus as it stands, the deepest successful evaluation nests 36 and the longest takes 10233 steps. The budget only covers reference lookups — see C20. `f=f` was **not** an instance of this: see C16. |
| C2 `[fixed]` | **Identifiers cannot start with `i`.** `Interpreter::Lexer` routes `'i'` to `Number_Lexer` alongside the digits, so `ii=3` fails with `Syntax error`. Any name beginning with `i` is unusable, and `i` itself can never be shadowed. Fixed: `i` is the imaginary unit only when the next character cannot continue a name. No golden moved — every `i` in them is followed by an operator or a bracket. |
| C3 `[fixed]` | `ReferenceStack::WrapRecursiveExpression` never incremented its index `i` when filling `recursive_placeholders`, so every slot after the first stayed null. `EvaluationVisitor::visit(RecursiveExpression*)` then `dynamic_cast`ed each child and silently ignored the nulls — the `else` branch was an empty block with a `// TODO` in it. Recursive expressions with more than one self-reference were quietly mis-evaluated. Fixed by deleting the substitution machinery outright (phase 4 item 4). |
| C4 `[fixed]` | `ParametersCall`'s constructor contained `return; subexpr_ = subexpr;` — the assignment was unreachable. `subexpr_` was therefore never set, and the `if(subexpr_)` branch in `TryEvalIndex` was dead code; every index went through `SubVisitor`'s `a*name + b` reconstruction instead. Fixed by deleting `SubVisitor` and evaluating the index expression, which is what that dead branch had meant to do. |
| C5 `[fixed]` | **`0^0` is NaN, so `exp(0)` is NaN.** Complex `pow` is specified as `exp(b*log(a))`, and `log(0)` is `-inf`, so `std::pow(complex(0,0), complex(0,0))` is NaN — where real `std::pow(0.0, 0.0)` is `1` by IEEE 754. Every series whose first term is `x^0` is poisoned at `x == 0`. `numeric_interface_imp<std::complex<T>>` already declares a `pow(complex, int)` overload that returns `1` for this; nothing ever calls it. The sign of the NaN is unspecified, so GCC and Clang disagree — caught by the cross-compiler CI. The `nan*nan` recorded against three definitions in `sequences.ink` was this bug, not a rule about what a definition returns (that is C10). Fixed by routing an integral exponent to that unused overload; the goldens lose their last three NaNs, and the harness lost the workaround that normalised the NaN sign across compilers. |
| C10 `[fixed]` | **A definition's value is undocumented and disagrees with the README.** `EvaluationVisitor::visit(EqualExpression*)` binds the name and then returns *the left-hand side evaluated after binding*, not the right-hand side. The two are not interchangeable: the left-hand side is a **lookup**, so it can resolve to a definition other than the one just installed. `f_n=2*n` followed by `f=5` prints `60`, not `5` — lookup order is indexed, then general, then simple, so the pre-existing general term wins and its convergence loop runs to the 30-iteration cap. README §4.3 shows `0` for parameterised and general-term definitions, which is merely what the rule produces when the parameters happen to be undefined. Defining a series also runs the convergence loop immediately, with every parameter defaulted to zero. Fixed: a top-level definition is a statement handled before evaluation. It binds and echoes what was written, evaluating nothing. |
| C11 `[fixed]` | **Lookup precedence is undocumented, and a simple definition cannot shadow a general term.** A reference holds three definitions at once — singular terms (`f_0=`), a general term (`f_n=`) and a simple value (`f=`) — and `Reference::EvalImp` tries them in that order. A bare name on a sequence therefore means *the limit of the general term*, which is exactly how `exp(1)` works. It also means that once `f_n` exists, `f=5` is unreachable: bare `f` keeps iterating the series. README §4.3 states only that a singular definition beats the general term; it says nothing about the simple case. Fixed by making the two kinds mutually exclusive: an indexed clause extends a sequence, a plain definition replaces it. There is almost no precedence left to document — C26 is the exception, and it is silent. A bare sequence name and an uncovered index are now diagnostics rather than zero. |
| C12 `[fixed]` | **Asking a recurrence for its limit segfaults.** `reference.hpp:187` dereferences `memoized_index_.rbegin()` unguarded, but the enclosing condition at line 180 is `!memoized_index_.empty() \|\| !indexed_expr_.empty()` — so the body is entered with `memoized_index_` empty whenever only a singular term exists. `ReferenceStack::Eval` evaluates a *copy* of the `Reference`, so memoisation never survives and that map is in practice always empty on entry. Three lines reproduce it: `f_n=2*n`, `f_0=7`, `f`. This is the textbook way to write a recurrence — an initial value plus a general term — and asking for its limit kills the process. `exp(1)` escapes only because `exp` has no singular term. Distinct from C1: an invalid dereference, not a stack overflow. Fixed by guarding that dereference; `sequences.ink` gained the repro as a regression test. |
| C6 `[fixed]` | Parser errors printed the raw token value through `std::complex`'s stream operator: `Missing operator ']' after '(2,0)'` where the user typed `2`. Fixed by giving each token its lexeme, which also deleted `Token::Print` and its switch. |
| C7 `[fixed]` | **Failure is not expressible.** `Interpreter::Eval` wraps everything in `try`/`catch(...)`, writes the message to `std::cout`, and returns a default-constructed value. A caller cannot distinguish a successful `0` from a failure, cannot redirect the message, and cannot test error behaviour without capturing `std::cout` — which is exactly what the new test harness had to do. Fixed: `Eval` returns `std::variant<U, Echo, Diagnostic>` (`Echo` arrived with C10), the REPL formats at the edge, and `catch(...)` is gone. |
| C8 `[fixed]` | `Reference::TryEvaluateGeneralExpression` iterated a series until `diff > 1E-10 && iter_count < 30`, with both the epsilon and the cap hardcoded and neither reachable by the user, who saw only a number. They are now named constants, of which the diagnostic quotes one: `max_terms` appears in the message, `tolerance` only in the README. The cap is 100 rather than 30 so that a geometric sequence reaches the tolerance, and it cannot go much higher: each term of a recurrence nests one more reference, so the term budget is bounded by `ReferenceStack::max_depth`. Making either settable is deferred — no requirement asks for it. Its other half — a `size_t index` computed from a subtraction of `int`s, which wrapped for a negative result — went with C17. |
| C9 `[fixed]` | `numeric_interface_imp<std::complex<T>, false>` provided no `sqrt`, yet its own `abs` called `numeric_interface<T>::sqrt` — which resolved only because `T` happened to be `double`. `Matrix<T>::sqrt` threw unconditionally. The interface was only accidentally complete for the one type actually instantiated, and `zero()`/`one()` were never defined for `Matrix` at all. Fixed by deleting `sqrt` (nothing asks the value type for one; `^0.5` goes through `pow`) and by stating the rest as a `Numeric` concept, which `zero()` and `one()` are deliberately not part of. |
| C14 `[fixed]` | **Four `catch` blocks discarded errors inside a constructor.** `parameters.hpp:28, 38, 105, 115` caught `SubVisitor`/`ParametersVisitor` exceptions in the `ParametersDefinition` and `ParametersCall` constructors and returned, leaving the object half-built — `parameters_names_`, `index_name_`, `a_` and `b_` never assigned. The errors never reached `Eval`, so the phase 3 error channel did not surface them; they were invisible by construction. A sibling of C13 rather than an instance of it: there the interpreter has nothing to report, here it had something and threw it away. Fixed with C4: two of the four catches guarded `SubVisitor` and went with it, the other two are gone and `f(x=1, y)=x+y` is a golden. |
| C15 `[fixed]` | **The parser read one token past the end.** `ParseParameters` tested `Peek().type` after `ParseMatrix` had consumed the input — an `end()` dereference before the token cursor became an index, `vector::operator[](size())` after. Neither ASan nor UBSan catches it while the index lands inside the allocation, which for a vector with spare capacity is most of the time; `_GLIBCXX_ASSERTIONS` does, and sanitizer builds now define it. `f(1+2` is the repro and is a golden. A sweep of all 117 prefixes of ten representative inputs found no other instance — only C1. |
| C16 `[fixed]` | **`f=f` crashed on an uninitialised pointer, before recursing at all.** `RecursiveExprVisitor::to_transform_` was assigned only when descending into a child, but the constructor visits the root directly — so a self-reference at the root of a definition read an indeterminate `PExpression<T>*` and then wrote through it. The stack was ten frames deep, not overflowing; ASan does not flag a read of an uninitialised pointer, which is why this looked like C1 for several rounds. Fixed by initialising the member and not wrapping a root self-reference, which has no enclosing slot to substitute into. `f=f` and `w_n=w` are goldens and now reach the budget. |
| C17 `[fixed]` | **Indexed clauses are keyed by `size_t`, so a negative index destroys their ordering.** `reference.hpp` declares `std::map<size_t, ExpressionDefinition<T>>` while `ParametersDefinition::b()` is `int`, so `f_(-1)=0` stores at key 18446744073709551615. Lookup round-trips through the same conversion and appears to work, but `TryEvaluateGeneralExpression` picks its starting term with `indexed_expr_.rbegin()`, which then finds the negative clause as the *largest*. With `g_(-1)=0`, `g_0=5`, `g_n=g_(n-1)+1`, `g_1` is a correct `6` while bare `g` is `34`. The same signedness confusion is half of C8: `size_t index = ai_parameters.b() - gen_params_def.b()` wraps for a negative result. Matters now because an explicit base case at a negative index is exactly how the implicit zero would be written out. Fixed by keying the clause maps `long long`, which also stops the subtraction wrapping. |
| C13 `[fixed]` | **Nothing ever fails, which is why everything else survived.** An unknown identifier evaluates to `0`. Surplus arguments are dropped. A missing parameter is looked up by name in the enclosing scope — `h(x)=x^2` with `x=5` in scope makes bare `h` evaluate to `25`, which is dynamic scoping arrived at by accident. A non-integer index is truncated. None of these reports anything. The interpreter always returns a number, so a typo and a correct series are indistinguishable from the outside; that is how a decade of defects stayed invisible. Root cause of the plausibility of C1, C2, C5, C10 and C11 alike. Arity, the undefined name and the truncated index are all diagnostics now, and the dynamic-scoping half went with phase 4 item 5 — but the class is not closed: C25, C26, C27, C29 and C30 are the same shape, found by the second pass. |

### Design and dead weight

| # | Defect |
|---|--------|
| D1 `[fixed]` | `template <typename T> using PExpression = std::shared_ptr<Expression<T>>` is written out identically in six headers. |
| D2 `[fixed]` | `sequence.hpp` is included by nothing. `pmath.hpp`/`pmath.cpp` define a `fact()` that nothing calls — superseded by `numeric_interface<T>::fact`, which is a verbatim copy of it. `main.cpp` carries a dead `inkamath_test()` function that duplicates the test data. |
| D3 `[fixed]` | `interpreter.hpp` includes `expression_visitor.hpp` *in the middle of the file*, after the class definition, to break a circular dependency. `make_matrix_array_from_vector` is called four lines before it is declared and resolves only through ADL at instantiation. |
| D4 `[fixed]` | `Matrix<T>` owned a raw `T*` with `new[]`/`delete[]`, copied it with `memcpy` (undefined for any `T` that is not trivially copyable), had no move constructor or move assignment, and exposed `Matrix(const T&)` as an implicit converting constructor. Its `std::vector` constructor could leak on exception and carried the author's own note: `// todo : reimplement this matrix class...`. Rewritten on `std::vector<T>` with the rule of zero, explicit constructors and an `Extent` for the dimensions. 385 lines become 184, plus a 19-line header; every golden is byte-identical, and so is a sweep of twenty matrix operations and errors — a sweep which, as C22 and C23 show, contained no matrix wider than two and no exponent above two. |
| D5 `[fixed]` | `dynarray` was a hand-rolled container written while waiting for a `std::dynarray` that C++14 never shipped. `std::vector` covered every use here, and value-initialises where `new T[n]` left `size_t` elements indeterminate. |
| D6 `[fixed]` | `Expression` exposed `children` as a public mutable member while subclasses also offered `m_e1()`/`m_e()` accessors over the same storage, with mutable overloads of their own. `children_` is private now, reachable through a const `Children()` for the generic case and the named views for the rest, and none of them can write: the tree is immutable once parsed. `Clone()` deep-copied subtrees that `shared_ptr` already lets us share — its last caller was the recursion machinery phase 4 deleted, so it went too, along with `transform_visitation`. |
| D7 `[not a defect any more]` | `ExpressionVisitor` had eleven pure virtual `visit` overloads plus two that defaulted to returning `{}` — the recursive nodes. Those two went with the machinery in phase 4, so every overload is pure virtual and forgetting one is a compile error in both visitors, not a silent mistake. What remains is that adding a node type touches two visitors, which is the check working. |
| D8 `[fixed]` | Reserved identifiers: `_EXPRESION_EPSILON` (misspelled, and unused) and `_NUMERIC_INTERFACE_PRECISION`. A leading underscore followed by a capital is reserved to the implementation. |
| D9 `[fixed, differently]` | `Interpreter<T, U = Matrix<T>>` templates on the token scalar `T`, but every AST node is instantiated on `U`. Every literal in every expression was therefore a heap-allocated 1×1 `Matrix<complex<double>>`. Measured: those 16-byte allocations are 44–67% of all allocations, but removing them is worth only 0–15% of the time — they are cheap and hot in cache. A 1x1 matrix now keeps its cell inline, which gets that saving for ten lines; the scalar/matrix *split* the plan prescribed is not justified by the numbers. |
| D10 `[fixed]` | `getlines.hpp` reimplements line iteration on top of `std::iterator`, deprecated since C++17. Its only user was the Boost test file. |
| D11 `[fixed]` | Comments and commit history were in French, the README was half French and half English, and the public documentation described behaviour the code does not have — not only C5 but the implicit limit, the dynamic scoping and the three kinds of definition, all of which phase 4 removed. The README is rewritten in English and is now executable, so that half cannot recur. The sources followed as their code was rewritten; what was left at the end was two French words, two misspellings and the French habit of a space before a colon. The commit history stays French, and so do the two quotations of the 2014 README in this file: a citation translated is a citation weakened. |

---

## Verified defects — second pass

Found after phase 6, by a review that ran the interpreter rather than reading
it. Every entry was reproduced at `3fedbcd`; the input shown is the whole
repro. Most predate the modernization and survived it because nothing
exercised them — the golden corpus never built a matrix larger than 2x2, never
raised one to a power, and never typed a line that was only a comment. Three
are the modernization's own: C18 is a regression, C20 and C29 are claims it
made that are not true.

### Crashes

| # | Defect |
|---|--------|
| C18 `[fixed]` | **A comment-only line kills the process.** `# hello` segfaults. `Lexer`'s `case '#': return;` leaves before its own `if (m_tokens.empty()) Fail("empty expression")` guard, and `Eval` then reads `m_tokens[0].type == Query`. A **regression**, introduced with `?` in `e53a564`; the same input printed `0` at its parent. `basics.ink` tests a trailing comment and never a line that is only one. Fixed by making `#` skip to the end of the line rather than leave the function, so a comment-only line reaches the same `empty expression` diagnostic as a blank one. |
| C19 `[fixed]` | **An empty matrix kills the process.** `[]`, `[ ]`, `[;]` and `[]+1` segfault. `ParseMatrix` accepted zero elements and built an n×0 `MatExpression`; `EvaluationVisitor::visit(MatExpression*)` then called `rj_cols.back()` on an empty vector. Fixed in the parser: a matrix with no elements has no extent to give. `[1;]` still pads to `1 0`, which is a row that is merely short. |
| C20 `[fixed]` | **The evaluation budget covers reference lookups and nothing else.** Both the recursive-descent parser and the AST fold are unbounded C++ recursion: `(` ×8000 segfaults while parsing (4000 is fine), and a flat `1+1+…` of 50000 terms segfaults while folding. `README.md` §6 claimed "a runaway recursion is reported rather than crashing the process" and C1 read as though the whole class was closed; both were true only of recursion through a name. Fixed with a limit on the token count of one line, which bounds every recursion a line can provoke — the parser's, the evaluator's and the destructor's — since the tree has at most one node per token. 1000, against a measured overflow at about 2000 nested parentheses under the sanitizer and about 8000 without it. A crude bound, but one check covers the class, where a parser depth limit would leave the flat case building a tree too deep to destroy. Later bounded by depth instead, which is what the recursions follow: the parser's nesting and the tree's, each at 1000, so that a flat literal of thousands of cells is one line; the length is bounded at 100000 tokens only for the memory C57 found (next in line, *A line bounded by its depth*). |
| C21 `[fixed]` | **The REPL never exits on end of input.** `printf '1+1\n' | ./build/inkamath` loops forever: `getline` fails, `s` stays empty, and `error: empty expression` is printed until the process is killed. `src/main.cpp` checked `s=="q"` and never `cin`'s state. Fixed by breaking on a failed `getline`. A regression here hangs rather than fails, so the test is a `ctest` entry with a timeout — the first coverage the REPL loop has ever had. |

### Wrong answers

| # | Defect |
|---|--------|
| C22 `[fixed]` | **A matrix with three or more rows or columns cannot be built.** `[1 2 3]`, `[1;2;3]`, `[1 2 3;4 5 6]` and `[pi, e, 1]` all give `error: Out of matrix range.` In `EvaluationVisitor::visit(MatExpression*)` the block-offset prefix sum is `rj_cols[j] += j_cols[j-1]`, reading the unmodified source array, so it yields `{w0, w0+w1, w1+w2}` instead of a running total; `rm` is then too small and the result is allocated undersized. Correct for two blocks, which was every size the corpus and the README used. Fixed by writing the prefix sum plainly — it is shorter than the rotate-and-zero it replaces — and the corpus gains matrices three blocks wide and three tall, plus a cell extended to fill its block. |
| C23 `[fixed]` | **Matrix exponentiation computes the wrong power.** `Matrix::pow` does `r = a; for(i = 1; i < n; ++i) r = r*r;` — squaring the accumulator, so `a^n` is `a^(2^(n-1))`. With `a=[1 1;0 1]`: `a^2` is right by luck (one iteration), `a^3` gives `a^4`, `a^4` gives `a^8`. `a^0`, `a^0.5` and a negative exponent all returned `a` unchanged, with no diagnostic. Fixed: the accumulator starts at the identity and is multiplied by `a` once per power, and the three cases that have no answer — a fractional exponent, a negative one, and a matrix that is not square — say so. That also bounds the loop, since the exponent is now the iteration count rather than a doubling. |
| C24 `[fixed]` | **The interpreter gives different answers on GCC and Clang.** `a=1` then `g=(a=a+1)+a` then `g` prints `3` under GCC and `4` under Clang. Every binary node evaluates `m_e1()->accept(*this) OP m_e2()->accept(*this)`, and C++ does not order the operands, so any expression containing a definition is compiler-dependent. Both builds passed `ctest`. Fixed at the root rather than by removing what observes it: `EvaluationVisitor`'s binary nodes now sequence their operands left to right. `aaa+bbb` is the regression test, which needs no side effect at all — it reported `bbb` under GCC and `aaa` under Clang. Left to right is also the order phase 8 needs, so this is its prerequisite rather than a workaround for C29. |
| C25 `[fixed]` | **A keyword argument is never checked against the parameter names.** `CheckArity` counts arguments and never compares names, so `f(x)=x+1` called as `f(y=1)` passes, leaves `x` unbound, and lets it fall through to a global: with `x=99` in scope the answer is `100`. A duplicate is accepted the same way — `g(x,y)=x*10+y` called as `g(1,x=2)` discards the positional argument and answers `27`. Fixed by checking names rather than only counting: an unknown keyword, a keyword that repeats a positional argument, and a keyword that fills an optional parameter while leaving a required one empty (`k(a,b=5)` called as `k(b=2)`) each say so. That last case the count alone could never have caught. |
| C26 `[fixed]` | **An index on a plain definition is discarded in silence.** `m=5` then `m_3` prints `5`; so does `m_(-2)`, and `pi_7` prints `3.14159265`. `EvalImp` returns the plain clause before it ever looks at whether an index was supplied. `?m_3` on the same definition *does* report `m has no clause for index 3`, so the two paths disagree. The last survivor of the class C13 set out to end, and `references.ink:98` recorded it without saying so. Fixed in both paths, which now give the same message; the golden moved and says what it is testing. |
| C27 `[fixed]` | **`lim` reports a limit for sequences that have none.** `Converge` seeds `previous` with a default-constructed `T`, then compares the first real term against that fabricated zero, so `u_n=n-1` — which diverges — gives `lim u` = `0`. The same seed breaks a matrix-valued sequence with an unrelated message, since the first subtraction is 2x2 minus 1x1. Separately, the stopping test `!(diff > tolerance)` is true for NaN, so `w_0=2; w_n=w_(n-1)^2; lim w` answers `inf*-nan` instead of reporting non-convergence. Fixed: with no base clause the first term is not compared to anything, and the test is `<= tolerance` so that NaN counts as not converged. What remains is inherent to an absolute tolerance rather than a defect — `v_n=n/100000000000` still reports `2e-11`, because consecutive terms really do agree to 1e-10; `lim` reports the first term within tolerance of its predecessor, which is what the README says it does. |
| C28 `[fixed]` | **An out-of-range exponent is converted to `int` unchecked.** The C5 fix routes any exponent with `imag()==0 && real()==floor(real())` through `static_cast<int>(b.real())`. `2^2147483648` gives `0` and `0.5^3000000000` gives `inf*-nan` — undefined behaviour, and silently wrong either way. GCC's `-fsanitize=undefined` does not include `float-cast-overflow`, so the sanitizer job does not see it; adding that check to `INKAMATH_SANITIZE` would. Fixed by taking the integer path only for an exponent that fits: `2^2147483648` now overflows to `inf`, which is the answer. |
| C29 `[fixed]` | **A definition that is not at the root still evaluates its left-hand side.** `Interpreter::Eval` handles a definition as a statement only when it is the whole input; anywhere else `EvaluationVisitor::visit(EqualExpression*)` binds and then returns `m_e1()->accept(*this)`, which is the C10 mechanism. So `1+(b=3)` is `4`, `0+(h(x)=x^2)` is `error: x is not defined`, and `0+(p=p)` exhausts the depth budget. A definition's index is evaluated at bind time too: `g_(1+zzz)=5` reports `zzz is not defined` and binds nothing. C10's own entry is worded correctly — "a **top-level** definition is a statement" — but phase 4 item 3 and `README.md` §3 drop the qualifier and so claim more than is true. **Decision: a definition nested in an expression becomes a syntax error**, as a step towards phase 8 rather than as a verdict on the idea. The construct was meant to bind a local reusable later in the same expression — the 2014 README says so: "L'assignation étant une expression comme une autre, on peut trouver une assignation aussi bien dans la liste des paramètres d'une référence ou dans la partie droite d'une autre assignation." Measured, it never delivered that. It was compiler-dependent — `(t=3)+t` was `6` under Clang and `t is not defined` under GCC — until C24 fixed the order, and it is now `6` on both; that removed the strongest argument for the error, since C24 no longer depends on it. What remains: since phase 4 item 5 a local cannot see the parameters it exists to capture — `f(x)=(t=2*x)+t` then `f(5)` reports `x is not defined`, because `t` holds `2*x` and is evaluated in a frame that sees only globals. At the top level the binding is not local either: `(t=3)+t` leaves `t` defined as `3`. Phase 8's prototype fixes both without an error, so the decision stands only as a stepping stone and should be revisited against that phase rather than taken as settled. The error costs one function body — `EvaluationVisitor::visit(EqualExpression*)` becomes a throw — and removes nothing phase 8 would reuse, since that body gets the scope, the timing and the value semantics all wrong. `EqualExpression` itself stays: the parser, the statement path in `Eval`, and `ParametersVisitor`'s keyword arguments are its other three users. `(a=2)` alone keeps working, because parentheses build no node and the root is still a definition. **Resolved as the local, not the error** (phase 8): the throw was never written. An evaluated line opens a scope and a bare name bound inside one takes its value there, which gives the construct the meaning the 2014 README described and leaves `(b = 7)` alone a definition. |
| C30 `[fixed]` | **Default arguments are evaluated when they are not used, and in the wrong scope.** `f(x,y=zzz)=x` then `f(1,2)` reports `zzz is not defined`, although `y` was supplied and `zzz` is never needed: `SetCallParameters` evaluates every entry of `parameters_dict_` before the positional ones. They also resolve in the *caller's* scope, so `x=100` then `f(x,y=2*x)=y` then `f(5)` gives `200` rather than `10` — against `README.md` §3, which says a name inside a definition resolves to that definition's own parameters first. Fixed by binding defaults after the supplied arguments rather than before: only a parameter the call left empty gets one, and it is evaluated in the callee's frame, where the definition's other parameters are visible. |
| C31 `[fixed]` | **A NaN imaginary part prints as malformed output.** `numeric_interface_imp<std::complex<T>>::toString` tests `imag > 0` and `imag < 0`, both false for NaN, so no `i` is emitted — but the following `imag != 1 && imag != -1 && imag != 0` is true, so it appends `"*" + toString(imag)`. `1/0` prints `inf*-nan` and `0/0` prints `-nan*-nan`, neither of which the lexer can read back. Everything else in that function was correct, including negative zero and infinities. Fixed by asking whether each part *is* zero rather than how it compares to zero, so a NaN counts as present and keeps its `i`: `1/0` reads `inf+i*-nan`. GCC and Clang agree on the sign, so the goldens can pin it. |

### Cost

| # | Defect |
|---|--------|
| C32 `[fixed]` | **Parsing a nested call is exponential.** `ParseEqualExpr` speculatively parses name, parameters and subscript for any `Parse()` beginning with an identifier, then on finding no `=` rewinds and parses the same text again; nesting multiplies. `f(f(f(…1…)))` takes 0.04s at depth 16, 0.59s at 20, 2.34s at 22 and 9.34s at 24 — a factor of four every two levels. Depth 40 is a 121-character line that does not finish in a minute. The cursor restore itself is correct on every path; the defect is cost. Parenthesis nesting alone is unaffected, and so is `f(1*f(1*…`, which never enters the speculative path. Fixed by not rewinding: when there is no `=`, the left-hand side already parsed is a perfectly good leading operand, so it is handed to `ParseAddExpr` instead of being thrown away. Depth 200 now parses in 5ms. Thirty-one representative inputs give byte-identical output either way. |

### Design and documentation

| # | Defect |
|---|--------|
| D12 `[fixed]` | **The `Numeric` and `Parsable` concepts do not state what they claim to.** `Numeric` omits five things `Interpreter` requires of `U` — `value_type`, construction from the token scalar, `Size()`, construction from `Extent`, and `operator()(size_t,size_t)` — so a type can satisfy it in full and still fail to compile. `Parsable` rejects nothing at all: a *declaration* satisfies a `requires` expression, so `Parsable<int>` is true and the failure is still only the link error that existed before the concept. Both were added in `3b12778`, whose message claimed more than they delivered. `Numeric` now names all five, and the concepts move to `interpreter.hpp`, which is where they are used and the only header that sees both `Extent` and `numeric_interface`. `Parsable` gains a `numeric_interface_parses` trait specialised beside each definition, since a `requires` clause cannot tell a declaration from a definition. Checked by static assertion: a type with the arithmetic and none of the matrix surface is rejected, and `Parsable<int>` and `Parsable<float>` are both false. |
| D13 `[fixed]` | **`Matrix`'s diagnostics are in a second voice.** `Out of matrix range.`, `Incompatible dimensions in matrix operation.`, `Fact is not implemented for Matrix type.` and `Incompatible dimension in matrix assigmentation. Conversion` are capitalized, punctuated, newline-terminated and in one case misspelled, against `interpreter.hpp`'s own rule that messages are lower case, unpunctuated and quote what the user typed. All are reachable from a one-line input. Part of D11's remainder, and larger than that entry admits. Rewritten in the one voice, and made to say something while they were being touched: the subscript error now names the subscript and the extent it was outside, rather than only that something was. All six are goldens now. |
| D14 `[fixed]` | **Comments cite README sections that phase 6 deleted.** `references.ink:1` and `:15`, `sequences.ink:1`, `errors.ink:57`, `matrices.ink:1`, and `parameters.hpp:14` and `:28` all cite the old French numbering (§3 Matrices, §4.1/4.2/4.3 references). `errors.ink:57` goes further and attributes to the README a statement it no longer contains. `sequences.ink:46` cites C4 for the implicit zero, which is phase 4 item 4. The `readme` test replays fenced blocks and so catches none of this. All seven citations renumbered to the current sections, `errors.ink` no longer attributes a sentence to the README that is not there, and `sequences.ink` cites phase 4 item 4 rather than C4. |
| D15 `[fixed]` | **`CLAUDE.md` promises a list that does not exist.** §3 says some recorded outputs are deliberately wrong and "are listed in `DESIGN.md`". The only list there was phase 0's, naming C5 and C6; both are fixed and both goldens have moved. `references.ink:98` (C26) was the first entry a restored list would have needed, and C26 is fixed, so the list would be empty. §3 now asks for the comment on the entry itself plus a register entry, which is what the goldens already do and what does not rot. |
| D16 `[fixed]` | **Two smaller ones.** `numeric_interface_imp<std::complex<T>>::one()` returns `(1,1)` rather than `(1,0)`; latent, since nothing instantiates it. `reference_stack.hpp`'s `friend struct Frame;` declares a namespace-scope `::Frame` as a friend, not the nested `Frame` on the next line, which needs no friendship at all. |

### What the review confirmed

Worth recording, so it is not re-derived. The historical claims in the first
pass are accurate: the 2014 baseline was rebuilt and the *old* behaviour
reproduced for C1, C2, C5, C6, C10, C12, C13, C14 and C16 before confirming
each is gone. Goldens re-record byte-identically. `-Werror` is clean on GCC
and Clang and `ctest` is green on all three configurations. The three items
marked as deliberately not implemented as written — D9's mechanism, half of
C9, and D7 — match the code as it stands, and phase 4 item 2's measurement
reproduces: the residual in `lim exp(1)-e` is the tolerance and not the cap,
and does not move between `max_terms` of 100, 200 and 1000. The figure itself
is now `-8.149037e-13`, C34 having corrected the constant it subtracts.

The copy-on-write definition store has no lifetime or aliasing hazard under
sixty sanitizer fuzz runs; frame push and pop are balanced and exception-safe;
the `size_t` underflow in `CheckArity` is unreachable by construction; and
outside C18 there is no out-of-bounds token read, checked by running every
prefix of a 35-input corpus and twelve thousand random lines under the
sanitizer build.

---

## Verified defects — third pass

Found two ways: by sitting at the prompt and writing what a student would
write -- factorials, Fibonacci, Newton's method for a square root, a sum of a
series, compound interest, binomial coefficients, the roots of a quadratic,
then matrices -- and by reading the code adversarially for what a session
would not show. Neither way finds the other's: no session would have shown
C59, and no amount of reading would have shown C48.

| | |
|---|---|
| C65 `[fixed]` | **A class-type number stopped compiling at phase 10, and nothing said so.** The comparisons ask `numeric_interface` for a value's real and imaginary parts, and the specialisations for `double` and `complex` answer; the generic path -- the one every class goes through -- forwarded neither, so `Interpreter<Rational>`, which *Other number systems* cites as running, failed inside `matrix.hpp` from phase 10 on. The same shape as C44: a type satisfying everything the concepts state, rejected deep in the template. Found by rerunning that experiment while specifying phase 13, whose number type is a class and is the test: two forwarding lines, without which it does not build. |
| C60 `[fixed]` | **An arrow key trapped the prompt.** The REPL read lines with `getline`, so an arrow arrived as its escape sequence, `\x1b[D`, and the `[` in it counted as an open bracket: `1+2`, Left, Enter asked `..` for the rest of a line that had never been open, and kept asking until a `]` was typed. C52 made that possible by continuing a line whose brackets are open; before it, an arrow was only an `unexpected character`. At a terminal the prompt now edits the line -- the arrows, Home and End, history on Up and Down, Ctrl-C to drop a line -- in a header of its own rather than a library, which §5 rules out and which for GNU readline would have made the binary GPL. What keeps it small is that the language is ASCII: the hard part of a line editor is how wide a character is, and nothing wider than one column can be part of a line here. One trap on the way: switching the terminal's mode with `TCSAFLUSH` discards what is waiting to be read, and a pasted matrix is several lines that arrive at once -- the prototype kept the first and hung at `..`. A pipe, a file and the transcripts are read exactly as before. The Windows console half is checked by CI for building and nothing more: no runner has a console to press an arrow key in. |
| C59 `[fixed]` | **`isalpha` and `isdigit` were called with a plain `char`.** Passing a negative value to a `<cctype>` function is undefined, and every byte of an accented letter typed at the prompt is negative on the platforms where `char` is signed; `Reference_Lexer` and the lexer's default case did it on each one. glibc happens to answer for the whole signed range and no sanitizer says a word, so nothing here observably misbehaves -- which is why three of the six call sites had the cast and three did not. All six have it now. No test: a test can only assert what the platform already does, and the next platform is the one that would break. |
| C58 `[fixed]` | **A tab was an error, and so was every line of a file written on Windows.** The lexer's whitespace case was `' '` and nothing else, so `1<tab>+2` answered *unexpected character* -- and since `getline` leaves the `\r` of a CRLF line in place, piping such a file to the REPL made every line of it an error, naming a character that prints as nothing. Anything pasted from an editor that indents with tabs failed the same way. A tab and a carriage return separate as a space does now. |
| C57 `[fixed]` | **The token limit ran after the lexer had finished.** C20 bounds a line at 1000 tokens so that no recursion a line can provoke overflows the stack, and the check sat below the loop that fills the vector: twenty million brackets became twenty million `Token` objects -- 1.84 GB and 7.75 seconds -- before the limit had a word to say, and what it then said was the refusal it could have given at token 1001. The count is tested inside the loop, and the same line now costs 41 MB and 0.30 s. A limit written against one cost read as though it bounded another: the recursion was bounded, the work of getting there was not. |
| C56 `[fixed]` | **A value no index can hold was cast to an `int` anyway.** `AsIndex` asked `toInt` for the integer -- a bare `static_cast<int>` on a double -- and then compared the answer with the value it came from to see whether the value was whole. Everything out of `int`'s range, infinity and NaN included, therefore went through undefined behaviour *before* the check meant to catch it; GCC's sanitizer says nothing without `float-cast-overflow`. The diagnostic was wrong as well: `m[2147483648,1]` answered *an index must be a whole number*, sending the reader to look for a fraction that is not there. `toInt` clamps, and out of range is now its own complaint. |
| C55 `[fixed]` | **Two clauses with different guards could share one name.** A guarded clause is named by its left-hand side as the tokens spell it (C46), and the tokens were joined with nothing between them: `x < [1 2][1,1]` and `x < [12][1,1]` both spell `x<[12][1,1]`, so writing the second replaced the first and the definition silently lost a case. Any two guards that differ only in where one number ends and the next begins collided. The tokens are kept apart now, which is what *as the tokens spell it* was meant to say. |
| C54 `[fixed]` | **`lim` did not go through clause dispatch, so guarded clauses did not exist for it.** `Converge` called the general clause's expression directly, and `General()` is an unguarded-only predicate, so a limit and the terms it claims to be the limit of were two different sequences: a definition whose guarded clause halves each term had its limit computed from the unguarded clause that multiplies by ten, and a definition whose *only* general clause was guarded was told it had none. Every term now goes through `EvalImp`, the seed included, and a guarded general clause counts as one. What this cannot fix, and what cost me a golden: a sequence that is flat for five terms and then jumps is *correctly* called converged at the flat value, because no finite step test can see the jump coming. |
| C53 `[fixed]` | **A guard that did not hold left its index bound.** `Selects` wrote the index into the callee's frame before evaluating the guard and never removed it, so a clause that did not answer still shadowed a global (`rg_0 = nn` read `0`, not the global `7`), clobbered an argument the call had just bound, poisoned the guards of the clauses after it, and could give a name a value it never had anywhere. The index is bound on trial now -- a small RAII on `ReferenceStack` that restores the slot unless the clause is the one that answers. |
| C52 `[fixed]` | **A printed matrix read back as a different value.** `1-[1 2;3 4]` printed `0 -1 / -2 -3`, and typing that back gave `-1 / -5`, because a space before a signed element does not separate: `0 -1` reads as `0-1`. `README.md` section 2's own worked example was not re-enterable, and neither was any matrix with a negative cell. Commas alone would not have been enough -- the prompt reads one line and a matrix prints on several -- so the two halves are: a matrix prints as **the literal that would produce it**, columns right-aligned so the grid survives, and the prompt **continues a line whose brackets are open**, asking `..` for the rest. What is printed can now be pasted back over the lines it printed on. A 1x1 still prints as its bare value, which is what every scalar answer and every diagnostic quoting one is. Every recorded matrix moved, in this commit; the `matrix` unit tests and `README.md` with them. A typed literal kept the trap until a sign with a space before it and none after it was made to begin an element, as in MATLAB (next in line, *A sign that begins an element*). |
| C51 `[fixed]` | **A clause could name parameters the definition does not have.** `g(x) | x < 0 = 0-x` followed by `g(y) | 1 = y` was accepted, listed by `?g`, and uncallable: `Eval` binds the arguments once, using `clauses_.front()`'s names, so `g(3)` bound `x` and clause two's body read the *global* `y` -- `99` where `3` belongs, or `y is not defined` where there is no global. The same for a clause with more parameters, or with a default the first clause lacks. One call binds the parameters once for whichever clause answers, so the clauses have to agree; one that disagrees is refused where it is written. A plain definition still clears the name, so there is a way to change them. |
| C50 `[fixed]` | **The memo key held an argument's cells and not its shape.** `gg(m)=m+m` answered `2 4 / 6 8` for both `gg([1 2;3 4])` and `gg([1 2 3 4])`, whichever was asked first deciding for both, because `MemoKey` appended `count()` raw cells and nothing about rows and columns. It could also make an error disappear: a cell access that is out of range for the second shape returns the first shape's answer. The extent goes in the key, which incidentally makes each argument's run of bytes self-delimiting -- the separator was a bare NUL and the values are doubles that contain NULs. |
| C49 `[fixed]` | **A local's answer was remembered as the global's.** The phase 9 comment claimed *"only a global gets here: a frame holds plain, unindexed bindings"*, and that is false: a definition written inside an expression carries its whole left-hand side, so `(f_0 = 99)` binds an **indexed** clause into the line's frame, which is memoisable, and `Set` skips clearing the cache while a frame is up. `f_0 = 1` then `(f_0 = 99)+0` then `f_0` answered `99` -- a local outliving its line, which `locals.ink` says in as many words it must not do. The same mechanism in reverse let a remembered global answer for a local just bound, and let a local reach a callee that lexical scoping keeps it out of. The stack now says whether the definition came from a frame, and only a global's answer is kept. Three transcripts, one mechanism, found by an adversarial review. |
| C48 `[fixed]` | **A unary sign bound looser than the `*` and `/` after it.** `6/-2/3` answered `-9` where every calculator and every language says `-1`, because `ParseSimpleExpr` gave the sign `ParseMultExpr()` as its operand, so the minus swallowed the whole multiplicative chain: `6/(-(2/3))`. `8/-2*2` was `-2` instead of `-8`, `2^-3*4` was `0.000244140625` instead of `0.5`. The operand is `ParsePowExpr()`, which still gives `-2^2 == -4` and `-2*3 == -6` -- the only two cases the corpus held. The defect is from 2014 and C37 copied it verbatim into unary plus, six days ago, because the entries written for C37 were exactly the two agreeing cases. Silent wrong arithmetic on an ordinary line, which is the worst class this project has. |
| C47 `[fixed]` | **`!(10^20)` never returns, and takes the session with it.** The factorial counts up in a `T` -- a double -- and at 2^53 `++i` rounds back to where it started, so the loop cannot advance: non-termination in principle, not slowness, for every argument at or above 2^53, and for infinity. C42 had just taught the factorial to refuse a fraction, a negative and a complex number, and this was not on that list: `1e16` is whole, real and positive. `171!` already overflows to `inf`, so the fix is to stop at the first infinite product and answer `inf`, which is what `!171` answered all along. A hung interpreter is worse than a wrong answer -- every definition in the session goes with it. Found by an adversarial review of the value layer. |
| C46 `[fixed]` | **A guarded clause could not be corrected.** It was appended and never replaced, so writing `abs(x) \| x < 0 = 0-x` again with a different body left the old clause in front of the new one and the correction did nothing -- in a language whose definitions are built at a prompt, by trial. The unguarded shapes name themselves (a plain definition, an index, the general term); a guard needed a name, and it is its left-hand side as the tokens spell it, which is the same however it was spaced. Found by the question *how do you overwrite a clause?*, which had three answers and should have had one. |
| C45 `[fixed]` | **Replacing a clause moved it to the end, and order is what dispatch follows.** Re-typing `root_0 = 1` -- the same text, the same value -- put the base clause behind the guard that reads the term before it, so the guard was reached at index zero, asked for `root_(0-1)`, and ran to the depth budget. A definition broke because it was re-entered unchanged. The cause was an erase followed by a push back, from the phase 10 refactor that made clause order meaningful in the first place; clauses are replaced in place now. |
| C44 `[fixed]` | **The concepts do not ask for what the product needs.** `Matrix::mul` accumulates with `c(i,j) += ...`, and neither `Numeric` nor `Parsable` mentions `+=` on the cell type. It goes unnoticed because the concept checks `a*b` as an expression and a template body is not instantiated by overload resolution, so the requirement only bites when someone supplies a number type and gets a template error inside `matrix.hpp` rather than a concept failure at the interface. Found by instantiating `Interpreter<Rational>`: a type that satisfied everything the concepts state still failed to compile. Stated, in the one line `Numeric`'s requires-clause had room for. Writing the accumulation as `c(i,j) = c(i,j) + ...` instead would have asked less of the type at the cost of a copy per term, which is the wrong trade for a bignum. No test: the only one that proves it is an out-of-tree instantiation, and a negative concept test -- a type built to satisfy everything but this -- costs more than it protects. |
| C43 `[fixed]` | **A number could not start with the point.** `.5` reported `unexpected character '.'`, while `1e3`, `0x10` and `2i` all lexed -- the exotic spellings worked and the common one did not, because the lexer's case list holds the ten digits and nothing else. `strtod` reads `.5` without being asked; only the dispatch was missing. A point that does not begin a number still reports the same message, so `a.b` is unchanged. |
| C42 `[fixed]` | **The factorial answered for every argument it had no business accepting.** `!5.5` was `120`, `!(0-3)` was `1`, `!(2+i*3)` was `2` and `!i` was `1`. The loop multiplies `i` while `i <= n`, so a fraction truncates, a negative gives the empty product, and the complex layer passed `a.real()` down without looking at the rest, dropping the imaginary part before the loop ever saw it. Four plausible numbers where there should be four diagnostics, which is C13 in an operator nobody had pointed at -- the corpus tested `!5` and `!0` and stopped. Found by typing `!5.5` while checking what the lexer accepts. |
| C41 `[kept]` | **A block that does not fill its band continues with its own last cell.** `[a, [3 4]]`, with `a` 2x2, gives `1 2 3 4 / 3 4 4 4`; `[[1 2 3], a]` fills a whole row with `3 3 3`. The code says so in as many words -- *"extend the previous (up and left) evaluated cell result"* -- so it is deliberate, and read charitably the rule is **a block continues with its last value**, which is an ellipsis: `[a, 0]` pads the band with zeros and `[a, 1]` with ones, and that is a notation a matrix wants. The author does not remember the use case and suspects exactly this: an attempt at the `...` of matrix notation. Where it stops being statable is the multi-cell block, where continuing `[3 4]` with `4 4` is a corner repeated rather than a continuation anybody wrote. It is the third answer to one question -- a literal's short row pads with zeros (README.md section 2), a single value stretches, a short block repeats a corner -- and only the first two can be said out loud. **Decision: kept, and recorded as it is** in `matrices.ink` with the entry saying so. Not turned into a diagnostic while it may still be the residue of an idea; when the idea is found or ruled out, this goes and an explicit notation for the continuation replaces it, rather than a fourth fill rule. Phase 13 leaves `...` unclaimed for it. |
| C40 `[fixed]` | **A matrix could be built and never read.** `a(1,2)` was `a takes no arguments` and `a_1` was `a is not a sequence`: the language had no way at all to get a value back out of a matrix, which is half of what the second of the three ideas is for. `Matrix::operator()` was there, with a good out-of-range message, and nothing in the language reached it. `m[i,j]` now does, one-based as the rows and columns are written, and composing with everything a name can carry -- `f(3)[1,2]` and `s_3[1,2]` both work. The brackets were chosen over parentheses to match array indexing elsewhere, and they collide with the matrix literal in exactly one place: inside a literal, and inside an argument list, a space between two expressions separates them, so `[[1 2] [3 4]]` is a row of two blocks. Outside one, juxtaposition means nothing, and the brackets index whatever is in front of them -- `[1 2;3 4][2,1]` and `(a*a)[1,1]` both work. Inside one, only a name takes an index, which leaves a single form changed: `[a [3 4]]`, a row of blocks whose second follows a name with a space, now reads as an index of `a` and reports that it needs a row and a column. That form was legal, unused in the corpus and in `README.md`, and is written `[a, [3 4]]` instead; the change turns it into a diagnostic rather than a wrong answer. `Matrix::Offset` takes a signed index so that `m[0-1,1]` names the row it asked for instead of one that wrapped. |
| C39 `[fixed]` | **A limit that cannot be taken reports an internal-sounding reason.** `lim k` on a sequence of matrices said `a matrix has no absolute value`, which names neither the sequence nor what the interpreter was doing when it needed one. Every other refusal from `lim` names the sequence -- `k has no general clause, so it has no limit`. It now reads `k has no limit: a matrix has no absolute value`, keeping the reason and adding the context, which also covers the case where the terms change size between iterations. |
| C38 `[fixed]` | **A scalar stretches over a matrix for `*` and for nothing else.** `a*2` and `2*a` worked, `a/2`, `a-1` and `1+a` all reported `these matrices have different sizes`. The scalar case lived in `mul`, which needs one because matrix multiplication does; `BinaryOp`, behind `+`, `-` and `/`, compared extents and gave up. Nothing chose that: `/` is documented as working cell by cell, and a literal already stretches a scalar -- `[a; 1]` spreads the 1 across the block above it. `a*0.5` working while `a/2` did not is the sharp form. Now a single value stretches on either side of all four, with the operand order kept, so `1-a` subtracts each cell from one. |
| C37 `[fixed]` | **`README.md` documents an operator the parser does not have.** Section 1's table lists `+expr` as unary plus; `+5` reported `unexpected '+'`. The table is not a fenced block, so the README replay -- which does catch prose drifting from the interpreter -- never reached it. Found by typing `+5`. Fixed on the parser's side rather than the documentation's: the notation is ordinary, and the surprise costs more than the two lines. |
| C36 `[fixed]` | **`lim` measures the step, not the remainder.** It stopped when two successive terms agreed to 1e-10, which is a statement about how fast the series is moving and not about how far it still has to go. A series whose every step is `1e-11` and whose sum diverges was reported as converging to `1e-11`. Raising `max_terms` — the obvious reading of `lim s` giving up on `1/n^2` — makes it worse rather than better: since phase 9 the cap is no longer technical (a hundred thousand terms cost 250ms, no depth, no step budget), and at a hundred thousand terms the series *does* stop, answering `1.64492407` where `pi^2/6` is `1.64493407`. The cap was the only thing preventing a confidently wrong answer, which is C13's failure mode dressed as a limit. The fix is in the test, not the cap: with the steps shrinking by a factor `r` the remainder is about `step*r/(1-r)`, and convergence now requires that to be under the tolerance as well as the step itself, with at least two steps to form a ratio. It is a conjunction, so it can only refuse where the old test accepted: every recorded output is byte-identical, and `lim` on `1/n^2`, on `1/n^3` and on a divergent series of tiny steps now says so at any cap. |
| C35 `[fixed]` | **The one diagnostic that guesses the user's intent covered the case they are least likely to type.** `3(4)` answered `unexpected '(' -- the operator '*' is probably missing`, while `2pi`, the multiplication every student writes, got a bare `unexpected 'pi'`, and so did `2 3`. Juxtaposition is how mathematics writes a product and this language does not read it; the parse stops at the same place either way, so the hint is the same hint. It now covers an identifier and a number as well as an open parenthesis, and nothing else: an operator or a bracket there is a different mistake. |
| C34 `[fixed]` | **`pi` and `e` stop at fourteen digits.** They were written as `3.1415926535898` and `2.7182818284590`, which is a 7e-15 error in pi and a 4.5e-16 one in e, both far above what a double rounds to. `e^(i*pi)` reported an imaginary part of `4.58636533e-14`, two hundred times the true rounding error, and every series a student checks against a constant inherits it. Writing the digits out costs nothing. It moved one recorded output: `lim exp(1)-e`, from `-7.69606601e-13` to `-8.149037e-13`, because the constant it subtracts is now the right one. |
| C33 `[fixed]` | **Unary minus flips the branch of every fractional power.** `(-4)^0.5` answered `i*-2` where `(0-4)^0.5` answered `i*2` — the same number, two roots, decided by how the minus was written. `std::negate` on a `std::complex` negates the zero imaginary part too, and a `-0` there puts the value just below the branch cut, where the principal root is the conjugate. Negating by subtracting from zero keeps the `+0`. Found writing `(-b + (b^2-4*a*c)^0.5)/(2*a)`, where the discriminant comes out of a subtraction and is right, while the same root typed with a literal negative is wrong. |

### What the fuzzing confirmed

C40 to C43 came from typing at the prompt, so the next pass was mechanical,
and aimed at *answers* rather than crashes -- the crash surface was swept in
the second pass. Four kinds of oracle, about forty thousand checks:

| | |
|---|---|
| **Against Python** | 10,300 fully parenthesised scalar expressions, then 9,000 with no parentheses at all, which tests the precedence table itself rather than the arithmetic under it -- **but whose generator emitted no unary signs**, and so missed C48 entirely; teaching it to write `6/-2/3` found 406 disagreements in 3,000 cases, and none after the fix; 1,500 powers with fractional, negative and complex exponents over negative and complex bases, which is where C33 lived. |
| **Against a model** | 1,650 random two-term recurrences, six queries each, including indices below the lowest base clause and negative ones; 2,800 expressions containing locals, against a model of left-to-right evaluation and line-scoped binding, with a probe that nothing survives its line. |
| **Against itself** | 540 random matrices through eleven algebraic laws -- distributivity, `A^3` against `A*A*A`, `(A*B)[1,1]` against the definition of the product -- and 500 more across rectangular shapes and block round-trips. |
| **Against a fresh process** | 1,500 programs asking a query twice, then redefining what it depends on and asking again, against a process that started from the redefined value. This is the only oracle that can see a stale memoised answer, and it is the reason to keep it. |

Phase 10 added a fifth, and the one worth keeping: **2,400 editing sessions**
against a model of the clause rules written from `README.md` -- random
definitions of one name in every shape, plain and indexed and general, guarded
and not, interleaved with the queries that read them back and with `?f`, whose
whole listing the model predicts. It watches the list itself and not only what
the list answers, which is where C45 lived.

A clean run means nothing until the oracle is shown to fail on purpose, so it
was run against three deliberately wrong models: the plain clause answering
first, a replaced clause appended rather than replaced in place (C45 exactly),
and a dead guard accepted rather than refused (C46's neighbour). All three were
caught, in 4, 11 and 43 sessions respectively. Then the real rules: 2,400
sessions, no disagreement.

Nothing moved. The three bugs the earlier campaign found were all in the
oracles: a
prompt off by one, a banner counted as an answer, and a Python evaluator that
computed `3^3^3^3` exactly and never came back where the interpreter, working
in doubles, answered `inf` in microseconds. An oracle has to be at least as
robust as the thing it judges.

What no oracle covers, and where the risk therefore still sits: whether `lim`
converges to the right value rather than merely converging, the wording of
diagnostics, `?` round-tripping through its own output, and C41's block
continuation, which was excluded on purpose because there is nothing to
compare it against until it is decided.

---

## Verified defects — found by building on Windows

Nothing had been built anywhere but Linux since the modernisation began. The
first MSVC job in CI compiled once two warnings were dealt with -- a `long long`
index narrowed to `double`, and `getenv`, which MSVC deprecates in favour of
functions that are not standard -- and then failed where the code had relied
on the Linux toolchain without saying so.

| | |
|---|---|
| C61 `[fixed]` | **A whole power relied on a libstdc++ extension.** `pow` sends an integer exponent to `std::pow(complex, int)`, with a comment saying that overload multiplies, which is why `0^0` is `1`. The standard removed that overload; libstdc++ keeps it, and elsewhere the `int` becomes a `double` and the power goes through `exp` and `log`. On MSVC `q_100` in `sequences.ink` answered `0.688172179-i*7.83297859e-16`, and `lim o` reported a last term of `inf` where it is `inf+i*-nan`. The multiplication is written out now, in the order libstdc++ does it, and checked against it bit for bit on 173,290 bases and exponents -- infinities, NaNs, zero and `INT_MIN` among them -- so no answer on Linux moved. |
| C62 `[fixed]` | **How a NaN printed was the library's choice.** A number printed through `ostream`, and a NaN spells itself however the library likes: libstdc++ writes `-nan`, MSVC writes the NaN that `0/0` gives as `-nan(ind)`. So `1/0` and `0/0` in `errors.ink` failed on MSVC with the right values. A NaN now prints as `nan` or `-nan` by its sign, which is what Linux printed already, so no recorded output moved. Whether a NaN should show a sign at all is a separate question, and one for a golden that says so. |
| C63 `[fixed]` | **The token limit was measured on one platform's stack.** C20 bounds a line at 1000 tokens so that no recursion it provokes can reach the end of the stack, and the margin was measured on Linux, whose main thread has 8 MB. Windows gives 1 MB, and an MSVC Debug build -- no inlining, large frames -- overflowed on the line `token limit` checks the limit *accepts*: four hundred nested parentheses. So on Windows a legal line could kill the process, which is the one thing the limit exists to prevent. The executables link with an 8 MB stack there too, which puts the measurement back under the limit instead of lowering the limit everywhere. |
| C64 `[kept]` | **Arithmetic on an infinite complex number is the platform's.** GCC multiplies complex numbers under C's Annex G, which recovers an infinity when the plain formula gives NaN in both parts; MSVC's `std::complex` uses the plain formula. Squaring `inf+i*nan` therefore gives `inf+i*nan` on Linux and `nan+i*nan` on Windows, and `lim o` -- `o_n=o_(n-1)^2` from 2 -- reported a different last term on each. Confirmed on Linux with `-fcx-limited-range`, which gives GCC the plain formula and reproduces MSVC's answer exactly. Kept: making infinity identical everywhere means writing complex multiplication and division into the value layer, fifty-odd lines, for the one corner of the language where both answers are NaN anyway. The entry was about something else -- that a NaN difference between terms is not convergence -- so it now reaches its NaN by addition, which works part by part and is the same everywhere; its input changed, not its expectation to fit. A mutation that reads NaN as converged fails it as it failed the old one. |

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

## Phase 2 — Write the language down `[done]`

The redesign was specified as transcripts before it was implemented, in
`test/data/spec/*.ink`. They used the phase 0 harness, so the specification
was executable: running them printed a diff between the language we had and
the language we wanted. They were registered under a `spec` doctest suite and
marked `may_fail`, so they reported without gating CI, and `record_goldens`
never rewrote them — recording a specification from current behaviour would
have defeated its purpose. The gap closed from 36 failing assertions to none
over phases 3 and 4.

As each part of the design landed, its entries moved into `test/data/` and
became ordinary goldens. With phase 4 complete the directory and the suite
went away; phase 8 designs rather than repairs, so both are back.

This ordering is deliberate. The README and the code disagree today (C5, C10,
C11) because the prose was written once and then drifted. A specification that
is run on every push cannot drift.

## Phase 3 — Failure exists `[done]`

C13 first, because everything else is easier to see once the interpreter stops
answering every question with a number.

1. **An error channel** (C7) `[done]`. `Eval` returns a `std::variant`
   instead of printing to `std::cout` and returning a default-constructed
   value. The REPL prints at the edge, where it belongs;
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
   are not needed either: nothing renders one. The diagnostics quote the
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
   and total steps, reported as a diagnostic. The arithmetic-geometric mean
   runs instead of dying, and a sweep of all 117 prefixes of ten representative
   inputs now runs clean under ASan, UBSan and `_GLIBCXX_ASSERTIONS` — it
   previously overflowed the stack.
5. **Fix the `i` lexing rule** (C2) `[done]`.
6. **`0^0`** (C5) `[done]`. Moved exactly the three predicted lines and left
   `exp(1)-e` unchanged, as forecast when the fix was first measured.

## Phase 4 — The definition model `[done]`

The redesign proper. Replaces C10 and C11 rather than deciding them.

1. **One definition per name** `[done]`. `Reference`'s three parallel slots
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
   The justification given for choosing 100 — that a recurrence nests one
   reference per term against a depth budget of 256, so it could not go much
   higher — is wrong. Measured, a recurrence reaches index 254, so there are
   some 150 terms of headroom, and the cap is what bites a legitimate input:
   `cos(40)`, built from the README's own definitions, reports
   `exp did not converge within 100 terms`. See also C27.
3. **A definition is a statement** `[done]` at the top level, and **only**
   there — see C29, which is this item's unfinished half. It binds and echoes
   what it bound; it evaluates nothing. No left-hand-side lookup, no right-hand-side evaluation,
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
6. **`?name` prints a definition back** `[done]`, as written, without
   evaluating it. On a sequence it prints every clause, so the whole definition is visible
   at once — which the three parallel slots made impossible. Together with
   item 3 this is what makes the core idea legible: after `b = a+a` and
   `a = 2`, `?b` is `b = a+a` while `b` is `4`. Each clause stores the line
   that bound it, so `?` quotes what was typed rather than rendering the
   parsed expression; there is no pretty-printer to disagree with the parser.
7. C3, C4, C14 and C17 were small bugs in machinery this phase rewrote; they
   went away with it rather than being patched first.

## Phase 5 — Value types and the core `[done]`

1. Rewrite `Matrix<T>` (D4) `[done]`: `std::vector<T>` storage, rule of zero,
   explicit constructors, dimensions as one `Extent` type. The author asked
   for this in a comment in 2014. `Extent` also replaces the
   `std::pair<size_t,size_t>` that `Expression::Size` returned, so rows and
   columns can no longer be swapped by a `std::tie` in the wrong order.
   Making the converting constructor explicit turned D9 from a claim into a
   compile error: the one place that relied on it is the literal in
   `ParseSimpleExpr`, which is exactly the per-literal 1x1 allocation item 3
   is about.
2. Delete `dynarray` (D5) in favour of `std::vector`, and delete its test
   `[done]`. 192 lines of container plus 140 of test, for six uses.
3. Stop allocating a 1x1 matrix per literal (D9) `[done, not as written]`.
   Measured first, as the item asked. The allocation is real — 44–67% of all
   allocations across five workloads — but it is *not* the largest single win:
   removing it is worth 0–15% of the time, and nothing at all on the series
   case. `Matrix` now holds a 1x1 cell inline, which buys that for ten lines
   and no change to any caller.

   Separating the types, as this item prescribed, would need a `variant`
   value, four-way dispatch on every operator and two `numeric_interface`
   instantiations, for the same 0–15%. It is ruled out on the size constraint.

   The measurement found the larger win elsewhere: `ReferenceStack::Eval`
   copied the whole `Reference` on every lookup — maps, strings and parameter
   lists — to protect against the name being redefined while its own body
   ran. Definitions are now shared and copied on write, which is both cheaper
   and stronger: the hazard is structurally impossible rather than defended
   against. Worth 24–30% on every recursion-heavy workload. Together the two
   changes are 17% to 33% across the five, and the agm goes from 708ms to
   507ms for 200 evaluations of `gm(1,2)_10`.
4. Replace `numeric_interface`/`best_promotion`/`numeric_interface_imp_types`
   with C++20 concepts (C9) `[done, partly]`. `best_promotion` and
   `numeric_interface_imp_types` are deleted outright: the first served a
   generic `parse` that nothing ever reached, the second declared return
   types that `auto` deduces. `sqrt` is deleted with them.

   `numeric_interface` itself stays. It is not an abstraction to replace but
   the dispatch between "the type has static members" and "the type is a
   builtin", and a concept does not do that job — replacing it would mean
   writing `pow`, `fact`, `abs` and `toString` as free functions for `double`,
   `complex` and `Matrix`, which is more code for the same behaviour. What the
   concepts do is state the requirement: `Interpreter` is declared
   `template <Parsable T, Numeric U>`, so a type missing an operation fails at
   the declaration, naming it.
5. Make `Expression::children` private with a narrow accessor (D6) `[done]`.
   The redundant view this dropped is the raw one, not `m_e1()`/`m_e()` as
   written here: the named views are what thirty call sites read and are the
   clearer of the two, while `children[0]->children[1]` in `Bind` was the
   unreadable half. What made the two views a hazard was that both were
   mutable; neither is now.
6. Collapse the visitor interface (D7) `[dropped]`. The item rested on two
   `visit` overloads that defaulted to returning `{}`, and those were deleted
   in phase 4 along with the nodes they served. Its own complaint — forgetting
   an overload is silent — is now false: all eleven are pure virtual, so a new
   node type is a compile error in both visitors. Adding the prescribed
   default would *reintroduce* that silence.

   The mechanism is also wrong for both visitors this project has. Recursing
   over `children` has no meaning for a fold returning one `T` — which child's
   value would it be? — and `ParametersVisitor`'s correct default is to record
   the whole subtree as one argument, which is the opposite of descending into
   it. Its eight identical one-line overrides are the price of the check, and
   at two visitors that is cheap.

   What did go is `StatefulVisitor`: an empty class over `TransformationVisitor`
   whose ten-line comment warned that nothing stopped a visitor modifying the
   AST in secret. After D6 nothing can.

   The `std::variant` + `std::visit` alternative would delete the
   `accept`/`visit` double dispatch outright and is the more modern design, but
   the double dispatch is a choice the author documented in
   `expression_visitor.hpp`. Ruled out on recognisability, not on the merits.

## Phase 6 — Documentation `[done]`

1. `README.md` in English, matching actual behaviour `[done]`. Its examples
   are not merely drawn from the transcripts: every fenced block in the file
   is replayed as one session by the `readme` test, so the documentation
   cannot drift without failing CI. Writing it that way found three
   statements that were wrong — sections that quietly assumed a fresh
   interpreter.
2. Keep the French history — it is the project's provenance — but write new
   comments in English (D11) `[done]`. The sources were rewritten as their
   code was, and the sweep at the end found only `Implementation de`,
   `Symetric`, `kewword_params_begin` and three French-typographic colons.
3. A short note on the interpreter's model `[done]`: references name
   expressions, not values. It was explained halfway down section 4; it is now
   the first thing the README says, with the example that makes it concrete.

## Phase 7 — What the review found `[done]`

Ordered by what a user hits first, not by where the defect lives.

1. **Nothing kills the process** `[done]`: C18, C19, C21, C20 — and C24,
   which belonged here once it was clear that sequencing the binary operands
   left to right costs five lines and is what phase 8 needs anyway. That
   removed the argument for doing C29 early; see the note under phase 8.
2. **Matrices work** `[done]`: C22 and C23. The corpus gained matrices three
   blocks wide and three tall, a cell extended to fill its block, and `^` on a
   matrix at all. `Matrix` also gains the unit test it never had: `CLAUDE.md`
   §4 reserves those for containers, and after `dynarray` and `Mapstack` went
   there were none left, which is how both of these survived eleven years.
   Verified against the defect rather than assumed — restoring the squaring
   loop fails six of its assertions.
3. **No answer to a question nobody asked** `[done]`: C26, C25, C27, C30.
   This was C13's unfinished business; C29 is the same shape but was taken
   out of this group — see the note under phase 8.
4. **The rest** `[done]`: C28, C31, C32, then D12 to D16.

C29 was all that was left of this phase, and it went to phase 8, which
resolved it as the local rather than as the error.

Coverage the corpus does not have today, beyond the repros above: a matrix
larger than 2x2 in any operation; `^` on a matrix; the step-budget message,
which is reachable but never tested; `lim` on a plain definition, on nothing,
and on a term; default parameter values, which work and are undocumented; a
two-base-case recurrence such as Fibonacci, which is the exact shape C3 was
about; `2i`, `1e3`, `0x10` and `.5` in the lexer; and shadowing a built-in
with `pi=3`.


## Phase 8 — Locals `[done]`

A definition written inside an expression binds a **local**: a name that lives
to the end of the line and is invisible outside it. The 2014 README says the
construct was meant for exactly that — *"L'assignation étant une expression
comme une autre, on peut trouver une assignation aussi bien dans la liste des
paramètres d'une référence ou dans la partie droite d'une autre assignation"* —
and C29 found it had never delivered it.

Specified as transcripts before it was implemented, the way phase 2 did it:
`test/data/spec/locals.ink` and `test/data/spec/laziness.ink`, `may_fail`,
never recorded. `locals.ink` is now `test/data/locals.ink`, every entry and
every expected output unchanged; `laziness.ink` is retired and what it
specified is Deferred.

### One rule

> An evaluated line opens a scope, and a name bound inside one takes its
> value there — where the parameters are still visible.

Two changes, fifteen lines. `EvaluationVisitor::visit(EqualExpression*)`
evaluates the right-hand side and binds the value; binding the *expression*,
as it did, means reading it back through a lookup, and a lookup pushes the
callee frame in which the captured parameter is no longer visible, which is
exactly what C29 measured. `Interpreter::Eval` wraps an evaluated line in a
`Frame`, which is what makes the extent the line. C24 is the prerequisite and
was already done: without a defined operand order there is no "before" for a
local to be bound in.

### Measured against the specification

Each prototype replayed against both specification files, 20 assertions and
17:

| | `locals.ink` | `laziness.ink` |
|---|---|---|
| before | 13 | 14 |
| call-by-name parameters | 16 | 14 |
| **locals, bound to values** | **20** | 14 |

Two things that decided the phase:

- **The four locals assertions call-by-name misses are all extent**, not
  capture: `t` surviving its line, `t + (t = 3)`, `a` reverting after
  `(a = 2) + a`, and `?t`. Nothing about how parameters bind addresses them;
  the line's own scope addresses all four.
- **Call-by-name scores nothing in the file written to specify it.** Its three
  failures there are the same three either way, and all three read `undefined
  is not defined` — an argument evaluated at the call site and never read. Why
  that is the right answer rather than an accepted loss is under Deferred:
  the language's one lazy construct is clause dispatch, and everything else is
  total.

### Values, not expressions

The phase as first written had a local bind an expression, like every other
name, on the argument that this is what the language is. Measured, the value
is indistinguishable across all twenty entries, costs one evaluation per
binding rather than one per read, and needs none of the lookup-depth machinery
the expression form does. The two differ only where a bound expression fails
or diverges and is never read — which is the laziness question, and is
deferred with it.

### What the transcripts settled

- **C29 resolves as the local, not the error.** The form the error would have
  rejected is the form this phase accepts, so rejecting it first would only
  have had to be undone. C29's entry stands as the record of why it was ever
  considered.
- **Extent is the line.** A line being *evaluated* opens a scope; a line that
  is only a definition still writes to the globals — `(b = 7)` alone stays a
  definition, parentheses or not.
- **A local shadows**, both a global and a parameter, for the rest of the line
  and no further. A call's frame is one scope and the local is bound in it.
- **Left to right**, so a local is not visible before its own binding:
  `t + (t = 3)` is an error, not `6`.
- **`?` does not see locals.** It prints a definition, and by the next line
  there is none.
- **It earns its place.** `f(x) = (t = 2*x) + t` genuinely cannot be written
  as two lines: the second would be a global that cannot see `x`.

---

## Phase 9 — Memoisation `[done]`

The oldest idea in the project, and the one that would make it more than a
calculator: an evaluation result belongs to a *context* — the definition, its
index, and the values bound to its parameters — and the same context twice is
the same answer twice. The 2014 code reached for it and missed: `memo_` was
keyed on the index alone, ignored arguments entirely, and was copied away
with every `Reference` that held it. It was deleted as dead in `259e643`.
Deleting it was right; leaving the idea deleted is not.

### Why it is the largest single win

A general clause that names itself twice is evaluated as a *tree*, not a
chain. The arithmetic-geometric mean is the canonical case — two sequences,
each reading both at the level below:

```
am(x,y)_0=x
gm(x,y)_0=y
am(x,y)_n=(am(x,y)_(n-1)+gm(x,y)_(n-1))/2
gm(x,y)_n=(am(x,y)_(n-1)*gm(x,y)_(n-1))^0.5
```

Counted, not estimated — calls made against answers that differ:

| | calls | distinct contexts | wasted |
|---|---|---|---|
| `gm(1,2)_6` | 127 | 13 | 10x |
| `gm(1,2)_10` | 2,047 | 21 | 97x |
| `gm(1,2)_14` | 32,767 | 29 | 1,130x |
| `gm(1,2)_16` | 131,071 | 33 | 3,972x |

Which is 2^(n+1)-1 calls for 2n+1 answers, exactly. The step budget is what
the user meets: `gm(1,2)_17` gives up after a million steps. It is not a
large computation — it is thirty-three answers computed four thousand
times each.

### Measured

One `std::unordered_map` on `ReferenceStack`, keyed on the callee's name, its
index and the raw bytes of its argument values, consulted in `Reference::Eval`
once the index and the arguments are evaluated and before the frame is
pushed. Fifty lines.

| | eager | memoised |
|---|---|---|
| `gm(1,2)_14` | 33.6 ms | 1.9 ms |
| `gm(1,2)_16` | 135.1 ms | 1.8 ms |
| `gm(1,2)_17` | gives up after 1,000,000 steps | 1.6 ms |
| `gm(1,2)_254` | gives up after 1,000,000 steps | 2.8 ms |
| `gm(1,2)_255` | nests more than 256 references | nests more than 256 references |

Whole-process times, of which 1.7 ms is starting up: the memoised column is
measuring the loader, not the agm.

Exponential becomes linear, and the step budget stops being the limit: what
stops the agm now is `max_depth`, at the term where the recursion itself is
256 deep. No recorded output moved, and the two entries that record the change
are both new: `gm(1,2)_20`, which the step budget used to refuse, and an
`r(2)` whose global changes under it.

The cost, on work with nothing to reuse — two hundred evaluations of a linear
recurrence, each with different arguments — is **23.0 ms against 24.2 ms**,
and all of it is in building the key rather than in the cache: a first version
that built the same key out of `+` temporaries cost five times that.
Expressions that name nothing are unaffected — twenty thousand lines of
arithmetic measure the same either way.

**Lifetime mattered more than the cache.** Two were measured. Clearing at each
top-level evaluation is the obviously-safe rule; clearing when a *definition
changes* is both simpler and worth far more, because a session is a
conversation — `gm(1,2)_240` asked two hundred times is 210 ms under the first
and 3.1 ms under the second. The second is what landed.

### Why it is sound

Not because nothing changes, but because of a rule phase 4 already paid for:

> A call sees its own parameters and the globals. It never sees its caller's.

So the value of a call is a function of exactly three things — which
definition, which index, which argument values — and the globals. The first
three are the key. The globals change only when the user redefines one, which
happens between lines, or in a top-level assignment inside a line; either way
`Set` is reached with no frame on the stack, and clearing there is enough.
`a=1`, `b=a+a`, `a=2`, `b` is still `4`, measured rather than argued.

Two smaller rules the prototype needed:

- **Only calls that carry an index or arguments are cached.** A frame-bound
  parameter is read by bare name with neither, and two frames' `x` must never
  meet in one map. Nothing is lost: a parameter read is a map lookup already.
- **Nothing that threw is stored.** An evaluation that ran out of budget is
  not an answer about the language, and must not be remembered as one.

### What it is not

- Not a cache across *definitions*: `?` still prints what was written, and
  nothing here evaluates ahead of being asked.
- Not symbolic. Two calls that are obviously equal but differently written
  are two contexts. Recognising them is the deferred item at the end of this
  file, and it is a larger program.
- Not free of memory: one entry per distinct context, for as long as the
  definitions stand. The agm to term 254 is about five hundred entries, but a
  session that sweeps a parameter accumulates one per value, so there is a cap
  — `max_memoised`, a hundred thousand — and reaching it drops the whole map.
  Eviction by age would keep more of the cache and needs an ordering to
  maintain; dropping everything costs time and can never cost an answer.
  That held until phase 14's fills relied on the memo (C69): the older half
  goes now, which is eviction by age without an ordering.
- Not a reason to raise `max_depth`. Depth is a recursion the user wrote;
  steps were an accident of how it was evaluated. Removing the accident is
  this phase; the other is a separate argument, with a stack to size first.

---

## Deferred

Recorded so they are not re-litigated later, or drifted into by accident.

- **Lazy parameters (call-by-name).** A parameter binding the argument
  *expression* rather than its value, so that an argument the body never reads
  is never evaluated. The 2014 README lists *"Évaluation paresseuse"* first
  among the project's features, which is what put it in the plan. That claim
  is fair as far as it goes: a definition evaluates nothing, so `later=zzz+1`
  is accepted and only `later` reports the unknown name. What has never
  existed is laziness *inside* an expression, and the re-reading that matters
  is why that turns out to be the right shape rather than an omission.

  **The language already has exactly one lazy construct, and it is the one
  worth having: clause dispatch.** With `f_0=zzz` and `f_n=n`, `f_5` answers
  `5` and never looks at the base clause. Choosing a clause is choosing not to
  evaluate the others, which is what laziness is *for*.

  Everywhere else an expression is **total**: every operand contributes to the
  answer, and `0*zzz` is an error rather than `0`. There is nothing to skip,
  because nothing is discarded. Call-by-name can only skip an argument the
  body never mentions — a parameter that is not used, which is a mistake
  rather than an idiom. So it is not that laziness does not belong here; it is
  that it is already where the choices are made.

  Which turns the open question into a different one: **should the language be
  able to choose anywhere other than at a clause index?** A conditional is
  what creates skippable work, and it would have to be lazy in itself — an
  `if` that evaluates both arms cannot guard a recurrence — without making
  every parameter lazy. That is the argument to have if this is ever
  revisited. It also names the family the language belongs to, which is the
  spreadsheet rather than the lazy functional language: cells holding
  formulas, answers remembered until an input changes (phase 9), locals over a
  formula (phase 8), and one lazy construct for choosing.

  The measurements against call-by-name stand, and are the reason not to take
  it as a consolation prize in the meantime:

  - It passes **no assertion** of `laziness.ink`, the transcript written to
    specify it, that eager arguments do not already pass. The three it fails
    are three `undefined is not defined` reports for arguments nothing reads,
    and they fail identically with laziness, because an unknown name is worth
    reporting at the call site whether or not the body wants the value.
  - It **costs half the depth budget**: a lazy level nests two references
    where an eager one nests a single one, so the agm reaches `gm(1,2)_127`
    against `_254`. Since phase 9 the depth budget is the limit users meet,
    so this is the whole of the cost, and it is paid on every call.
  - It **fights the cache**: phase 9 keys a memoised answer on argument
    values, and laziness is the decision not to have them. Forcing each
    argument only to build the key, and treating a force that throws as "not
    cacheable", does work — but it is laziness kept only for the calls where
    it does not pay.

- **Numbers the user cannot tune.** Two constants decide every answer's
  accuracy and neither can be reached from the prompt: `lim` stops at a
  tolerance of `1e-10`, and results print to nine significant digits
  (`numeric_interface_precision`). Where it shows: `cos` and `sin` defined as
  series -- the way `sequences.ink` defines them -- give `rot(pi/2)` a
  `5.26e-13` where zero belongs, which is the tolerance and not the
  arithmetic. At nine printed digits it is invisible except next to a zero,
  which is exactly where a student looks.

  Recorded rather than fixed because every way of exposing them costs
  something. A second argument to `lim` changes a keyword into a call. A
  magic global -- reading a user's `tolerance` if they define one -- is the
  cheapest and is hidden coupling: a name that means something only because
  the interpreter looks for it. A flag is a command-line option for a language
  that has none. The observation stands; the syntax does not, yet.

- **A distance between matrices, so that `lim` works on a matrix sequence.**
  The obvious student example converges visibly -- a Markov chain,
  `k_n = k_(n-1)*t`, whose `k_30` is the steady state to nine digits -- and
  `lim k` cannot say so, because the convergence test needs `abs` and a matrix
  has none. A max-norm over the cells is three lines and would make it work.

  Not taken yet, because it is a language change rather than a repair, and it
  asks a question this file should answer first: is the limit of a matrix
  sequence the cellwise limit? For a Markov chain yes; for a sequence whose
  terms change size the question is meaningless, and the tolerance would then
  be comparing a number against the largest cell of a difference rather than
  against a value the user chose. C39 made the refusal say what it is refusing;
  this entry is the feature behind it.

  Taken up after the conformance suite's first entries asked for it (implicit
  layers, Newton for systems, power iteration): see next in line.

- **Built-in series acceleration.** `lim` applying Aitken's delta-squared, or
  offering it behind a keyword, so that a slowly converging series gets an
  answer instead of C36's refusal. Declined, and the reason is not cost.

  **There is no universal accelerator.** Matching the method to the shape of
  the error is the mathematics, not a detail below it: Aitken for a
  geometric-looking error, Richardson for a power law, the Euler transform for
  an alternating series. Measured on `1/n^2`: one Richardson step leaves
  `9.9e-05` at a hundred terms, while the Euler-Maclaurin tail leaves `1.9e-11`
  at twenty. A built-in would have to pick one of them for every series a user
  will ever write.

  **And the language already expresses all of them.** Both the tail correction
  and Aitken are ordinary clauses — `test/data/sequences.ink` records them, and
  the README shows the first — because a clause may index another sequence at
  any expression, including one that reaches forward. Adding a keyword would
  spend syntax on a convenience for one method while taking the choice away
  from the only party who can make it. It would also make `lim` answer where it
  now refuses, which is the direction C36 measured as harmful.

- **Substitution and partial expansion.** `?b` showing `2+2` rather than
  `a+a`: evaluating some references while leaving others symbolic. This is a
  different feature from printing a definition back, not an option on it. It
  needs its own syntax, a rule for how far expansion goes, and an answer for
  what a partially evaluated sequence or matrix of expressions even means.
  `?name` (phase 4, item 6) prints what was written and nothing more.

- **A third-party matrix library.** Raised explicitly, as `CLAUDE.md` §5
  requires, and declined for now. Measured against what the code actually is:
  `matrix.hpp` is 190 lines, of which roughly eighty are the arithmetic a
  library would replace, and they are now unit-tested. Eigen, the obvious
  candidate, is a megabyte of headers.

  The semantics do not line up either, in three ways that would each need
  adapter code: `/` here is element-wise, not scalar division; a 1x1 matrix
  is deliberately also the scalar type, which D9 measured and chose to keep,
  where a library makes that a type distinction; and the block expansion that
  sizes `[a, a; a, a]` from what its cells evaluate to is this language's own
  rule, living in the visitor rather than in `Matrix`.

  What decides it is that inkamath asks for nothing a library is good at. It
  has `+`, `-`, element-wise `/`, `*` and an integer power — no inverse, no
  determinant, no transpose, no solve. A library earns its place at the point
  those arrive, because pivoting and conditioning are genuinely hard and
  worth not writing. The seam is clean when that day comes: those operate on
  an evaluated numeric matrix, so the library sits *below* `Matrix`, on
  values, and never meets the expressions.

- **Symbolic simplification.** `x+x` to `2*x`, `x^1` to `x`, folding constant
  subtrees. The shape of the program invites it: names bind expressions, the
  AST survives evaluation, and `TransformationVisitor`'s own 2014 comment
  lists "simplify the tree" as one of the two things it exists for. D6's
  immutability helps rather than hinders — a simplifier builds a new tree
  rather than editing one — and `?` would give the result somewhere to show.

  What stops it being small is specific to *this* language rather than to
  simplification generally: **the identities are unsound over matrices.**
  `x*0` is not `0` when `x` is a matrix, because the result's extent comes
  from `x`; nor is `x-x`, for the same reason. Almost every algebraic rule
  needs the extent of its operands, and an extent is only known after
  evaluating them — which is the thing simplification is meant to avoid. The
  complex arithmetic adds the usual IEEE caveats: `x-x` is not `0` at NaN,
  and `(x^2)^0.5` is not `x` off the positive reals.

  So the sound subset is roughly constant folding over literals, which buys
  little, and everything beyond it needs a type-and-extent analysis the
  interpreter does not have. A real CAS is a larger program than this
  interpreter, and the size constraint at the top of this file is the reason
  to say so out loud rather than drift towards one. Recorded, not scheduled:
  if it is ever wanted, it starts with extents, not with rewrite rules.

## Openings

Not scheduled, and not Deferred either -- Deferred is for what has been argued
and declined. These are the directions worth taking, with what is known about
each measured rather than assumed. New directions go in `MANIFESTO.md`; what is
here stays for what it measured.

**Other number systems.** The seam D9 and C9 argued about is real, and this is
not a guess: `Interpreter<double>` compiles and runs with no complex numbers at
all and no change to the project, and `Interpreter<Rational>` -- eighty lines
of exact fraction over two `long long`s, written to find out -- runs sequences,
matrices, cells and the memoisation on top of them, needing one thing the
concepts never stated (C44). Then `1/3+1/3+1/3` is `1` rather than nearly one,
`s_n=s_(n-1)/3` gives `1/243`, and `a/3` is a matrix of fractions.

Three things it opens, at three prices. *Exact division* is the eighty lines
above. *Exact magnitude* is a bignum -- `fib_100` prints `3.54224848e+20` for a
number with twenty-one digits, and `!21` and `2^64` lose the same way -- which
is several hundred lines to write or a dependency to raise, and §5 makes that a
decision rather than something to slip in. *Both* is a rational over a bignum,
which is the real prize and the real cost. The question worth answering first
is not whether it works but what the prompt should be: one interpreter per
number type, chosen when it is built, or a language where the kind of a number
is part of the number. Taken up as phase 13, for the second.

**A standard library.** There are no functions: `sqrt` is `^0.5`, `exp` is
`e^x`, and `ln`, `sin` and `cos` are nothing at all. Two shapes, and they are
not the same project. A **prelude written in inkamath** is in character and is
the better demonstration -- `sequences.ink` already defines `cos` and `sin` from
the exponential series, and they work -- but it needs a way to load a file, and
its accuracy is bounded by `lim`'s tolerance, so *Numbers the user cannot tune*
comes first. **Built-in elementary functions** are accurate and fast and
contradict `README.md`, which says there are none as a statement of design
rather than an apology. Deciding which of those two sentences is true is the
whole of the work.

**A conditional.** Done -- phase 10. It was the one opening specified before
it was built, and the specification is what found both of its mistakes.

**Performance.** No session has ever been too slow -- a prompt evaluates one
line -- so this is about where the time goes if the interpreter is ever asked
to do real work, and it was measured before anything was written. Two
workloads under callgrind, and they disagree completely. On a 20x20 matrix
product, 74% is `matrix.hpp` and 11% is complex arithmetic: compiled C++
already, nothing to win. On a small tree folded through `lim` thousands of
times with the memo cache dropped each line, the arithmetic is **0.9%** --
the interpreter spends a hundred times more effort administering a value than
computing with it, and that effort is strings hashed and compared (20%), an
allocation per intermediate (19%), clause dispatch (17%) and an atomic
refcount per node (10%).

A prototype took that workload down 43%, in four steps, each measured on top
of the last and each leaving every golden byte-identical:

| | sequence | matrix |
|---|---|---|
| `Name()` by reference, a frame as a flat vector | -6% | +1% |
| a parameter, a default and an index bound as a **value** | -33% | +3% |
| every name interned to an integer | -34% | +3% |
| the memo key a struct rather than a rebuilt string | -43% | +3% |

Two of those four lines carry it. Binding one parameter cost a
`make_shared<Reference>`, a `Clause`, a `ParametersDefinition` holding two
vectors and two strings, and a heap `ValExpression` -- per argument, per call,
and again per term of a sequence for the index variable; a frame that holds
values instead is three quarters of the saving. And **interning bought
nothing**: the 20% the profile blamed on hashing names was the memo key and
the allocations under it. Slot resolution by index -- the obvious reading of
that profile, and the reason the experiment was run -- is the one change here
not worth making. The matrix column is the warning attached to all of it: a
frame carrying two vectors costs 3% where there are no references to resolve.

What the goldens caught, and no reasoning would have: `h(x) = (x = 10) + x`
answers 20. A local shadows a *parameter* of the same name, so a frame is one
slot per name, not a value list beside a definition list.

It also answers the question that started it, which was an LLVM JIT. The tree
walk such a backend would replace is 7% of the baseline and the arithmetic it
would compile is 1%, and a `Matrix` whose shape is dynamic leaves it either
specialising on shape -- a project of its own -- or calling this same runtime
and keeping every cost above. 43% came with no dependency, no codegen, and
mostly by deleting what stood behind a bound parameter. If the codegen is the
point rather than the speed, `Interpreter<double>` already compiles and is the
honest target, out of tree.

The second line of the table is what landed, rewritten rather than applied: one
slot per name in a frame, carrying a value or a definition, no symbol table.
**-32%**, and *nothing* on the matrix workload where the prototype cost 3%,
because a frame is one vector and not two. +89/-32 lines, which does not pay
for itself; what it buys besides the time is that the `from_frame` flag
threaded through three functions is gone, the frame shadowing a global being
now a branch one can read. Checked three ways, because a faster wrong answer
measures nothing: every golden byte-identical, 4000 random lines and 35 edge
cases byte-identical against the previous build.

A second step, eight lines: every child accessor handed its `shared_ptr` out by
value, so reading a child cost an atomic pair -- on every child of every node
of every fold. By reference it is another 6%, and half the refcounting.

**-36%** together, and what remains, measured on what is in the tree: clause
dispatch 18%, names 17%, the allocator 12%, `matrix.hpp` 11%, the AST fold 9%,
refcounting 4%, the arithmetic 1.7%; 216M instructions down to 122M. Most of
that 17% is the memo key, still a string built per call, and the prototype's 7%
for making it a struct needed a symbol table that earned its place nowhere
else -- so that step wants a different idea, not that one.

One correction to what this entry first claimed: the boxed value is *not* what
is left, because D9 already unboxed it -- `Matrix(Extent, value)` skips
`cells_` when the extent is one cell, and a 1x1 keeps its number in `scalar_`,
so no scalar intermediate allocates. What the profile shows is smaller and
worse placed: a `Matrix` carries a `std::vector` member even when it is one
number, so every intermediate constructs and copies an empty vector for
nothing, about 5% between the two. Closing that is the scalar/matrix split D9
declined on its own measurement, and 5% does not reopen it.

If they were mine to order: the conditional, because it is the one missing
primitive rather than a convenience; then the number systems, because the seam
is already open and the experiment above took an afternoon; then the tolerance,
and a prelude behind it. Performance sits outside that order: it is the one
direction that can end with fewer lines than it started with.

---

## Phase 10 — Definitions in cases `[done]`

A clause may carry a **guard**: the condition it applies under, written
between the left-hand side and the `=`, as set-builder notation writes "such
that".

```
abs(x) | x < 0 = 0-x
abs(x) | x >= 0 = x
```

Specified first, in `test/data/spec/conditional.ink`, 57 of its 69 assertions
failing when it was written; it is now `test/data/conditional.ink` with every
expected output unchanged.

### Why it is small

The language has had half of this since 2014. `f_0 = 1` is already a clause
matching a literal index, tried before the general one, and the clause not
chosen is never evaluated. A guard is that same dispatch with a condition
instead of an index, so nothing new had to become lazy. A ternary would have
computed the same things and required lazy arms to do it, which is the
call-by-name argument this file already had and declined.

Comparisons answer `1` and `0`, because every value here is a number and a
truth type would be a fifth thing to carry through `numeric_interface`. A
guard holds when it is not zero, so `sgn(x) = (x>0) - (x<0)` needs no guard at
all, and `(n > 0) * (abs(x) < 1)` is a conjunction.

### What writing the specification found

Both of the design's mistakes, before either cost a day:

- **`==`, not `=`, for equality.** `f_n | n = 0 = 1` puts two `=` on one line
  doing two different jobs, one asking and one telling. The author read that
  line and stopped, which is the only evidence that counts. `<>` for
  inequality follows from `!` being the prefix factorial.
- **Clauses are tried in written order**, and not "a guard is more specific
  than an index", which is what the first draft said. Implementing that draft
  made `root` -- a recurrence whose guard reads the previous term -- recurse
  for ever at the base index, because the guard was reached before `root_0`.
  Written order serves both cases: `c` puts its guards first so that they rule
  an index out, `root` puts its base first so that its guard is never asked
  there. The unguarded general clause is still tried last wherever it stands,
  so `README.md` section 4's rule is untouched.

The specification also asked for something the language cannot say: the
Kronecker delta was written `d(i,j)`, and `i` is the imaginary unit. It is
`d(row,col)` now, which reads better anyway.

### What it buys

Pascal's rule, whose base case sits at an index a parameter decides and which
therefore could not be written at all -- the third pass found that by hand,
through `binom` having to go via factorials. A removable singularity, where
the guard is what keeps `0/0` from being evaluated. A recurrence that stops
itself, which is `lim` with a tolerance the user chooses. And a matrix whose
cells are a definition in cases: `d(row,col)` gives the identity, three
clauses give the second-difference matrix.

### What it costs, and the deletion still owed

Two tokens and six comparison operators; a `CompareExpression` carrying an
operator rather than six near-identical classes, which would have been
eighteen visit methods for one idea; a vector of guarded clauses on
`Reference`, and the guard riding on `ParametersDefinition`, where the rest of
the left-hand side already lives. Every recorded output is byte-identical.

Writing a clause again replaces that clause, where it stands: the unguarded
ones are named by their shape, a guarded one by its left-hand side as the
tokens spell it. A plain definition still clears the whole definition (C11),
which is how one is started over. What is still missing is a way to drop a
single clause -- the language has no notion of deletion at all, and inventing
one for clauses alone would be a syntax nobody asked for.

The deletion this phase promised was done, and **it was not a deletion**.
`Reference` now keeps one vector of clauses in written order instead of a plain
clause, a map of base clauses, a general clause and a vector of guarded ones;
`Clause::order` and the merge in `EvalImp` are gone, and `Describe` no longer
collects and sorts. The file went from 341 lines to **356**.

Fifteen lines the wrong way, and the reason is worth keeping: a `std::map`
keyed by index *is* an index, and replacing it with one heterogeneous list
means writing by hand what the map gave for free -- find the clause at this
index, the lowest, the highest. The prediction that guards would pay for
themselves in deletions was wrong, and measuring it is the only way that was
ever going to show.

Kept anyway, on the argument that the model is now the one sentence the
language actually follows -- clauses in written order, the general one last --
where the old shape needed that rule spread across `EvalImp`, `Describe` and
`Converge`, and needed a merge-by-order once guards arrived. Every recorded
output is byte-identical. If the fifteen lines matter more than the sentence,
the revert is one commit.

### Patching a definition afterwards

The refactor exposed a hole, found by the question *can a guarded clause be
added as an afterthought?* -- and the answer was three different answers.
After a general clause it worked, because the general clause is tried last.
After a base clause or a plain definition the new clause was silently dead,
because both were tried in written order and both answered first.

So the rule gained its one exception, and the exception is the sentence that
makes it coherent: **an unguarded clause that would answer every call -- a
plain definition, or the general clause -- is the definition's default, and is
tried after the guarded ones wherever it was written.** A base clause answers
for one index rather than for every call, so it is not a default and keeps its
place; a guard written after one is refused, since it could never apply.

That is what lets a definition be written the way one is actually built:

```
ramp(x) = x
ramp(x) | x < 0 = 0
```

which also gives the "otherwise" case a spelling without a second reserved
word -- it is the clause with no guard. Re-typing that clause still clears the
definition (C11), which is the way to start over.

---

## Phase 11 — Series `[done]`

A sum or a product over an index, written as on paper with the language's own
`_` and `^`, so that a series is no longer a recurrence that names its own
previous term:

```
exp(x)_n = sum_(k=0)^n x^k/!k      # was exp(x)_n = exp(x)_(n-1) + x^n/!n
prod_(k=1)^5 k
sum_(k=0) 1/!k                    # no upper bound: the series itself
```

Specified first, in `test/data/spec/series.ink`, 42 of its 48 assertions
failing when it was complete; the six that passed were three ordinary
definitions, the check that the index does not leak -- which means something
only once a sum runs -- and the two the harness makes of its own. It is now
`test/data/series.ink` with every expected output unchanged, and the suite
passed on the first build. `lim`'s stopping rule moved into a class of its own
first, so that the two share one rule rather than two copies of it.

What the specification decides, each point from a probe of the parser rather
than a guess:

- **The body is a term.** It runs to the next `+` or `-`, as on paper.
  Juxtaposition after the bound is a syntax error today ("the operator `*` is
  probably missing"), so the notation takes nothing away.
- **The upper bound is a number, a name, or parenthesised**, and a name there
  is never a call: `n (2)` is a call today, spaces or not, so `^n (k+1)` has to
  be read as the bound `n` and the body `(k+1)` by rule.
- **`sum_(` is recognised before the index is parsed**, because `(k=5)+k`
  already means a local binding and answers 10.
- **The index is bound for the body and restored after it**, the way a guard's
  index is tried (C53). A frame would hide the parameters of the call the sum
  is written in; the body sees everything around it.
- **No upper bound is the limit**, by `lim`'s rule and with its report. It adds
  a term per step, where `lim` over a sequence defined by a sum recomputes every
  partial sum. The cost of one rule for "converged" is written into the
  specification: `sum_(k=1) 1/k^2` does not converge within a hundred terms,
  any more than `lim` of it does (C36). Whether that rule is right is a question
  for the tolerance and the cap -- *Numbers the user cannot tune*, in Deferred
  -- and not for this phase.
- **`sum` and `prod` are reserved**, as `lim` is.

---

## Phase 12 — The command line `[done]`

`argv` was never read: a file could only be fed through standard input, which
printed a banner and a prompt before every answer, and a transcript could not
be fed at all -- its recorded answers would have been evaluated too. The
precedents decide the shape. Read from a pipe, a tool is a filter that prints
bare answers, as `bc` and `sqlite3` are; echoing each input is a flag, as
`psql -a` and `sqlite3 -echo` make it, and here it writes exactly what the
recorder writes, so `inkamath --echo questions.txt > answers.ink` is a golden
and `diff` checks one -- the doctest, cram and `pg_regress` way. Files run in
order, `-i` reads standard input after them (a prelude, then the session), and
a file whose first line that is not blank or a comment starts with `>>` is a
transcript. The banner, at a terminal only, reads the version from
`CMakeLists.txt`, which becomes 1.0.0: `0.8` was 2014's guess, written in two
places and never tagged.

Specified first, in `test/cli.cmake`, which runs the binary: a transcript
drives the interpreter and never reaches `main()`, so it cannot say what the
program prints or how it exits. Sixteen cases, all failing when written and all
passing on the first build; `repl_eof` became one of them. A seventeenth came
from replaying every golden through `--echo`, which the spec had not thought to
do: a transcript's entry is the whole of an input, as the recorder evaluated
it, so an unclosed bracket there is its error rather than a line to continue,
and an entry that is only a comment is evaluated rather than skipped. All ten
goldens now come back exactly. No parsing library: five flags are a loop over
`argv`.

---

## Phase 13 — Exact numbers `[done]`

The kind of a number becomes part of the number: `1/3+1/3+1/3` is `1`, not
nearly one, and `1/10*3 == 3/10` holds. This answers the question *Other
number systems* left open, and answers it against one interpreter per number
type, on a measurement: `Interpreter<Rational>` over two `long long`s truncated
`pi` to `3` and `e` to `2`, refused `2^(1/2)`, wrapped `!21` and the harmonic
number `h_50` to negative numbers, and failed the exponential series with
*division by zero* when a denominator wrapped to 0. The constants, the roots and
most limits -- the language's signature -- can only be approached, so an exact
interpreter must refuse them or lie; and a mode chosen on the command line would
make the meaning of a file depend on how the interpreter was started. This is
Scheme's numeric tower instead: exact where it can be, inexact where it must be,
and visibly which.

In two steps, each worth shipping alone:

1. **Exact fractions over 64 bits.** A result whose reduced numerator or
   denominator does not fit becomes inexact -- never wrong, never refused.
2. **A bignum, written here** rather than depended on (section 5 of CLAUDE.md),
   which moves the end of exactness from 2^63 to never.

Specified first, in `test/data/spec/exact.ink`, 29 of its 57 entries failing.
The 28 that passed were 13 definitions echoing themselves, 11 inexact values the
rules leave alone, and four exact answers a double happens to get right --
`1/3+1/3+1/3`, `6/3`, `dbl(1/2)` and an inverse multiplied back to the
identity -- which still tested something, because an inexact 1 prints `1.`.
Step 1 passed it as written on the first build; it is now `test/data/exact.ink`
with every expected output unchanged, and the spec suite retires again.

What the specification decides:

- **A literal written as a whole number is exact.** A point or an exponent
  makes it inexact -- so does `0x10`, which reads only because `strtod` does --
  as do `pi`, `e`, `i`, a power that is not whole, `lim` and a sum without an
  upper bound; and an inexact number makes inexact whatever it touches. A
  literal too large for 64 bits is inexact, as an overflow is.
- **An exact number prints as the literal that makes it** (C52): a whole number
  in full, anything else as a reduced fraction with its sign on the numerator:
  `10/4` answers `5/2`.
- **An inexact number prints as today, with a trailing point wherever the
  printed form would read as whole**: `1e3` is `1000.`, `i*i` is `-1.`, and a
  sum that approaches 2 is `2.`. The rule is on the printed form, not the value.
- **`lim` is inexact even when it lands on a whole number** -- it approaches, it
  does not reach -- which moves `lim lt` in `sequences.ink` from `5` to `5.`.
- **Division by an exact zero is an error.** `1/0` and `0/0` stop answering
  C31's complex infinity and NaN.
- **The boundary is the reduced result's, nearly.** The specification stays
  well clear of it -- `h_30`'s denominator fits by six orders of magnitude,
  `h_50`'s misses by two -- and the implementation reduces before it multiplies
  (Knuth's method), so a product or quotient is exact exactly when its reduced
  result fits. A sum is not quite: its two cross products can overflow when
  their difference would not, and their sum can exceed the result by a common
  factor still to come out. A fuzz at the edge found it -- 3 of 4000 sums of
  fractions near 2^63 went inexact though their results fit, none of 15000
  ordinary expressions did, and none was wrong. Closing it needs 128-bit
  division, which is step 2's bignum by another name; MSVC has neither
  `__int128` nor `__builtin_mul_overflow`, so the checks are written out.
- **A memo key carries the kind** (as C50's had to carry the shape): `dbl(1/2)`
  is `1`, `dbl(0.5)` is `1.`, whichever is asked first.
- Out of scope: exact complex numbers.

The first thing the implementation met was already broken: since phase 10 a
class-type number did not compile, because the comparisons ask
`numeric_interface` for real and imaginary parts and the generic path -- the
one a class goes through -- had neither (C65, fixed first).

`Number` is that class: an exact fraction of two `long long`s, or a
`complex<double>` once inexact, marked by a zero denominator. Every field is
always set and nothing pads them, because a memo key is a value's bytes -- so
`dbl(1/2)` and `dbl(0.5)` are different keys without anyone asking. An inexact
number goes through the `complex<double>` code it always went through, which is
why no inexact answer moved; one rule in `Convergence` makes a limit inexact for
`lim` and an unbounded sum alike. Checked, besides the goldens, against Python's
`Fraction` on 15000 random expressions -- every exact answer identical, every
division by zero an error on both sides -- and on 4000 sums, differences,
products and quotients of fractions near 2^63 under the sanitizers, with no
report and no wrong answer. It costs 303 lines of header, 282 of them
`number.hpp`: a feature, and the largest single addition since phase 10.

What moved, all in the one commit and each by a rule above: quotients to
fractions in `basics.ink`, `matrices.ink`, `sequences.ink` and `README.md`;
inexact whole numbers gaining their point (`1.5+.5`, `.5e2`, `i*i`,
`(1+i)*(1-i)`, `0.5^3000000000`); `lim lt` and `sum_(k=0) 1/2^k`; and the
diagnostic for an index too large, which now quotes `2147483648` as typed
rather than `2.14748365e+09`. Three entries needed an inexact input to keep
their purpose -- C31's `1/0` and `0/0`, and the NaN that passes a guard -- since
an exact zero now cannot be divided by. One moved for a reason no rule states:
`lim exp(1)-e`, from `-8.149037e-13` to `-8.15347789e-13`. The partial sums are
exact now, so the only error left is `e` itself as a double -- 1.4e-16 against
the true remainder, where the accumulated sum was 5.8e-16 off.

### Shown as decimals `[done]`

Revised before step 1 reached master. A fraction is exact, and alien to a
reader who does not care whether an answer is; and a decimal need not go
through a double -- long division of an exact fraction gives as many correct
digits as are asked for, all fifty of `ex(1)_40`'s matching `e`. A decimal
cannot hold most exact numbers whole: `1/97` repeats every 96 digits and
`h_30` every 11,088.

Specified in `test/data/spec/decimals.ink`, 62 of its 91 entries failing. The
29 that pass are 18 definitions echoing themselves, four whole numbers already
printed in full, `1/3+1/3+1/3`, `2+3*i`, three inexact answers that happen to
print today as their exact successors will, and the two ellipses, errors meant
to stay errors. Every expected output was computed from the exact value by a
reference printer written apart from the interpreter, which caught one guess:
`!52/(!5*!47)` through double factorials is exactly `2598960`, so it prints
unmarked.

What it decides, revising step 1's display:

- **Every number prints in decimal**: an exact whole number in full, anything
  else rounded to 9 significant digits, with an exponent below 1e-4 and from
  10^9 up.
- **`~` in front says the digits are not the whole value**, whatever the kind:
  `2/3` is `~0.666666667`, `pi` is `~3.14159265`. Rounding and `~` belong
  together -- "about" is true of a rounded decimal, where an ellipsis claims
  the digits continue, and `0.666666667...` claims a 7 that 2/3 does not have.
  An inexact number that is exactly what is printed needs no mark (`i*i` is
  `-1`), a complex number is marked part by part where it is inexact, and step
  1's trailing point goes.
- **`~` is also an operator**: in front of anything, it makes it inexact, so
  every answer reads back as what it says it is, and `root_0 = ~1` starts an
  approximate iteration. With a bignum that is no nicety -- exact Newton's
  method doubles its digits every step, 392 at the tenth and 401,370 at the
  twentieth -- and step 2 should keep a bound past which a result goes
  inexact, as 64 bits does now, so that a forgotten `~` costs speed and not the
  session.
- **A literal is exact as written**, point and exponent included, so
  `0.1*3 == 0.3` holds. This reverses step 1's rule that a point makes a
  literal inexact.
- **`frac` begins a line and shows its answer as the exact fraction**, and
  refuses an approximation. The default display may hide the kind; `frac`
  shows it.
- **`digits = n` sets how many significant digits are shown**, in the session
  and so in the file that needs it. An inexact number shows 17 at most, which
  is where an exact partial sum of `e` overtakes the built-in one.
- **`...` stays unclaimed**, for the matrix notation C41 suspects. An ellipsis
  marking approximations was specified first and dropped: it had to mean "the
  digits continue" for an exact number and "about" for an inexact one, and
  could not honestly mean both.

It passed as written on the first build; it is now `test/data/decimals.ink`
with every expected output unchanged, and the spec suite retires again. The
printer is exact, not a `printf` of a double: long division for a fraction,
and for a double every one of its digits from `to_chars` -- at most 767 --
rounded half to even as a string. Fuzzed against the reference printer on
36000 fractions and doubles at digits from 1 to 40, where it found step 1's
conversion to a double rounding twice: numerator and denominator became
doubles before they were divided, so `~9.233944204712703` was a double off
once the fraction had more than 53 bits, which exact decimal literals made
common. It is a binary long division now, checked on 4000 halfway cases.
`frac` and `digits` are reserved and begin a line; anywhere else they are an
error. `digits` stops at 1000: printing is linear in the digits, some
25 ns and 20 bytes of peak memory a digit in every cell (measured), and
without a ceiling `digits = 10^7` made a 2x3 matrix cost 1.6 s and 200 MB.
An index -- of a sequence, a cell or a series -- must be exact, as in Scheme:
taken as a whole number, `s_(~2)` dropped its `~` and answered an exact term,
and shared `s_2`'s memo key. `frac`'s refusal says a number *was approximated*
rather than *is approximate*: after `4^0.5` prints a plain `2`, "2 is
approximate" contradicted the line above it.

Measured against master on the earlier workloads, exact numbers first cost
integer matrices 7.6x and sequences 1.5-1.8x: every whole-number product paid
for gcds of 1, found by 64-bit divisions, and every mixed operation converted
its exact operand by long division. Whole numbers now skip the gcds, and a
fraction whose parts fit in 53 bits converts by one division, already
correctly rounded. Both return what the general code did -- 184,000 lines of
output identical before and after -- and leave matrices at 1.7x, exact
sequences at 1.3x and inexact ones at 1.05-1.2x; printing and one-line
expressions are 8% faster than master. A diagnostic that quotes a number
quotes it at 9 digits whatever the setting, because it is written before the
session's display is known.

What moved: every quotient and approximation in the goldens and `README.md`,
from fractions and bare doubles to decimals and `~`; step 1's trailing points,
gone. `exact.ink` kept its entries and its purpose, but six showed the kind
through the display, which now hides it when the digits are all of the value:
they show it through `frac` instead, with `~0.5` where step 1 wrote `0.5`, and
`[1/2 0.5]*2` became `[1/10 ~0.1]*3`. C31's `1/0.` and `0/0.` and the guarded
NaN are `1/~0`, `0/~0` and `0/~0` for the same reason, and answer as they did
before step 1.

Deferred, to come back to: **`exact`**, showing an exact number losslessly in
decimal, its repeating block in parentheses -- `2/3` as `0.(6)`, `22/7` as
`3.(142857)` -- which reads back exactly. At 9 digits the block fits for 64 of
the 99 denominators from 2 to 100; where it does not, `exact` should give the
fraction instead, so that it is always exact and never an error.

**What asked for it.** Step 1 showed the digits only by accident.
`sequences.ink` says twenty terms of Aitken's acceleration beat a hundred raw
ones, and showed it as `q_100` = `0.688172179` beside `r_20` =
`6938333221/10010080080` -- the claim still true, no longer visible. `README.md`
compares `y_20` with `pi^2/6` digit for digit, and `y_20` survived only because
its exact value overflows; with a bignum it would print
`445714427153104648117/270961879956768000000`. A fraction is the right answer
and the wrong display wherever the point is to compare digits, which is most of
the places a number is read.

---

### Step 2: a bignum `[done]`

Specified in `test/data/spec/bignum.ink`, 16 of its entries failing -- each one
an exact answer past 64 bits. The 24 that pass are definitions and settings,
inexact answers, and what lies at or past the bound, which today approximates
the same way. Writing it found two defects in the real powers step 2 relies on,
fixed first: `2^1024` answered `inf+i*-nan`, and `2^0.5` a double off the
nearest one, both from taking a real power as a complex one.

What it decides:

- **Exact up to a thousand digits** in the reduced numerator and in the
  denominator, as many as `digits` can show, and approximated past that as 64
  bits are today, so a forgotten `~` costs speed and not the session. Exact
  Newton's method for the square root of 2 crosses it at the twelfth step, the
  harmonic numbers at `h_2309`; `!449` fits and `!450` does not.
- **A whole number too large for a double is infinite** once approximated:
  `2^3322` and `!450` are `inf`. That is the double's limit, not the bound's,
  and a bound below 10^308 would only move numbers from exact to inexact
  without saving any of these.
- **Written here** (CLAUDE.md, section 5): a magnitude in 32-bit limbs, because
  a 32-bit product fits in 64 bits on every compiler CI has, with schoolbook
  multiplication, Knuth's division and Euclid's gcd. At a thousand digits
  nothing cleverer should pay for its lines; that is to be measured, not
  assumed. Python is the yardstick, measured at a thousand digits: 10 us a
  product, schoolbook below about 630 digits as ours will be; 160 us a gcd,
  by Lehmer's method, which is where Euclid would lose; 82 us a `Fraction`
  sum, the gcd and interpreted code on top. Lehmer's gcd, about a hundred
  lines, goes in only if a sum at a thousand digits measures slower than that.
  It went in with phase 14's PID: the exact `y_5000` spent 97% of its 1.2 s
  in Euclid's gcd, and with Lehmer's, seventy lines, takes 0.2 s -- ten times
  fewer instructions, and 8 us rather than 30 for a gcd at a thousand digits.

What it costs before a line of it: **the memo key.** A key is a value's bytes,
and a value on the heap has a pointer for bytes -- two equal values would be
two keys, and a freed address reused could make two different values one. The
key becomes an encoding each number type writes, a refactor that leaves every
output byte-identical and goes first. It also frees the layout: nothing needs
a `Number` without padding any more, so it can hold a 64-bit fraction, a big
one or a double in 24 bytes rather than 32, which is where the 1.7x on integer
matrices most likely lives.

It passed as written on the first build -- in 7 ms, `rt_11`'s 784-digit parts
included -- and is now `test/data/bignum.ink`; the spec suite retires again.
`Natural` is 256 lines in `bignum.hpp`, checked against Python's integers on
60000 operand pairs shaped to reach division's add-back step, which ran 24828
times; `Number` over it against Python's `Fraction` on 9000 expressions and
27000 conversions and printouts, the overflow to `inf` and the subnormals
among them. What moved in the goldens is what 64 bits used to approximate --
`2^63`, `!21`, `fib_93`, the binomial through `!52` -- and three entries meant
to test doubles now say so with `~`: `!~171`, `(~2)^1024`, `o_0=~1e308`.

**The layout was the wrong guess.** A variant of the three kinds is 24 bytes and
cost *more* -- 700M instructions on the matrix workload against 644M for 48
bytes and 456M before the bignum -- because every copy has to ask which kind it
holds. The cost was elsewhere, and a profile found it: the fallbacks to
naturals had made `+`, `-`, `*` and `/` too large to inline into a matrix
product, and returning an `optional<Number>` moved a value that is no longer
trivial to copy. With the fallbacks kept out of line and the fast paths
returning parts, integer matrices went from 784 ms to 596 ms against 548
before the bignum, and a short whole literal skipping the naturals made
parsing faster than it was. What is left, about a tenth, is the shared
pointer's copies: a build that leaked big numbers through a raw pointer was
that much faster on sequences, and no faster on matrices. The 1.7x against
master was never the size of a `Number` either; phase 14 has it.

## Phase 14 — The evaluator `[partly done]`

Step 1 landed its cheap end, a recomputed limit from 68.2M instructions to
32.5M; the closures it ends with wait. Step 2 landed as the C target, and its
exact C++ target is deferred.

Measured against the same computation written in Python, inkamath's arithmetic
is ahead and its evaluation is far behind: exact harmonic sums 169 ms against
`Fraction`'s 302, a 20x20 integer matrix product 638 ms against lists' 1336,
and a limit of doubles recomputed 6000 times 355 ms against a plain loop's 14.
Python's side was a loop written by hand and inkamath's parses and runs its
language, so the third is not a fair race -- but it says where the time goes,
and it is not the numbers.

A step of evaluation walks the tree through virtual calls, wraps every scalar in
a 1x1 matrix, builds a string for every memo key and opens a scope frame. In
order of cost to change: key by a hash rather than a string built per call,
keep a scalar unwrapped where the tree says it is one, and at the far end
compile the tree once to a flat form -- closures or a bytecode, 3-10x in
interpreters of this shape. Profile first, as phase 9 did; every recorded
output byte-identical throughout, which is what makes it safe to try.

**Step 1: compile in process.** A profile of that limit is flat: one term of
`s_n = s_(n-1)*~0.5+1.25` costs about 8,500 instructions, where Python's loop
spends a hundred, and no function holds more than five per cent of them --
1x1 matrices built and destroyed for every value, a name looked up by string
and hashed for every reference, a memo key built as a string for every call,
a budget and a frame for every call. A flat profile is not fixed by point
fixes but by doing once what is done every time: names resolved to slots when
a line is defined, invalidated by what already invalidates the memo, then the
tree turned into closures that call each other directly. No code is
generated, and every recorded output stays byte-identical.

The profile, taken before anything was changed, ordered the work differently
than the plan did: hashing names cost about one per cent, while values cost the
most -- 1x1 matrices built and copied (about a quarter), allocation (about
fifteen per cent), memo keys and index checks (seven each). The cheap end came
first: two single values added without the matrix loop, a closed frame's
storage kept for the next call, a memo keyed by the definition's address and
the index rather than a string -- the address holds while the memo does,
since redefining a global clears it first -- and an index tested with one
equality. Together they take a recomputed limit from 68.2M instructions to
55.3M, and the sequence workloads 12-17 per cent faster by the clock.

A call site then kept its parameters (52.4M), and a sign or a tilde on a
literal came to be applied when parsing (47.3M): the 1 of `n-1` had been
negated at every term. Keeping a scalar unwrapped, the middle of the order
above, was measured before it was built and is dropped. A 1x1 whose copy and
destruction skip its cell vector saves 1.1 per cent, and what a 1x1 still costs
over a bare `Number` is a call's prologue, about five per cent -- not worth a
second value type through the whole interpreter. The quarter the first profile
put on matrices was mostly the numbers inside them.

The call tree then showed a limit computing every term twice. It walked its
terms without remembering them, so each term's call for the one before missed
the memo and computed it again. Each term is now evaluated as its index would
be -- in a frame of its own, and remembered -- which takes the recomputed limit
to 32.5M and fixed C66 on the way. Names resolved to slots are measured and
wait: finding a name by string is at most six per cent, since libstdc++ scans a
map this small rather than hashing it and a frame holds two or three names.
Winning part of that would thread a symbol table through every binding;
closures need one anyway, and can bring it.

| | |
|---|---|
| C66 `[fixed]` | **A limit could disagree with its own terms.** `Converge` evaluated every term in the one frame of the limit's call, so a local bound by one term was still there for the next: with `c = 100`, the terms of `w_n = w_(n-1)/2 + c + 0*(c = 1)` tend to 200 and `lim w` answered 101. Each term is now evaluated as indexing evaluates it, in a frame of its own and through the memo, which also stops each term's call for the one before from computing it a second time. |
| C67 `[fixed]` | **Cell brackets after a named index went to the index.** A subscript's index was parsed as any simple expression, and a name there takes cell brackets, so `r_n[1,1]` was `r_(n[1,1])` -- the whole term, since the first cell of a single value is itself -- while `r_1[1,1]` was the cell. Found by the compiler, whose Kalman filter read `x_n[1,1]`; the quote had just been through the same fault, `x_(n-1)'` transposing the index. An index no longer takes brackets or a quote after it; those are the term's. |
| C68 `[fixed]` | **Exactness ran out without a word.** Past a thousand digits a number is approximated, by design, and phase 13 made `~` say only that the digits shown are not the whole value, whatever the kind, leaving `frac` to tell the kinds apart. That leaves nothing to tell a user who does not ask that an answer from exact inputs is now a double: an epidemic model is computed in doubles from its eighth step, and an exact third looks the same as a value whose exactness ran out. An inexact number now carries whether it was approximated past the bound, in the half of its layout an inexact number did not use, so a `Number` is no larger; what is computed from it carries it on, and an answer that has it ends in `# approximated past a thousand digits`, a comment, so that it still reads back as what it says. What was inexact by nature, or made so by `~`, is not marked: it never was exact. A comparison is not either: it is a truth, exactly. (Reversed by *A truth read from an approximated number*, next in line: a truth read from an approximated number is approximated, and so is the clause a guard chooses by it.) `frac` says why it refuses. Specified in `bignum.ink` first; five goldens that crossed the bound moved with it, `!(10^20)`, `!1e400`, `2^2147483648`, `0.5^3000000000` and `(0-2)^(2^70+1)`, each exact until it passed the thousand digits. |
| C69 `[fixed]` | **A long recurrence failed at 256 deep, which was neither the cause nor true.** Phase 14 fills a sequence from its base up so that each term finds the one before it remembered, and two things undid that past a few hundred thousand terms. The step budget counted the whole fill as one evaluation, so `g_600000` gave up at the millionth step; and a full memo was dropped whole, which phase 9 could call harmless because nothing then relied on it, so `ma_60000`, which reads `mb` as `mb` reads `ma`, lost the other sequence's latest term at the hundred-thousandth entry and nested down again. Either way the fill failed and reported the depth. The memo now keeps two generations of half the size, dropping the older when the newer fills, so the latest terms of every sequence survive at no cost per entry; each filled term has the step budget a line of its own would have, since a fill stands for asking them in order; and a fill goes ten million terms from its base at most, about three seconds, so that a slip such as `g_2000000000` says how far it is rather than hanging the session. A term that reads back further than fifty thousand entries of the memo can still be lost. |
| C70 `[fixed]` | **A guarded clause with an index left the value it followed.** An index turns a value into a sequence, so an unguarded one drops the plain clause; a guarded one did not, and after `r = 5` and `r_n \| n > 0 = 1` the name was both, `r` answering 5 and `r_1` answering 1. The interpreter coped, and the compiler, which asks a definition's first clause whether it is a sequence, took it for the value and compiled nothing. Found through the built-in `e`: `e_n[j<=2, k<=2] \| j == k = n` is only guarded clauses, and left Euler's number beside them. Any clause with an index drops the value now. `sequences.ink` says so; nothing else moved. |
| C71 `[fixed]` | **A compiled step started a term later than the interpreter where a guard decides what the term it reads back reads.** With `a_n \| n/8 - 3/4 > 2 = u_(n-1) + ...`, `a_n = 1/8` and `b_n = a_(n-1)/2` without a base, the interpreter answers `b_0` from `a_(-1)`, where the guard fails and the constant clause reads nothing. The compiler started `b` where every term `a` might read exists, so at 1, and left `b_0` at 0; it computed a term again at an earlier index only for a closed form, which reads no term at all. Found by the random models under another seed, and older than phase 15. Now a sequence with no base clause begins, for its readers, wherever some path through its clauses answers, and a reader that needs one of its terms before the window holds it computes the term again at that index, its guards tried in order and each read checked where it is read, NaN before it exists as the interpreter reports it. Where no reader needs it the step is as it was, so no header compiled before moved; `test/compile/back.ink` holds both cases. |
| C72 `[kept]` | **A limit's derivative where its terms' derivatives converge too slowly near the point.** `grad` takes a limit's derivative as the limit of its terms' derivatives, which holds where those converge uniformly near the point, and a single point cannot show that they do. `h(x)_n = h(x)_(n-1)/(1 + x^2)` from `h(x)_0 = x` tends to 0 for every x, so its limit's derivative is 0; at 0 every term's derivative is 1, and `grad` answers 1. Found by the review of the specification; `grad.ink` records the wrong answer so that it cannot change unseen. |
| C73 `[fixed]` | **`grad`'s checks for a jump read slopes, so a tangent passed.** A comparison or a guard at its threshold, `floor` at a whole number and an exponent were refused only where a first derivative was not zero, so `grad_(x = 0) (x^2 > 0)` answered 0 at a jump, a clause `x^2 == 0` gave its slope at the one point it holds, `2^((x-1)^2)` answered 0 at 1 and was refused at 2, and the same comparison under a nested `grad` was refused. Found by the review of the design. Whether a derivative is there at all is what is asked now. It refuses what is flat where the argument moves and lands on a whole number, as `floor(x^2)` at 0 does, which `grad.ink` records: a refusal over a silent wrong answer. |
| C74 `[fixed]` | **A hold inside a sequence at another rate was compiled in the input's steps.** In `s_k = x_(4*k) - u_(floor(k/2))` from `s_0 = 0`, with `u_m = x_(2*m)` from `u_0 = 0` and `x_n = n^2`, the hold names a term of `u` by `s`'s index, and the compiler, which resolves every hold by the step, read the latest term of `u` instead: at 4 it gave `s_1` as 0, where the interpreter gives 16. Found specifying what several rates left refused (next in line). The hold reads every 8 steps a sequence computed every 2, so the term it names falls further behind at each tick and no window holds it: it is to be refused by its rate. The hold at the input's rate sampled, which the refusal of a slow sequence read by another names, would say what the compiler gives, not what the interpreter does. Now a hold's period is checked once its reader's is known, as its `a` ticks of the reader, and `s` is refused, "read every 8 steps, and u is computed every 2"; `compile_c74` in `test/cli.cmake` holds it. |
| C75 `[fixed]` | **`--check` holds a term read from before the stream to the input there.** With `c_n = x_(n-1)` in a model and an instance giving `x_n = n^2`, the interpreter answers `c_0` from `x_(-1)`, 1, where the check feeds the compiled step its inputs from the first step on, as a host does, so its window holds 0, and the report has the two part at 0. Found specifying what several rates left refused (next in line), whose models feed a stream that starts at 0, `s_n \| n >= 0 = n^2`, where the interpreter has no `x_(-1)` either. Whether the check should give the window the input's earlier terms, holding the step to what no host gives it, or say that the step cannot have them, was open; neither is taken: the model states its input's history, which the interpreter and the step both read, and a read before the stream that no history gives is refused (next in line: a model's history of its inputs). Now `delay(x_n) = { x_n \| n < 0 = 0; c_n = x_(n-1) }` given `x_n = n^2` answers `c_0` 0 both ways, `init` folding the history into the window, and the check parts where the interpreter has no term and the step has one; `test/data/history.ink` and `test/compile/history.ink` hold it. |
| C76 `[fixed]` | **A compiled hold rounded its lag toward zero.** A hold's place in the held sequence's window was `(n - phase)/a - (n - b)/a - d` in C's division, which truncates, and its first step was clamped to `b`: a hold whose numerator could be negative, `y_(floor((n - 3)/2) + 1)` on a sequence based at a negative index, read the wrong cell or left its first steps 0 where the interpreter gives a term. Found by the review of the rates gaps, with 1200 random models compared against master, one of which master compiled to NaN where the interpreter has a value. The lag is now a floor, its first step where the term exists, and the emitted division keeps each numerator non-negative; `trail` in `test/compile/decimate.ink` holds it. |
| C77 `[fixed]` | **`grad` took a tensor's slice name for a global.** Its check that no definition reads the global of its name binds a clause's index, row and column, not its slice, so after `Gc[b<=2, j<=1, k<=1] = b`, `grad_(b = 2) b*Gc` is refused, "Gc reads the global b, which grad's b does not reach", where `Gc` is a constant and the answer `[1;; 2]`. Found specifying `grad` of definitions by cells (next in line), whose specification holds it. Now the slice is bound as the row is; `Gs` in `test/data/grad.ink` holds it. |
| C78 `[fixed]` | **An index that moves with `grad`'s name answers as if it did not.** An index is a whole number, so one that moves is at a jump, as `floor` is at a whole number, which C73 refuses; but with `s_n = n^2` and `v = [5; 7; 9]`, `grad_(x = 2) s_(x)` and `grad_(x = 2) v[x]` answer 0. Found specifying `grad` of definitions by cells (next in line), which refuses a moving size by that rule; an index wants the same check where it is read, not part of that entry. Now a cell's index and a term's are read with their parts where they read the name, and one that moves is refused, "an index jumps at x = 2", by the structure C73 asks of `floor`, so `vk[x - x + 3]` is refused as `floor(x - x)` is; the C78 entries in `test/data/grad.ink` hold it. |
| C79 `[fixed]` | **`grad` did not read a size for the global of its name.** Its check that no definition reads that global scans a clause and its guard, not the bounds of its cells, so after `gz[j<=x] = j` with `x = 100`, `grad_(x = 2) x*gz` answered `gz`, a hundred cells long, as a constant. Found building `grad` of definitions by cells (next in line), which reads a size with its parts. Now the bounds are scanned as the clause is, and the call is refused, "gz reads the global x, which grad's x does not reach"; `gz` in `test/data/grad.ink` holds it. |
| C80 `[fixed]` | **`grad` reads a parameter that an index or a row hides.** A clause's index, row, column and slice are bound where its parameters are, and hide one of the same name, but `grad` seeks a name among the arguments it differentiates before the values bound: with `g(n)_n = n`, `g(x)_2` is 2 for every `x`, and `grad_(x = 1) g(x)_2` answers 1; with `f(j)[j<=2] = j`, `grad_(x = 1) [1 1]*f(x)` answers 2, not 0. Found building `grad` of definitions by cells (next in line), whose walk binds a row as the evaluator does. Hiding the clause's names from the arguments where they are bound, as a sum's index is hidden, would fix both; the walk binds them in `Reference`, where `grad` cannot hide them, so for now a clause whose index, slice, row or column names a parameter is refused under `grad`, "grad cannot differentiate sm yet: its row t hides its parameter t": a temperature `t` beside attention's rows `t` answered `[10; 10]` for `[1; 2]`. The C80 entries at the end of `test/data/grad.ink` hold it. |
| C81 `[fixed]` | **`grad` kept the slope of a single value added to a matrix single.** A sum adds a single value to every cell, and a matrix to every slice of a tensor, but a part that only one side had kept that side's shape, so `grad_(x = 1) [1 1]*(x + [1; 2])` answered `[1, 1]` where the slope is 2, `([1 2; 3 4] + x)^2` the slope it would have were x added to the diagonal only, and `f(x)[j<=1] = (x + [1 2])*[1; 1]`, whose cell stores its part's first, a slope of 1 for 2. Found reviewing `grad` of definitions by cells (next in line), whose walk stores a cell's part as its value is stored. Now a sum's part is widened to its value's shape; `grad.ink` holds it. |
| C82 `[fixed]` | **A guarded clause written whole beside clauses for cells is never asked.** With `Q2(x)[j<=2] = x` and `Q2(x) \| x > 1 = [0 0]`, `Q2(2)` is `[2; 2]`, and with `R \| 1 > 0 = [5 5]` before `R[j<=2] = j`, `R` is `[1; 2]`: the walk over the cells takes a clause written whole only as the value of the cells no clause gives, and a guarded one not even then. Found by the review of `grad` of definitions by cells (next in line), with which `grad` agrees. Taking the whole definition where it holds, before its cells, would add nothing a guard on each cell does not say, `R(x)[i<=2] \| x > 1 = 0`, so such a clause is refused where it is written, in either order, "Pw is defined by its cells, so a clause for all of it cannot be guarded; guard its cells"; a clause written whole and unguarded stays the matrix the cells override. The entries at the end of `test/data/matrices.ink` and `test/data/terms.ink` hold it. |
| C83 `[fixed]` | **A model's input is compiled as a single value, whatever it is.** The step takes one double for each input and the compiler reads the input as 1x1, so with `mm(x_n) = { y_n = [1 2]*x_n }` the header computes a 1x2 term where the interpreter, given `x_n = [n; 1]`, answers a single value. `--check` of that instance crashed, reading a second cell of each term the interpreter gave one of; it now refuses the instance by name, "v.x_(0) has 2 cells, where the compiled step takes a single value", and `check_matrix_input` in `test/cli.cmake` holds it. Found specifying a model's history of its inputs (next in line). What stays open is the header: an input wants a shape, taken as an array as a matrix parameter is, or a refusal by name. Specified in next in line, *an input of more than one cell*: the model states the size in its signature, `x_n[j<=2]`, and the step takes the input's cells, as built. One whose size is not stated compiles as a single value still, as specified, and each refusal of another now says how to state it: a cell past it, `cannot compile y: x is a single value, as cl states no size for it: write 'x_n[j<=2]'`, and `--check`'s, which ends `, as mm states no size for x: write 'x_n[j<=2]'`. |
| C84 `[fixed]` | **`--check` compares nothing of an instance a model writes unnamed.** With `bare(u_n) = { c_n = u_(n-1) }` and `wrap(x_n) = { c_n = bare(u_n = x_n).c_n }` checked as `nest = wrap(x_n = n)`, the step keeps the instance as `c_bare`, and the check asks the interpreter for `nest.c_bare.c_0`, which it cannot name, "a term has no names", so every term of `c_bare.c` and `c_bare.u` is skipped and each line reads `within 0`. No checked instance writes one. Found reviewing the specification of a model's history of its inputs (next in line), whose check reports a term the interpreter cannot give instead of skipping it, which would turn each such line red. The check wants to ask for such a term through the definition that writes the instance, or to say by name that it cannot. It said so, `veiled.c_bare.c: not asked, as the interpreter cannot name it`, where each line read `within 0`. Now the check makes the instance again where the instance checked has it, from the model, call and captured places the compiler kept it by, and asks its terms there: `veiled`'s lines read `within 0` having asked, `cloaked`'s unnamed instance is written within a named one, and `masked`'s parts at 9 as `wild` does, in `test/compile/history.ink`; 24 lines more. |
| C85 `[fixed]` | **`tex` sets a base term apart from the guarded clauses written before it.** Clauses are tried in the order written (C45): after `y_n \| n < 0 = 0`, `y_(-1) = 3` and `y_n = n`, `y_(-1)` is 0, but `tex ?y` sets `y_{-1} = 3` on a line of its own above the cases, as if it held. Found reviewing the specification of a model's history of its inputs (next in line), whose clauses on an input may come in either order. `tex` wants the clauses in the order they are tried, or the base term left out where an earlier clause covers it. Now a term written after a guard is a case in its place, `3 & \text{if } n = -1`, and one written before every guard keeps its line, which holds; the C85 entries at the end of `test/data/tex.ink` hold it. |
| C86 `[fixed]` | **A clause whose value is a constant that fails refuses the whole model when compiled.** The compiler folds a constant value exactly, and a failure there is taken as the model's: in a model `gd(a = 2)` defining `h(x) = 1`, `h(x) \| x <= 0 = 1/0` and `y_n = h(a + n)`, `--compile` says "cannot compile y: division by zero" where the interpreter answers 1 for every term and never takes the clause, and `[1 2]*[1 2]` there says a product's sizes. `log`'s `log(x) \| x <= 0 = 1/0` is one, so a model calling `log` is refused in those words instead of "a sum or a product with no upper bound", which its series earns. Found specifying a function applied to each cell, `f.(x)`, which sizes inferred replaced (next in line). A clause written to refuse wants compiling as what a step does where the interpreter refuses, NaN as for a limit that does not converge, or a refusal naming the function and its clause. Now a guarded value whose fold fails is NaN where its guard holds, as where no clause applies, so `gd` compiles and `log` is refused for its halving, `log(x/2)` while `x > 2`, which recurses, "calls nested 64 deep"; `compile_c86` and `compile_log_refused` in `test/cli.cmake` hold it. A value that always applies and fails still refuses. |
| C87 `[fixed]` | **The hint for a definition of one-cell clauses alone is written for another.** With `w[2] = 5`, reading `w` says "w has no size; write it as w[j<=rows, k<=cols]", two indices where its clause writes one; with `f(x)[1,1] = 5`, `f(1)` says "write it as f[j<=rows, k<=cols]", without its parameters, a clause `f` refuses, as it takes `(x)`. Found reviewing the specification of sizes inferred in a definition by cells (next in line), whose hints are written as the clause is, `sh(z)[i<=rows]`. The hint wants the clause's parameters and as many indices as it writes. It now has them, `w[j<=rows]` and `f(x)[j<=rows, k<=cols]`, written from the clause as a hint for a clause for all cells is, and one defined inside an expression, which keeps no text, from its names, `pc[i<=rows]`; the C87 entries of `test/data/matrices.ink` hold it. |
| C88 `[fixed]` | **A product or quotient of two real numbers grew a NaN imaginary part where it overflowed.** An inexact number is complex, and `*` and `/` took two real ones by the complex formula, whose cross terms multiply an infinity by the other's zero imaginary part: `10^400*~1` answered `inf+i*-nan`, `1/~0` `inf+i*-nan` and `0/~0` `-nan+i*-nan`, and an unrolled training run whose learning rate overshot printed its loss so. A power was fixed the same way before. Found training a learning rate by `grad` through an unrolled descent. Two real numbers now multiply and divide as reals, so these are `inf`, `inf` and `-nan`, which moves the two recorded in `test/data/errors.ink`; `i/~0` keeps C31's NaN part in view. Complex arithmetic on infinities stays the platform's (C64). |
| C89 `[fixed]` | **A history by cells leaves the argument nothing.** A clause of a history by cells is guarded, but its guard chooses cells, not terms, and a cell no clause gives is 0, so it gives every term: with `x_n[j<=2] \| n < 0 = 0` and `c_n = [0 1; 1 0]*x_(n-1)` in `sw(x_n)`, `sw(x_n = [n; 2*n]).c_3` is `[0; 0]`, where the argument gives `x_2 = [2; 4]`. An unguarded clause, which does the same, is refused by name, `x is an input of sw, so its body can give it only a history`; this one is not. The compiler finds no history in it, `cannot compile c: c_0 reads x_-1, before the stream, where x has no history`, so nothing compiled differs. Found specifying an input of more than one cell (next in line). A history by cells is now refused in those words, its size stated or not; the C89 entries of `inputs.ink` hold it. |
| C90 `[fixed]` | **`--check` kept a NaN in an int where a guard of `and` or `or` defers.** A guard whose right side reads a term its left does not is a truth of 1, 0 or NaN, and the clause the check keeps tests it as the step does, `m_->z_clause_ = isnan(t1_) ? NAN : t1_ != 0.0 ? 1 : 2;`, so where the truth is NaN a NaN becomes an int, which C leaves undefined. With `y_0 = 1`, `y_n = y_(n-1) + x_n`, `z_n \| x_n > 0 and y_(n-2) > 0 = 1` and `z_n = 0` in `gate(x_n)`, the program checking `g = gate(x_n = 1)`, built with `-fsanitize=float-cast-overflow`, stops at step 0, "nan is outside the range of representable values of type 'int'"; without it the report is right, as no clause is compared where the interpreter gives none. No checked instance has one. Found reviewing the specification of a NaN that reaches every term that reads it (next in line), which keeps 0 there. Now the clause kept is 0 where its guard is NaN, as no clause is taken, `isnan(t1_) ? 0 : t1_ != 0.0 ? 1 : 2`; `check_c90` in `test/cli.cmake` holds it. |
| C91 `[fixed]` | **`--check` writes a value that is no finite double as no C.** An input and a guard's margin are written by `%.17g`, so where the interpreter's is infinite or NaN the program reads `inf` or `-nan` and does not compile: with `y_n = x_n - x_n`, `h_n \| y_n > 0 = 1` and `h_n = 0` in `ov(x_n)`, checking `w = ov(x_n = ~(10^400))` writes `inf` for each input and `-nan` for each margin of `h`. A term the interpreter gives so is reported instead, "where the interpreter's term is not a finite number". Found reviewing the specification of a NaN that reaches every term that reads it (next in line). Now a value no finite double holds is written as C reads it back, `INFINITY`, `-INFINITY` or `NAN`; `vast` in `test/compile/history.ink` holds it. |
| C92 `[fixed]` | **A guarded sequence named before a late term its guard reads computed both from the stream's start.** With `y_0 = 1`, `y_n = 2*y_(n-1)`, `d_n = y_n - y_(n-1)`, `a_n \| d_n > 0 = 1` and `a_n = 0` in a model, `d` starts at 1, but the step computed `d` and `a` from 0, reading `y_(-1)` as the 0 `init` leaves, where the interpreter refuses: C13's class. A guard is compiled with what it reads deferred, and `d`, compiled on the way while `a` sorted first, filed its own reads as deferred too, so it read nothing and started at once; named `e`, the reader came after. A sequence compiled on the way now starts with nothing deferred, as it starts with no shift. Found reviewing the specification of a NaN that reaches every term that reads it; `kin` in `test/compile/history.ink` holds it. |
| C93 `[fixed]` | **An input whose history is terms alone has no limit.** `lim x` asks the input's own clauses for a general one, and a history of terms has none, so with `x_(-1) = 0` and `l = lim x` in `last(x_n)`, `last(x_n = 2).l` says "x has no general clause, so it has no limit" where the terms are 2 from 0 on; a guarded history, `x_n \| n < 0 = 0`, is general, and the limit is taken. Found reviewing the implementation of an input of more than one cell, whose stated input, holding no clause at all, was refused so for any argument. An input's terms are its argument's where its history gives none, so it now has a limit whatever its history; `last` in `test/data/history.ink` holds it. |
| C94 `[fixed]` | **`--check` steps an input its instance gives by the model's default for it.** The check compiles the instance with its inputs unbound, to feed them, and an input with a default then falls back to it: the step computes the default and takes no input, while the interpreter reads the argument. With `y_n = 2*x_n` in `dd(x_n = n)`, checking `w = dd(x_n = 1)` reports `w.x: 0 at 0, where the interpreter gives 1` and `w.y` likewise, a drift of the compiled step that is a different stream. Found reviewing the implementation of an input of more than one cell, whose sized default does the same. Such an instance wants refusing by name, as the header of its model cannot be given that input, or its argument compiled in place of the default. Now an input the instance gives is fed as the interpreter reads it, and one it does not give compiles the model's default, as the interpreter reads that; `fed` in `test/compile/inputs.ink` holds it. |
| C95 `[fixed]` | **Five check reports were held to half their text.** A report's expected text is a CTest regular expression, and CTest reads a `;` in it as the end of one expression and the start of another, passing a test that matches either: the guard-flip lines of `brink`, `ledge`, `rift`, `sill` and `seam` hold `'...'; the guard of the first is ...`, so each report passed on its first half alone, and `brink`'s with its threshold words replaced by others still passed. Found implementing the interpreter's own error, estimated by `--check`, whose new expressions escape it. The five now write `\\;`, and the replaced words fail. |
| C96 `[fixed]` | **`tex` showed a decimal of more than nine digits to nine, after a `~`.** A literal is set by the printer of values, which shows an exact number that is no short decimal to nine digits and marks it so: `c = 1.4426950408889634` is set `c = ~1.44269504`, and `f(x) = x*0.12345678901` as `x\,~0.123456789`, the `~` a space in LaTeX and the product a thin space where a digit asks a dot. What was typed is exact and should be set in full. Found specifying `exp`, `log` and `tanh`, whose constants have seventeen digits. A literal is set with as many digits as end its decimal now, `c = 1.4426950408889634` and `x \cdot 0.12345678901`, and one whose decimal does not end as before, in 11 lines; latex.ink holds it. |
| C97 `[fixed]` | **The checks failed where the target fuses a multiply and an add.** The header leaves contraction to its build, as *Fused multiply-adds* decided, and the test targets set nothing, so `clang -march=native`, which contracts within an expression on a machine with FMA, parted from the interpreter in `brink`, `ledge`, `rift`, `sill` and `seam`. Found reviewing the specification of the prelude written in inkamath (next in line). The C test targets now build with `-ffp-contract=off` under GCC and Clang, and the five pass so built; the header's policy is unchanged. |
| C98 `[fixed]` | **A compiled negation of +0 is -0.** The compiler writes a negation as C's `-`, and the interpreter as `0 - x` (C33), so at +0 the step has -0 and the interpreter +0: `1/(-x)` at `x = 0` is -inf compiled and inf interpreted, and `tanh(0)` in the prelude being written in inkamath (next in line) is -0.0 compiled. Found reviewing that specification. A negation is now written `0.0 - x`, which moves `pid_clamped.h`, `kernel.h`'s `ceil` and the prelude's header and nothing else; `--check` holds -0 equal to 0, so `naught` in `test/compile/drift.ink` holds it through a guard reading `1/(-x_n)`. A subtraction stayed C's, until C109. The prelude called compiled (next in line), which left an answer of 0 to the definition for it, no longer does. |
| C99 `[fixed]` | **`--check` estimated a huge term infinitely far from the exact one.** The absolute value of an inexact number, which is complex, was the root of the sum of the squares of its parts, and a difference past 2^512 squared is infinite: two disturbed runs of `n*~(3*10^200)`, some 10^186 apart, were `inf` apart, so its report said the interpreter's terms were "about inf from the exact ones, past the tolerance from 1", as it did of `exp`'s from about 390. A limit's step and a pivot's size took the same absolute value. Found reviewing the implementation of `exp`, `log` and `tanh` written in the prelude (next in line). A real number's is now its magnitude; a complex one's is as it was. `giant` in `test/compile/estimate.ink` holds it. |
| C100 `[fixed]` | **`ilogb(0)` depends on how 0 is written.** The prelude's `ilogb` means something only of a positive number, but it is a public name, and at 0 its thirteen steps take every threshold that rounds to 0 as reached: an exact 0 passes the exact powers down to 2^-3321 and stops where they are approximated, `ilogb(0)` being -3322, and a double 0 where the double powers vanish, `ilogb(~0)` and the step's being -1075. `log` guards 0 before it asks. Found reviewing the implementation of `exp`, `log` and `tanh` written in the prelude (next in line). Now `ilogb(x) | x <= 0 = 1/0` refuses 0 and below as `log` does, and the header's `ilogb` is NaN there. |
| C101 `[fixed]` | **`log(~1/0)` and `log(2^3072)` name a NaN never written.** Both refuse with "a comparison needs a number, not -nan": `ilogb` answers 4095, as a power of two past a thousand digits is approximated to inf and the argument passes it, and inf over 2^4095, inf too, is NaN, which `logm`'s fold cannot compare. The step answers NaN and libm inf. A guard saying so costs more than three lines and moves two entries of `test/data/elementary.ink`, so the implementation's ruling kept it. Found reviewing that implementation (next in line). Two clauses now: `ilogbs(x, k, s) | k + s > 3321 = k`, so that `ilogb` tries no power past 2^3321, which every exact number is below, and answers 3072 at 2^3072 and 3321 at inf; and `log(x) | 2*x == x = x`, inf being the one positive number its own double, so `log(~1/0)` is inf, as in libm and now the step, and `log(2^3072)` 2129.34814. Five entries of `test/data/elementary.ink` and two of `fastprelude.ink` move; no double's answer does. |
| C102 `[kept]` | **A compiled double limit creeping by less than half a unit answers where the interpreter refuses.** With `s_0 = 10^17` and `s_k = s_(k-1) + 1`, `lim s` does not converge in the interpreter, whose terms are exact, but in a double every term is 10^17, a step of 0, which the rule takes as converged, so the step answers 1e+17. `--check` catches it: `y: 1e+17 at 0, where the interpreter gives none: s did not converge within 100 terms`. Inherent to doubles: a step below half the spacing at its term is no step. Found reviewing the specification of a float target (next in line), whose extra stop answers so for a step of a unit of a float, `crawl` in `test/compile/float.ink`. |
| C103 `[fixed]` | **A parameter's default of negative infinity was written `INFINITY`.** A constant in an expression is written as its magnitude after its sign, but a default is written whole, in `init` and in the header's comment, and a whole infinity lost its sign: `p = 0 - ~(10^400)` was `m_->p = INFINITY;`, as, in a float header, was `q = 0 - 10^39`, which no float holds. Found implementing a float target (next in line). It is now `-INFINITY`; `compile_c103` in `test/cli.cmake` holds both. |
| C104 `[fixed]` | **A header named after a word C keeps, or after a header it includes, was no C.** A header or a program is named after its file, which had only to be letters, digits and `_`: `-o double.h` wrote `typedef struct double`, and a float header written to `float.h` both is no C and, built with `-I.`, includes itself for `<float.h>`. Found reviewing the implementation of a float target (next in line). Such a name is now refused, a keyword or `math`, `string` or `stdio`, in 3 lines; `compile_c104` and `check_c104` in `test/cli.cmake` hold it, and `history_short`'s model is now `scant`. The names of libm's functions stay open: `-o nan.h` declares a type `nan` over `<math.h>`'s function, and so would `pow`, `exp` or `floor`, a list with no end the C library fixes. |
| C106 `[fixed]` | **`--check` heard no guard of a term the file had asked.** A file's lines run before its check, and a term one of them asks is memoised, so the main run read it back without asking its guards: with `brink.g_(1)` added to `drift.ink`, `brink`'s report said the compiled step took another clause at 2, not 1. Since the prelude is called compiled (next in line), such a term was not walked either, which every run of `--check` must. Found reviewing that implementation. The main run now forgets the memo before its first input, in 1 line; `check_walked_asked` in `test/cli.cmake` holds it. |
| C107 `[fixed]` | **Two named instances of one model shared the unnamed instance it writes.** An unnamed instance was kept by where it is written and what it reads there, not by the instance that writes it, so with `a = veil(x_n = y_n)` and `b = veil(x_n = 2*y_n)` in a model, `veil` writing `bare(u_n = x_n).c_n`, the header kept one copy, `a`'s, and `b.c` read `a.c_bare.c`: `--check` reported `two.b.c: 1 at 2, where the interpreter gives 2`. It is now kept by its scope too, in 1 line; `two` in `test/compile/history.ink` holds it. |
| C108 `[fixed]` | **`--check` refused an unnamed instance with a guarded clause.** The check reads a guarded sequence's clauses, to report a guard that takes another clause, from the instance checked by the sequence's name, and an unnamed instance's is no name there: with `c_n \| u_n > 2 = 2` and `c_n = u_n` in `capped(u_n)`, and `c_n = capped(u_n = x_n).c_n` in `cap(x_n)`, checking `hid = cap(x_n = n)` stopped, "the instance has no sequence c_capped.c", before and after C84's fix. They are now read from the instance made again for its terms (C84), in 4 lines; `hid` in `test/compile/history.ink` holds it. |
| C109 `[fixed]` | **A compiled subtraction of +0 from -0 is -0.** The interpreter reads `a - b` as `a + -b`, the negation `0 - b` (C33), and the compiler writes it as C's `a - b`, alike but at a = -0 and b = +0, where the step has -0 and the interpreter +0: `1/(~0*(-1) - ~0)` is inf interpreted and -inf compiled. `--check` holds -0 equal to 0. Left open by C98's fix; grad compiled had it too, the quotient rule's `a' - q*b'` at x = -0 making `1/grad_(t = 1/2) ((t*x)/(1 - t))` -inf compiled. The owner's decision: the compiler writes the interpreter's operations, `a + (0.0 - b)`, `0.0f` in a float header, so they agree by construction; the interpreter keeps its +0 (C33). A negative number written stays `a - 2.0`, the same in IEEE arithmetic. 43 subtractions moved, in 9 of the 14 expected headers, both float ones and the prelude's, whose compiled and walked functions still agree to the bit, signs included, on 80,000 calls; `hand` costs 0.02% more instructions. `zeroed` in `test/compile/drift.ink` and `cusp` in `grad.ink` hold it through `1/x`. |
| C110 `[kept]` | **A compiled gradient through a clause with no part times an infinity is NaN.** A part is there by the clauses, 0 where the clause taken has none, so `grad_(t = 6) f(t)*~(10^400)`, f a clamp at 1, has 0 times inf for its part, NaN, where the interpreter has no part to multiply and answers 0. Kept: a part of 0 is what lets a chain of clauses be one C expression, and only an infinity tells it from none; testing where each product's part is there costs a condition per product for a value no model reaches. A power whose derivative is infinite at a base of 0 reached it without an infinity written, and is tested since (C118); a quotient by a dead clause's 0, `grad_(t = x) (~(1/3)/relu(t) + t)` NaN where x < 0 though the interpreter answers 1, is too: its part is a'/b where its divisor's is not there, 0 without a', in 11 lines, `e` in `edge` holding it. What stays needs an infinite value times a part of 0, written or a quotient's: `grad_(t = x) (~(1/3)/relu(t)*relu(t) + t)` is NaN where x < 0 and the interpreter answers 1. Found specifying `grad` compiled (next in line); `apart` in `test/compile/grad.ink` holds it, its program failing. |
| C111 `[fixed]` | **A compiled gradient answered where the jump inside `exp`'s part is dropped.** `exp` reduces by `floor(x*1.4426950408889634 + 1/2)`, and at the few doubles where that is whole, ~0.34657359027997264 among them, the interpreter refuses `grad` of `exp` as it refuses any `floor` at a jump, and the compiled part function is NaN there. Where a `floor` or a comparison then reads `exp`'s value and drops its part, `grad_(t = x) floor(exp(t))*t` is 1 compiled and refused interpreted. The interpreter's refusal is itself spurious, `exp` being smooth there; the compiler's NaN follows the interpreter's. Found reviewing the specification of `grad` compiled. Fixed with C117: `exp`'s value where its argument moves is a function of its own that tests the jump, so the `floor` reads NaN there; `smooth` in `test/compile/grad.ink` holds it. |
| C112 `[fixed]` | **A global read inside a compiled function reads the function's parameters.** A function is compiled where it is called, its parameters bound for its body, and a global its body reads is compiled where it stands but still sees those bindings: with `f(x) = x + g`, `g = x*2`, `x = 5` and `y_n = f(n)`, the interpreter's y_3 is 13, and the header computes `m_->g = (double)y * 2.0`, the call's argument, in `update`, which is no C. `grad` refuses one that reads grad's own name, through `Derivative::Names`; nothing refuses the rest. Found implementing `grad` compiled (next in line). An instance without memory read the same, its parameters for the call's. A global is now compiled with no call's names, as with no index or place, in 1 line; `compile_c112` and `compile_c112_instance` in `test/cli.cmake` hold it. A header holding a control character, a mark left unresolved as this one was, is now an error, in 3 lines, which no input reaches since. |
| C113 `[fixed]` | **A compiled gradient answered where the interpreter refuses in a value no part reads.** The test for a refusal is written in the value, a `floor`'s or a comparison's, and `grad` keeps only the part, so where no part reads that value and the header writes no NaN otherwise, the test was dropped with it: `grad_(t = x_n) (t - floor(t))` was 1 at every whole x, where the interpreter says the floor jumps, and so for a comparison, a clause refused wherever taken and a power whose derivative is infinite. Found reviewing the implementation of `grad` compiled (next in line). A body whose value writes NaN now makes the header write it, as a cell not taken does, so its gradient is NaN where that value is, in 1 line; `saw` in `test/compile/grad.ink` holds it. |
| C114 `[fixed]` | **Where one of two operands' parts is there was read by C as one conditional.** Each is a chain of conditionals, `z < 0.0 ? 0 : 1`, and they were joined by `||` unparenthesised, which binds tighter: `a ? 0 : 1 || b ? 0 : 1` is false wherever the first is, so a comparison of a dead ReLU unit with a live one, `grad_(t = x_n - 1) (r(t - 1/2) > r(t))*t` with r a ReLU, was 0 at t = 0, where they meet and the interpreter, the live one moving, says the comparison jumps. Found reviewing the implementation of `grad` compiled (next in line). Each is now parenthesised, in the same line; `m` in `edge` in `test/compile/grad.ink` holds it. |
| C115 `[fixed]` | **A constant gradient at a point that moves refused what read it.** A gradient whose part is a constant, `grad_(t = x_n) 3*t`, was a constant of the step, and an operator reading two constants folds by the interpreter, which takes the grad again, point and body, without the step's names: `u_n = x_n - grad_(t = x_n) 3*t` said "cannot compile u: n is not defined", in a header that writes no NaN, where an aware one compiled it. Found reviewing the implementation of `grad` compiled (next in line). It is now a constant only where its point and body are, in 1 line; `compile_c115` in `test/cli.cmake` holds it. What reads it then rounds in the step: `(1/10)*grad_(t = x_n) 3*t` is `0.1 * 3.0` there and 3/10 interpreted, a unit apart. |
| C116 `[fixed]` | **A power's infinite derivative at a constant point was infinite.** The test for a base of 0 under a power whose derivative is infinite there was written only for a base that is not a constant, and at a constant point the part folds: `n*grad_(t = 0) t^(1/2)` was `index_ * INFINITY`, where the interpreter refuses every term. Found reviewing the implementation of `grad` compiled (next in line). A constant base of 0 is now tested too, so the step is NaN at every n, as for a comparison or a `floor` at a jump at a constant point, in the same line; `v` in `edge` in `test/compile/grad.ink` holds it. |
| C117 `[fixed]` | **A compiled gradient through `ceil` or `mod` answered at `floor`'s jump.** Each is the prelude's by `floor`, compiled as a function of the header's from its body without parts, so its `floor` tested no jump: `grad_(t = x) mod(t, 2)` was 1 and `grad_(t = x) ceil(t)*t` was `ceil(t)` at every whole x, where the interpreter says the floor jumps. Found reviewing the implementation of `grad` compiled (next in line). Where an argument moves, the value is now its body carrying parts, a function of its own, `mod_ja`, where that tests for a jump, and the plain one where the arguments' clauses leave nothing moving; its call makes the header write NaN, so that a grad that drops the value is NaN where it is (C113). In 14 lines; `ring` and `notch` in `test/compile/grad.ink` hold it. |
| C118 `[fixed]` | **A compiled gradient through a dead ReLU under a root was NaN.** A power whose derivative is infinite at a base of 0, `c*u^(c-1)*u'` for c < 1, multiplied the part 0 of a clause that has none by the infinity of its base's 0: `grad_(t = x) relu(t)^(1/2)*t` was NaN wherever x < 0, where the interpreter, the clause taken having no part, answers 0. C110 reached by a model, with no infinity written. Found reviewing the implementation of `grad` compiled (next in line). Such a power's part, for 0 < c < 1, is now tested where its base's is there, 0 elsewhere, in 4 lines; below 0 its value is infinite there too, the interpreter refusing the division, and its part stays NaN. `h` and `i` in `edge` in `test/compile/grad.ink` hold it. |
| C119 `[fixed]` | **A term read where the clause's index is not seen named no index.** A call's body, or a grad's taking the index's name, does not see the clause's index, and the refusal of every term read there named it as empty: `f(t) = x_t` and `y_n = f(n)` said "x_(...): an index other than a whole multiple of  plus a constant", where the interpreter's y_3 is x_3. Found alongside C112. It now says "a term read where y's index is not seen", in 3 lines; `compile_c119` in `test/cli.cmake` holds it. A call given the index, give or take a constant, now reads the term as the interpreter does: each such argument's distance from the caller's index is kept for its parameter, in 18 lines, `compile_c119_call` holding it; under a grad taking the index's name the read stays refused. |
| C120 `[fixed]` | **`--check` fed an input of -0 as +0.** Its program writes each input with `%.17g`, which gives `-0`, and C reads that as the integer 0, so a check whose input is -0 stepped +0 and could pass against the wrong value: `1/x_n > 0` at `x_n = ~0*(-1)` was taken compiled and not interpreted. Found fixing C109. Fixed by writing a number with no point or exponent as a double, `-0.0`, as the compiler does. |
| C121 `[fixed]` | **A compiled `log` in a NaN-aware header computed each power of 2 twice.** A decision tests each operand for NaN, and each of `ilogb`'s thirteen steps compares x with `2^(k + s)`: its test was `isnan(pow(2.0, k + s))`, a second call that C, pow setting errno, does not share with the comparison's. Under callgrind a step of `y_n = log(x_n)` took 3,430 instructions with GCC -O2, against 2,011 with no NaN test, and 1,581 against 794 with Clang. Found reading the prelude header. A power of a constant above 0 but 1 is NaN only where its exponent is, and is now tested by it, in 8 lines: 1,705 and 982 instructions. Of the headers, `inkamath_prelude.h`'s `ilogbs` moves so, its values bit-identical to the walked prelude's on 480,000 arguments, each also as 1/f; `check_els_functions` holds it. |
| C122 `[fixed]` | **A cell of one term outside its size was written past the term's cells.** The cells of one term that no base term gives are folded into the general term's, each at its row and column, and one outside the size, `y_1[3] = 5` beside `y_n[j<=2] = n`, was stored there unchecked: the compiler wrote past its cells and crashed, where the interpreter refuses y_1, "row 3, column 1 is outside a 2x1 matrix". Found implementing tensors compiled (next in line). It is now refused in those words, as a cell of every term is, in 1 line; `compile_c122` in `test/cli.cmake` holds it. |
| C123 `[kept]` | **An exact argument nearer a multiple of pi/2 than pi/2 is held gives `sin` or `cos` wrong.** The reduction's five parts are 7.4e-49 below pi/2 (2^-159.9), and an exact argument is reduced by them exactly and rounded once, so r is off by k times that: under a unit only where r is above about k 1.3e-32, 1e-31 from pi and 1.4e-26 from a multiple near 2^20. `sin(314159265358979323846264338327950288419716939937510/10^50)`, 5.8e-51 from pi, gives ~-1.47387998e-48 where mpmath gives +5.82097494e-51. No double comes so near: below 2^20 the nearest is 6.2e-19 from 29 pi/2, and the fast path takes doubles alone. Found reviewing `trig`. Kept: more parts move the bound and do not remove it, as an exact argument may lie as near as it likes, and reducing it exactly needs pi to as many digits as it has. `trig.ink` records it. |
| C124 `[fixed]` | **A tensor of one cell was moved as a single value.** Its member keeps its three dimensions, `double y[2][1][1][1]`, as tensors compiled decided, but the step shifted its window and copied its input as a single value's, `m_->y[1] = m_->y[0]`, which C refuses: the header did not compile. Found reviewing tensors compiled, by random models. The window now moves as an array's and the input is written to its one cell, in 2 lines; `check_speck_report` holds it, `test/compile/tensor.ink`'s `speck`. |
| C125 `[fixed]` | **A clause for one cell that names a slice a value lacks, or none it has, compiled.** Beside a value written whole, `y_0 = [1 2;; 3 4]` with `y_0[1,2] = 5`, or `A = [1 2; 3 4]` with `A[1,1,2] = 9`, the compiler took the missing slice as the first, or dropped the one named, and wrote the cell, where the interpreter refuses the value: "a clause for one cell of y_0, a 2x1x2 tensor, names its slice, row and column". Found reviewing tensors compiled, which let through what "a tensor" had refused. The compiler now asks the interpreter's own test, `Reference::Named`, made public, in 7 lines; `compile_c124` in `test/cli.cmake` holds it. |
| C126 `[fixed]` | **A cell of every term met a base term's own cell at its row and column alone.** Beside `y_0 = [1 2;; 3 4]` and `y_0[1,1,1] = 7`, `y_n[2,1,1] = n` was taken as given by y_0's own clause and compiled, where the interpreter asks which of y_0 and `y_n[2,1,1]` gives slice 2, row 1, column 1 of y_0; with no clause of y_0's own the refusal named `y_n[1,1]`. Found reviewing tensors compiled. The slice is now compared and named too, in 2 lines; `compile_c125` in `test/cli.cmake` holds it. |
| C127 `[fixed]` | **A clause for one cell outside the size compiled where every cell was given before it.** A term's clauses for one cell were held to its size cell by cell, each until one gave the cell, so beside `y_0 = [1 2]`, `y_0[1,1] = 3` and `y_0[1,2] = 4`, `y_0[1,3] = 5` was never held and compiled, where the interpreter refuses y_0, "row 1, column 3 is outside a 1x2 matrix". Tensors compiled made that test build a value of the term's size for each cell and each clause, so a 150x150 matrix with two such clauses took 2.4 s to compile instead of 0.09. Found reviewing tensors compiled. Each clause is now held once, before the cells, in 4 lines; `compile_c126` in `test/cli.cmake` holds it. |
| C128 `[fixed]` | **`--check` judged a term's cells by its first that no double holds.** A term with one such cell was "no finite double" whole, so every cell of it parted, finite ones included: `p_n = p_(n-1)/2 + 7/16*p_(n-1)*p_(n-1)` from `[5/4 0; 0 2]` reported `p[1,1]`, 1.2e+19 in both, at 12, where `p[2,2]` passed every double. Found reviewing `--check`. Each cell is now judged by itself, and says why it is none, in as many lines as before; `check_sever_report`, `test/compile/history.ink`'s `sever`, holds it. |
| C129 `[fixed]` | **A header named its fixed parameters wrongly.** Its first comment said "Compiled in, as a size, a bound or a lag cannot change", but a parameter is compiled in wherever the step needs a constant, and a matrix power's exponent and a cell's place are none of the three: `pw(k = 2, i = 1, x_n)` reading `([1 1; 0 1]^k*[x_n; 1])[i]` listed i and k so. Found reviewing tensors compiled. It now says "Compiled in as constants, these cannot change", as does `README.md`; `test/compile/expected/heat.h`'s comment moves so, and `compile_fixed_power` in `test/cli.cmake` holds it. |
| C130 `[fixed]` | **`--check` gave no size to state for a tensor input.** An instance feeding a model's unsized input a matrix is refused with the size that would hold it, "write 'x_n[j<=2]'" (C83), but one feeding it a tensor was refused with no hint, where the compiler, for a cell read past the same input, says "write 'X_n[b<=1, j<=2, k<=1]'". Found reviewing tensors compiled. It now gives the slices too, in 2 lines; `check_tensor_input` in `test/cli.cmake` holds it. |
| C132 `[fixed]` | **A compiled gradient with respect to a matrix was a single 0 where nothing in its body moves.** With no part in any cell of the body, the gradient was a zero of the body's shape where the interpreter's is a zero of the point's: `grad_(v = [x_n; 1]) [1 1]*f(v)`, f a step by its cells, `f(z)[i] \| z[i] > 0 = 1` and `f(z)[i] = 0`, was `u[1]` in the header where the interpreter answers `[0; 0]`, and `--check` held the first cell alone, past which it wrote its estimates. Reached before by a guarded function of constants, `grad_(v = [x_n; 1]) q(v[1])`; found reviewing guards on cells at run time, whose masks reach it. It is the point's shape now, in the same line; `compile_c132` in `test/cli.cmake` holds it. |
| C133 `[fixed]` | **A compiled gradient was NaN where a comparison read as a value in a guard met its threshold.** Its sides moving, such a comparison was tested for a jump, as one outside a guard is, where the interpreter's guard asks only for its value and refuses an equality alone: beside `h(z) = 0`, `h(z) \| (z > 2)*z > -1 = z` gave `grad_(t = x_n) h(t)` NaN at 2, where the interpreter answers 1. Found reviewing guards on cells at run time, by fuzzing functions by cells, which reach it per cell. In a guard only an equality is tested now, and in a function it calls anything, as the interpreter's call leaves the guard, in 7 lines; `compile_c133` and `compile_c133s` in `test/cli.cmake` hold it. |
| C134 `[fixed]` | **The interpreter refused under grad where a clause read only for its shape jumped.** A definition by cells measures its size by its clauses' reads, and under grad the walk measured them with their derivatives, so a jump in a read whose shape alone was asked refused the whole: beside `jg(z)[i] = (z[i] > 0)*z[i]`, `jf(z)[i] | z[i] > 100 = jg(z)[i]` and `jf(z)[i] = 2*z[i]`, `grad_(t = 0) [1 1]*jf([t; 1])` was "a comparison jumps at t = 0" where the compiled step gives 2. Found comparing the compiled step with the interpreter. A read is measured by its value now, in 2 lines; gradcells.ink holds it, and no other output moved. |
| C135 `[fixed]` | **A constant was refused where what it was folded from read a name that moves.** An operator of two constants folded by evaluating its expression again, which reads what its operands read: beside `h(z) = 3`, `b_n = h(x_n) + 1` was refused, "x is not defined", and so were `grad_(t = x_n) (h(t) + 1)*t` and `f([x_n; 1])` with `f(M) = h(M) + 1`, for t and M, where the interpreter answers 4. Found comparing the compiled step with the interpreter. Operators, literals, cells and sums are folded from their operands' constants now, in 27 lines; `compile_c135` in `test/cli.cmake` holds it. A product whose left is a power, as `A^-1*b` solves, and a limit are still folded again, and refuse so. |
| C136 `[fixed]` | **A compiled step answered where the matrix written whole beside cells of their own was refused.** The interpreter evaluates a definition by cells written whole before any cell's own clause, as its size, so it refuses where that does, but the step read only the cells it used, and a cell with a clause of its own dropped the whole's NaN: beside `k1(z) | z[1,1] > 0 = z`, `k2(z) = k1(z)` and `k2(z)[1,1] = 2` gave `k2([x_n])` 2 at x = 0 where the interpreter says "no clause of k1 applies", and the same under grad, a guard of the whole's whose power has an infinite derivative, and for a term, `y_n = k1([x_n])` with `y_n[1,1] = 2`. Found comparing the compiled step with the interpreter. Where a cell of the whole writes NaN every cell does now, in 13 lines; `compile_c136`, `compile_c136_term` and `compile_c136g` in `test/cli.cmake` hold it. The interpreter evaluating the whole only where a cell needs it was the other way, but it would answer where it refuses, a decision for the language, and needs the whole's size anyway. |
| C137 `[fixed]` | **`--check` read and wrote past a term whose shape was not the step's.** It took the compiled shape for the interpreter's, so a term of another read cells it lacked and wrote its estimates past its own, as with C132; beside `h(z) = 3` and `h(z) \| z > 2 = [1; 2]`, the instance of a model with `y_n = h(x_n)` crashed it, the step's y being 2x1 and the interpreter's 1x1 at 0. Found hardening `--check` after C132. It says so now, "m.y_(0) is a 1x1 matrix, where the compiled step's is a 2x1 matrix", in 6 lines; `check_c137` in `test/cli.cmake` held it until C138, after which no model known reaches the guard, and no test does. |
| C138 `[fixed]` | **A function whose guarded clauses give different shapes compiled to the largest.** The interpreter answers each clause's own shape, but the step stretched every clause to the largest of the guarded ones: beside `h(z) = 3` and `h(z) \| z > 2 = [1; 2]`, `y_n = h(x_n)` was a 2x1 term, `[3; 3]` at 0 where the interpreter's is 3, and `h(x_n)[2]` 3 where the interpreter says there is no row 2. A sequence whose clauses differ so is refused, "its clauses have different shapes". Found by C137. A function is refused so too now, "the clauses of h have different shapes", in 4 lines; `compile_c138` in `test/cli.cmake` holds it. It refuses also one read only where it would be stretched anyway, `h(x_n) + [0; 0]`, which the step got right: such a function can be written with clauses of one shape. |
| C139 `[fixed]` | **`grad` walked `ilogb` wherever it walked `log`.** Under grad a walked `log` walked `ilogb`'s thirteen `ilogbs` steps with the jet, some 4,000 instructions of walk for each guard, and C101's guard and C100's clause added one to each: `bench/grad.ink` went from 14,870 to 16,469 million instructions at 62dbd86. Since the prelude's part functions, `log` walks only for an exact part, a grad inside a grad, a 0 or NaN part, and in `--check`, and those walks still walked `ilogb`. Found measuring that merge. Under grad `ilogb` of a moving double the header takes is now the header's value with no part, which is what its walk answers, at a power of two too, since none of its guards is an equality; never in `--check`. A call is one step and one reference deep, where walked it was 14 and 2, so a walked `log` is 6 steps where it was 19. Under callgrind a grad of `log(t/3)` summed over 20,000 points went from 20,913 million instructions to 13,623 under GCC 13 and from 13,241 to 7,005 under Clang 18, and a second derivative of `log(t*u)` over 5,000 from 8,382 to 6,313 and from 4,872 to 3,095; `bench/grad.ink`, which walks no `ilogb`, from 1,352 to 1,356, GCC inlining the header's `log` part otherwise, and from 976 to 977. Eight sweeps of earlier work and six of `ilogb` and `log` at, beside and between powers of two, 1,563,273 lines with 243,874 calls answered so, 48,917 at a power of two, print byte for byte as walked under both. 4 lines; `fastgrad.ink` holds it, a call at depth 254 now answered. |
| C140 `[fixed]` | **Calls nested in an argument made the header exponential.** A call compiles where it is made, its parameters written as the text of its arguments, so a parameter read three times wrote its argument three times, and a call given to it tripled again: beside `f(p) = p/2 + p/3 + p/5`, `x_n` of ten nested calls of f was 1.2 MB of C, and with `g` eight of them, `x_n = g(g(x_(n-1)))` exhausted memory where the interpreter answers at once. Found compiling a cart-pole's Riccati recurrence. An argument in which a call was compiled is a temporary of the step now, and its part under grad too, in 11 lines: the same operations in the same order, and no expected header moved; `compile_c140` in `test/cli.cmake` and `nests` in `test/compile/drift.ink` hold it. A limit's function has no temporaries, so its terms still write such calls out, as they do a product (`lim P` of that recurrence still exhausts memory). |
| C141 `[fixed]` | **`lim` took no qualified name.** A term is read through an instance, a model applied or a file, `d.s_3`, but its limit was "unexpected '.'": `lim d.s`, `lim m().s` and `lim filters.P`. The names before the last are read as a term's are, in 16 lines, and `tex` sets the limit before them. models.ink and latex.ink hold it. |
| C142 `[fixed]` | **`use my_data` said "unexpected '_'".** A file's names are reached qualified, `data.X`, and a name is letters then digits, so `my_data.ink`, which `--compile` takes (C104), cannot be used; the error named a character instead. It says so now, "use reads a file named as a name is, letters then digits, and my_data is not one", in 7 lines; models.ink holds it. Using such a file through its listed names alone, `use my_data (X)`, would be a decision for the language. |
| C143 `[fixed]` | **`--check` said "within 0" of a sequence it held no value of.** Where the interpreter refuses a term the step is to be NaN, and that is all a sequence refused at every step is held to, but its line read as for values compared: a limit that never converges, `y_n = lim s(x_n)`, was "never.y: within 0", exit 0. It says "no value compared, the step NaN where the interpreter gives none, as at 0:" and the interpreter's reason now, in 11 lines; the reports of trim, held, smooth, edge's j, o and v, and blast in float moved with it, each refused at every step. It still passes: those reports hold the step to the interpreter's refusals on purpose, and failing an instance that compares no value, as `vacuous.ink` and blast do, is a decision for `--check`. |
| C144 `[fixed]` | **An input whose model states no size, read past its cell through another sequence, was refused as out of range.** C83 says how to state the size where the input itself is read past its one cell, but in `mon(x_n)` with `e_n = 2*x_n` and `k_n = e_n[2]`, k was "row 2 is outside a 1x1 matrix". A single value read past its cell names the input it is computed from now, "e reads x, a single value, as mon states no size for it: write 'x_n[j<=2]'", in 12 lines; `compile_unstated_through` in `test/cli.cmake` holds it. The bounds are those of the read, which are the input's where the sequence keeps its shape. |
| C145 `[fixed]` | **grad took a used file for an instance.** A name read after a point was refused under grad whenever it read the variable, so `grad_(t = 1) sq.f(t)`, f defined in `sq.ink`, was "grad cannot differentiate through an instance", where the compiled step differentiates it. A file holds definitions as the session does, and a call into one is differentiated as the session's are now, reading the file's names, in 15 lines; models.ink holds it, with `power` added to filters.ink. An instance's terms are still refused. |
| C146 `[fixed]` | **A guard written as a sequence of its own was refused where the same guard inline compiled.** Beside `w_0 = 0`, `m_n = w_(n-1) <= 0`, `w_n \| m_n = w_(n-1) + 1` and `w_n = w_(n-1)`, m was "m_0 reads w_-1, below w's base clauses, where only its guards could give a term", where `w_n \| w_(n-1) <= 0 = ...` compiles, and the interpreter answers both. Below its base clauses w's first guard asks m there, which asks w lower still, so no guard gives a term there and the interpreter refuses m_0 as the step leaves it. A first guard that always reads the sequence back, itself or through sequences of one clause, no longer refuses, in 21 lines; `latch` in `test/compile/drift.ink` holds it. A hold below such a sequence's base, at another rate, is still refused. |
| C147 `[fixed]` | **The prelude's functions refused a complex number in their walk's words.** `exp(i*pi)` was "a comparison needs real numbers, not i*~3.14159265", the first guard of the walk speaking, where `sin([1 2])` names sin. exp, log, tanh, sin, cos and ilogb now say "exp needs real numbers, not i*~3.14159265" before they walk, in 6 lines, under grad too; fastprelude.ink, trig.ink and fastgrad.ink moved with it, `exp(~1 + i)`, `cos(i)`, `sin(~1 + i)` and `grad_(x = ~1 + i) exp(x)`, and fastprelude.ink adds `exp(i*pi)`, `log(i)` and `tanh(i)`. `abs`, `max` and `min` refuse as README's `abs` does. |
| C148 `[fixed]` | **`--check` called a term the interpreter ran out of steps for a disagreement.** A term past the million steps is no answer, but the program held the step to it as to a refusal, "toil.y: 25.40430835089893 at 2, where the interpreter gives none: evaluation gave up after 1000000 steps", and failed. Such a term is not compared now, and the sequence's line says so, "toil.y: within 0; the interpreter ran out of steps at 2, not compared", in 20 lines; `toil` in `test/compile/drift.ink` holds it. The budget is still a line's, so whether a term runs out depends on what the session asked before it: a decision for the language, not taken here, but in C149. |
| C149 `[fixed]` | **Whether a term answered depended on what the session had asked before it.** The million steps were a line's, and a memoised term cost its caller one step when remembered and its whole body when not, so a line refused cold answered once the terms below it were remembered: beside `w_n = f(w_(n-1))/2`, f summing ten thousand calls, `w_30` gave up cold and answered after `w_20`, and so did a logistic regression's `w_100` after `w_50` and a network's `E_30` after `E_20`. Phase 9 calls steps an accident of how a term is evaluated; this one was the memo's. Approved as a change of behaviour: each memoised term now has a million steps of its own, from none, not counting the terms it computes, which have theirs, and its caller takes one step for it, remembered or not, so a term's steps no longer depend on the memo. Work that is not memoised -- a sum, a plain definition, a local's call -- counts against the term or line it is in, as before; a grad's memo lasts one grad, so its walked calls count against what asked it, and fastgrad.ink's softplus still runs out walked. Depth is unchanged, and still depends on the memo, which a fill (C69) makes up for. A fill's terms each start from none, the term asked included. What bounds time: a line computes at most ten million terms, `max_filled`, as a fill goes ten million terms from its base, and a filled term counts afresh as a line of its own does; so a line's time is bounded as a fill's was, and a tree of 2^40 distinct calls gives up after ten million terms in 13 s, where it gave up after a million steps in 0.6 s. That count depends on the memo as time does, but a line that answered had a million steps and took about one for each term it computed, so none is refused by it. `--check` holds a term refused by either as out of steps (C148). Only refused lines moved: `w_25` and `lim p` answer in recursion.ink, each over a million steps that gave up cold, and drift.ink's `toil`, whose limit of thousand-term sums now answers, reads such a sum at each of a thousand terms to stay out of steps. 28 lines. |
| C150 `[fixed]` | **A parameter that is not a name was dropped in silence.** A function's parameters are parsed as an expression and its names alone kept, so `h(x, 2) = x` took one argument and answered `h(1)`, `h(2) = 3` took none, and `tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]` none, "tr takes no arguments", its body reading M and n as the session's. Found specifying a size bound by a signature, which closes it: a parameter is a name, `x` or `x = 1`, or a name and its size, `v[j<=n]`, and anything else is refused where written, "a parameter is a name, as 'x' or 'x = 1', or a name and its size, as 'v[j<=n]'"; `signatures.ink` holds it. |
| C151 `[fixed]` | **`--check` held a sequence from step 0 when its lowest base clause was above it.** Beside `v_0 = 0`, a sequence `x_1 = 1`, `x_n = x_(n-1)/2` starts at 1 in the step, whose header guards it so, but the program held it from 0, "x: 0 at 0, where the interpreter gives none: x has no clause for index 0", and failed: a sequence with base clauses had its start set and not its first term, which the program reads. Both are set now, in 1 line; `late` in `test/compile/drift.ink` holds it. Found transcribing Reddi, Kale and Kumar (ICLR 2018), whose iterates start at 1. |
| C152 `[fixed]` | **`tex` set a fraction raised to a power without its parentheses.** `(a/b)^2` was `\frac{a}{b}^2`. A fraction binds as a power now, so a power, a factorial, a transpose or an index parenthesises it, `(\frac{a}{b})^2`, as `(x + 1)^2` is, in 1 line; latex.ink holds it. Found transcribing Reddi, Kale and Kumar (ICLR 2018). |
| C153 `[fixed]` | **A power of 1/2 was not a correctly rounded square root.** `x^(1/2)` was `pow(x, 0.5)` in the interpreter and the header, and glibc's pow is not correctly rounded: `(1 - 1/2^53)^(1/2)` answered 1, where the root rounds to 1 - 1/2^53, so Kahan's 128 roots then 128 squares (2006, §10) gave 1 for every x > 0, and `--check`, both sides sharing the error, could not see it. Of a positive real number it is `sqrt` now, in both, and `sqrtf` in a float header, in 6 lines; exact.ink and `compile_c153` in `test/cli.cmake` hold it. No golden or expected header moved; reddi's reports, left loose for a libm's pow, are exact now. A negative or complex base keeps its branch (C33), and `(-0)^(1/2)` was -0 in a header, as C's sqrt has it, until C163. Found transcribing Kahan (2006). |
| C154 `[fixed]` | **A clause's default was never read.** C51 compared the names of a clause's parameters with the definition's and not their defaults, so beside `g(x, k = 2) \| x > 0 = k*x`, `g(x, k = 3) \| x < 0 = k` was accepted, listed by `?g`, and `g(-1)` answered 2: one call binds the parameters once, with the first clause's defaults. The same for a default the first clause lacks, which C51 named and did not refuse. Fixed by *Cases in a paper's order, and `clear`* (next in line), which compares the tokens between a clause's parentheses, defaults and all, and refuses a clause whose defaults differ as one whose names do; `clauses.ink` holds it. |
| C155 `[fixed]` | **A call of the prelude's given to a function was written at each reading of the parameter.** C140 made an argument a temporary where a call of the source's was compiled in it, and a call of the prelude's, a C function of the header's, did not count as one: beside `h(t)`, which reads t four times in its guards and body, `f(z) = h(exp(z))` and `x_n = f(f(x_(n-1)))` wrote the outer `exp(t1_)` four times where once does, and Kahan's Gk (2006, §6) likewise, a header growing with each nesting. It counts now, in 1 line; `compile_c155` in `test/cli.cmake` holds it, and no expected header moved. Found transcribing Kahan (2006). |
| C156 `[fixed]` | **log, ilogb, sin and cos refused out of their domain with a division nobody wrote.** `log(0)` was "division by zero", the prelude's `1/0` speaking, so Kahan's Spike(4/3) (2006, §7) reported a division the user never wrote; so did `ilogb` of 0 and below, and `sin` and `cos` past 2^20. They say "log needs a number above 0, not 0" and "sin needs a number between -2^20 and 2^20" now, before they walk, as C147 has them refuse a complex number, under grad too, in 4 lines more. elementary.ink, fastprelude.ink, prelude.ink and trig.ink move with it, 14 entries; the header, whose C functions are NaN there, does not. Found transcribing Kahan (2006). |
| C157 `[fixed]` | **`log` of an exact number from 5*10^999 was the number itself.** C101's clause `log(x) \| 2*x == x = x` took inf as the one positive number its own double, and an exact number from 5*10^999 to 10^1000 is one too, its double past a thousand digits and approximated to inf: `log(5*10^999)` and `log(2^3321)` answered the thousand-digit number, exact and unmarked. The clause is `log(x) \| 1/x == 0 = x` now, inf being the one positive number whose reciprocal is 0, exact or not; `log(~1/0)` is still inf, and the header's log, `_jx` and `_dx`, rewritten in `inkamath_prelude.h`, give every double the bits they gave, ±0, subnormals, infinities and NaN among them, over twenty million doubles. elementary.ink holds it, its `?log` and `tex ?log` moving with the clause. Found reviewing C156. |
| C158 `[fixed]` | **A refusal misquoted a clause's parameters.** *Cases in a paper's order* showed them as the tokens it compares, spaced one by one, so beside `gm(x, k = [1 -2]) \| x > 0 = k`, `gm(x) = x` was refused with "gm takes (x, k = [1 - 2])", one element where two were written, and `k=f(1)` read `k = f (1)`. A refusal quotes them as written now, as `?gm` shows the clause, in 14 lines; the tokens stand in only for a line's local, which keeps no text. `clauses.ink` holds it. Found reviewing *Cases in a paper's order*. |
| C159 `[fixed]` | **Two left sides that differed by a space that parts two elements were one.** A clause is named by the tokens of its left side (C46, C55), and C154 compares the defaults by them, but the tokens keep no space, and inside a matrix or an argument list a space before a sign, or before a bracket, begins the next element: `[1 -2]` is two elements and `[1 - 2]` one, `[pi [1]]` two and `[pi[1]]` one. So `sn(x) \| x == [1 - 2][1,1] = 20` replaced `sn(x) \| x == [1 -2][1,1] = 10`, and beside `gm(x, k = [1 -2]) \| x > 0 = k` a clause with the default `[1 - 2]` joined, and `gm(-1)` answered `[1, -2]`. The parser marks the space that parts two elements now, and the left side keeps it, in 9 lines; a space anywhere else still names nothing, so `x > 0 -1` and `x > 0 - 1` are one guard. conditional.ink and clauses.ink hold it. Found reviewing *Cases in a paper's order*. |
| C160 `[fixed]` | **`use` refused to bring in a name of the file's own.** *Cases in a paper's order* binds a name brought in only over nothing or itself, and `use eig (cp, eig)` bound the file `eig`, then refused the function `eig` over it, "eig is already defined; write 'clear eig' first", which `clear eig` could not help, and left the file bound, where a refused `use` binds nothing. Twenty-eight explorations of eigenvalues and H-infinity norms stopped at their first line. The name takes the file's place now, as it did, in 1 line; `cases.ink` defines `cases`, and `clauses.ink` brings it in, twice. Found reviewing *Cases in a paper's order*. |
| C161 `[fixed]` | **A float header's matrix inverse did not build under MSVC's warnings.** The inverse starts from the identity, written `r[i][j] = i == j`, an int converted to the cell's type: nothing to say of a double, but MSVC's /W4 warns of one converted to a float, so a float header inverting a matrix stopped a build with /WX. Written `i == j ? 1.0 : 0.0` now, `1.0f : 0.0f` in a float's, in 1 line; `compile_float_c161` in `test/cli.cmake` holds it, and the expected heat.h and kalman2.h move with it, their inverse's identity alone. Found by CI, checking Kahan's ProSolveur (2006, §9) in float. |
| C162 `[fixed]` | **`?f` listed the fallback where it was written, not where it is tried.** An unguarded clause for every call or every cell is tried after the guards wherever it stands (*Cases in a paper's order*), and `tex ?f` sets it last, "otherwise"; `?f` listed the clauses as written, so after `use cases (clamp)` and `clamp(x) \| x < 0 = 0` it showed `x > 1`, then `= x`, then `x < 0`, the fallback in the middle. It is listed last now, in 6 lines; clauses.ink holds it, and `?max` there, `?exp`, `?log` and `?tanh` in elementary.ink, `?u` in queries.ink and `?w` in terms.ink move with it, their fallback or general clause last. |
| C163 `[fixed]` | **`(-0)^(1/2)` was -0 in a header and +0 in the interpreter.** C153 made a power of 1/2 C's sqrt, which keeps the sign of zero, where the interpreter's power gives +0; `--check` holds -0 equal to 0 and could not see it, though `1/x^(1/2)` is -inf in the step and inf in the interpreter. The header writes `sqrt(0.0 + x)` now, -0 plus +0 being +0 and every other x itself, `sqrtf(0.0f + x)` in a float's, in 1 line; `radix` in `test/compile/drift.ink` holds it. No expected header moved, none having a power of 1/2; `compile_c153`'s lines in `test/cli.cmake` move with it. |
| C164 `[fixed]` | **A float's check program with an infinite value did not build under Clang's warnings.** `--check` writes the interpreter's terms, their estimates and each guard's distance from its threshold into arrays of doubles, an infinity as `INFINITY` and a NaN as `NAN`, both floats in C; a float's program is built with `-Wdouble-promotion`, which Clang reads into the promotion of either, so checking in float a guard infinitely far from its threshold, `1/x_n^(1/2) > 0` at -0, stopped with -Werror. GCC does not warn of a macro's constant. They are written `(double)INFINITY` and `(double)NAN` now, in 2 lines; `radix` in `test/compile/drift.ink`, checked in float, holds it. Found checking C163 in float. |
| C165 `[fixed]` | **A part a clause lacked, chosen by an approximated guard, marked nothing it reached.** Under `grad` such a part stays lacking, so that nothing it leaves constant moves, and grad marked its answer only where the body had no derivative at all: beside `gc(x) = x` and `gc(x) \| x < c = 3`, `grad_(x = rt_20) (gc(x) + x)` answered 1 unmarked, where 2 is right, and `[gc(x) x]` and a matrix by its cells likewise, against the specification's "a part the clause chosen lacks is 0 approximated". A grad that differentiated a clause such a guard chose marks its whole answer now, in 2 lines; approximated.ink holds it. Found reviewing *A truth read from an approximated number*. |
| C166 `[fixed]` | **A clause written again kept the sizes of the one it replaced.** The sizes are the definition's, stated in one clause for all, and were held as every clause written had stated them: after `f(x[j<=n])_m = n`, `f(x)_m = 1` replaced the only clause stating a size and `?f` showed it alone, yet `f([1 2])_0` was "f takes x[j<=n], not a 1x2 matrix". A clause is still checked against those before it, as the names are (C51), but the sizes held are those the remaining clauses state, in 3 lines; signatures.ink holds it. Found reviewing *A size bound by a signature*. |
| C167 `[fixed]` | **A jump ruled out by an approximated number marked nothing beside it.** Under `grad`, floor and a comparison are flat where no jump is, and whether one is reads the number: `grad_(x = rt_20) (floor(x) + x)` and `((x > c) + x)` answered 1 unmarked, though C165 marks what a guard so decided reaches, and only a body with no derivative at all, `floor(x)` alone, was marked. Such a test marks grad's whole answer now, as C165's guard does, in 2 lines; approximated.ink holds it. Found reviewing *A truth read from an approximated number*. |
| C168 `[fixed]` | **A limit's refusal quoted an approximated term without its mark.** Beside `W_n = W_(n-1) + rt_20`, `lim W` was "W did not converge within 100 terms (last term ~141.421356)", the value an answer would mark, and so were a series' last partial sum and a limit's under `grad`. The three quote it as `~141.421356, approximated past a thousand digits` now, in 7 lines; approximated.ink holds it. Found reviewing *A truth read from an approximated number*. |
| C169 `[fixed]` | **The compiler read a function with every default, called with no arguments, as a plain value.** `f()` parses as `f`, and the interpreter calls f with its defaults; the compiler emitted f's body with no parameter bound, so beside `pd2(v = [1; 2]) = v[2]`, `t_n = pd2()` was "cannot compile t: row 2 is outside a 1x1 matrix", and in a model, where v named another of its sequences, "v is a sequence; index it". Such a name is compiled as a call with no arguments now, refused in the interpreter's words where a parameter has no default, in 6 lines; compile/signatures.ink's d holds it. Older than *A size bound by a signature*; found reviewing it. |
| C170 `[fixed]` | **A guarded clause written again without its sizes was added, never to apply.** A guarded clause is named by its left side, and the sizes, the definition's, were part of the name: after `k(x[j<=n]) \| n > 1 = n`, `k(x) \| n > 1 = 1` was appended behind the clause it meant to replace, which answered first, where an unguarded clause written again replaces its own whatever sizes it states (C166). The left sides are compared with their sizes apart now, as the parameters are (C154), in 3 lines; signatures.ink holds it. Found reviewing *A size bound by a signature*. |
| C171 `[fixed]` | **`tex` set one left side for several clauses with the first one's sizes.** Cases, and a definition's cells, are one left side for their clauses, and it was the first clause's: beside `gs(x) \| n > 1 = 1`, `gs(x[j<=n]) = n` was set `\operatorname{gs}(x) = \begin{cases} 1 & \text{if } n > 1 ...`, its n bound nowhere. Such a left side has the definition's sizes now, and a clause on a line of its own still those it states, as `tex ?fm` has them, in 9 lines more; signatures.ink holds it. Found reviewing *A size bound by a signature*. |
| C172 `[fixed]` | **A size past an int was refused in an index's words.** `bad(x[j<=4000000000]) = x` was "an index must be between -2147483648 and 2147483647, not 4000000000", a range no size has; a cell's bound and a model's input's alike. It is "a size must be between 1 and 2147483647, not 4000000000" now, in 3 lines, a bound that is not whole still refused as an index is; signatures.ink holds it. Found reviewing *A size bound by a signature*. |
| C173 `[fixed]` | **A clause refused for its names or defaults was quoted with its sizes.** The clauses agree on their names and defaults with the sizes apart (C154, C166), yet the refusal quoted the sizes as written: beside `hf(v[j<=n], s = n) \| s > 1 = s`, `hf(v) = 1` was "hf takes (v[j<=n], s = n), so a clause cannot take (v)", pointing at a size where the default differs. Both sides are quoted without their sizes now, "hf takes (v, s = n)", in 17 lines more; signatures.ink holds it. Found reviewing *A size bound by a signature*. |
| C174 `[fixed]` | **The principal square root of a negative number was not exact.** A power of 1/2 of a negative or complex number went through `exp(log(x)/2)`, which leaves cos(π/2) in the real part: `(-1)^(1/2)` was `~6.123234e-17+i` and `(-1)^(1/2) == i` 0, where C's csqrt gives i exactly (Goldberg 1991, *Signed Zero*). It is the complex `sqrt` now, in 1 line, the +0 imaginary part of C33 keeping its branch; goldberg.ink holds it. basics.ink's `(-4)^0.5` and `(0-4)^0.5` lose their real part of 1e-16, and approximated.ink's root of a negative approximated number its 9e-25. Found transcribing Goldberg (1991). |
| C175 `[fixed]` | **A fractional power of an exact number past a double's range overflowed or underflowed.** The base was made a double first: `((3*10^200)^2 + (4*10^200)^2)^(1/2)` was inf where the root, 5e200, is a double (Goldberg 1991, *Infinity*), `(10^-400)^(1/2)` 0, and `(2^1100)^(1/4)` inf. An exact number no normal double holds has a power of 2 taken out first now, x^y = 2^(e*y) (x/2^e)^y with e even, so a root of 1/2 is still C153's correctly rounded sqrt, in 14 lines; goldberg.ink holds it. No golden or expected header moved. A constant folded by `--compile` gets the same root, and `--check` of a parameter past a double's range, inf in the step, now reports the interpreter's finite term where both were inf. A complex exponent is as it was. Found transcribing Goldberg (1991). |
| C180 `[fixed]` | **Compiling took a time that grew with the square of a step's temporaries.** `Declare` looked a value up in its sequence's list one temporary after another, and `Temporaries` searched the text kept for each name, a text growing with each line kept: backpropagation through time by 32 nested calls of `R(V, h, x) = V*h + x*h0`, hidden 8, and `grad_(V = W)` of the sum of its last state, is 10,872 temporaries and 3.2 MB of C, and took 15.1 s to compile, 16 calls 3.4 s. Found reviewing *A sequence with parameters read at a constant index, compiled*, whose terms written out reach such counts. A value's place is kept by its list, and the names read are a set, in 16 lines: 1.7 s and 0.7 s, the same headers, and no expected header moved. No test holds it, a bound on time being one the Debug and sanitized builds would cross for reasons of their own; this measure is its note. |
| C185 `[fixed]` | **A sequence with parameters by cells was compiled as a matrix by cells.** `Chained` handed a definition with a clause by cells to `Cells`, which knows no index: with `y(x)_0 = [x; 1]` and `y(x)_k[j<=2] = x*j`, `y(x_n)_0` compiled to `[x; 2x]` where the interpreter answers the base clause, `[x; 1]`, and a clause reading its index said "k is not defined". Found reviewing *A sequence with parameters read at a constant index, compiled*. Refused by name now, "a sequence with parameters by cells, for now", in 2 lines; `compile_iterates_refused` in `test/cli.cmake` holds it. |
| C186 `[fixed]` | **A term of a sequence with parameters saw the index another clause had bound.** `Chained` bound each general clause's index where the function's parameters are, and left it there for the clauses after it and the terms after it: with `c = 10` a parameter of the model, `f(x)_c | c < 3 = c` and `f(x)_j | j > 0 = f(x)_(j-1) + c` read c as the last index tried, so `f(x)_4` compiled to 9 where the interpreter answers 22, and a base clause `q(x)_3 = c` after `q(x)_c` to 2, not 10. Found reviewing *A sequence with parameters read at a constant index, compiled*. Each clause sees its own index alone now, as `Reference::Selects` binds it, in 4 lines; `memo` in `test/compile/iterates.ink` holds it. |
| C187 `[fixed]` | **Terms of a sequence with parameters were shared between arguments written alike.** A term was kept by the text of its arguments' cells, which two exact constants of one double, or a column and a row of the same cells, share: `g(1/2)_20` and `g(1/2 + 2^-60)_20`, with `g(x)_k = g(x)_(k-1)*4 - 3/2`, both compiled to 1/2 where the second is 1/2 + 2^-20, and `h([x; 1])_1` and `h([x, 1])_1` to one column, so a function weighing each cell by its column answered 4 where the interpreter answers 6. Found reviewing *A sequence with parameters read at a constant index, compiled*. A term is kept by its arguments' shape and exact value too now, in 3 lines; `memo` in `test/compile/iterates.ink` holds it. |
| C188 `[fixed]` | **A term read on the left of an `and` was not folded where the step starts.** Whether the left decides before the stream is asked by compiling it again there, where no list of temporaries is, so a sequence with parameters read in it was refused and the right taken as read: with `t(y)_k = t(y)_(k-1)*2 - 1`, `l_n | t(n)_3 > 5 and x_(n-1) > 2 = 1` was "l_0 reads x_-1, before the stream", where the interpreter's left is -7 > 5 and answers l_0, as the compiler does for a call. Found reviewing *A sequence with parameters read at a constant index, compiled*. The left is compiled with a list of its own there, kept nowhere, in 4 lines; `memo` in `test/compile/iterates.ink` holds it. |
| C189 `[fixed]` | **A compiled call answered where the interpreter refuses an argument the function never reads.** The interpreter computes every argument, so with `F(M) = 1` and `r(v) \| v > 0 = v`, `F(r(x_n))` refuses where no clause of r applies, but the step wrote 1, the argument nowhere; so did `grad_(t = r(x_n)) F(t)`, whose body ignores its variable, and `grad_(t = x_n) F(r(t))`, 0. In a header that writes NaN, a call is NaN where an argument that may be NaN and that its value does not read is, in 18 lines, an argument passed on through another call counted as read only where that call reads it; `idle` in `test/compile/nan.ink` holds it. A division by 0 is inf in the step, not NaN, and is as it was (`pole`). |
| C190 `[fixed]` | **`rho` and `abscissa` of an inexact matrix whose cells sum to 2^1023 or more refused with a NaN.** B, the power of two above the sum, was 2^1024, made inexact with the matrix, so inf: `rho(~1e308)` was "a comparison needs a number, not -nan", as were `abscissa(~-1e308)` and `rho([~1 2; 3 4]*10^307)`. The bracket bisected is A/B's now, `rhou(A)_m` and `abscissau(A)_m`, from [0; 1] and [-1; 1], and `rhod` scales by B's power in two halves, each a double, A to A/B and the bracket back: `rhob` and `abscissab` are still A's. 3 lines more and one of comment; charpoly.ink holds it, and no other answer moved. A matrix whose sum is itself past a double, `[~1 2; 3 4]*2*10^307`, still refuses. Found reviewing *The characteristic polynomial and stability*. |
| C191 `[fixed]` | **`rho` of an exact matrix whose answer is past a double's range refused with a NaN.** `rho([1 2; 3 4]*10^400)` and `/10^400`: A/B's tests are past a thousand digits, so approximated, and the bracket of them was multiplied by an exact B no double holds. Fixed by C190's change: they are inf and 0, the doubles `~` makes of 5.37e400 and 5.37e-400, marked. Found reviewing *The characteristic polynomial and stability*. |
| C192 `[fixed]` | **An answer the 256-halving cap ended was printed as certified.** The cap stops a bisection the relative stop has not, and the bracket it leaves is no answer below 2^-203 of B: `abscissa([-10^-100 0; 0 -1])` was ~-3.45e-77, -2^-254, `abscissa([10^-80 0; 0 -1])` 0 for an unstable system, and `rho([0 1; 10^-200 0])` 0, all unmarked. An answer whose bracket at the cap is wider than 2^-53 of its end nearer 0 is marked now, `+ 0*10^-1000` adding a 0 approximated past a thousand digits, so a comparison of it is marked too. A 0 is never within 2^-53 of itself, so a nilpotent matrix's rho and a marginal system's abscissa, right, are marked as well: the tests cannot tell them from a value below the last bracket, and README's section 2 says so. bisection_cap.ink holds it, and its three such 0s move to marked. 2 lines more and one of comment. Found reviewing *The characteristic polynomial and stability*. |
| C193 `[fixed]` | **A constant product whose left is a power was refused where it read a name that moves.** C135 left it folded by evaluating its expression again, for `A^-1*b` to be solved as the interpreter solves it, and that reads what its operands read: beside `h(z) = 3`, `f(z) = 2^(1/2)*h(z)` called as `f(x_n)` was refused, "x is not defined", and so was `[2 0; 0 4]^-1*[h(x_n); 1]`, where the interpreter answers 4.24264069 and [3/2; 1/4]. A power is multiplied from the constants now, as any operator, and `A^-1*b`, whose -1 the interpreter reads as written, solved from A's and b's, in 7 lines more; `compile_c193` in `test/cli.cmake` holds it. Only a limit is still folded again. |
| C194 `[fixed]` | **A complex power of an exact number past a double's range overflowed or underflowed.** C175 took a power of 2 out of such a base for a real exponent only: `(10^400)^(1/2+i)` was inf+i*-nan where it is 10^200 at an angle of 400 ln 10, about -8.54e199 - 5.20e199i, and `(10^-400)^(1/2+i)` 0. The power of 2 is taken out for a complex exponent too now, 2^(e*y) turning by e*Im(y)*ln 2, in 2 lines; goldberg.ink holds it, and no other answer moved. Found reviewing C175. |
| C195 `[fixed]` | **`rho` of a nilpotent matrix was marked as approximated.** Its radius is 0, which no bracket is within 2^-53 of, so since C192 the cap ended its bisection and marked it: `rho([0 1; 0 0])` and `rho([0 0; 0 0])` were 0 "approximated past a thousand digits", where every eigenvalue is 0. A matrix whose polynomial is lambda^n, every coefficient after the first 0, is answered 0 without a bisection now, unmarked, in 1 line of the prelude and one of comment; bisection_cap.ink holds it, its two such answers move to unmarked, and README's section 2 says so. `abscissa` is as it was, its 0 marked, a nilpotent matrix's among them. |
| C196 `[fixed]` | **`rho` and `abscissa` of an inexact matrix whose cells sum past a double refused with a NaN.** C190's residual: B's power was taken of the sum of |A[j,k]|, which was inf, and `rhop`'s `0*s`, kept to carry a mark, made it NaN: `rho([~1 2; 3 4]*2*10^307)` was "a comparison needs a number, not -nan" where it is (5 + 33^(1/2))/2 times 2e307, 1.07e308. A sum of 2^1024 or more is taken of the cells over 4^n, which n^2 cells within a double cannot take past one, and its power raised by 2n, in 2 lines; exact, that is the same power, and an inexact sum below it never takes the clause. charpoly.ink holds it, and no other answer moved. |
| C197 `[fixed]` | **`rho` and `abscissa` past a double's range were unmarked where the tests stay exact.** The bracket is certified, but the answer is its double: `rho(10^400)` was inf and `rho(10^-400)` 0, unmarked, and `abscissa(-10^-400)` 0, a stable system shown as marginal. An answer whose double is not normal is marked now, as one the cap ended, a subnormal `rho(10^-320)` among them, its ~9.99988867e-321 not 10^-320's digits, unless its exact end is itself 0, a certified 0 whose double is exact, in 1 line changed, one more and one of comment; charpoly.ink holds it, and no other answer moved. So is the inf of an inexact matrix whose radius passes a double, `rho(~1e308*[1 1; 1 1])`, which no test can tell from the exact case. |
| C198 `[fixed]` | **`abscissa` of a marginal system with an exact eigenvalue 0 was marked as approximated.** Its 0 is no bracket's, as C195's rho was not: `abscissa([0 0; 0 -1])` and `abscissa([0 1; 0 0])` were 0 "approximated past a thousand digits". A polynomial lambda^k q, k at least 1 and q passing Routh's strict test, has 0 for an eigenvalue and none right of it, so its abscissa is answered 0 without a bisection now, unmarked, in 4 lines of the prelude, the factor stripped of q a cell at a time; bisection_cap.ink holds it, its `abscissa([0 1; 0 0])` moves to unmarked, and README's section 2 says so. A pair on the axis, `[0 0 0; 0 0 1; 0 -1 0]`'s ±i beside its 0 or `[0 1; -1 0]`'s alone, fails the strict test as one right of it would, and is marked still: telling the two apart takes Routh's singular cases, an auxiliary polynomial, which no cheap test gives. |
| C199 `[fixed]` | **`smax` of a matrix far past a double's range refused with a NaN.** `smaxd` checks its answer by scaling it back, and `rhod` scales by A's power in two halves, each a double only up to about 2^1074: from about 10^647 up or 10^-646 down a half was inf or 0, and scaling the inf or 0 back multiplied it by the other, so `smax(10^700)` and `smax([10^-700 0; 0 10^-700])` were "a comparison needs a number, not -nan" where `eig` answers them inf and 0, marked. A nonzero answer whose double is 0 or inf is marked without the round trip now, in 1 line of the prelude, wrapped; eig.ink holds it, and no other answer moved. |
| C200 `[fixed]` | **`eigb` gave a bracket where there is no eigenvalue to bracket.** eig's guard, Hermite's test, was eig's alone, so its bracket counted the roots of a matrix with complex ones as if all were real: `eigb([0 1; -1 0], 1)_64` was [0; 0], an exact bracket certifying 0 for ±i, `eigb([2 1; 1 3], 3)_64` [~8; 8] for a third eigenvalue of a 2x2, and `eigb([2 1; 1 3], 3/2)_64` the second's. eigb asks what eig asks now, and a whole k from 1 to n, and `eigk` scales `eigu`'s bracket back itself, so eig pays the test once, in 2 lines changed, one of them wrapped; eig.ink holds it, and no other answer moved. |
| C201 `[fixed]` | **abs's slope of a real value whose derivative is complex was complex.** abs answered x or -x, and so passed on x's derivative or its negative: with `T(z) = 1/(z - 1/2)`, `grad_(w = 0) abs(T(e^(i*w)))` answered `-i*4`, T's own slope, where |T| is real and peaks at w = 0, its slope 0. Found specifying the parts and modulus of a complex number (next in line), which fixed it, its real clauses taking re(x); `cparts.ink` holds it. |
| C202 `[fixed]` | **A part moving with a real variable was refused under a complex grad.** A point was complex for grad where any grad around it had one, and a refusal named the innermost: `grad_(v = i) v*grad_(w = 1) re(w)` and `grad_(w = 1) grad_(v = i) v*re(w)` were "re has no complex derivative" where both are 1, and `grad_(v = i) grad_(w = 1) re(w*v)` was refused "at w = 1", a real point. A jet's parts are indexed by the grads they are of, so the grads at a complex point are a mask of those, and a part, or abs, is refused where it moves with one of them, in the innermost one's name, in 9 lines more. Found reviewing the parts and modulus of a complex number; `cparts.ink` holds it. |
| C203 `[fixed]` | **abs's second derivative of a real value whose parts are complex was re(x)'s.** C201's real clauses give |z|'s first part, sign(z) re(z'), and so its parts past the first, re(z'') at the second, which lack im(z')^2/|z|: `grad_(v = 0) grad_(w = v) abs(1 + i*w)` was 0 where (1 + w^2)^(1/2)'s is 1, and |T(e^{iw})|'s at its peak -12 where it is -4. Where a nested grad differentiates the prelude's abs of a real nonzero value whose parts are not all real, each part past the first is solved from |z|^2 = re(z)^2 + im(z)^2, as `Inverse` solves A*B = 1, in 18 lines; a real argument's parts are its clause's, as before, so no compiled grad moves. Found reviewing the parts and modulus of a complex number; `cparts.ink` holds it. |
| C204 `[fixed]` | **A compiled `im(x)` discarded its operand.** im of a compiled value, real, was a literal 0, so x was never computed: `x_n = im(log(n - 5)) + a` checked "1 at 0, where the interpreter gives none: log needs a number above 0", and so did sin out of its range. im is NaN where x is now, x computed once and the header written aware, as a call's unread argument is (C189), in 11 lines and 3 of comment; `imag` in `test/compile/nan.ink` holds it. In a guard it is 0 still, so that abs's first guard folds, its other clauses reading x, and the prelude's header is as it was: a guard that reads im alone, `| im(log(n - 5)) == 0`, holds where the interpreter refuses. A division by 0 is inf in the step, not NaN, and im of it 0, as C189's pole. |
| C205 `[fixed]` | **max's and min's slope of a real value whose derivative is complex was complex.** Each answered a or b, and so passed on its derivative: `grad_(w = 0) max(T(e^(i*w)), 0)` and `min(3, T(e^(i*w)))` answered `-i*4`, T's own slope. Unlike |T| (C201), neither is defined but at w = 0, where T is real, their comparison refusing T anywhere near it, so they have no derivative there, and re(T')'s 0 would be an extension nobody wrote. Under grad, max and min are refused where a real argument's parts leave the real line, "max has no derivative where its argument leaves the real line at w = 0", in that grad's name, whichever argument is taken, and at a complex point in their own words, as abs is, where `grad_(z = i) max(z - i, 0)` was 1, in 15 lines more and 2 of comment; cparts.ink holds it, and no other answer moved. |
| C206 `[fixed]` | **`rho`, `abscissa`, `eig` and `smax` of an inexact cell that is itself inf refused with a NaN.** Its matrix scaled by B is inf, and its polynomial NaN: `rho([1 ~10^310; 0 1])`, `abscissa([~1 1; 1 1]*10^310)`, `eig` and `smax` of such cells were "a comparison needs a number, not -nan". An inf cell is a double whose value is lost, unlike an exact one past a double, whose radius C191 and C197 answer inf, marked, where it passes a double: no answer is certified of it, inf or other, as [1 x; 0 1]'s radius is 1 and [x x; -x -x]'s 0 whatever x is. So the four refuse it in plain words, "rho needs finite cells, not inf", a NaN cell too, in 7 lines and 2 of comment; charpoly.ink and eig.ink hold it, and no other answer moved. `eigb`, of two arguments, is as it was. |
| C207 `[fixed]` | **A product of an exact number past a double's range with a 0 part was NaN.** The exact number was made a double, inf, first, and inf times the other's 0 is NaN: `10^400*i` was `-nan+i*inf` and `re(10^400*i)` -nan, `10^400/i` `-nan-i*inf` and `10^400*~0` -nan, where the exact number is finite and each 0 part of the product 0. Such a product takes each part of the other alone now, 0 by a 0 and inf by any other, and a quotient by a complex number its conjugate's, in 9 lines and 2 of comment; errors.ink holds it, and no other answer moved. An inf that is a double, `~10^400*i`, is as it was: its value is lost, as C206's cell's. |
| C208 `[fixed]` | **`dhinf` of time scales far apart refused with a NaN.** hinf's bound on the norm, 2 tr(D'D) + 8n tr(PQ), is at least its square, and its Gramians, approximated past a thousand digits, left a double first: `dhinf([1/2 0; 0 1 - 10^-200], [1; 1], [1 1])`, the bilinear map's cells of 200 digits, was "a comparison needs a number, not -nan", and so was hinf of such cells. It is refused as the tests are, "hinf needs tests within a double's range", by a guard on the bound where it is inf, in 1 line; hinfdata.ink holds it, and no other answer moved. Cells of 400 digits 10^400 apart still make the bound NaN, `hinf([-1 0; 0 -10^-400], [1; 1/(3 - 10^-400)], [1 1])`, its solve multiplying an exact 0 by a cell approximated past a double, which is NaN, as `0*(10^2000/3)` is. |

**Measure instructions, not the clock.** One of those changes made the matrix
workload 20 per cent slower by the clock and not by a single instruction: with
loops and functions aligned explicitly, the builds before and after it ran
alike, at the slower time. The faster build had been a fortunate layout. A
wall-clock difference between two builds is evidence only when callgrind
agrees with it, or when the two are timed interleaved and aligned.

**Step 2: compile ahead of time.** `inkamath --compile model.ink -o model.hpp`
prints what step 1 builds as C++ against the headers already here -- `Number`,
`Matrix`, `Natural` -- so the semantics are the interpreter's by construction
and any C++ compiler builds the result: no LLVM, no new dependency. A module is
the definitions as they stand at the end of the file, each a function: `fib_n`
becomes `Value fib(long long n)`, memoised inside, and `area(r)` becomes
`Value area(const Value& r)`, with `double` wrappers on request for a C ABI,
and through it Python and WebAssembly. A name the file uses without defining
is an input the host supplies -- a measurement stream, `z_n` -- and a plain
definition such as `kp = 2` is a parameter with a setter that clears the memo
tables, the same invalidation the interpreter does.

**Which backend, and who optimises what.** An LLVM backend was closed as a
JIT (phase 9) on the grounds that the tree walk it would replace was 7 per cent
of the time. That argument does not reach a compiler, which replaces the whole
interpreter, so it was weighed again, and C still wins on other grounds. The
work of step 2 is analysis -- that `y_n` is a real scalar, that `P` is 2x2,
that a recurrence is a loop -- and once it is done, emitting `fadd` or `+` over
`double` ends in the same machine code, because the C compiler has LLVM's
optimiser or one like it. LLVM would add a dependency whose API breaks every
release, and emit what the toolchains of the embedded targets, GCC and vendor
compilers, do not take, and what a user cannot read or step through. What it
alone gives -- an object file with no compiler on the host -- serves no one who
embeds a header. So the line between the two halves runs where the semantics
stop being visible:

- **Ours**: folding exact constants exactly, then rounding once -- `0.1 + 0.2`
  is `3/10` here, and a C compiler folding the doubles would answer
  `0.30000000000000004`; types and shapes; recurrences turned into loops over a
  window of past terms, which no C compiler can do to a memoised function; and
  quantities derived from parameters, recomputed where a parameter is set.
- **The C compiler's**: everything over typed doubles in fixed shapes --
  folding, common subexpressions, inlining, unrolling, vectorisation.
- **Neither's**: reassociation. Without `-ffast-math` a C compiler keeps the
  order it is given, and the order emitted is the interpreter's, so that the
  compiled filter can be held to the interpreter's answers.

That makes two targets rather than one: C over `double` with no runtime, what
an embedded filter wants, and C++ over `Number` and `Matrix`, the exact model.
The analysis produces one typed form and each target prints it; the C target
comes first, because the proof of concept below is a filter.

**The exact C++ target is deferred** until a model needs it. Seven domains
were explored for one (below): exactness paid everywhere as the reference a
compiled model is held to, which is the interpreter's part, and nowhere in
what is deployed, where it would bring a heap and a bignum to a
microcontroller. Where an exact result is the product -- a probability, a
geometric sign -- the interpreter gives it. The work that follows is the
small fixes that make the interpreter more useful as it is; a sparse matrix
in the interpreter is the next large piece, if it is taken on at all.

**The first increment** compiles sequences of real numbers to a C header, and
refuses everything else by name. A file's definitions are run by the
interpreter, as they would be at a prompt; each sequence becomes a window of
its recent terms in a struct, as deep as the definitions reach back; a step
advances the index and computes that index's terms in dependency order. A plain
definition that reads no name is a parameter, a field initialised to its exact
value rounded once, which the host may assign; one that reads others is
recomputed where it is read, so that it follows them. A name used and never
defined is an input, a stream `y_n` passed to each step or a plain value the
host assigns. Assigning a parameter changes the terms still to come, as a
controller's gain is changed while it runs -- where the interpreter, clearing
its memo, answers as if the parameter had always had its new value. Where the
interpreter reports an error at run time -- a division by zero, a power it
would take in the complex plane -- the compiled code answers an infinity or a
NaN.

It is `compile.hpp` and `inkamath --compile pid.ink -o pid.h`. The header for
the PID was written by hand first, as the specification, and the compiler emits
it byte for byte (`test/compile/expected/pid.h`); built as C11 it holds the
controller, closing the loop in a harness, within 1e-14 of the loop's exact
values computed with Python's fractions. What it refuses -- functions, guards,
limits, series, factorials, locals, complex numbers, an index other than `n`
less a constant, a term read before its sequence starts -- it names.

**The third increment** is guards, because a PID that does not limit its
output is not one anybody runs: `pid_clamped.ink` holds the output to what the
actuator can give and the integral while the output is at a limit. A
sequence's guarded clauses become a chain of C conditionals -- base clauses,
then guarded ones in the order written, then the unguarded one, as the
interpreter tries them -- and a comparison used as a guard is emitted as itself.
The header was written by hand first and is emitted byte for byte; the loop is
at its limit for two steps and within 1e-14 of the exact one after. Where no
guard holds the interpreter reports it and a step answers NaN. Still refused: a
guard on a base clause or a plain value, and a base clause written after a
guarded one, which the chain would reach before the guard.

**The fourth increment** is sums and products with constant bounds, for the
filters that are one: `fir.ink` weighs its last four inputs,
`y_n = sum_(k=0)^3 b[1,k+1]*x_(n-k)`. The sum is unrolled with its index bound
as a constant, as a cell's names are, so `b[1,k+1]` is a cell at a constant
place and `x_(n-k)` a constant lag; a lag is now read from `n` plus constants
however they are spelled, `n-k-1` included. With nothing before it, `y` starts
at 3, where its four reads first exist. The header was written by hand first
and is emitted byte for byte. Refused: a sum with no upper bound, bounds that
read a parameter, and more than a thousand terms, each of which is a line of C.
A product of matrices shows what inlining costs -- each cell repeats the
cells before it -- which is what temporaries are next for.

**Temporaries** are made where the compiler itself would repeat a cell: an
operand of a matrix product, whose cells each feed a row or a column of the
result, and a single value stretched over a matrix. Such a cell, unless it is
already a name or a number, is computed once per step into a `const double`
before the sequence that reads it, and text that is the same within one step
is one temporary; one that nothing reads, as when a single cell is read out of
a product, is dropped. The values are the same, computed once, so the
harnesses hold as they did: the PID, clamped PID and FIR headers do not move,
and the Kalman filter's reads its gain's denominator and its innovation once
each. The product that repeated every cell before it is now as long as its
products.

**A matrix power** is unrolled by squaring as the interpreter computes it, of
the inverse for a negative exponent, and the inverse is the interpreter's own
Gauss-Jordan: the largest pivot, any rather than an exact zero. The pivot
depends on the values, so it is chosen as the step runs, by a helper the
header defines once for each size it needs, into a temporary array; where the
matrix is singular, which the interpreter reports, every cell is NaN.
`kalman2.ink` measures position and the sum of position and velocity, so its
gain inverts a 2x2, and its estimates and first gain are within 1e-14 of the
exact ones; its header was recorded and read, the helper's form being new but
the rest the Kalman filter's. The interpreter starts a power from the identity
and multiplies it in, which the compiled code skips: that differs only where a
cell is infinite. An exponent must be a whole constant; `A^0` is the identity,
kept apart from the folding of constants, since it reads `A`.

**Random models** test the claim the hand-written ones cannot: that a compiled
model answers as the interpreter does. A generator, run at build time, makes
three hundred -- sequences with bases, guards, lags, sums, inputs, parameters
and a derived value, and half of them a 2x2 block with a product, a transpose,
an inverse and cells read out -- runs each through the interpreter, compiles
it, feeds both the same inputs, and writes one C file that steps every compiled
model and holds each term to the interpreter's within 1e-9. Constants and
inputs are quarters, which a double holds exactly, so a difference is rounding,
not noise. A model the compiler refuses is skipped, and the build fails if
fewer than half compile, so that the test cannot drift into testing nothing;
280 do. Nearly fifteen thousand more, under sixteen other seeds, agree.

It found two things the compiler had wrong, both about where a sequence
starts, and one of its own. A sequence's start was the index where every
clause's reads exist, but a guarded clause's reads matter only where its guard
holds: the interpreter answers from the unguarded clause below that index, and
the compiled step did not answer at all. Each clause now carries its own index
from which its guard, and its value, can be evaluated -- NaN before it, where
the interpreter reports the term it could not read -- and a sequence with
guards starts where some path through them can answer. And a sequence with no
base clause that reads no term is a closed form the interpreter answers at
every index, before the model starts too, so `c_n = a_(n-1)` with `a_n = n/8`
reads `a_(-1)` at 0; a closed form read back in time is now compiled again at
that index, rather than read from a window that holds nothing from before the
start. The interpreter's guarded clauses also answer below the lowest base
clause, where its unguarded one does not, which a step has no terms for; a read
that could reach there is refused. The test's own: it wrote the inputs into C
as `(7/4)`, which C divides as integers.

**The second increment** is the Kalman filter, which is matrices of fixed
shape: a value is its cells, each one C expression, and a matrix product is the
sum over the inner dimension in the interpreter's order. A sequence's shape
comes from its base clauses, or else from its general clause, compiled on first
reading -- so `K`, whose general clause is the first to need `P`, finds `P`'s
shape in `P_0` without the cycle through `P`'s own general clause. A sequence
with no base clause starts at the first index where every term it reads exists,
which is where the interpreter would first answer it: the filter's `xp`, `Pp`
and `K` start at 1, a step after `x` and `P`. The header is
`test/compile/expected/kalman.h`, recorded from the compiler and read rather
than written first, since the PID's had already fixed the form; fed the
measurements of a target moving at 2 per step, the compiled filter is within
1e-14 of the exact estimates. Still refused: a matrix power, a matrix built
from matrices, a cell whose place is not a constant, comparing matrices. Each
derived matrix is inlined into its readers, so a cell's expression can repeat a
shared subexpression -- the C compiler's common subexpressions take it out, but
a larger filter would want temporaries.

**What step 2 is for, decided by building it.** Waiting for a use case that
nothing yet can serve would wait for ever, so a proof of concept manufactures
one: a PID controller, then a Kalman filter. Both already run in the
interpreter -- a closed-loop PID is eight definitions, `frac y_3` is exactly
`2697/6250`, and a constant-velocity Kalman tracker gives its gain exactly,
`[752651/857701; 1203101/1715402]`. That is the product: a filter written once
in the notation of the paper, exact reference outputs from the interpreter, a
C++ header from the compiler, and the compiled floating-point filter tested
against the exact transcript. Writing the two found four things they need:

- `[done]` **Deep recurrences.** `y_5000` failed -- evaluation nests more
  than 256 references deep -- because each term reaches back through every
  earlier one. A call that runs out of depth now fills its sequence from the
  lowest base up and tries again, each term finding the one before it
  remembered: `y_5000` of the PID and `x_2000` of the Kalman filter answer.
  Only what failed takes that path, so nothing that answered before moved but
  `sequences.ink`'s `g_500`, which was the depth error and is now `501`; the
  bound it stood for (C1) is shown by a recurrence reaching up instead. A
  recurrence that steps by two from a single base failed: filling it asked
  for the odd terms, which it never defined. It fills with its own stride now
  (next in line).
- `[done]` **Inputs from the host**, in the C target: a stream passed to
  each step, or a value the host assigns.
- `[done]` **A window on the memo**, in the C target, where each sequence
  keeps its terms only as far back as they are read. The interpreter's memo
  is bounded only by its size, and keeps the latest terms (C69).
- `[done]` **Transpose**, which the Kalman filter spelled out by hand as `Ft`
  and `Ht`: `F'`, as MATLAB and Julia write it, specified in `matrices.ink`
  before it was built. The quote belongs to what it follows, subscript and
  cell brackets included, before any operator -- `2*a'` is `2*(a')` and
  `a^2'` is `a^(2')`, as in Julia -- and it does not conjugate, where theirs
  does. The first spelling bound the quote so tightly that `x_(n-1)'`
  transposed the index; a transcript entry now says it is the term's. The
  identity stays written out, `I2 = [1 0; 0 1]`: the language has no built-in
  function, and the one filter that needs it does not yet justify the first.
- `[done]` **Cells**, in the interpreter: a matrix defined by its cells,
  `I[j<=2, k<=2] = j == k`, which also writes the identity. The size is in the
  brackets, as bounds on the names, and a guard says only which cells, since a
  guard such as `j + k <= 4` is no rectangle; a cell no clause gives is 0, and
  a clause for one cell beats the others, as a base clause beats a sequence's
  general one. Specified in `matrices.ink` first, after three spellings were
  drafted side by side -- `M[i^3, j^3]` read as cubes, and bounds in the guard
  mixed sizes with conditions. A matrix written whole keeps its place beside
  cell clauses: it is the size and every cell no clause gives, so `a[1,1] = 9`
  after `a = [1 2; 3 4]` changes that cell and nothing else, as a base clause
  overrides a sequence's general one. The first spelling had the cell clause
  replace the matrix, as an index replaces a value, and leave `a` with no size;
  nothing needed that, and it surprised. A sequence's terms can now be
  defined cell by cell too (next in line). The compiler takes such a matrix one cell at a time,
  its names bound to the cell's place as constants, so that the size, the
  guards and the clause that gives each cell fold exactly; one whose size or
  guard reads a parameter it refuses. The Kalman filter's identity is now
  `I[j<=2, k<=2] = j == k`.

**Beyond filters.** Five sessions, run in the interpreter, asked what other
models need: a loan schedule, gambler's ruin and dice, an SIR epidemic, a
triangle-orientation test, and heat on a line by finite elements. Three of the
five ran out of exactness before they ran out of language. 1.003^360 is past a
thousand digits, so a 30-year loan's payment is inexact before the schedule
starts; the epidemic's digits double every step, so from step 8 the interpreter
computes in doubles; the heat equation's grow by twelve a step and cross near
step 80. None of it showed (C68): `~` marks a value not printed in full, so an
exact third and an exactness that ran out looked alike. The reference a compiled model
is held to therefore has a horizon, and a short one for anything nonlinear.
Where the exact answer is the product it is well within reach: gambler's ruin
is `5832/12691`, from its linear system and from its closed form alike. Where
it is not, doubles did as well: rounded to the cent each month, they match the
exact schedule for all 360 months. The orientation test is where they do not,
a point 7 units in the last place off a line being -1 exactly and 0 in
doubles. Wanted: `floor` or `mod` (the loan wrote its own, by binary descent),
`and` and `or` for guards (written as products), and compiled functions (the
orientation test is one). Two documented behaviours were traps on the way: a
plain definition erases the guarded clauses written before it, and `[1 -1]` is
`[0]` (C52), which an element matrix writes on every line. The exact C++
target is deferred on what this found (above).

Compiling the heat equation found two defects:

- `[done]` **A size given by a name is refused.** A plain definition is a
  parameter the host may set, so `N` is not a constant; only `7` is. A
  parameter read where the compiled code needs a constant -- a size, a sum's
  bound, a cell's place, a matrix power, a lag, which is a window's depth --
  is now fixed: the file is compiled again with it as a constant, and what
  reads only fixed names folds exactly, so the heat equation's `h`, `K`, `I`
  and `B` are numbers in its header and `dt` is its only parameter. The
  header names what was compiled in. Letting the host set `N` would need
  arrays sized at run time, which is layer 3's loops, not this.
- `[done]` **What derives from parameters alone is recomputed at every
  step**, where this plan says it is recomputed where a parameter is set: the
  heat equation's `A` inverts its matrix each step. At n = 100 that is 0.5 ms
  a step against 7 us for the product alone. Such a value is now a field that
  `update` computes, which `init` calls and the host calls after assigning a
  parameter; a C struct has no setter to do it for them. A cell that is a name
  or a number is still read as itself, so the Kalman filter's step, whose `F`
  is `[1 dt; 0 1]`, did not move, and a field no cell is read from is left
  out. `test/compile/heat.ink` doubles its step halfway and calls `update`.

**Finite elements in two and three dimensions.** Assembly already reads as on
paper, `K = sum_(e=1)^E P(e)'*Ke*P(e)`, where `P(e)[a<=2, p<=N] = p == e-2+a`
gathers element `e`'s nodes; it reproduces the 1D stiffness matrix exactly, and
a mesh is two matrix literals, coordinates and connectivity, read through
`X[C[e,a], 1]`. What is missing, by layer:

1. **Language**: `mod`, to number a structured grid's nodes.
2. **Interpreter**: sparse storage, which a matrix chooses for itself, with
   products and transposes that keep it, and `A^-1*b` done as a solve, since
   the inverse of a sparse matrix is dense. Dense, 10^4 nodes are 10^8 cells,
   and assembly through `P(e)` costs E n^2. Exactness has a ceiling of its
   own: the solution's denominators divide det K, which grows by about half a
   digit per unknown in 2D and 0.7 in 3D, so an exact solve stops near a
   thousand or two unknowns.
3. **Compiler**: loops over arrays that hold the sparsity as data, and a
   solver -- a Cholesky factor computed once, or conjugate gradients. That is a
   different compiler from one that writes every cell out.
4. **Out of scope**: meshing, 10^5 unknowns and up, preconditioners,
   parallelism, visualisation.

**Without layer 3 the compiled model is dense, and still runs.** Where the
interpreter with layer 2 would overtake it was measured by the clock, one run
each, GCC -O2 and RelWithDebInfo, so as orders of magnitude:

- Compiled, a multiply-add costs 0.66 ns: `A*u` at n = 200 is 26 us a step.
- Interpreted, over inexact numbers, it costs 11 ns in a product with a vector
  and 50 in a product of matrices, plus 1.7 us a step. A sparse matrix would
  pay the same per nonzero.
- The unrolled header is the wall: GCC takes 0.7 s at n = 25, 20 s at 100 and
  96 s at 200, where the header is 3 MB.

For a constant matrix, a compiled step costs 0.66 n^2 ns once the second defect
is fixed; an interpreted one, two triangular solves over a sparse factor, costs
22 ns per nonzero of the factor, which holds about 2n of them in 1D, n^1.5 in
2D and n^(5/3) in 3D, banded. The interpreter overtakes at about n = 100 in 1D,
1100 in 2D and 37000 in 3D. With the inverse at every step, as today, it
overtakes from about n = 20. The dense path stops before either: at n = 200 for
GCC's patience, and near 180 for one matrix of doubles in a microcontroller's
256 KB. So without layer 3, compiling is for models of up to a couple of
hundred unknowns, and in 2D and 3D the crossover lies beyond what the dense
path can build. Emitting a constant matrix as an array read in a loop, rather
than cell by cell, lifts the GCC wall cheaply and leaves n^2 in time and
memory. The interpreter is not a deployment target, and a fill stops at ten
million terms (C69), so overtaking means runs at design time.

**Small neural networks and point clouds**, tried the same way. Running a
trained network is fixed-shape linear algebra, which is what the compiler
does: a convolution is cells and two sums, and answers exactly (the Laplacian
of `j^2*k` is `2k`); a linear recurrent layer, `s_n = A*s_(n-1) + B*u_n`, the
shape of S4 and Mamba, compiles as the Kalman filter does. A dense layer with
ReLU compiles only as one sequence per unit, since a sequence's terms cannot
be defined cell by cell. Also wanted: `exp`, for sigmoid and tanh; `max`, for
pooling; and loops over arrays, since each weight is a multiply-add written
out. On a microcontroller a network runs in int8, which is integer arithmetic
with rounding and saturation, and so exact: the interpreter could hold an int8
kernel to the bit, as TensorFlow Lite Micro's reference kernels do, given
rounding in the language and an integer target. Training is out of reach: it
needs gradients, loops over data, and randomness. Point clouds fit only in
their arithmetic: a streaming centroid and covariance compiles, as would a
rigid transform, a plane fitted from running sums, or an exact orientation
test once functions compile. The algorithms around it -- neighbour search,
voxel grids, sorting, RANSAC, ICP -- are arrays of data-dependent length,
indices computed from data, and loops, which is another language.

What every domain explored so far asks for, by how many ask: compiled
functions (orientation tests, point kernels, a layer called per input); loops
over arrays instead of cells written out (finite elements, networks); `floor`
or `round` (the loan, grids, int8); a sequence's terms cell by cell
(networks); `exp` (networks, signal processing).

Beyond Python on very large numbers is not on this path. What is fast at a
hundred thousand digits -- PARI/GP, Julia, Mathematica -- is GMP, with FFT
multiplication and subquadratic division; writing that here is not a phase but
a career, and depending on it is a decision CLAUDE.md, section 5 reserves.
Nothing asks for it while exactness stops at a thousand digits. Relevance is
the notation -- recurrences, `lim`, series as on paper, exact by default --
which Python spells as code; speed is a guardrail, not the race.

## Phase 15 — Models, files and their scopes `[done]`

After `and` and `or`. Three things asked for it. The domains explored in
phase 14 want reuse: the PID, the Kalman filters and the heat equation repeat
the identity by cells and a stiffness matrix, and `round`, `ceil` and `mod`
are a line of `floor` each, documented rather than there. A compiled model's
interface is not one a reader can find: `pid.ink` and its header do not say
how the one is meant to be used from the other. And a comparison with SCADE
and Simulink found composition the weakest point: a model cannot be used
twice, under two sets of parameters, or wired to another. That comparison is
a check, not a template; the forms below were argued from inkamath's own
notation.

**Loading needs scoping, from the first file.** Names in mathematics are short
-- the heat equation alone defines `N`, `h`, `K`, `I`, `B` and `A` -- and a
global is read when it is used, not when it is defined: after `A = 2*h`,
redefining `h` changes `A`. Running a file's definitions into the session
would break both ways at once, silently: its `h` replaces the user's step, and
the user's later `h` changes what its `A` computes. So a file is a scope, and
so is a model:

- Its definitions read its own names first, then those of the file around
  it, then the prelude's and the built-ins', never the session's. The prompt
  is the file of a model written there, so that one reads the session.
- The session reaches them qualified, `filters.a0`, `fast.v_3`: `.` after a
  name is an error today, so the spelling is free. `use filters (lowpass)`
  brings in unqualified only the names listed.
- The prelude is the outermost scope, its names unqualified and replaceable,
  as the built-ins are. From the session a name is sought in the line's
  locals, the session, then the prelude and the built-ins; a file or an
  instance is reached only by its qualified names.
- Each definition knows the scope it was written in and resolves there; the
  globals become one table per scope. The memo is keyed on the definition,
  so it needs nothing.

Visibility, re-exports and declared outputs wait for a need.

**What the interface is today**, read from `pid.ink` and `pid.h`, is three
conventions no file states. An input is a name nothing defines, so `y` is one
only by its absence: the interpreter cannot run `pid.ink` alone -- `u_5` is
*y is not defined* -- and a misspelled name compiles into a second argument
of the step. A parameter is any plain definition, a gain and an internal
constant alike, each a field the host assigns before calling `update`. An
output is any sequence, read from its window, where `m.u[0]` is `u_n` and
`m.x[3]` is `x_(n-3)`.

- `[done]` **The header opens with the interface** it compiled, in the terms
  of the file: its inputs, as arguments of the step; its parameters, with
  their values, as fields assigned before `update`; and each sequence, with
  what `m.u[k]` is. A reader of the header needs nothing else to call it.
  Done ahead of the phase, since it needs no scope: a usage sketch and a
  paragraph, which absorbed the line naming what was compiled in.

**Two forms of reuse were compared**, in a draft transcript, on the same three
examples: a gain fed an input, two low-pass filters in series, and a
controller closing a loop around a plant.

- **A. A file is a model.** `use lowpass as fast` loads `lowpass.ink` into a
  scope named `fast`, and a second name is a second instance. An input is
  declared, `u_n = input`, and it and any parameter are replaced from the
  session by a qualified definition, `fast.a = 1/2`.
- **B. A model is a function whose value is a group of definitions.**
  `lowpass(a = 1/10, u_n) = { ... }`, in braces, one definition a line. An
  instance is a definition, `fast = lowpass(a = 1/2, u_n = 1)`, and `use
  filters` only imports a file.

|                         | A: a file is a model                      | B: models in braces                           |
|-------------------------|-------------------------------------------|-----------------------------------------------|
| new syntax              | `use F as name`, `x_n = input`            | `= { ... }`, a call's result read qualified   |
| an instance             | a load under a name                       | a definition, as any other                    |
| its inputs              | declared in the file, then replaced       | in the signature, supplied as arguments       |
| a library of small ones | one file per model                        | one file, which `use` imports                 |
| at the prompt           | no: a model is a file                     | yes: a brace continues the line               |
| a model used once       | not without a name                        | `lowpass(...).v_3`, unnamed                   |
| a closed loop           | two files, wired afterwards               | two definitions, in either order              |
| builds on               | scopes; a file loaded under two names     | scopes; named arguments with defaults         |

**B was chosen**: it reads better, and every row but the first goes its way.
A's one advantage, a model without new syntax, is paid for by wiring done
from outside, after the fact, which is where A reads worst. What B decides:

- **The signature is the interface.** A parameter has a default, an input
  has none, and an input indexed is a sequence. A misspelled argument is an
  error where it is written, *gain has no parameter z*, which is what A's
  declared input was for.
- **An instance's names are all readable**, qualified: its terms, its
  parameters, its inputs and the instances it holds, `h.low.v_3`. Outputs are
  not declared.
- **An argument is a clause written in the session**: it reads the session's
  names, and follows them as any definition does. The session never defines
  an instance's names, nor a file's, from outside; an instance is changed
  where it is defined.
- **An instance evaluates nothing when defined**, so the two halves of a
  closed loop may be written in either order, and reads its model when it is
  read, so that a model redefined changes its instances, as a function
  redefined changes what calls it.
- **Compiled, a model is compiled by name**, `inkamath --compile filters.ink
  lowpass`: its parameters are fields, with their defaults, and its inputs
  the step's arguments, the interface the header already states. A file of
  instances compiles to nested structs, one step for them all.

`[done]` In the interpreter, specified in `test/data/spec/models.ink`, over
`filters.ink` and `bad.ink` beside it, and now `test/data/models.ink`
unchanged; it replaced a specification of form A written first. `use filters` names a file by its stem, beside the file that
names it, so that no string enters the language; a file is loaded once, so a
diamond or a cycle loads nothing twice; a file that cannot be read or parsed
loads nothing and names the file and line; `?gain` prints the model as
written; and at the prompt an open brace continues the line as a bracket
does, keeping the breaks, since the body is one definition a line.

The prelude is not a file the session uses but text built into the
interpreter and included bare beneath the session, as C includes a header:
no scope and no qualified name, and its names seen unqualified from every
scope, a model's body included. It holds `ceil` and `mod`, not `round`, whose
rule is the model's, as `floor` decided. A name it defines, as a built-in,
is replaced for the session only, so that `floor = 3` leaves the prelude's
`ceil` reading the built-in. Pasted into the session instead, the prelude
would be invisible from a model and broken by that replacement. Later, when
a library needs it: an `include` of a user's file, beneath the session as
the prelude is, of the definitions its header marks `export`.

What the interpreter does, beyond the transcript. The globals are a chain of
scopes, the built-ins at its root, and a definition carries the scope it was
written in, where what it reads is sought. An instance is made when it is
first read and kept until a top-level definition clears the memo, which is
when anything it read could have changed: a named one by its definition, an
unnamed one by where it is written and the scope reading it. An unnamed
instance inside a function, `f(a) = lowpass(a, 1).v_2`, takes the values its
arguments read of the call's frame, since that frame is gone by the time an
argument is evaluated, and so is one instance per value. Two outputs moved:
`a.b` in `basics.ink` reads `b` of `a` rather than refusing the point, and
`f()` is `f`, so that `gain()` is the instance with every default.

What it leaves. A message raised inside an instance names the definition as
the model wrote it, `y is a sequence`, not `g.y`; only the messages the
scopes themselves raise are qualified. A file is named by its stem, beside
the file that names it, and so never from a directory below.

`[done]` **A model compiled by name**, `inkamath --compile mix.ink mix -o
mix.h`, specified by `test/compile/mix.ink`, its harness and its expected
header. The file is read as `use` reads it, so that the model sees the
file's names, and what is compiled is its instance with every default and no
input given: the compiler's own rules, run over that instance's scope rather
than the session's. The signature is the interface the header states. Its
inputs are the step's arguments in the signature's order, and each is one
whether the body reads it or not; its parameters are the fields, holding
their defaults, and nothing else is -- `tau` in `mix.ink` is compiled in
through `a`'s default. A name nothing defines is refused, where a file
compiled whole takes it for one more input. An instance inside a model is
refused for now.

Next, **a file of instances**, `fast` and `slow` wired as in section 5 of
the README, compiled to one step. Instances read each other's terms at the
same index, so their sequences must be ordered together, not instance by
instance: the step is the one the compiler already orders, over every
instance's sequences named by where they are, `fast.v`, and the struct nests
one struct per instance, `m.fast.v[0]` and `m.fast.a`.

`[done]` Specified by `test/compile/loop.ink` and `chain.ink`, their
harnesses and their expected headers. Every definition is keyed by where it
is, the scope's label and its name, `plt.x` or `h.low.v`, which is how C
reaches it in the struct, so a session's own names keep their bare keys and
no header compiled before moved. A name is sought where its definition was
written: an argument, which an instance holds, reads the session. The sort is
the one the compiler had, over every instance's terms; an argument is a copy,
`m.ctl.y[0] = m.plt.x[0]`, which the C compiler sees through. An instance's
parameter given or defaulted to a constant is a field of its instance, one
given what reads the session's parameter follows it and is none, and a file's
own name is a constant. Refused: an unnamed instance, which has no name for
the struct to give it, and an input nothing gives.

The order is found once, when compiling, not at every step. The step could
resolve it as the interpreter does instead: a getter per term, computing it
the first time an index asks and marking it done, so that asking for the
outputs computes the rest in whatever order they need. That needs no sort and
no qualified names, lets each model be compiled once and wired by the host,
and accepts a guarded term whose branches read different terms at the same
index, which a sort taking both branches refuses. But it finds a loop without
a delay in the field rather than at build time, pays a branch and a store per
read, recurses, and hides behind pointers what a C compiler would inline --
to recompute, at every step, an order the definitions fix once. A step that
goes into a device wants the fixed sequence of assignments a reviewer reads
top to bottom, with its stack bounded and its time measurable; the laziness
belongs to the interpreter, where a model is explored. So the sort, whose
refusal is the static check of causality. The getters stay in mind for models
compiled one header each and wired by a host that cannot be recompiled whole,
which would be a feature of its own, and a smaller one.

`[done]` **The prompt is a model's file.** A model written at the prompt read
first the built-ins, so that it could use no other model, no function and not
itself, which a file allows its own. Trying the models on kernels showed the
cost: a filter bank, a convolution and a recursion each needed a file. Now a
model reads the scope it was written in, the session at the prompt, which
removed the one case the instances made for it. What the isolation was for
holds where it matters, between a file and the session that uses it: at the
prompt the model and the values are the same writer's, and a model reading
`h` follows it as `A = 2*h` does. Two entries of `models.ink` moved, `l.y`
reading the session's `z` and `q.y_5` the session's `mod`.

`[done]` **A loop without a delay is refused where it is closed**, rather than
found as nesting too deep when a term is read: after each top-level
definition, the terms every general clause reads at its own index are
followed from the scope defined in, the session's and those of the instances
they name, and a definition that closes a loop is taken back with the loop
named, `left.y_n reads left.x_n, which reads right.y_n, ...`. A guarded
clause is not followed, since its guard may break the loop: nothing that
answers is refused, and a loop through a guard is still found where it is
read. The compiler's sort refuses the same loops in a header.

And an idea, not a plan: what a body requires of its inputs and
parameters -- a sequence rather than a value, a shape -- stated by the
signature or inferred from the body, and checked where an instance is made,
as C++ checks a `requires` clause. An error inside an instance would then be
reported at the instance, in the terms of its interface, which is what the
unqualified messages above lack.

## Sequencing

Phase 2 gated everything: no implementation work started before the
transcripts said what it should do. Phase 3 came next because C13 made every
later change observable, then phase 4, the redesign the rest of the plan
exists to serve, then phase 5 and phase 6.

Phase 7 is the second pass's queue. Its order is the reverse of the first
plan's: there, the argument was that nothing could be seen until failure
existed, so C13 came first. Here the interpreter reports failure well and
crashes anyway, so the crashes go first. C22 and C23 come next not because
they are subtle but because they are not — the headline feature is broken at
three columns, and a corpus that never exceeded 2x2 is why nobody noticed.

Phase 8 waits for phase 7. It is the only phase that changes the language
rather than repairing it, and it was worth nothing while four inputs still
killed the process.

Phase 9 is numbered after phase 8 and landed before it, on the argument that
the cache does not depend on the language change and the language change
depends on the cache for its cost. That turned out to matter more than
expected: with the step budget gone, depth became the limit users meet, which
is what measured the cost of lazy parameters and sent them to Deferred.

The honest risk was phase 4. It changed what existing sessions mean, so it
could not hide behind unchanged goldens — every moved line was justified
against a spec transcript, in the commit that moved it.

## Next in line

After phase 13's decimals and before its step 2: three gaps found by using
the interpreter, each one commit once specified.

- `[done]` **`==` and `<>` on whole matrices.** A printed matrix reads back but cannot
  be checked against what printed it. The objection `conditional.ink` records
  is to comparing cell by cell, which answers a matrix nothing can reduce to a
  truth; two whole matrices are equal or not, which is one truth. `<` stays
  refused.
- `[done]` **A negative matrix power as the inverse.** `[1 2;3 4]^-1` is refused because
  there was no inverse to give. With exact numbers there is one, exactly --
  `[-2 1; 3/2 -1/2]` -- and a singular matrix is an error, as dividing by an
  exact zero is.
- `[done]` **`1 ~2` says "unexpected '~'"**, where every other juxtaposition
  is told that `*` is probably missing. So did `2 !3` and `2 [1 2]`.

Then phase 13 step 2, the bignum -- done, and the cost exact numbers carry
turned out not to be a `Number`'s size (see step 2). Then phase 14, the
evaluator, where the larger gap is.

After phase 14's step 2, with the exact target deferred: the small fixes
that exploring seven domains asked of the interpreter, by how many asked.

- `[done]` **Exactness ran out without a word** (C68).
- `[done]` **A long recurrence failed at 256 deep** (C69).
- `[done]` **`floor`**, for the loan's cents, a grid's node numbers and
  int8's rounding. Specified in `test/data/spec/floor.ink`, now
  `test/data/floor.ink` unchanged but for one entry added before it was built,
  that a session may define it again, as it may `pi`. It is the first built-in
  function, and the only one: `round`, `ceil` and `mod` are a line of it
  each, because which rule a model rounds by -- halves up or to even, to the
  cent, toward zero -- is the model's to say, not the language's. Exact of an
  exact number, so `floor((0.7+0.1)*10)` is 8 where doubles say 7, and cell
  by cell of a matrix. Spelled as a call because every language a reader
  knows spells it so, and `[x]` is a matrix here. It is a definition the
  interpreter starts with, as `pi` is, whose body is the one node the
  language cannot write; the compiler passes over it until a model redefines
  it, and refuses a model that calls it, as it does any function.
- `[done]` **`and` and `or` for guards**, written as products and sums
  before. Specified in `test/data/spec/logic.ink`, now `test/data/logic.ink`
  unchanged: 1 or 0 exactly, as a comparison
  answers; any value but zero true, as in a guard; `and` tighter than `or`,
  both looser than a comparison; and the right side read only when the left
  has not decided, which products cannot do -- `n > 0 and c_(n-1) >= 4` never
  asks for `c_(-1)`. No `not`, since every comparison has its opposite.
- `[done]` **Compiled, `floor`, `and` and `or`**: C's `floor` while the
  built-in stands, C's `&&` and `||`. What short-circuiting means for a step
  is where a sequence starts: a term read only on the right of `and` or `or`
  cannot delay it, since the interpreter answers wherever the left decides,
  so `n > 0 and c_(n-1) >= 4` answers at 0. Such a read is checked where it is
  read instead -- the test becomes a double that is NaN where the right was
  needed and its term did not yet exist, as the interpreter reports it -- and
  a right side that reads only what its left already did stays plain C. The
  random models found it at once: a guard whose left decided at index 0 was
  answered by the interpreter and skipped by the step. `floor` is generated
  only of values a double holds exactly, since of any other its jump at an
  integer is where rounding shows; `adc.ink` is the readable case. Then
  phase 15, models, files and their scopes, before what follows.
- `[done]` **A sequence's terms cell by cell**, which a layer of a network
  needs. Specified in `test/data/spec/terms.ink`, now `test/data/terms.ink`
  unchanged: `h_n[j<=3, k<=1] = ...` as a
  matrix is defined, every cell seeing the index and a guard choosing cells;
  a base term by its cells beating the general clauses, as any base does; a
  size that may come from the index; a term written whole as well being the
  size and every cell no clause gives, as for a matrix; and one cell, of
  every term (`g_n[1,2]`) or of one (`g_2[2,1]`), overriding the rest as
  `M[1,2]` does. A clause wins where it is at least as specific in both the
  index and the cell. A base term and a cell of every term are each more
  specific in one and less in the other, so where both give a cell the
  answer is an error naming the clause that settles it, `g_0[1,2]`: no
  choice between them was principled, and a guess would have been silent.
  Compiled cell by cell into the chain the interpreter tries: a guard that
  reads only the cell's place folds away, so a delay line is one expression
  per cell and a ReLU one test; a base term and a cell of every term that
  both give a cell are refused with the interpreter's question. `net.ink` is
  a delay line, a layer and a sum; the random models gain a term by cells.
  Compiling it found that a sequence compiled on the way to another undid
  the names of the cell being compiled, as cells use the same `j` and `k`;
  each sequence now compiles with names of its own.
- `[done]` **Filling with a recurrence's own stride.** A fill steps by the
  greatest common divisor of how far back the general clauses read terms,
  theirs or another's that reads them back, so `st_n = st_(n-2) + 1` fills
  every other term and `st_5000` answers; a read that is not the index less
  a constant falls back to every term. Every term a fill asks for is then one
  the term asked for reaches, so a fill that fails says why -- `st_5001` has
  no clause for index -1 -- rather than reporting the depth.
- `[done]` **`A^-1*b` as a solve**: where both are exact and `b` is columns
  `A` can solve for, `A` is factored with the inverse's own pivots and `b`
  solved for, which is the product's answer exactly -- 13M instructions
  where the inverse took 60M, for the 40 unknowns of a stiffness matrix.
  Inexact, the two round differently -- `[1 2;3 4]^-1*[~1;1]` is `-1` by the
  inverse and `~-1` solved -- so there, and for anything else, it is the
  inverse times `b` as written. The order is the product's too: `A` is
  factored, and a singular one refused, before `b` is read. A sparse matrix,
  if ever, comes after it.
- `[done]` **Models, files and their scopes** (phase 15): in the
  interpreter, a model written in braces, an instance a definition read after
  a point, `use` for a file and the prelude beneath the session; compiled, a
  model by its name and a file's instances into one step; and a loop without
  a delay refused where it is closed. What it leaves, and an idea for errors
  inside an instance, close the phase.
- `[done]` **What would not compile, known before compiling**:
  `--compile file [model]` without `-o` writes nothing and lists every
  definition it would refuse, and why. The compiler stops at the first, so a
  refused definition is set aside and the rest compiled again, until none is
  refused or one is refused that no single definition is to blame for; one
  that reads a definition set aside says so. The interpreter itself refuses
  nothing for not compiling: exploring is its part, and what compiles means
  the same in both, which the random models hold it to.
- `[done]` **Calls and unnamed instances compiled**, specified by
  `test/compile/kernel.ink` and `bank.ink`. A function, and an instance of a
  model without memory, are compiled where they are called: the parameters
  bound to the code of the arguments, read where the call is, the clauses a
  chain the guards fold, and the body's names computed when first read. So a
  constant folds through -- `power(2, 5).v` is 32 in the header -- and a
  recursion unrolls where its guards fold, sixty-four deep at most. Folding a
  constant is now the interpreter's, where the expression is read, with what
  a call gives and a cell's names as locals: the scratch interpreter it used
  knew neither, nor any function. An instance with memory is kept between
  steps, so it is one of its own, made once for each place it is written and
  each value it reads of the cell there, and named after them, `bank_smooth_1`;
  its arguments keep those values. One whose argument reads the step's index
  would be made anew at each step and run from its base at every one, and is
  refused, as is one inside a call. No header compiled before moved.
- **`--check`, reserved** for two checks: that a compiled header answers
  what the interpreter answers, to within how far doubles drift from the
  exact values; and that a transcript's recorded answers are what the
  interpreter gives now.
- `[done]` **The first check, `--check file instance -o check.c`**, the oracle
  of `MANIFESTO.md`. What is checked is an instance, not a file: the instance
  is the drive, written in the language, its parameters the ones compiled and
  its arguments the inputs, so a closed loop needs nothing new. The inputs are
  the interpreter's terms replayed, not the compiled plant's, so a difference
  is the step's own rather than the loop's. inkamath writes the program and
  does not build it, as it writes a header: no C compiler is found or run. A
  hundred steps, and a term parts beyond a billionth of one plus the exact
  term, the fuzzer's tolerance; both are constants until a model asks for
  more. A term the interpreter cannot give is not compared, and where its
  terms stop being exact the program says from which index. `wild` in
  `test/compile/drift.ink` is the case that motivates it: a tenth recomputed
  at every step, exact in the interpreter and off by ten times more at each
  step compiled, parting at 9. It cost 254 lines of sources.
- `[done]` **Guards that flip.** The first step at which a compiled guard
  takes another clause than the interpreter's is its own event, reported
  before the values it makes part, with the guard's exact margin there:
  rounding at a margin near zero, a bug at a large one. A header built for
  checking keeps, beside each guarded sequence, the clause its latest term
  took, so the one a host builds is unchanged; the interpreter tells a
  listener of each guard it asks while the index is still bound, and the
  margin is measured there. Of the two clauses, the one tried first decided,
  so its guard's margin is the one given. A comparison's margin is the
  distance between its sides; that of `and` and `or` is the operand's that
  decided, or the nearer where both agree. Only a sequence's guarded general
  clauses are followed: a guarded cell, and a guarded value compiled where it
  is called, part only by their values. `brink` and `ledge` in
  `test/compile/drift.ink` were specified before it was built, at 1 exactly on
  the threshold and at 6 a trillionth from it, and answered so. It cost 206
  lines of sources.
- `[done]` **The transcript check**, `--check transcript.ink`: replayed, and
  each recorded answer the interpreter no longer gives shown, as recorded and
  as given, under its line. The parser the tests used moved beside the
  interpreter, and the command line's own reading of a transcript and its
  rendering of an answer went with it: 22 lines fewer for that, 57 more for
  the check.
- `[done]` **The fuzzer, reading `CompileC::Build`** rather than the header's
  text for the step's inputs and its sequences: 33 lines fewer, and the
  program it writes byte-identical. Not through `CheckC`, as first proposed:
  its random models are files, compiled as a session is, and as instances
  they would be compiled as a model is, so the path that found C71 would no
  longer be fuzzed by anything.
- `[done]` **Fused multiply-adds, said in the header.** A compiler may fuse
  `a*b + c` into one rounding where the definitions give two: GCC does in its
  default GNU modes on hardware that has the instruction, and the tests, built
  as strict C11, never see it. It was first taken for drift to forbid; it is
  most often closer to exact, and the manifesto asks a bound against the exact
  terms, not agreement between builds. What it does break is that agreement:
  a check built one way vouches for nothing built the other, a guard near its
  threshold included. So the header says to build the check as the step is
  built, and forbids nothing; `#pragma STDC FP_CONTRACT OFF` was weighed and
  dropped, since GCC rejects it under `-Werror` and honours none of it. Every
  expected header moved by those four lines of its first comment.
- `[done]` **The first entries of the paper conformance suite**
  (`MANIFESTO.md`): `test/compile/logistic.ink`, a logistic regression trained
  by gradient descent, and `test/compile/softmax.ink`, softmax and
  cross-entropy, each an instance held to the interpreter by `--check`, the
  training bit for bit over a hundred epochs. Neither needed a built-in:
  `exp` is its series under `lim`, and `log` Newton's method on it, which
  answers in part the question of a prelude written in inkamath. Writing them
  found the `tex` faults above, and that a paper's sample index, `i`, is the
  imaginary unit here: the entries use `r`. Also found: each `lim` is a
  function of its own, so `logistic.ink`'s four sigmoids are four identical
  ones; and the series behind `exp` needs more than its hundred terms past
  about 25, where a model would reduce its argument first.
- `[done]` **The limit of a sequence of matrices** is the limit of each cell, as the
  Deferred entry asked first. Its steps are measured by the largest of its
  cells', by the rule a number's stops by, and terms that change size are
  refused by name rather than stretched, as a single value would be in a
  difference. Specified in `test/data/spec/limits.ink`, its 4 limits failing;
  `lim mm` in `matrices.ink`, the refusal C39 worded, will move with it.
  Built as specified, every entry passing as written, now
  `test/data/limits.ink`, and `lim mm` moved as said. The distance is
  `Matrix::distance`, and `Convergence` asks it rather than `abs` of a
  difference. Compiled, a limit of matrices is a function that fills an
  array, declared where it is read as an inverse's is, its terms a window of
  arrays and its step the largest cell's; a matrix argument is passed as an
  array too, which power iteration needs. `test/compile/steady.ink` holds a
  chain's steady state and a matrix's dominant direction to the interpreter,
  within 4.4e-16 and 0. A limit's function shares no temporaries, so a product
  its clause writes twice is computed twice. 115 lines of sources.
- `[done]` **`i` a name a bound one shadows.** A paper's samples are `x_i` and its
  sums run over `i`, which was the imaginary unit here and could be neither:
  the lexer made it a number. It becomes a built-in name, as `pi` and `e`
  are, so a sum's index, a cell's row or column, a parameter or a sequence's
  index named `i` shadows it in its own scope. Unlike `pi`, it cannot be
  defined again. A setting choosing the unit's name was weighed and declined:
  the same text would mean what a mode said, and a printed answer would need
  the session to be read. Shadowing ends where its scope does, and an answer
  is printed outside every one, so its `i` is always the unit; inside a sum
  over `i`, the unit is a name defined outside it, `j = i`, as an engineer
  writes it. Specified in `test/data/spec/imaginary.ink`, 12 of its 16
  entries failing.
  Built as specified, every entry passing as written, now
  `test/data/imaginary.ink`, and no other recorded output moved: the lexer no
  longer makes a number of `i` alone, which costs the eight lines that did,
  the built-ins gain it as `pi` is gained, and a global definition of it is
  refused where every global definition is made. `2i` was a literal, the one
  juxtaposition the parser read, and inside a sum over `i` it was twice the
  unit where a paper means twice the index; it is now refused as `2n` is,
  `2*i` being the product either way. Nothing recorded wrote it.
  `test/compile/logistic.ink` now sums over `i`, as its paper does.
- `[done]` **Two more entries of the conformance suite**: a recurrent cell,
  `test/compile/rnn.ink`, and scaled dot-product attention for one head,
  `test/compile/attention.ink`, each held to the interpreter by `--check`,
  the attention's output matching NumPy's to the digits shown. Neither needed
  anything new: one index reads a cell of the column an affine map gives,
  `tanh` and softmax are `exp` under `lim`, and `i` indexes rows as on
  paper. Per head and per batch, attention needs a tensor of rank 3.
- **What the conformance suite asks next, by what each unlocks for what it
  costs**, sized against what landed (one index 57 lines, matrix limits 115,
  a compiled `lim` 167, guard flips 206, `tex` 237):

  | | cost | to decide first | unlocks |
  |---|---|---|---|
  | momentum, Adam, LQR by Riccati | models only | -- | three entries |
  | one function for identical `lim`s | ~15 lines | -- | smaller headers |
  | `exp` past 25 | no interpreter code | a prelude of `exp`, `log`, `tanh` in inkamath | activations anywhere |
  | `tex` of cells and models | ~100 | how bounds and a model's head are set | layers read against the page |
  | guard flips in cells | ~100-150 | -- | ReLU watched at its threshold |
  | data from files | small | a tool, or `use data.csv` | real datasets |
  | several rates | ~250, compiler only | -- | the decimator |
  | differentiation, forward | ~300-450 | its form, its rule under `lim` | the training half |
  | tensors of rank 3 | the matrix core rewritten | many | attention per batch and head |

  The order taken: the small ones whenever; differentiation, specified first,
  for it unlocks most per line; then `tex` of cells and models, through which
  gradients will be read; then flips in cells; rank 3 last, when attention
  per batch is what is missing, since one sample or one head is writable now.
  Differentiation is planned as a transformation of the parsed tree into
  derivative definitions, evaluated exactly by the interpreter and compiled by
  the compiler as any definition is, not as the dual numbers `MANIFESTO.md`
  sketched: a second number type through every template is where C44 and C65
  broke, and the compiler could not have used it.
- `[done]` **Differentiation, `grad_(x = point) expression`**, the partial
  derivative of an expression with respect to a name at a point, bound as a
  sum binds its index: the point read where `grad` is written, the name only
  in the expression. A call's form was dropped, since its first argument would
  have been read in a scope its second opens. A derivative at a parameter's
  point, `df(x) = grad_(t = x) f(t)`, is the derivative as a function. A
  single value with respect to a matrix is shaped as the matrix, a matrix with
  respect to a single value as itself; a Jacobian is refused, and so is
  differentiating through an instance, for now. What would be silently 0 is
  refused: an expression that never reads the name, and a definition that
  reads the global of the name, which `grad`'s name does not reach. A
  definition in cases takes the slope of the clause that holds at the point,
  one side of a threshold, so the order of guards decides a ReLU's slope at
  0; a clause that holds only at the point (`==`) is refused, as are `floor`
  and a comparison where they jump, and a power whose derivative is infinite.
  An exponent that changes with the name is refused unless the base is the
  built-in `e`, since any other needs a logarithm. A matrix power is a product
  of factors that do not commute, its inverse differentiated as one. A
  limit's derivative is the limit of its terms' derivatives, taken until both
  have converged, and refused if the derivatives diverge, as Newton's square
  root does at 0; where they converge too slowly near the point the rule is
  wrong, and nothing at the point can tell (C72). Two reviews against the
  first draft of `test/data/grad.ink` found its ReLU written in the order that
  clears the guard, two inexact answers that print exact, and the scalar rule
  applied to matrix powers.

  **Not the transformation planned.** What decides it is that a value's
  shape is known only once it is evaluated: a power, a quotient, the Jacobian
  refusal and a gradient with respect to a matrix each take one rule for a
  single value and another for a matrix, so a tree of derivative definitions
  would have needed a node choosing by shape at run time, besides an error
  node for the refusals, a limit walking value and derivative together, and
  the per-cell assembly of a gradient. The guards were never the obstacle: a
  derivative definition can copy them. Nor would the definitions have
  compiled as any definition is, as planned: the compiler inlines what it
  compiles and refuses a sequence with parameters, which `dp(x)_k` is.
  `derivative.hpp` evaluates the expression forward instead, each value
  carrying one part per set of the grads it is under: a grad inside a grad is
  a second derivative, and mixed partials come out of the same products. The
  parts are values of the one number type, so nothing goes through the
  templates C44 and C65 broke on. Compiling `grad` will be the same rules
  over the compiler's cells, whose shapes are known while compiling: the
  static refusals stay refusals, and a check that needs the values, a jump
  or a zero base, gives NaN as a limit that does not converge does. It is
  about 830 lines against the 300-450 estimated above, much of it a second
  copy of how a call chooses its clause and how `lim` walks its terms,
  beside `Reference`'s; the two can drift, and a change to either is a
  change to both.
- `[done]` **A sign that begins an element.** Inside a matrix literal or an argument
  list, a `+` or `-` with a space before it and none after it begins the
  next element, as in MATLAB: `[1 -1]` is two numbers, `[1 - 1]` and `[1-1]`
  one. C52 left the sign binary and made printing round-trip instead, which
  kept what was printed re-enterable but not what was typed: the RNN
  conformance model's `[1/2 -1/4; 1/4 1/2]` lost a cell unseen.
  `series.ink`'s `[sum_(k=1)^3 k -1]` is the one golden that moved, from
  `5` to `[6, -1]`. Specified in `test/data/sign.ink`, which passed as
  written. The parser knows it is in a list by the innermost bracket open,
  so parentheses, an index and a series' bounds read a sign as before.
- `[done]` **`exp`, `log` and `tanh` in the prelude**, beneath the session as `ceil`
  and `mod` are, so that an activation and a log loss need no definition of
  their own. `exp` is the built-in power of `e`, accurate past the 25 where
  its series ran out of terms and differentiable by `grad`'s rule for `e`;
  `log` a series in `(x-1)/(x+1)`, fast between 1/2 and 2 and reached by
  halving or doubling; `tanh` from `exp`. Specified in
  `test/data/prelude.ink`, with a logistic regression trained by `grad` on
  its log loss, held to the gradient written by hand and to the weights
  NumPy gives; it passed as written, and no golden moved.
- `[done]` **Guard flips in cells.** `--check` reported the first step whose compiled
  guard took another clause only for terms chosen whole; a term chosen cell
  by cell, a ReLU on each cell, showed only the values that followed. Each
  cell's guards are asked alone, so each cell is reported by its place:
  `rift` in `test/compile/drift.ink` is `brink` cell by cell, and its report
  is specified in `test/CMakeLists.txt`. The interpreter's hook is told the
  cell; a compiled term chosen by cells keeps `name_clause_[cells]`, each
  cell's chain of guards beside its chain of values; and `--check` makes one
  entry per cell, so the report of a flip is the one it was.
- `[done]` **Momentum, Adam and an LQR gain** in the conformance suite,
  models only: `test/compile/momentum.ink` and `adam.ink`, the logistic
  regression trained with each, the moments sequences beside the weights;
  `lqr.ink`, a double integrator's gain by its Riccati recurrence, exact
  over a hundred steps. Each instance is held to the interpreter by
  `--check`, and the hundredth term of each to NumPy's, to every digit
  printed. None needed a change to the language.
- `[done]` **One function for identical limits.** Each `lim` the compiler
  walked was a function of its own, so `logistic.ink`'s sigmoid, applied
  cell by cell, was four identical ones. A limit's function is named once
  its text is known, and one written again is the same function;
  `check_gate_one_limit` holds the logistic model to one.
- `[done]` **A line bounded by its depth**, not its length, so that data is
  a file of definitions written from anywhere: C20's 1000 tokens refused a
  matrix of 160 numbers, though a flat literal is as deep as one cell. The
  parser counts its nesting, every recursion of it passing through
  `ParseSimpleExpr`, and each node its depth as it is built, refused past
  1000 while what was built can still be destroyed, which answers C20's
  reason for a token count. 100000 tokens bound what a refused line costs.
  `depth limit` in the tests holds both bounds and both extremes; the
  README shows a matrix written from Python. A Python binding, later, would
  read other formats; until then, writing `.ink` is the binding.
- `[done]` **Several rates**, as `MANIFESTO.md` sketches them: rates are index
  arithmetic, not a new kind of thing. A sequence reading another at
  `x_(a*m + b)` samples it, so its period is `a` steps of the input's; one
  reading at `y_(floor(n/a) - d)` holds a term. The compiled step stays
  one, the input's, and computes a slow term on its ticks, the first at
  which every sample it reads exists: `y_m` reading `x_(2*m - 1)` and
  `x_(2*m)` at step `2*m`. A hold that reads a term before its tick is
  refused, naming the delay that fixes it, as a loop without a delay is; a
  read at any other index, or at another period than the sequence's, is
  refused by name. `--check` holds a slow sequence at every step to its
  latest term. Specified in `test/compile/rates.ink`, a decimator and two
  holds, and passed as written but for `y_0`: written `x_0`, it met an older
  limit, that no base clause reads a term when compiled, so it is 0. The
  refusals are in `test/cli.cmake`. The compiler keeps every index in the
  input's steps: a slow sequence's base terms are keyed by the step that
  computes them, a sample's lag and a hold's place in the window are set once
  the phase is known, and a hold whose term's tick falls on alternate steps
  reads one of two places by the step's parity. A slow sequence is refused,
  for now, with a guard, by cells or without a base clause; a ratio of rates
  that is not whole, `x_(3*m)` beside `y_(floor(2*n/3))`, is not taken.
- `[done]` **A base clause that reads a term**, as `y_0 = x_0` starts a filter at
  its first sample. The interpreter always took it; the compiler refused it,
  "a term read outside a general clause", as it compiles base clauses first,
  for their shapes, and reads only an index less a constant. A base term is
  computed at its own index, so a term it reads is a constant step back from
  there: a lag like any other, refused where it is ahead or where the
  sequence read has not started. Found writing `rates.ink`, whose decimator
  had to start at 0. Specified in `test/compile/seeded.ink`, the decimator
  restored, and two refusals in `test/cli.cmake`; it passed as written. A
  base clause compiled notes each term it reads and where, and the lag is
  set with the steps: after the phase of a sequence at another rate, which a
  base's reads move as its samples do, and after where each sequence starts.
- `[done]` **What several rates left refused**, decided by what the interpreter
  answers, for the models that ask: a decimator, an interpolator, a cascade
  whose slow outer loop is clamped. A slow sequence takes guards, its clause
  set on its ticks and kept between them as its terms are, 0 before the
  first; refused, the cascade has no clamp. Until now a sample in any guard
  was refused as one "where a term is computed again", since a guard's reads
  are deferred as the right of an `and`'s are, and a sample there is a
  sample. It takes cells, each cell's samples at a constant offset as a
  whole term's are; a frame, `x_(2*m + j)`, whose sample moves with the
  cell, is one too, since a cell's place is a constant there. It needs no
  base clause, since a
  stateless decimator would otherwise write its first term twice: a term is
  computed at the step of its latest sample, so without one the phase is the
  largest `b`, and the first term is the first whose tick is a step and
  whose samples exist; a closed form's exist at every index. A sequence with
  no base clause is sampled from its window, and refused where the window
  cannot hold a term the interpreter has, closed form or not. A hold in a
  term computed again is the same hold that much earlier, `b` plus the lag.
  The cascade never needs one: `f_0` is a base clause, so the window holds
  every `e_(n-1)` that `f` reads; but C71 writes `e` computed again before
  the starts show it is not needed, and a hold there refused the cascade. A
  sample is computed again only where a slow sequence
  is read back at the input's rate, which its rate refuses. A hold of a
  term the step never computes is NaN where the interpreter has none
  either, and refused where it could give one: below a guarded sequence's
  base clauses, as at the input's rate, and before the first tick of one
  whose samples could give it. Computing that term again, as C71 computes
  one at the input's rate, was rejected: it would compute samples again,
  for slow sequences that sample closed forms and nothing else. Two things
  stay refused. A ratio that is not whole, `y_(floor(2*n/3))`, is still an
  index of neither form: two ticks in three steps are a pattern rather than
  a period, and the rational resampler a paper draws first interpolates,
  faster than the input whose step this is. And a slow sequence read by
  another, by a sample or a hold, is refused naming the fix, a hold at the
  input's rate sampled, which says the same exactly; composing periods and
  phases is a second rule for what one already says, and waits for a model
  of three rates that the fix makes unreadable. A hold says the same only
  where its `a` ticks of its reader are one period of the sequence it
  holds; at another period the term it names falls further behind at each
  tick, which no hold says, and it is refused by its rate, as at the
  input's rate. Such a hold was compiled wrong (C74). `--check`
  holds a slow sequence from the step that computes its first term, and
  reports a flip at the step that computes the term taking another
  clause, with that term's margin, cell by cell for one
  by cells; reporting the term's index instead was rejected, as every other
  line of the report counts steps and the clause quoted names its own
  index. Specified in `test/compile/cascade.ink`, a servo whose outer loop
  runs every fourth step with a dead band and a clamp; in
  `test/compile/decimate.ink`, an anti-aliasing filter, a decimator, a
  hold, a linear interpolator, a second stage through the hold, and a
  strided convolution; and by `sill` and `seam` in `test/compile/drift.ink`,
  `brink` and `rift` every second step. The refusals and the reports are in
  their headers, and each joins the suite with the compiler's half. Writing
  them found that `--check` holds `c_n = x_(n-1)` at 0 to a term read from
  `x_(-1)` where the instance's input is a closed form, which no step has,
  and it parts (C75); the models feed a stream that starts at 0, as a
  host's does. Passed as specified, but for these. A sample of a sequence
  with no base clause is not computed again before its window holds the
  term, as C71 computes one, even for a closed form: its lag is the phase
  less `b`, known only once every sample is, after the term computed again
  is written, and a closed form that reads another inherits it. It is
  refused, `y_0 reads k_-1, before the step computes k`, which no model
  meets. A hold before the first tick of a sequence with no base clause is
  refused where any of its samples exists, as one path through its guards
  may need no more. `stride`'s input, `3 - n/2 + (-1)^n`, never clamped its
  second channel; `3 - n/2 + (-1)^n*(1 - n/8)` clamps the first from its
  fourth term and the second from its sixth, and its report was worked out
  by a simulation in Python of the exact terms and the doubles, not
  recorded. `frame` in `decimate.ink` holds the frame; `x_(2*m + j - 1)` is
  still of neither form, its constant being two terms. A hold's period is
  checked with its reader's, after compiling, which is where the refusal of
  `v` in `compile_rates_refused` now comes from, in the same words. The
  refusals are in `test/cli.cmake`, with C74's.
- **Steps and tolerance as options**, when a model asks: a hundred steps is
  short of what a slow filter settles in, and a billionth is loose for a
  well-conditioned step.
- `[done]` **The parts and modulus of a complex number**: a frequency
  response |T(e^{jw})| cannot be written, as `abs` refuses a complex value and nothing
  gives its real or imaginary part. Decided: `re(z)` and `im(z)` built in,
  and `abs` extended in the prelude:

      abs(x) | im(x) < 0 or im(x) > 0 = absz(re(x), im(x), 2^ilogb(max(abs(re(x)), abs(im(x)))))
      abs(x) | x < 0 = -re(x)
      abs(x) | x >= 0 = re(x)
      absz(a, b, s) = s*((a/s)*(a/s) + (b/s)*(b/s))^(1/2)
      absz(a, b, s) | 1/a == 0 or 1/b == 0 = 1/~0

  **re and im** are built in as `floor` is, a definition the interpreter
  starts with whose body is a node the language cannot write: nothing it
  has takes a complex number apart, a comparison and `floor` refusing one
  and `'` not conjugating. floor's node becomes the one node of the three,
  carrying its name and the function it applies to each cell: floor, and
  `Number`'s `real` and `imaginary`, which exist. That rename is a
  refactor commit of its own, first, every golden and header
  byte-identical. Cell by cell of a matrix, as `floor` is, for nothing.
  Of an exact number, itself and an exact 0; of
  a complex one, a part of a pair of doubles, inexact and marked as the
  pair is. Named as the paper writes Re and Im, lowercase as every name
  here; a session may define either again, as `floor`, and the prelude
  keeps its own. `tex` shows `\operatorname{re}(z)`, as any call, not
  the paper's `\operatorname{Re}`, and the node as that call: `tex ?re`
  is `\operatorname{re}(x) = \operatorname{re}(x)`, where floor's is
  `\lfloor x \rfloor`. `?re` is `re`, as `?floor` is `floor`: a built-in
  has no written form, and the node is not asked.

  **abs** is the root of re^2 + im^2, each part first divided by s =
  2^ilogb of the larger, exactly, so where a double holds the squares it is
  the double sqrt(a*a + b*b) gives, and elsewhere it neither overflows nor
  underflows: unscaled, `abs(3*10^200 + 4*10^200*i)` is inf and
  `abs(3*10^-200 + 4*10^-200*i)` 0. |3+4i| is the double 5, all of it, so
  printed `5`, but inexact, `frac` refusing it: a complex number is a pair
  of doubles, and exact ones are not this item. |1+i| is the correctly
  rounded root of 2, the double `2^(1/2)` is (C153). An infinite part's
  is inf, as hypot's is, where the scaling would divide inf by inf: a
  clause of its own. A real number's is exact, as before. The guard reads `<` and `>`, not `<>`: a whole matrix
  `<>` 0 answers 1, so abs of a matrix would take the complex clause, where
  `<` refuses it in abs's words, as now.

  **The real clauses take re(x).** A real value's derivative may be
  complex: T(e^{iw}) = 1/(e^{iw} - 1/2) is 2 at w = 0, and T' = -4i, so the
  clause `x` makes the slope of |T| -4i, which `grad_(w = 0) H(w)` answers
  today; |T| is real and peaks there, its slope 0, re(T'). C201, a
  defect of today's abs found writing this spec, which this fixes.

  **grad.** Of a variable at a real point, a part's derivative is the
  derivative's part, and abs's follows from its clauses: re(conj(z) z')/|z|
  off 0, and re(z') at 0, as the order of guards gives a real abs's slope
  at 0. At a complex point neither part is complex differentiable, so
  where its argument moves and a grad's point has an imaginary part,
  `re has no complex derivative at z = 1+i`. abs is refused in its own,
  `abs has no complex derivative at z = i`, not in those of the im its
  guard reads first, in at most 3 lines; the prelude's abs alone, as a
  session's function whose guard reads a part may be holomorphic where
  the guard holds, so is refused in the part's words, not its own. A
  holomorphic function keeps its derivative at a complex point, as now.

  **The prelude's refusals.** `hurwitzt`, `hurwitz`, `schurcohn` and
  `rhoe` read abs only to refuse a complex argument, as the prelude's
  comment says; abs taking one, they read `max(x, -x)`, which refuses in
  the same words naming the same number. Tried: with that alone, every
  golden passes unchanged.

  **Compiled.** Every compiled value is real, a complex literal being
  refused, "a complex number", so `re(x)` compiles to x and `im(x)` to 0:
  abs's first guard folds to 0 and its clause away, as a constant guard
  does, so `inkamath_prelude.h` and the expected headers do not move. A
  frequency response does not compile: `e^(i*w)` is refused as now, which
  a case of `test/cli.cmake` added with the implementation holds,
  `y_n = abs(e^(i*w*n))` refused "cannot compile y: a complex number".

  Rejected:
  - **`real` and `imag`**, MATLAB's, NumPy's and C's: longer, and not the
    paper's.
  - **A name of its own for |z|**, `cabs` or `mag`: a second way to say a
    real number's abs, which the paper writes |x| as it writes |z|.
  - **abs native**, hypot: abs is the prelude's, redefinable and read by
    the compiler; within an ulp of the root above, and not readable.
  - **im as re(-i*z)**, one node: 0*inf is a NaN part, and im of an exact
    number would be inexact.
  - **re and im refusing a matrix**, as abs does: abs refuses because a
    function of the prelude with guards does; a built-in node is cell by
    cell for free, as `floor` is.
  - **conj**, `re(z) - i*im(z)`: a line a session writes as `round` is,
    which the magnitude does not need, and which would ask for a quote
    that conjugates, decided against (README section 2).
  - **arg**, the phase of a Bode plot: atan2, which the prelude does not
    have; an item of its own when a model asks.
  - **abs's real clauses unchanged**: the slope -4i above.
  - **A node of their own**, one for both with a flag: a class, its three
    visitor slots and a copy of `Matrix`'s map for floor, about 50 lines
    of C++ where sharing floor's is about 30.

  What moves: trig.ink's `?abs`, which lists the new clause, and its
  `abs(i)`, 1 where it was refused, with the comments over them; the
  comments of charpoly.ink and eig.ink that say hurwitz and rho refuse in
  abs's words, the same words now max's. No header. README's section 1
  lists `re` and `im` with `floor` and says abs takes a complex number,
  with the frequency response as its example.

  About 34 lines: 3 of the prelude and 8 edited, 1 of its comment; about
  30 of C++, the refactor 4, the node's name and function, the
  definitions 4, grad 13, the parts' rule 6, the complex point 2, floor's
  jump asked of floor alone 2 and abs named 3, the compiler 6 and `tex`
  3. 15,327 lines at ebf47fd, by `wc -l include/inkamath/*.hpp src/*`.
  Past 51 the implementation stops and reports.

  Specified in `test/data/spec/cparts.ink`, 53 entries replayed by the spec
  suite, 40 failing by design, those passing being definitions and
  `clear` echoing themselves, the refusal of a matrix, a real number's abs,
  H(0) and a call's `tex`: the response, its parts and slopes exactly
  with sympy, each rounded to 9 digits with mpmath and none within 0.04 of
  a digit's half; the range and the marks in Python's doubles through the
  scaling above.

  Built as specified: the spec is the golden `cparts.ink`, 53 entries, and
  the compiled frequency response a case of `cli.cmake`; README's section
  1 lists `re` and `im` and shows |T|. Departures: the node is
  `BuiltinExpression`, and `Matrix`'s `floor` became `Cells`, a cell's
  function applied to each, which the compiler's constant floor calls
  too; a part is refused where it moves with a grad whose point is
  complex, in that grad's name (C202); the prelude's abs is known by its definition in the built-in
  scope. 37 lines landed against about 34: the refactor 7, where 4 were
  estimated, the node's constructor and `Cells`; the prelude 3, its
  comment rewritten in place; the definitions 4 with a line of comment;
  grad 15, the parts' rule and floor's jump 6, the complex point 5 and
  abs named 4; the compiler 5 and `tex` 3. 15,364 lines in all, after
  `cparts`.
- `[done]` **A sequence with parameters read at a constant index, compiled.**
  `--compile` refuses `f(x)_K` anywhere but under `lim`, "a sequence with
  parameters", where the interpreter answers: `det` by Faddeev-LeVerrier in
  `test/data/signatures.ink`, an MPC horizon of a few steps of projected
  gradient, a window of backpropagation through time, Kahan's §10
  `r(x)_128`, and shooting with a fixed number of iterations, for a step
  whose time must be bounded (`MANIFESTO.md`). The plan said a loop of K
  iterations.

  Decided: written out, as a sum with constant bounds is, not a loop.
  `f(args)_K` is compiled where it is read, as a call is, its terms filled
  as the interpreter fills them (`Reference::Filled`): from the lowest base
  clause up to K, by the stride the reads give, each term by the clause the
  interpreter takes at its index, the index bound as a constant as a sum's
  is, so a guard on it folds and `fc`'s `/m` is a number. Each term the read
  reaches is a temporary of the step, by C140's mechanism, and the term read
  is written where it is read, as a call's value is; a term is compiled once
  for the code and parts of its arguments, so `fm`'s two reads of the term
  before do not double at each term, and `cp`'s four cells share `fc`'s
  terms. A term that reads another sequence with parameters reads it the
  same way, so `fm` and `fc` fill each other. A term the fill computes and
  the read does not reach is not written, a temporary nothing reads being a
  warning the step is built without; a term the fill cannot compute is
  refused only where a read reaches it, as the interpreter's recursion is.

  The index is any that folds to a whole number where it is read: a literal,
  a constant, a cell's place (`cp`'s `j - 1`), a size a signature binds
  (`det`'s n), a parameter of a function compiled where it is called. A
  model's parameter read there is compiled in, as one bounding a sum is, and
  the header says so. One that reads the step's index or a term moves, and is
  refused: computed from its base at each step, it would make a step's time
  grow with n. At most 1000 terms from the lowest base, as a sum has, in the
  words of the interpreter's fill. Without a base clause a term is compiled
  where it is read, and its reads nest as calls do, 64 deep.

  A term being step code, what the step does carries through it: matrices
  and tensors, guards on cells at run time, NaN where the interpreter
  refuses, `--float`, and grad's parts, so `grad_(B = A_n) det(B)` is the
  cofactors through both sequences, which is backpropagation through time's
  case too. A read with other arguments, `w(x/2)_(k-1)`, nests as a call
  does, where a limit refuses it. The operations are the interpreter's in its
  order, so `--check` holds a term within 0 where its arguments are the
  interpreter's; guards inside a term part only by their values, as a
  function's do.

  Refused, by name: an index that is not a constant, "a sequence with
  parameters read at an index that is not a constant", as a sum's bounds are;
  past 1000 terms, `r_1001 is 1001 terms from its base, and a step writes
  out at most 1000`; below the lowest base, in the interpreter's words, `q has
  no clause for index -1`; a term reading itself, `z is defined by itself`,
  and one ahead of it, `p_(...): a term after the one being computed`, in a
  limit's words, where the interpreter runs out of depth; reads nested past
  64, in a call's; an index that is not whole, in the interpreter's; and a
  read in a limit's terms, "a sequence with parameters in a limit's terms,
  for now". A limit's function has no temporaries, so its terms would be
  written whole, K deep and for `fm` doubling at each. *Temporaries in a
  limit's function* is the same mechanism, a limit's own list, and lifts
  that refusal when it lands: Newton's shooting under `lim` waits for it,
  while shooting by a fixed number of secant steps is two nested reads and
  compiles now.

  Rejected. The plan's loop, a function of the header's beside `lim`'s, the
  terms in rolling locals and K a counter: its one gain is a header of
  constant size in K, and a K the host may set. It would share `lim`'s walk
  and its limits: one sequence reading only its own terms, so `det`'s `fm`
  and `fc` refused; no parts, so no gradient through it, neither `det`'s nor
  backpropagation's; no tensor, no guarded base clause, a matrix only where
  temporaries exist, and a row read at the term's index not a constant place,
  which a window of inputs is. Each would be lifted by a second copy of what
  the step does. Kahan's 256 terms are 256 lines; when a target's code size
  asks, a loop is an entry of its own. A term compiled as a call, by
  recursion, was rejected as it nests a call per term: Kahan's 128 past the
  64, and the C++ stack at depth K. Every term the fill computes written,
  for the warnings. A K that moves, for a step's time. And refusing other
  arguments, as `lim` does, where the interpreter answers and a call's form
  exists.

  About 65 lines of sources, in `compile.hpp`: the read, its index and its
  refusals 12, the arguments bound as `Call` binds them, shared, 5, the
  terms kept by their arguments' code 8, the fill by the interpreter's stride
  12, a clause chosen at an index, `Chained` given one, 15, a term read from
  itself or ahead 6, the rest 7; and `Reference::Stride` made public. The
  temporaries nothing reads, `Temporaries` drops already. Sized against
  compiled `lim`, 167, of which it shares only words, and C140, 11. Past 97
  the implementation stops and reports. 15,055 lines in all at 75c03ae.

  Specified in `test/compile/iterates.ink`: `poly`, `det` and the
  characteristic polynomial as `signatures.ink` writes them, with grad's
  cofactors, exact, in double and in float; `plan`, an MPC horizon whose
  count is a parameter compiled in; `graph`, Kahan's 128 roots then 128
  squares, held to the interpreter and with a header excerpt; `gaps`, reads
  that skip terms; and a file of eight refusals. Wired with the
  implementation: the four checks and their reports in
  `test/CMakeLists.txt`, `poly` in float, `graph`'s estimate by its form; the
  excerpt and `plan`'s `cannot change: iters.` in `test/cli.cmake`, with the
  refusals. Nothing that compiles today reads such a sequence, so no golden,
  header or report should move. README's paragraph on the compiler gains a
  sentence, and the closing note of `test/compile/signatures.ink`, that `det`
  stays refused, goes.

  Built as specified, with the review's rulings: `thirds`, `poly` at
  x_n = ~(n/3), where the order of the operations shows, held within 0;
  C180, a compile time growing with the square of a step's temporaries,
  fixed first; and `graph`'s first line corrected by hand, its terms
  inexact from 0, the interpreter's power of 1/2 being inexact wherever it
  is taken. Departures. A term is kept by the list of temporaries it is in,
  its definition, its arguments' code, parts and all, and its index, so the
  terms of a global's list go with it, and the NaN-aware compile, a
  compiler of its own, starts afresh. A term is a temporary once a term or
  the fill reads it, the one read outside them written where it is read. A
  read's arguments are always computed once, by `SharedAll`, which `Call`'s
  binding shares. The compiler always fills from the base, where the
  interpreter fills only past its depth; the terms are the same either way.
  A refusal of a limit in a local's call, which the limit's own path had
  already taken, went. 68 lines of sources against 65 planned,
  `compile.hpp` 66 and `reference.hpp` 2, and C180's 16: 15,160 lines in
  all, after `fixes13`. By its review, C185 to C188, 13 lines, and a
  ruling: a term the fill cannot compute was refused where no read reaches
  it, `g(x)_k = f(x)_k` filling g_1, which reads f_-1, so `g(x_n)_4` was
  refused where the interpreter answers; such a term is now kept with its
  reason, refused where a read reaches it, and `Chained` puts its index
  back however it ends, in 17 lines, held by `unread`. 15,190 lines in all.

  Written out, a step's code grows with K and with its term. Measured with
  `gcc -O2` on a program stepping it, `plan` widened to d moves and K
  iterations is 58 KB of header and 0.9 s at d = 2 and K = 100, 0.55 MB and
  5.0 s at d = 6, and 2.0 MB and 11 s at d = 10; at K = 1000, 0.59 MB and
  6.8 s, 5.8 MB and 76 s, and 22 MB and 400 s. Backpropagation through time
  by 32 nested calls, hidden 8, the review measured at 3.2 MB and 109 s;
  grad being forward mode, a part for each cell of the weights, it holds
  for small networks alone. Those are the threshold a loop, an entry of its
  own, is to be weighed against.
- **Block literals compiled**, `[A, B; C, D]`: refused as "a matrix built
  from matrices", which stops the doubling algorithm's three iterates packed
  into one term.
- **`tex` of `==`, a negative fraction and `exp`**: `==` is set as `=`,
  `\frac{-1}{2}` should be `-\frac{1}{2}`, `\operatorname{exp}` should be
  `\exp`; and one tensor literal refuses a whole model's `tex`.
- **A limit exact at an exact fixed point**: k-means reaches `C_3 == C_4`
  exactly, yet `lim C` is inexact and `frac lim C` refused.
- **Matrix `==` and `<>` compiled**, refused as "a comparison of matrices".
- **`--check`'s hundred steps as an option**, with `lim`'s.
- **A cumulative sum compiled as an accumulator**, `sum_(t=1)^n e(t)`:
  refused as "a sum whose bounds are not constants", where a regret or a
  running loss is exactly that (Reddi et al. 2018).
- **The forward form `x_(t+1) = ...`**: refused as "t is not defined", so a
  paper's update must be shifted to t-1 by hand, where transcriptions slip.
- **`--check` says how far a term parts**, not only where it first does: the
  largest difference and where, as MANIFESTO asks of how a difference grows.
- **`atan` in the prelude**, written as `exp` and `log` are, `asin` and
  `acos` from it: Kahan's angle formulas cannot be written without.
- **An instance named again**, `a = thm1.adam`, then `a.x_1`.
- **A constant folded from an approximated number, listed**: `--compile`
  folds `(2^4000 > 2^3999)` to 0.0 and `3^3000/3^2999` to NAN silently,
  where the interpreter marks them, and its header lists nothing.
- **`--check` measures relative to the term**: its tolerance, units and
  estimate are absolute below 1, so -1e-30 against 1e-30 passes "within
  2e-30" and 0 against 1e-200 "within 1e-200" (Goldberg 1991). Count units at
  the interpreter's own term, the pass rule unchanged.
- **`--float` and a constant folded in double**: a constant computed from
  inexact numbers is folded in double and written as a float, so a machine
  epsilon loop puts 2^-53 into a float header and checks "within 0". Refuse
  it, or name it in the header.
- **`--check` names the parameters the target rounds**, so a parameter no
  float holds (Goldberg's 4.53) is not read as the step's own error.
- **Refuse a repetition that adds nothing, in every context**: an index name
  repeated in a size (`x_n[j<=2, j<=2]` in a model's input,
  `M[j<=n, j<=n]` in a signature), a parameter or default written twice, a
  size stated twice. Accepting such a repetition is an artifact of the
  implementation, not a meaning, and hides the slip it usually is (`j, j`
  meant `j, k`). Low priority.
- **Temporaries in a limit's function**: C140 shares a step's nested calls,
  but a limit's terms are still written out whole, so a Riccati limit through
  `ric(ric(P))` is 163 MB of header. About 25-35 lines; moves headers whose
  limits multiply matrices, not their values.
- `[done]` **A definition as LaTeX**, `tex ?name`, a word at the start of a line as
  `frac` is, and reserved as it is. It renders what was parsed, not what was
  typed: the clauses for one index a line each, then those for every index,
  guarded or not, as one definition in cases, the clause that always applies
  last, `otherwise`. A name of one letter is itself, a Greek one its letter,
  any other one italic word; a function of more than one letter an
  operator's name; a product a thin space, or a dot before a digit. What has
  no form on paper, `~`, it refuses rather than drop, and a model it refuses
  for now. Specified in `test/data/spec/latex.ink`, 20 of its 44 entries
  failing, every one of them a `tex` line.
  Built as specified, every entry passing as written, now
  `test/data/latex.ink`. Its own header, `latex.hpp`, walks the parsed tree;
  an index is set tight, `s_{n-1}`, as a subscript is, and an operand is
  parenthesised by how loosely it binds. A definition by cells it refuses as
  well, for want of a paper's form for its bounds. 237 lines of sources,
  most of them the one function that knows each node.
  Transcribing a logistic regression then found two faults, now entries of
  `latex.ink`: a cell of a term rendered `p_{n-1}_{r}`, two subscripts in a
  row, which LaTeX rejects, and is now `p_{n-1,r}`; and a sum that ends a
  product was bracketed, `\eta\,(\sum ...)`, where a paper is not.

- `[done]` **`tex` of cells and of models**, so that a layer is read against the page
  it came from. A definition by cells is set by its entry at a row and a
  column with their range after it, `M_{j,k} = j + k, \quad 1 \le j \le
  2,\ 1 \le k \le 2`, its guarded clauses in cases as a sequence's are; a
  clause for one cell, and a matrix written whole, are lines of their own
  before it, as base clauses are. A column has one subscript, and a term's
  cell shares the term's, as reading one does. A model is its head, its
  defaults and its inputs with their index, then a line per definition,
  indented as `?` shows it, each set as `tex` sets it alone; a model inside a
  model is refused for now. An instance's input keeps its index, where
  `gain(x_n = n)` printed `gain(x = n)`. Specified in `test/data/tex.ink`,
  which passed as written; `latex.ink`'s model refusal is now the model set. Writing it found the RNN conformance model's `W` read as `[1/4, 0;
  1/4, 1/2]`: `[1/2 -1/4; ...]` subtracts, as C52 says a literal does.
- `[done]` **One index is a row.** `a[1]` is the first row of `a`, a 1xn matrix that
  keeps its orientation, so a column vector's element is `v[2]`, a row
  vector's is `r'[2]`, as a paper writes the transpose, and a definition by
  one index, `w[j<=3] = j^2`, is a column. Specified in
  `test/data/spec/vectors.ink`, 32 of its 39 entries failing. Two other
  readings were weighed and declined. One index as an element of whatever
  has one row or one column would leave a slice needing a notation of its
  own, and give one shape two rules. A row without orientation, as NumPy's,
  makes `a[1][2]` the cell, but accepts `x*A*x` for `x'*A*x` and `x*y` for
  `x'*y`, the transposes a paper writes and a size check would have missed:
  a missing transpose, among the commonest slips in a transcription, would
  answer rather than be refused. It also adds a kind of value beside numbers
  and matrices. The two differ only from rank 2 to rank 1, so a tensor's
  slice, a matrix of a rank-3 tensor, is the same under either. What this
  costs is that `a[1][2]` is not the cell `a[1,2]`, and the spec says so.
  Brackets now index only what they touch: with one index allowed,
  `[b [2]]` would otherwise answer row 2 of `b` where two blocks were
  written. A space before them separates blocks, as it does everywhere in a
  literal, which gives back the form C40 had to turn into a diagnostic.
  Built as specified, every entry passing as written, now
  `test/data/vectors.ink`; two goldens of `matrices.ink` moved, `a[1]` from
  an error to `[1, 2]` and `[a [3 4]]` to its two blocks. One rule took the
  place of two: a bracket that touches what it follows indexes it, named or
  not, in a literal or not, and indices and quotes chain in any order, so
  `r'[2]` reads. The distinction between a name and anything else inside a
  literal, and the depth it was counted with, are gone. With them goes one
  form: two literal blocks touching, `[[1 2][3 4]]`, were two blocks and are
  now an index of the first. It was only ever a way to try the block syntax
  quickly, a block being a name on paper, so `README.md` now shows blocks
  named, with a comma, and what a touching bracket does. A definition by one
  index is a column whose one column has a name nothing can be written to
  read, so the clauses for cells, and the compiler, needed nothing new; the
  compiler reads a row out of a matrix, and `test/compile/delay.ink` holds a
  delay line defined by one index to the interpreter. 57 lines of sources.

- `[done]` **`lim` compiled.** A limit whose arguments are constants and
  whose terms read no parameter is folded, by the interpreter, as any
  constant is. Any other is a function of its arguments emitted beside the
  step, which walks the terms from the highest base clause, or from 1 with
  none, and stops by `Convergence`'s rule, its constants included; where the
  interpreter would say the terms do not converge, it answers NaN, the
  step's word for what the interpreter reports, rather than the status field
  `MANIFESTO.md` sketched: a NaN reaches every term that reads it, a field
  only the host that looks. A sequence with parameters is compiled where a
  limit walks it, and only there. Its terms may read their own earlier terms,
  as far back as its base clauses reach, its parameters, its index and the
  model's; another sequence's term is refused, as in the interpreter it is
  not defined there. A guarded base clause and a matrix of terms are refused.
  `test/compile/newton.ink` solves each step of a stiff equation by Newton's
  method and holds every term to the interpreter by `--check`: within 0. A
  parameter named `m` first became `m_`, the struct's pointer, so a
  function's arguments are `arg_x`. Infinite series are still refused, and a
  fixed number of iterations, for a step whose time must be bounded, waits
  for a model that needs it. 167 lines of sources.
- `[done]` **Tensors of rank 3**, the table's last row, for attention per batch and
  per head. Specified in `test/data/spec/tensor.ink`, 75 of its 108 entries
  failing, multi-head attention over a batch among them, held to NumPy's
  output. A tensor is a stack of matrices of one size, its slices, along its
  first index, where a paper puts the batch: `X` is `B×T×D`. Rank 3 and no
  more: the extent gains a count of slices, and a fourth index, in a literal
  or a definition, is refused by name. A shape of any rank, `MANIFESTO.md`'s
  direction, costs little more to store, but its literal, its printing and
  its indices are a rule per rank, and the model does not ask for one: a
  paper writes a head `head_i`, a parameter, so heads need no index. A
  convolution's four is the case for it when its entry comes, and nothing
  specified here would move. The rank is part of the value, as the shape is
  (C50): a tensor of one slice is not its slice, which a batch of one would
  otherwise stop being, so it prints `[1, 2; 3, 4;;]` and `==` tells it from
  the matrix. Dropping a leading 1, as MATLAB drops a trailing one, was
  rejected for that. Nor is a tensor of one cell a single value: an index
  and a guard refuse `[7;;]`, and `T + [7;;]` wants as many slices. A slice
  of one cell is one, so `[1;; 2]*[1 2]` scales the row by each.

  The literal stacks matrices with `;;`, one semicolon more than separates
  rows, after Julia's convention of counting semicolons, one more for each
  axis -- not Julia's meaning, where `;;` joins horizontally and `;;;` stacks
  along the third axis: `[1 2; 3 4;; 5 6; 7 8]`, each slice a matrix literal
  with its blocks, and slices of two sizes refused rather than padded, which
  would be the fourth fill rule C41 warns of. A tensor is not a block, so
  `[T;; T]` does not join two batches. NumPy's nested brackets were rejected
  because `[[1 2; 3 4], [5 6; 7 8]]` already sets two blocks side by side,
  and `cat(3, A, B)` because a call prints nothing that reads back. A
  touching `;;` was an empty row padded with zeros, which nothing recorded
  wrote; with a space it still is. A tensor prints as a matrix does, its
  columns aligned across every slice and `;;` ending each slice but the
  last, or the only one, so it is pasted back as printed. One index reads a
  slice, as on a matrix it reads a row, and three a cell; two, which on a
  tensor name neither, are refused, and `T[b][t]` is the row when that is
  meant. A definition by three indices is a tensor, its clauses naming all
  three.

  Whatever meets a tensor meets it slice by slice: a single value or a matrix
  meets every slice, another tensor its slices in turn, so
  `(T op M)[b] = T[b] op M` for `+`, `-`, `/` and `*` alike, and
  `T'[b] = T[b]'`. That makes `*` a product batched over the first index, the
  paper's `XW^Q` shared across a batch and `QK^T` per sample, and lets a
  positional encoding be added to each sample. NumPy's broadcasting, which
  stretches any axis of size 1, was rejected for the reason *One index is a
  row* gave: `[1 2] + [1; 2]` would answer where a transpose is missing.
  Refusing `*`, every product then a sum over cells, would make each `QK^T` a
  line of indices; contracting the last index of one with the first of the
  other, `tensordot`'s product, breaks the identity; reversing every index for
  `'` would move the batch last. A power and an order are refused by name;
  `==` compares whole tensors, a function of cells maps over them as over a
  matrix's.

  A contraction is a sum over an index, as on paper. A limit's distance is
  its largest cell's, and terms whose shape changes, rank included, have
  none. `grad` of a single value with respect to a tensor is shaped as the
  tensor, of a tensor with respect to a single value as itself, and anything
  else is a Jacobian. `tex` sets three indices as it sets two, and refuses
  `;;`, which has no form on paper. The compiler refuses a tensor by name
  for now: its cells are arrays of known rows and columns, and the
  interpreter is the reference a model is held to first; compiling attention
  per batch is an entry of its own. The model departs from the paper twice:
  `Concat(head_1, head_2) W^O` is the sum of each head times its block of
  rows of `W^O`, the same product, since a tensor is not a block; and softmax
  is defined by its cells, as `MANIFESTO.md` asks of a function of cells.

  Built as specified, every entry passing as written, now
  `test/data/tensor.ink`; no other golden moved. The matrix core was not
  rewritten, as the table feared: the extent gained a count of slices, the
  cells are stored slice after slice, and every operator meets a tensor
  through one function that takes it apart into slices, applies itself to
  each and stacks the results. The literal is a node of its own whose
  children are its slices' literals; a cell, and a clause for cells, take a
  third index, the slice, named first. `lim`, `grad` and `tex` needed almost
  nothing more, being generic over the value: a limit's distance compares
  whole extents, `grad` seeds a tensor's cells as it seeds a matrix's and
  differentiates a tensor literal as it does a matrix's, and `tex` adds the
  slice to a subscript and to the bounds. The compiler refuses a tensor
  value, a tensor literal, three indices and a definition by three indices
  alike, `cannot compile y: a tensor`. 222 lines of sources.

- `[partly done]` **The evaluator's speed, specified** before any of it is
  built: phase 14's step 1 resumed, with no change to the language. Training a
  logistic regression by `grad` on 200 rows read from a file takes 1.9 s, by
  the gradient written by hand 0.28 s, and a file of thousands of rows is now
  writable. Six workloads are in `bench/`, each a transcript, so that a faster
  wrong answer fails rather than measures: `deep`, `d_200000` filled from its
  base; `limit`, 3000 limits of `s(a)_n = s(a)_(n-1)*~0.5 + a`, each argument
  a new sequence; `harmonic`, 400 harmonic numbers exactly; `matrix`, 100
  products of 20x20 matrices defined by their cells, one of them of fractions;
  and `hand` and `grad`, 20 steps of the training above, which end on the same
  weights. Their data is `bench/train.ink`, committed, with the Python that
  wrote it in its comment, so nothing is generated at build time. `cmake
  --build build --target bench` replays each under callgrind, or by the clock,
  best of five, without valgrind; it is not a test, so nothing gates on it.
  The baseline is master at 43e9fb5, GCC 13.3, RelWithDebInfo, a shared 2.1
  GHz Xeon:

  | | instructions | clock | where they go |
  |---|---|---|---|
  | `deep` | 835M | 86 ms | flat: 4,200 a term, no function over 5% |
  | `limit` | 1,123M | 127 ms | 20% building memo keys as strings |
  | `harmonic` | 860M | 75 ms | the bignum: gcd and allocation |
  | `matrix` | 862M | 136 ms | 65% the products, 33% `A` and `B` rebuilt cell by cell for each |
  | `hand` | 2,475M | 246 ms | 86% rebuilding `X` and `y` |
  | `grad` | 19,003M | 1,762 ms | 38% the same literals, 27% `grad`'s dispatch |

  The profile ordered the work again. `X[r]` evaluates the whole literal `X`
  to take one row of it, so a step of training is quadratic in the rows, and
  that, not the tree walk, is most of the training's time. The order, each
  step measured on the workloads before the next:

  1. **A literal of numbers built once**, kept in its node at its first
     evaluation that succeeds, and a constant to `grad`. A prototype of 14
     lines took `hand` to 426M and `grad` to 11,862M, moved nothing else, and
     left every golden as it was. It reads no name, so it never took a step,
     and caching it moves no budget. Not folded when parsed: `tex` and the
     compiler read the literal as written, and `[1/0]` fails where it is read,
     every time.
  2. **`grad` dispatched by one virtual call** rather than a chain of up to
     twenty `dynamic_cast`s per node, which with the type names they compare
     are 42% of what `grad` has left after step 1. No node class derives from
     another, so the chain's order decides nothing.
  3. **A memo key compared without building a string**: an argument's value
     encoded, its extent written out by `to_string`, at every call. Equality
     stays exact, so two calls share an entry where they do today.
  4. **A row or cell of a named matrix read without copying it.** After step
     1 a read still copies all of `X`, so training stays quadratic; this makes
     it linear. Reading the file is quadratic too, outside the evaluator:
     `Statements` counts the brackets of the whole statement again at each
     line of a literal, so `use train` alone is 85M, a fifth of what step 1
     leaves of `hand`, and 4,823M for 1,600 rows. Counting each line's
     brackets as it is read, three lines, took that to 66M in a trial.
  5. **Slots, then closures**, for the flat remainder of `deep`, `limit` and
     `matrix`'s cells, as step 1 planned: slots were measured at six per cent
     at most and come with the symbol table closures need. A slot belongs
     to a scope rather than to a node, and only to a name its body never
     binds: an instance's body is one tree read from every instance, and
     `g(x) = (x > 0 and (c = 2)) + c` reads its own `c` or the global one
     by the branch it took.

  The targets, in instructions against the table: `hand` a fifth after step
  1 and a tenth after step 4; `grad` 7,500M after step 2; `limit` 950M after
  step 3; after step 5, `deep` and `limit` a third of what they take when
  step 4 lands, the low end of the 3-10x quoted above since a `Number` costs
  what it costs, and `matrix` 700M, its cells a third and its products as
  they are. No step may make any workload more than one per cent worse,
  which is what `harmonic` is for. Each commit gives its
  workloads' counts before and after; the clock is reported beside them and
  is evidence only where callgrind agrees. A step that misses its target is
  recorded here with its numbers, not forced.

  What must not move: every golden, the README and every expected header,
  byte for byte, and every test, under the sanitizers too. The budget takes
  a step at every reference and every term a series or `grad` walks, as
  now, so a line gives up at the same step and nests as deep before it
  fails; a cache that skips steps is a language change. The deepest line the
  limits accept still fits the stack, in MSVC's Debug build and under the
  sanitizers (C20, C63): a visitor or a closure changes what a level costs,
  so a walk a step rewrites is tested at the limit. The memo remembers
  the same calls and only those, evicts in the same two generations, is
  cleared where it is now, and stores nothing that threw. An error is the
  same message, and the first one raised: operands are evaluated in the
  order they are today. `?`, `tex`, `--compile` and `--check` read the
  parsed tree, which stays; closures are beside it and cleared with the
  memo.

  Rejected: memoising a plain definition, one line that would have done
  most of step 1 and `matrix`'s cells besides; but a hit skips the steps
  its body took, so `sum_(k=1)^400 A*A`, which today gives up after a
  million steps, would answer. That is a language change and would be
  specified as one. A scalar unwrapped stays dropped, as step 1 measured.
  Benchmarks in ctest would gate on noise, and by the clock alone on layout.

  Built to step 4, and stopped before step 5. The baseline was measured
  again on the tensors' value type, and each step judged against it; the
  clock is the best of five:

  | | baseline | final | clock |
  |---|---|---|---|
  | `deep` | 859M | 861M | 84 to 80 ms |
  | `limit` | 1,181M | 878M | 138 to 104 ms |
  | `harmonic` | 864M | 863M | 75 to 74 ms |
  | `matrix` | 937M | 937M | 137 to 135 ms |
  | `hand` | 2,760M | 93M | 285 to 12 ms |
  | `grad` | 20,155M | 5,689M | 1,827 to 540 ms |
  | `read` | 85M | 10M | 11 to 3 ms |

  The file read came first: a statement's brackets are counted line by
  line, and `read`, `train.ink` alone, is a seventh workload so that the
  quadratic shows if it comes back; 1,600 rows went from 4,153M to 58M.
  Step 1 kept a literal whose cells are all numbers, its padding included,
  in its node: `hand` 357M and `grad` 11,966M. Step 2 picks `grad`'s rule
  by the node's exact `typeid`, each node class `final` so that none is
  missed: `grad` 7,533M, 33M short of its target, since libstdc++ compares
  two different types by their names. Step 3 keys the memo by the arguments
  themselves, compared bit for bit as the string was; the key owns the
  call's arguments, since copying them cost `deep` nearly five per cent:
  `limit` 878M. Step 4 reads a cell or a row of a name defined by a literal
  of numbers where step 1 keeps it, for the step the name's evaluation
  takes: `hand` 93M and `grad` 5,689M.

  Step 5 is stopped. Finding a name is now 1.7% of `deep` and 1.2% of
  `limit`, which is all a slot can save, and a slot that survives both
  conflicts above still asks the frame first, which is most of a lookup
  now. Closures are a rewrite of the evaluator, and of every walk tested at
  the depth limit, for what is left of `deep` and `limit`: flat, the
  scalar `Matrix` around each `Number` and the memo, no function over 5%.
  `matrix` is its products, 65%, and `A` and `B` rebuilt cell by cell,
  33%, which only the memo rejected above would remove.

- `[done]` **`grad` of a definition by cells**, so that attention trains more
  than `W^V` and `W^O`. Softmax, a ReLU on each cell and a layer norm are
  written by their cells, and `grad` refused every one, "grad cannot
  differentiate a definition by cells yet", so of the conformance model's
  weights only `W^V` and `W^O`, applied after the softmax, had a gradient, and
  only from a loss whose softmax reads none of its parameters; `W^Q` and `W^K`
  reach the loss through it.

  A definition by cells is differentiated as it is evaluated, cell by cell:
  each cell's clause chosen as `Reference` chooses it, its row, column and
  slice bound as values nothing differentiates, and evaluated with its
  parts. Each part of the definition is then a matrix, or a tensor slice by
  slice, of the definition's size: a cell a clause gives takes that clause's
  parts, 0 where it has none, and a cell no clause gives takes the matrix
  written whole's, or 0, as its value does. Whether that is a gradient or a
  Jacobian is decided after, as for any value: a softmax with respect to its
  vector is a Jacobian and stays refused, a loss read from it has a gradient
  shaped as the vector, and a product with the Jacobian, which is what
  training asks, is written `grad_(v = p) u'*f(v)`. Giving the Jacobian was
  rejected: a vector's is a matrix whose orientation would be a convention to
  pick, and a matrix's has rank 4. Rewriting a definition by cells into a
  literal of its cells, for `Literal` to differentiate, was rejected too: a
  guard is asked per cell at the point, the size is read from the bounds, and
  the literal would be built again at every call, for an assembly of a dozen
  lines.

  Each cell's guards are asked as a whole definition's are: the clause that
  holds at the point gives the slope, a comparison whose sides meet there and
  move taking the side its value gives, so the guard's comparison decides a
  ReLU's slope at 0, cell by cell, `<` giving 1 and `<=` 0. An equality that
  holds at the point only is refused, naming the cell as `--check` does,
  `sp[2,1] takes a clause at t = 1 that holds only there`, a term's with its
  index, `g_2[1,1]`, and a tensor's with its slice first, `T[1,2,1]`. What is
  asked is whether a side moves, not its slope (C73), and a cell's place never
  moves, so `j == k` chooses a diagonal and is no jump. Refusing guarded
  cells, the smaller change, was rejected: the ReLU is the case. The `--check`
  hook is not told of a guard asked under `grad`, as for a whole definition,
  since `grad` is not compiled.

  The order is the evaluator's: a clause for one cell beats those for all
  cells, guarded ones before the unguarded; a term's, from its own cell at
  its index to the general clause written whole; and a base term and a cell
  of every term that both give a cell are the error evaluating them is. A
  limit of a sequence by cells is walked from its highest base term, written
  whole or by its cells, where `lim` starts it, a clause for one cell being
  no base term; `Derivative::Limit` skipped a base by cells, which nothing
  could reach before.

  With parameters, its arguments carry their parts, as any call's do: the
  softmax of a vector, a layer reading its weights. Without, it reads only
  globals, which `grad`'s name does not reach, so it is a constant, as `N`
  read by `layer(w)[j<=2] = N[j]*w`, or reads the global of the name and is
  refused as any definition is; both are answered today, and nothing is
  added for them. A size is read with its parts: one that reads the name and
  does not move, `floor(x)` at 5/2, is the size; one that moves is refused,
  `the size of gr jumps at x = 2`, since a size is a whole number and one
  that moves is at a jump, as `floor` is at a whole number. Taking the size
  at the point was rejected as silent: what changes shape with the name has
  no derivative there.

  What stays refused, by name: a Jacobian, "grad of a matrix with respect to
  a matrix is a Jacobian, which it does not give"; a moving size; an
  equality holding at the point only, by its cell; a cell that is not a
  single value and a base term against a cell of every term, each in the
  words evaluating it uses; a definition by cells in an instance, "grad
  cannot differentiate through an instance yet", as anything there; a local
  definition, as now.

  The choice of a cell's clause is the third copy if copied, beside
  `Reference`'s and the compiler's chain per cell, and differentiation's
  entry already names its second copy as one that can drift. So
  `EvaluateCells` and `EvaluateTerm` become one walk over what evaluates a
  clause, asks a guard and stores a cell, instantiated for the value and for
  the parts, landed first as a refactor: every golden byte-identical, and
  `matrix` within one per cent, the evaluator's rule. If that cannot be had,
  the copy, recorded here. About 120 lines of sources with the walk shared,
  about 180 with a copy. Forward mode costs a pass over the loss per cell of
  the matrix differentiated against, four for this `W^Q` and d² for a d×d
  one; reverse mode, one pass, is a second evaluator with a tape, and an
  entry of its own when a model is too slow for this.

  Specified in `test/data/spec/gradcells.ink`, 40 of its 106 entries
  failing: a square per cell, softmax by `exp` and normalised by a sum,
  ReLUs either side of 0, cells reading cells, a term or a limit, a diagonal
  by guard, one-cell clauses, guarded or not, sizes, terms and a limit by
  cells, a tensor, guarded or not, and attention's `W^Q`, its gradient held
  to the one written by hand through softmax's Jacobian. Writing it found
  C77 and C78.

  Built as specified, every entry passing as written, now
  `test/data/gradcells.ink`; README shows a ReLU's gradient, and no other
  golden moved. The walk is shared: `EvaluateCells` and `EvaluateTerm`, and
  the choice of a clause under them, take what evaluates a clause, reads a
  size, asks a guard and stores a cell, as the value or as the parts. Landed
  first as a refactor, every golden byte-identical, `matrix` ran 934.2M
  instructions under callgrind against 934.7M before, and the other workloads
  within 0.35%. A cell's parts are stored where its value is, and a part a
  cell lacks is 0 there even where the matrix written whole had one.
  `Derivative::Fill` now starts where the evaluator's fill does, as the limit
  starts where `lim` does, so a clause for one cell is a base term for
  neither; a guard choosing a whole term is named by its definition, as any
  definition's is. Building it found C79, fixed, and C80, refused since; its
  review found C81, fixed, and C82, open. 197 lines of sources added and 136
  removed, the refactor's moves among them: 61 more in all, where about 120
  were planned.

- `[done]` **A model's history of its inputs** (C75). A model that reads its input
  before the stream, `c_n = x_(n-1)` in `delay(x_n)`, is answered two ways:
  given `x_n = n^2`, the interpreter reads `x_(-1)`, 1, where the step a host
  feeds from its first index holds 0, and `--check` parts at 0; given the
  workaround `s_n | n >= 0 = n^2`, the interpreter has no `c_0` where the step
  has 0, and `--check` skips the term, as it skips any the interpreter cannot
  give. Seven checked programs skip terms today, for three reasons. `boxcar`'s
  `f_0`, `z_0` and `w_0` to `w_2` reach `x` before the stream, `z` and `w`
  through `y_(-1)` and `y_(-2)`: C75 itself. `gain.K_0`, `pairs.w_0` and
  `w_1`, `odd.z_1` and `trail.w_0` are below the model's own base clauses,
  where neither has a term. `boxcar.y_0`, `boxcar.q_0` to `q_2`, `stride.h_0`
  and `frame.p_0` and `p_1` are a slow sequence before its first tick. Only
  the first is a discrepancy; the others are skipped in silence.

  The decision, the user's, and one that may be revisited: no discrepancy. The
  model says what its input was before the stream, by a clause of its own on
  the input, which both read:

      delay(x_n) = {
          x_n | n < 0 = 0
          c_n = x_(n-1)
      }

  In the interpreter, a history is clauses of the input in the model's body,
  terms, `x_(-1) = 5`, and guarded clauses, of any value, shape and guard,
  each input its own. The instance's argument is the input's default clause,
  so the order is the one every definition has: its clauses in the order
  written, then the default, so a term beats a guarded clause only written
  before it, as `y_(-1) = 3` after `y_n | n < 0 = 0` is never reached at
  the top level either (C45). Where both apply, the history wins; where neither
  does, it is the argument's own error, `no clause of s applies`, or, with
  none, `delay(...).x_0 is an input, and nothing defines it`. An unguarded
  clause would leave the argument nothing, and is refused, `x is an input of
  bad, so its body can give it only a history: a term or a guarded clause`; a
  parameter's stays refused as it is. The argument giving the input from 0 and
  the history below was rejected: the interpreter would need where the stream
  starts, which is the compiler's, 1 for `odd`, so an instance would mean what
  compiling it decides, and a guard would be read against a boundary it does
  not name. Without a history the argument answers where it did, so no golden
  moves.

  Compiled, the stream starts at the step's first index, as the header names
  it. Every read of a model's input before it is a term the history gives, or
  refused: `cannot compile c: c_0 reads x_-1, before the stream, where x has
  no history`. That holds in a clause, a base clause, a sample and a guard; a
  read on the right of `and` or `or`, or in a guarded clause's value, is one
  only where what keeps it, folded at that step, does not decide, so `n > 0
  and x_n > x_(n-1)` compiles as now and `n >= 0 and ...` is refused, where
  the interpreter reads `x_(-1)` and the step says NaN. Starting `c` at 1, as
  the step does today, was rejected: the interpreter answers `c_0` from the
  argument wherever it can, and the step cannot know the argument. A history
  is compiled as constants: `init` fills the input's window before the stream
  with each term a read reaches, folded, so it may read its index and
  constants; one that reads anything else is refused, `a history that reads
  a`, since the host may assign a parameter after `init` filled the window,
  and one that is not a single value too, as the step's input is one (C83). It
  must give nothing from the first index on, where the interpreter would read
  it and the step what the host feeds: a term there, or a guard holding there,
  is refused, `its history reaches x_0, in the stream`. So a guard is taken
  only as its index below a constant, `n < k` or `n <= k`, either way round,
  and any other is refused, `a history whose guard is not its index below a
  constant`, since no other can be shown to fail at every step. Compiling the
  history into the step, as a chain before the value fed, was rejected: any
  guard would compile, but every step would test it, a guard holding in the
  stream would replace what the host feeds without a word, and no header would
  stay as it was.

  Another sequence's term before the stream that a read reaches through the
  history is folded into that sequence's window by `init` too, at the input's
  rate or before a slow one's first tick: `boxcar`'s `z_0` reads `y_(-1)`,
  `(x_(-1) + x_(-2))/2`. Where it does not fold, reading a parameter, the step
  computes it again where it is read at the input's rate, as C71 does, and
  elsewhere refuses it as now, `before y's first tick, where its samples could
  give a term` or `before the step computes k`. Those refusals stand for a
  closed form's terms too, which the same fill would answer: one concern, and
  a model that asks first. The first is also what a hold before a slow
  sequence's first tick meets where its samples read the input before the
  stream and no history gives the term: `z_n = y_(floor(n/2) - 1)` on
  `y_m = x_(2*m)` compiles today, and `--check` parts at 0, C75 again. A
  history of 0 emits nothing, `memset` giving it,
  so a history line leaves the header of a model that reads no input before
  the stream byte-identical; one that does changes only where a reader now
  starts at the first step, the start having been the step's way of having no
  term. It is a requirement that every header in `test/compile/expected` stays
  byte-identical: none of their models reads its input before the stream. A
  file's input, a name nothing defines, has no argument for the interpreter to
  answer from and no body to state a history in, so its readers start where
  its terms exist, as now: `fir.ink` waits for four samples, `back.ink` keeps
  its NaN. An instance compiled within its file reads its argument's terms, as
  now, and its history where it holds. Within a model, its argument's terms
  before the stream are the outer input's history, which `init` folds into
  the inner input's window as any other, and the inner history beats it
  where both hold, as in the interpreter.

  `--check` holds a sequence from the step that computes its first term, its
  start or a slow one's first tick, and says so where that is after the first
  step: `pairs.w: within 0, from 2`. Before it the step has no term; at the
  input's rate the interpreter is asked, and must have none either, else the
  line is `none at` the step, `where the interpreter gives` its term; a slow
  sequence's terms before its first tick are not asked, since the step keeps
  none and the interpreter's latest term there is one no read needs,
  `stride`'s `h_(-1)` from its closed form. From its start, a term the
  interpreter cannot give parts, `pole.y: inf at 3, where the interpreter
  gives none: division by zero`, which passes today at `within 0`: the first
  check's rule that such a term is not compared is reversed, as it hid C75's
  second form. NaN where the interpreter has none agrees, NaN being the step's
  word for it. A line of its own for the steps before a start was rejected, as
  one line per sequence is what the report is read by. So no term goes
  unreported: `gain.K` from 1, `pairs.w` from 2, `odd.z` from 2, `trail.w`
  from 1, `stride.h` from 1, `frame.p` from 2, and `boxcar.y` from 1 and `q`
  from 3, with `f`, `z` and `w` held from 0 once `down` states its history.
  An instance a model writes unnamed is the exception: the interpreter
  cannot name its terms, so each of its lines would part (C84).

  Only `down` in `test/compile/decimate.ink` reads its input before the
  stream, and it gains `x_n | n < 0 = 0` in the commit that builds this; `s`,
  the workaround, goes, and `boxcar`, `lead` and `trail` read `x_n = n^2`,
  since `lead` and `trail` never read before the stream. No README example
  reads one; section 5 and the compiler's paragraph gain the history there.
  About 210 lines: 40 in the interpreter, 130 in the compiler, most of them
  the fill and the refusals, and 40 in the check.

  Specified in `test/data/spec/history.ink`, 47 of its 56 entries failing,
  those passing being definitions echoing themselves and a parameter's
  refusal; and in `test/compile/history.ink`, whose instances, refusals
  and reports, the seven programs' among them, are wired with the
  compiler's half.

  Built as specified, every entry passing as written, now
  `test/data/history.ink`, and every check and refusal of
  `test/compile/history.ink` wired; README shows `delay` in section 5 and
  says what the compiler and the check do before the stream. No other golden
  moved and every expected header is byte-identical; the seven programs report as
  specified. In the interpreter, an input's clauses are a definition of
  their own whose default is the argument, or the input where none is given,
  so the argument keeps reading where it was written. The compiler folds what
  `init` writes by asking the interpreter for the term, kept only where some
  clause of a history gave part of it, so a closed form's term before a slow
  sequence's first tick stays refused; the history's own clauses are checked
  as specified, folded in the order written, a term after a guarded clause
  folded rather than refused. Departures: a term folded so through an
  argument or a slow sequence's samples that can read a parameter is
  refused, as the host may assign the parameter after init, where the spec
  computes it again at the input's rate; a
  read in a term computed again (C71) has nothing to keep it, so is refused
  where its history does not give it even if a guard would have kept it; a
  slow sequence's first tick still waits for its samples from the stream,
  as `frame.p` from 2 asks; and a term the interpreter gives that no
  double holds now parts, saying why, where it was skipped: `huge.y: inf at
  31, where the interpreter's term is too large for a double`, or not a real
  number; and a closed-form argument to an instance whose model states part
  of a history is refused where it was computed again (C71): with `two(x_n)
  = { x_(-2) = 0; c_n = x_(n-1) + x_(n-2) }`, `p2 = two(x_n = n^2)` reads
  `p2.x_-1`, which no history gives. Known limits, each a refusal where the
  interpreter answers, never a discrepancy: the left of an `and` or `or`, or
  a guard, decides only where it folds to a constant, never through a
  history's terms, so `x_(n-1) > 0 and u_(n-1) > 0` is refused where `u` has
  none; a read in a term computed again is refused as above; and a slow
  sequence's first tick waits for samples from the stream, so `y_m | x_(2*m
  - 1) > 0 = x_(2*m)` read at `y_0` is refused, as is a hold below its
  folded terms. `fuzz.cpp`'s alignment with the
  reversed check rule, and refusing a top-level `x_n | n < 0 = 0` in a file,
  whose input has no body to state a history in, are left out of this work.
  Its review fixed four defects of its own: a fold reading a parameter, now
  refused; a term read only on the right of an `and` or `or`, refused as
  unfolded; a slow sequence's base clause reading before the stream, never
  held to the history; and a reader of an inner input given by a hold,
  started a step late.
  C84 stays open and is the check's one exception, its lines saying `not
  asked`, and C85 is untouched.
  331 lines of sources added and 62 removed, 269 more in all, where about
  210 were planned: 38 in the interpreter, 202 in the compiler and 29 in the
  check.

- `[done]` **Sizes inferred in a definition by cells.** A network applies every
  activation cell by cell, and the prelude's functions take single values:
  `exp([1 2])` says a matrix cannot be an exponent, `log([1 2])` speaks of a
  comparison the session never wrote, and each activation is a definition by
  cells with a size the page never writes, `act(z)[j<=n] = tanh(z[j])` where
  the paper has `tanh(z)`. A bound left out is read from the matrix the index
  reads: `rl(z)[i,j] = z[i,j]*(z[i,j] > 0)` is a ReLU of a matrix of any
  size, and `h[i,j] = Z[i,j]*(Z[i,j] > 0)` a layer written where it is used.
  Every bound written means what it does now.

  An index written without its bound takes it from each read, in the
  clause's value or guard, of a matrix at that index alone: `z[i,j]` gives
  `i` the rows of `z` and `j` its columns, `z[j,i]` the other way, `z[i]`
  the rows, as one index reads a row, `z[i,1]` `i` alone, and of a tensor
  `P[s]` the slices and `P[s,i,j]` all three. What is read is any expression
  that reads no index of the cell, `(W*x + b)[i]` or `P[1][i,j]`; a sum's
  index is the sum's, so `A[i,k]` under `sum_(k=1)^2` gives `i` only. `z[i+1]`
  gives nothing, nor does a matrix that reads a name bound in the clause, a
  sum's or a product's index, bounded or taken to its limit: under a sum over
  `k`, `P[k][i,j]` and `(A^k)[i,j]` read another matrix at each `k`, and
  `cs_n[i] = sum_(m=0)^n w_m[i]` another term at each `m`. Nor does a call
  of the definition itself, `g(z/2)[i,j]` in a clause of `g`, whose size
  would need its own first; a term reading an earlier term, `cn_(n-1)[i,j]`,
  reads another and gives its size. A single value is read as a 1x1 matrix
  is, so `rl(5)` is 5.
  Every read of an index agrees, or it is refused naming two that do not,
  `a[i,j] and b[i,j] give j different sizes, 2 and 3`, or one, `z[i,i] gives
  i different sizes, 1 and 3`. The first read winning was rejected: of
  `a[i,j] + b[i,j]` it would cut a longer `b` short in silence, read past a
  shorter one, and answer by the order the operands were written; the
  smallest winning, as a zip does, is the same silence. A bound written is
  the bound and is never checked against a read, of its clause or another's,
  so `top(z)[i<=2, j] = z[i,j]` takes two rows of any `z`. That
  `matrices.ink` says a size is written, not guessed from the cells a clause
  happens to give, stands: a read is not a guess, the size being the
  matrix's that every cell reads.

  The size is the definition's, as now: the matrix written whole and the
  bounds written agree, else "the clauses of V give it different sizes"; an
  index none of them bounds takes the size its reads give, and clauses that
  read it and disagree are refused in the same words. A clause that gives an
  index none takes the others', so a causal mask, `cm(s)[i,j] | j > i =
  0`, needs `cm(s)[i,j] = s[i,j]` beside it and nothing more. Where
  nothing gives one, it is refused when read, as a definition of one-cell
  clauses is today: `M has no size, as nothing reads a matrix at r alone;
  write it as M[r<=rows, c<=cols]`, the bounds written kept. So `M[r,c] = r
  + c`, refused where written in `matrices.ink`, is refused where read: a
  size may come from a clause written after, and refusing where written
  would depend on the order the clauses come in. A clause for one cell gives
  no size, as now, and one outside the size inferred says so as reading it
  would. A term's size is read at its index from the clauses at that
  index, as now, so a base term by cells, `q_0[i] = 1`, has none unless it
  reads one, and a term may read the one before, `cn_n[i,j] = cn_(n-1)[i,j]
  + j`.

  Each read's matrix is evaluated for its size each time the definition is,
  before any cell, in the frame its cells see: a parameter's, a term's at its
  index, a global's as it is then, so `h` follows `Z` redefined, as a bound
  written with a global does. Its steps count against the budget as any
  evaluation's, and an error there is the read's: `a 2x1x2 tensor takes one
  index or three, not two`. A definition whose bounds are written evaluates
  as today, so `bench/` does not move. The memo is unchanged, a call keyed by
  its arguments and their shapes (C50). `grad` reads a size with its parts as
  it reads a bound, and a shape never moves. `z*(z > 0)` has no slope at 0
  and `grad` says so, `a comparison jumps at t = 0`, where the guarded ReLU
  takes its guard's side. `?name` prints as written; `tex` sets a range only
  for a bound written, `\operatorname{rl}(z)_{i,j} = z_{i,j}\,(z_{i,j} >
  0)`, as a paper sets a function of cells, since it sees the definition and
  not a call. Compiled, a function by cells is compiled where it is called,
  and its size is the extent of the read compiled there, which is always
  known, the compiler's shapes being static: nothing new is refused, and a
  model's input is a single value, as C83 has it.

  Softmax's sum keeps its bound, `sm(z, K)[j] = exp(z[j])/sum_(c=1)^K
  exp(z[c])`, `K` written as the page that writes the sum writes it: nothing
  is added to name a size. A sum with no upper bound bounded by its reads was
  rejected, since `sum_(c=1)` is a series to its limit and its meaning would
  be decided by its body; so was a bound naming a size nothing defines,
  `[j<=n]`, Dex's index set, whose meaning would change the day a global `n`
  is defined, and which `t_n[j<=n]` already reads as the index; and
  `rows(z)`, a function no paper writes, for a size an argument carries.

  Two rulings from the review of `f.(x)` carry over. A call given a matrix
  that its own evaluation refuses where a single value is needed -- a
  comparison, a guard, `and`, `or`, an exponent, an index, a factorial -- is
  refused naming the innermost call written in the session or a file, not in
  the prelude, given an argument of the refused value's shape, and that
  shape: `tanh needs single values, not a 2x1 matrix; write it by its
  cells`. The argument is not quoted as written, which the parser keeps no
  text for and about 30 lines would add, nor is a definition by cells
  written out, whose name would be invented. So `tanh(M)` names `tanh`, not
  the `exp` inside it; `sig(z) = 1/(1 + exp(-z))` names its own `exp(-z)`;
  a layer that applies a function of single values to the whole names it,
  not its loss.
  Where no call was given a value of that shape, as `x*x' > 0` of a 2x1, and
  outside any call, an operator keeps its words, and `e^A`, on paper the
  matrix exponential, stays refused. And `mod` refuses a divisor that is a
  matrix, `mod needs a single value to divide by, not a 2x2 matrix; write it
  by its cells`: `b*floor(a/b)` is then a product of matrices, so
  `mod([7 8; 9 10], [3 3; 3 3])` answered `[-8, -7; -6, -5]`. The ruling
  named two matrices; `mod(7, [3 4; 5 6])` multiplies as well, so the
  refusal is the divisor's. The prelude has no test for a matrix to say it
  with, so the call of its `mod` is checked before the body. Two refusals
  recorded move: `nonzero([1 2])` in `conditional.ink` and `nz([7;;])` in
  `tensor.ink`.

  Rejected: the dot, `f.(x)`, specified on branch `cellwise`: a second way
  to say what a definition by cells says, which `MANIFESTO.md` asks a
  direction to avoid; `tex` sets it as `f(x)`, so a forgotten dot would not
  show on the page it is read against; and the one reason given for it, that
  the cell form states a size the page never writes, is what this removes.
  The prelude's functions mapped implicitly, as `floor` is: `exp(A)` would
  mean the cells here and the exponential on a control paper, `ex(x) = e^x`
  would differ from `exp`, and a guarded ReLU would still be refused. This
  answers `MANIFESTO.md`'s open question on a function of cells by its own
  default, the cell form, made light rather than replaced.

  About 120 lines: the reads found 25, the size read and its two refusals 40,
  `tex` 3, the compiler 20, the message of a call given a matrix 20, and
  `mod` 8; the check that a size is written, where a clause is, goes.

  Specified in `test/data/spec/sizes.ink`, 134 of its 181 entries failing,
  those passing being definitions echoing themselves and four answers that
  stay: `e^A`, `[1 2] < 3`, a comparison of a shape no call was given, and
  `mod` by a single value. A ReLU network and a softmax classifier are each
  trained one step by `grad` through functions of cells with no size written,
  held to the gradient written by hand, the first exactly, with the values
  the dot's specification had. The compiled half is `test/compile/sized.ink`,
  `logistic.ink`'s regression with a sigmoid by cells and no bound, whose
  header is to be the one its bounds written give, byte for byte; wired with
  the implementation.

  Built as specified, every entry passing as written, now
  `test/data/sizes.ink`; `compile_sized_header` holds `sized.ink`'s header to
  the one its bounds written give, and `--check` holds it to the
  interpreter. Three goldens moved, as specified: `M` in `matrices.ink`, now
  read as well, `nonzero` in `conditional.ink` and `nz` in `tensor.ink`.
  README shows the one-line ReLU. The size is measured once for interpreter,
  `grad` and compiler alike, `Reference::Measured`, given how each reads a
  bound and a read's extent; the reads are found as a clause is defined.
  Departures: the matrix written whole counts as a size written, never
  checked against a read; `grad`'s name is bound in its body as a sum's
  index is; a read of anything but a name is quoted `(...)` where two
  disagree; and a term with no size is named at the index asked, `cs_2`.
  C87 was found and fixed on the way, the hint for one-cell clauses now
  written from the clause as the others are. The review made `mod`'s check
  `grad`'s and the compiler's as well, and the call refused in its own name
  `grad`'s too, where a network is trained; a definition by cells so refused
  is not told to be written by its cells. Under callgrind,
  `harmonic` and `matrix` moved under 0.1%, `hand` 0.3% more, `deep`, `grad`
  and `limit` 0.1 to 0.5% fewer, and `read`, which defines nothing by cells,
  1.2% more, all of it `Number::Literal` no longer inlining `push_back` as the
  unit grew, until its digits were written by index, 0.6% fewer.
  420 lines of sources added and 143 removed, 277 more in all, about 40 of
  them the body of a call indented under the handler that names it, where
  about 120 were planned.

- `[done]` **A NaN reaches every term that reads it.** NaN is the step's word for what
  the interpreter refuses: a limit that does not converge, a function, a
  sequence or a cell no clause of which applies, a clause refused wherever it
  is taken (C86), a term before it exists, a singular solve, a parameter
  nothing defines. `lim` compiled chose it over the status field
  `MANIFESTO.md` sketched because a NaN reaches every term that reads it, a
  field only the host that looks. It does not: a comparison with NaN is
  false, so `high_n | y_n > 1/2 = 1` beside `high_n = 0` answers 0 where `y_n`
  failed, and a bare truth, `z_n | y_n = 1`, holds. The failure leaves as a
  plausible value, C13's class in generated code. `--check` parts there,
  `run.high: 0 at 8, where the interpreter gives none`, but a deployed header
  has no oracle. Found by an outside review.

  Decided: make the premise true rather than reverse the ruling. In a header
  that writes NaN, a decision on a value answers NaN where an operand is NaN,
  as the interpreter refuses a guard whose operand fails. An operand that
  reads a name other than the step's index is tested: the guard is
  `isnan(m_->y[0]) ? NAN : m_->y[0] > 0.5`, which C's right-associative
  conditional makes take no clause, `isnan(a) || isnan(b)` where both
  operands are tested; a comparison as a value is `(isnan(a) ? NAN : a > b ?
  1.0 : 0.0)`; a bare truth tests itself. A power decides too, where C's
  absorbs a NaN, `pow(1, NaN)` and `pow(NaN, 0)` being 1: its base is
  tested unless its exponent is a constant other than 0, and its exponent
  unless its base is a constant other than 1, so `1^y_n` is `(isnan(m_->y[0])
  ? NAN : pow(1.0, m_->y[0]))` and `stiff`'s `pow(t1_, 3.0)` is as it was.
  `and` and `or` take the path a
  guard that defers already takes, a truth as a double of 1, 0 or NaN,
  whose right side is read only where the left has not decided, as in the
  interpreter. Each guard of a chain tests its own operands, since one tried
  after a clause that holds is never read. That is every guard compiled: a
  sequence's, a function's, a limit's walk, a cell's, and `update`'s. The
  clause `--check` keeps, `name_clause_`, is 0 where a guard reads NaN, no
  clause being taken; today a guard of `and` or `or` writes the NaN into that
  int. The Interface comment of such a header adds "A term the interpreter
  would refuse is NaN, and so is every term that reads one, through a guard
  or a comparison as through arithmetic; -ffinite-math-only, which
  -ffast-math implies, removes the tests that make it so", so the host's one
  test is `isnan` on what it reads, and knows that a compiler told no value
  is NaN may take every `isnan` as false.

  The interpreter refuses the same decisions on its own NaN, which inexact
  arithmetic makes, `0/~0`: a comparison of one answered 0 and a truth of
  one held, so a guard reading it took a clause, where a step reading NaN
  now takes none, and the two parted. A comparison with NaN has no answer,
  and `MANIFESTO.md`'s interpreter refuses rather than guesses. In the words
  given for a value that is not single: `0/~0 > 0` and `0/~0 == 0/~0` are
  "a comparison needs a number, not -nan", as is a whole matrix compared
  with a NaN cell; a guard reading it, "a guard needs a number, not -nan";
  `0/~0 or 0` and `1 and 0/~0`, "or needs a number, not -nan" and "and
  needs a number, not -nan", the right side still read only where the left
  has not decided, so `0 and 0/~0` is 0. `errors.ink` holds the
  comparisons, `logic.ink` `and` and `or`, and `conditional.ink` the guard:
  `nonzero(0/~0)`, recorded as answering 1 because it was "the one place the
  convention bites", moves to the refusal. No other golden moves.

  A header that writes no NaN is byte for byte as now. The gate is the
  header: one that writes NaN anywhere is compiled again, aware. Of the
  fourteen expected headers ten stay; `back.h` gains the sentence alone, its
  guards reading the index; `heat.h` and `kalman2.h` gain it and a test
  after each matrix term, heat's one and kalman2's six, as their inverses
  write NaN; `adc.h` moves, its hysteresis, alarm and rising edge tested,
  because `rising` writes NaN before its first term, though no step reaches
  it. Of the programs `--check` writes for the instances checked, seven
  write NaN and move, their reports as they are: `ajar` and `rise` test a
  guard's operands, `cls`, `gate`, `mark` and `rnn` their matrix terms, and
  `stiff` gains the sentence alone.

  The interpreter refuses a matrix term whole where one cell fails, so in an
  aware header a matrix term with a NaN cell is NaN in every cell: after the
  cells of each matrix sequence's term, one test per step,

      if (isnan(m_->y[0][0][0]) || isnan(m_->y[0][1][0]))
          for (int i_ = 0; i_ < 2; ++i_)
              for (int j_ = 0; j_ < 1; ++j_) m_->y[0][i_][j_] = NAN;

  inside the block that computes the term where one does. Leaving a cell's
  NaN in its cell, the first choice, was overruled on review: a guard
  reading a healthy cell of a refused term, `h_n | y_n[1] > 1/2 = 1`, took
  its clause, 1 where the interpreter gives none. `--check` agrees cell by
  cell as today, each cell now NaN where the interpreter gives no term; a
  term by cells with one failing cell parted, `slope.y[1,1]: 1 at 2, where
  the interpreter gives none`. A matrix that is no sequence's term, a value
  computed on the way or from the parameters, keeps a NaN in its cell. A
  cell no clause gives is 0 in the interpreter, not a refusal, and stays 0,
  as C89 has it.

  Rejected: (a) a status field, the first failure's definition and step or a
  count. The ruling under `lim` compiled stands: a field is read by the host
  that looks, while the plausible value reaches every output. It costs a
  member, `init`, a write at each of eleven sources, and the limits' functions
  take the model `const`; naming the origin, its one gain, is the
  interpreter's, asked for the term. (c) both: two channels for one fact. (d)
  `--check` naming the first NaN's origin: it reaches no deployed header,
  which is where the failure goes unseen. Testing every comparison of every
  header: `kernel.h`, `net.h` and `pid_clamped.h` would move, each clamp and
  ReLU tested for what its model cannot produce. A may-be-NaN analysis per
  sequence over the reads, sparing `adc.h`: a fixed point, as recurrences read
  themselves, for about 25 more lines. `isunordered`: a second primitive where
  `isnan` is the compiler's. The complement, `a > b ? 1.0 : a <= b ? 0.0 :
  NAN`: `==` and `<>` have none.

  Known limits, each as today. A header that writes no NaN is not aware, yet
  it divides: 0/0 there is NaN and a division by zero an infinity, and its
  guards and comparisons decide on either as on a number. In an aware header
  0/0 propagates, while an infinity still compares as a number; testing
  every divisor would move every header that divides, and `--check` says
  it, `pole.y: inf at 3, where the interpreter gives none`. A host's NaN, an
  input it feeds or a parameter it assigns, propagates only in an aware
  header; in another, its guards decide on it. A guard reading a limit
  directly computes it twice, as its value already does. A NaN the
  interpreter's own arithmetic makes is a value, not a refusal, but the
  step cannot tell them apart: with `m_n = [(0/~0)*x_n; 1]` and
  `h_n = m_n[2]` the interpreter answers 1 and the step NaN, its matrix
  term NaN in every cell, as is a cell taken from such a matrix inline.

  About 30 lines and none removed: 25 in the compiler, the gate and its
  second run, the operand's test, the guard, the comparison, `and` and `or`,
  the clause kept and the sentence; 5 in the check. Each decision on a value
  grows by `isnan(a) ? NAN : `, in an aware header only.

  Specified in `test/compile/nan.ink`: eight instances, each a guard reading
  a failure, among them one inside a limit's walk, `lap`, one computed in
  `update`, `trim`, and an `or` reaching NaN as the step runs, `any`, with
  their steps, their reports and `adc.h`'s step, wired with the
  implementation. The interpreter's refusals are written in the
  goldens named above.

  Built as specified, every instance of `nan.ink` reporting as written,
  checked by `test/CMakeLists.txt` and its steps held line by line by
  `holds` in `test/cli.cmake`; `adc.h`, `back.h`, `heat.h` and `kalman2.h`
  move as specified, and the seven programs named, their reports as they
  were. C90 is fixed on the way, the clause kept taking the guard's own
  test to 0. Departures: an operand is tested unless it is a constant or
  the index itself, told by a flag on its cell, rather than unless it reads no name
  but the index, so `n - 1 > 0` is tested and a function's parameter given
  the index is not, which moves no header; a guard that folds to NaN, which
  the interpreter now refuses wherever it is tried, is refused in its
  definition's name, `cannot compile h: a guard needs a number, not -nan`,
  where the fold threw past it; and `--check` needed no change, every cell
  of a refused matrix term being NaN. Under callgrind `grad` takes 0.5% more
  instructions and `harmonic` 0.6% fewer, the rest as they were. 101 lines
  of sources added and 21 removed, 80 more in all, where about 30 were
  planned before the review added the matrix test, the power and the
  interpreter's refusals: 13,358 lines in all.

  On review, the interpreter's power of NaN is NaN, `1^(0/~0)` and
  `(0/~0)^~0` as an aware step answers them, where C's `pow` gave 1: the
  one parting the implementation left as a known limit; `errors.ink` holds
  it. The index is told by a flag on its cell, not by comparing its text
  with the index's, which moves nothing. A cell taken from a matrix that
  is no sequence's term, one written inline, given to a function, computed
  again or a value from the parameters, kept a healthy cell where the
  interpreter refuses the matrix whole: `cell` and `held` in `nan.ink`
  answered 1. Now, in an aware header, it is NaN where any cell of the
  matrix is, the cells shared first, and a matrix that writes NaN in a cell
  not taken makes the header aware, as it would otherwise write none. A
  term's window is whole already and is not tested; of the programs checked
  only `rnn`'s moves, its tanh taking each cell of a product. That every
  `and` and `or` defers in an aware header is said once, in `Defers`, not
  by its two callers. A constant matrix, which folds, is left out of the
  gate. The gate reads NAN as a word past the line naming the source, so
  a module, a file or a parameter named with it, `NAN_clamped`, no longer
  makes a header aware. A truth is given its subject, `and`, rather than a
  message to cut it from, and a guard that is a matrix says "a guard needs
  single values", as `and` and a comparison do, where it said "a single
  value", recorded nowhere until `conditional.ink`. The Interface sentence
  ends "Built with -ffinite-math-only, which -ffast-math implies, GCC
  removes the tests that make it so, and Clang warns of each NaN": Clang 18
  warns of every NAN there, `-Wnan-infinity-disabled`, which `-Werror`
  makes a refusal to build, and GCC is silent; `adc.h`, `back.h`, `heat.h`
  and `kalman2.h` move by it alone. 23 lines: 13,381 in all.

- `[done]` **An input of more than one cell** (C83). A model reading a vector or a
  matrix at each step -- `y_n = [1 2]*x_n` with `x_n` a column, a filter on
  several channels, a controller reading a sensor vector -- compiles to the
  wrong shape: the compiler compiles a model, not an instance, so it reads
  every input as a single value, and the step takes one double for each.
  `--check` refuses such an instance by name, and no header can be had.

  The model states the size in its signature, by bounds, as a definition by
  cells states its own: `dot(x_n[j<=2])`, a column as `w[j<=3]` is;
  `trace(x_n[j<=2, k<=2])`, a matrix; three bounds a tensor, slices first. A
  bound reads the model's parameters, `avg(d = 3, x_n[j<=d])`, the names its
  body defines and the session's constants, all compiled as constants, but
  not the index, which a cell's bound may: a size that moved from term to term could
  not be compiled, and would exist only to be refused there, so it is refused
  where it is written, `x is an input of grow, so its size cannot read the
  index n`. An input whose size is not stated takes any in the interpreter and
  compiles as a single value, both as now, so `history.ink`'s `swap`, a column
  given to `swap(x_n)`, still answers and no golden moves; making it a single
  value in the interpreter too would refuse that session for a size it never
  had to write.

  In the interpreter, every term of a stated input, whether the argument, a
  default or a history gives it, has that size, or is refused where it is
  read, naming the term and both sizes: `w.x_1 is a single value, where dot
  takes a 2x1 matrix`. A row given for a column is refused as well, as a
  missing transpose is everywhere (*One index is a row*), and a single value
  is not stretched, so a history of a column is a column, `x_n | n < 0 =
  [0; 0]`. An instance is defined without evaluating anything, as now, so
  the refusal is where a term is read, and before any of its cells is:
  `trace(x_n = [n 1]).t_0` is refused for its size, not for a cell outside
  a 1x2 matrix. `?` prints the signature as written;
  `tex` sets the size as a paper states it, `\operatorname{dot}(x_n \in
  \mathbb{R}^{2})`, `\mathbb{R}^{2 \times 2}` for two bounds and
  `\mathbb{R}^{d}` for one read. ℝ follows the paper and is not checked:
  the interpreter takes a complex input as it takes any. Any other bracket
  in a signature, `x_n[j]`, `x_n[2]` or a parameter's, is refused in the
  words that say what a parameter is, which gain the form: `a model's
  parameter is a name, as 'k = 2', or an input, as 'x_n' or 'x_n[j<=2]'`;
  `x_n[j]` states no size, and the compiler, which compiles the model, could
  not infer one. A history by cells, stated or not, is refused as a clause that always
  applies is, `x is an input of sw, so its body can give it only a history:
  a term or a guarded clause`, as its guard chooses cells, not terms
  (C89).

  Compiled, the step takes an input of more than one cell as a pointer to its
  cells, row by row, `const double x[2]`, as a host holds a vector; its window
  is a sequence's of that size, `double x[1][2][1]`, and the step copies it
  in, `memcpy(m_->x[0], x, sizeof m_->x[0])`. The first comment says so: `A
  step takes x_n (2x1), the input at its index. An input of more than one
  cell is a pointer to its cells, row by row.` Taking `const double
  x[2][1]`, the window's shape and a limit's matrix argument's, was
  rejected: in C11 a `double (*)[1]` passed where a `const double (*)[1]` is
  taken is a warning under GCC's `-Wpedantic`, though not Clang's, so a host
  would cast to call it, and a column
  is not held as rows of one. What a sequence of that size does, the input
  does: a cell is read, `x_n[2]` a single value; a size inferred from it is
  its size, so `g_n[j] = x_n[j]*(x_n[j] > 0)` is 3x1 for a 3x1 input; it is
  sampled and held at another rate; and it has a history, C83's refusal of
  one that is not a single value going, `init` writing each cell a history
  gives that is not 0, as it writes a parameter's. A bound that reads a
  parameter compiles it in, as any size does, and the header says so.

  `--check` feeds such an input by its cells, `in_k` holding each step's row
  by row, and the step reads them from the step's first, `&in_1[n * 2]`. An
  instance whose argument has another size than its model states is refused
  in the interpreter's words, where the check reads the input: `v.x_(0):
  v.x_0 is a single value, where dot takes a 2x1 matrix`. Where the model
  states none, `check_matrix_input` stands as it is.

  What stays refused, by name: a history of another size than its input,
  stated or a single value, `cannot compile x: a history of another shape`,
  the words an inner input's already has, so `swap` in `test/cli.cmake` and
  `test/compile/history.ink`, its size not stated, moves to them from `a
  history that is not a single value`; a tensor, `cannot compile x: a tensor`, as any; an instance within
  the model given an argument of another size than its model states,
  `cannot compile inner.x: a single value, where dot takes a 2x1 matrix`,
  and a default of another size, `cannot compile x: a single value, where
  nil takes a 2x1 matrix`, which the compiler can tell, its shapes being
  static. A
  file's input, a name nothing defines, has no signature to state a size in
  and stays a single value. It is a requirement that every header in
  `test/compile/expected` stays byte-identical, and every program `--check`
  writes for an instance whose inputs are single values.

  Rejected. A default giving the size, `m(x_n = [0; 0])`: a default already
  means the stream where no argument is given, and the model compiled with
  it takes no input at all, `m_step(&m)`, so the same text would mean a
  stream in one place and the size of another in the next. A history giving
  it, `x_n | n < 0 = [0; 0]`, which costs no syntax: a model that never reads
  before the stream would state a history it does not need, for its size;
  the clause speaks of `n < 0`, so the interpreter could not hold the stream
  to it without giving one clause two meanings; and two clauses could
  disagree. Inference from use: `[1 2]*x_n` admits a 2xk input, `2*x_n`
  any, the header's interface would move when an unrelated line does, the
  compiler's shapes run from the leaves forward and would need solving
  backwards through products, and the interpreter, which reads the argument,
  would never see it. A shape written as a type, `x_n : R^2`: a second way
  to write a size beside bounds. The instance's argument giving it: the
  compiler compiles a model, which two instances may give different sizes.

  About 85 lines: 35 in the interpreter, parsing the signature 10, the size
  held where a term is read 15 and `tex` 10; 40 in the compiler and 10 in
  the check.

  Specified in `test/data/spec/inputs.ink`, 54 of its 63 entries failing,
  those passing being instances and models without a size echoing themselves
  and an input whose size is not stated; and in `test/compile/inputs.ink`,
  whose instances, reports, header excerpts and refusals are wired with the
  compiler's half.

  Built as specified, every entry passing as written, now
  `test/data/inputs.ink`; `test/compile/inputs.ink`'s ten instances are
  checked, every term exact, and its header excerpts and refusals are held in
  `test/cli.cmake`. A stated input is a definition of its own in the
  instance, as a history makes one, whose terms, the argument's, the
  default's or the history's, are held to the size where they are read, the
  bounds read in the instance; the compiler reads the same bounds as
  constants, `Reference::Stated` serving both as `Reference::Measured` serves
  sizes. No golden moved, nor any expected header or program `--check`
  writes for an instance of single values; `swap` moved as specified, and
  the NaN work changed nothing the header excerpts assumed. Departures: the
  instance `pair` of `test/compile/inputs.ink` is `twain`, as `nan.ink`'s
  check is named `pair`; and a history by cells is refused on any input,
  stated or not, C89's own being one whose size is not stated. Under
  callgrind every workload is as it was to the million instructions but
  `grad`, 0.03% fewer. 154 lines of sources added and 35 removed, 119 more in
  all, where about 85 were planned: 71 in the interpreter, of which the size
  held where a term is read and its words, which the compiler shares, 36, the
  signature 24, the definition holding the size 7 and `tex` 3; 45 in the
  compiler and 3 in the check. 13,500 lines in all.
  Its review fixed three defects of its own: a bound below 1, taken for a
  size of 0 or of 2^64 - 1 rows; `x(a)_n` under `grad`, which read the first
  of no clauses and crashed; and `lim x`, refused, as an input whose history
  is terms alone was (C93). One wrapper now holds an input's history and its
  stated size alike, and `Expanded` no longer unwraps what no expansion
  holds: 5 lines fewer, 13,495 in all. C94, found on the way, is open.

- `[done]` **The interpreter's own error, estimated by `--check`.** Ranked first by an
  outside review: through `exp`, `log`, `tanh` or `lim` the interpreter's
  terms are doubles, so the oracle holds a double to a double, and its report
  says only "inexact ones from there", not how far. `gate` in
  `test/compile/logistic.ink` reports `within 0` at every term; its terms are
  4e-12 from the exact ones, by mpmath, because each sigmoid's `lim` stops at
  the tolerance, and the step is exactly as far. `within 0` is also what a
  step says whose terms have no digit left: the logistic map at 4 from
  `~(1/5)` agrees with the interpreter to the bit for a hundred steps, and
  both are past the tolerance from the exact terms at 26, and 0.95 from them
  at worst. And a limit stopped short can put a guard on the wrong side of
  its threshold, interpreter and step both answering 0 where the exact answer
  is 1, `within 0` again.

  Decided: the check estimates the interpreter's error by running it again,
  disturbed where it is inexact. After the terms it compares, `--check` asks
  the interpreter for each three times more. In every run, each rounding of a
  double is taken the other way with probability one half, as stochastic
  arithmetic does (CESTAC, Vignes): for `+`, `-`, `*` and `/` of real numbers
  an error-free transformation says whether the result was rounded and which
  way, so an exact one is never moved, Two-Sum for a sum and `fma` for a
  product and a quotient's remainder; a result whose rounding is not known --
  a complex operation, a power but one to an exact 0, a factorial, an exact
  number made inexact unless it is a fraction over a power of two whose
  numerator fits 53 bits -- is moved a unit in the last place, up or down
  with probability one half each and never left, unless it is 0, which is
  exact however it was reached: a square moved below it would be negative
  and its root complex. Each limit,
  and each series without an upper bound, is moved by the remainder C36
  estimated where it stopped, `step*r/(1-r)`, or by its last step where the
  one before was 0 and C36 had no ratio, so that a constant series, `gate`'s
  `ex(0)` or `run.y` at 0, is not moved: up in the first run, down in the
  second and either way at random in the third, so a guard near a limit is
  found on whichever side it lies; a matrix limit moves every cell by the one
  remainder its largest cell's steps give, as it stopped by them. The estimate
  of a cell is the largest distance of its three values from its term:
  infinite where a run gives none, or one that is not finite, and 0 where
  every run gives the term itself, as for every exact term no guard on an
  inexact value chose. Drawn by splitmix64 from seeds 1, 2 and 3, in the order
  the terms are asked, so a report is the same on every platform whose `pow`
  and `exp` agree. Disturbing every result, exact ones too (Monte Carlo
  arithmetic), was simulated on `fit` in `test/compile/adam.ink` first:
  2.9e-10 for terms 3.3e-16 from exact, as `exp(0)` and the gradient's exact
  zeros at `w = 0` moved and Adam's `1/eps` took them a hundred million fold;
  with the test, 3.2e-15.

  It is an estimate, not a bound, and says so. C36's ratio is a heuristic: a
  series whose remainder it underestimates passes the rule and is moved by
  too little. Three runs are a sample, and the largest of them errs high:
  simulated, `gate.p` 9.8e-12 for 4e-12 actual, `ball.p` in
  `test/compile/momentum.ink` 6.7e-16 for 2.2e-16, `chaos.x` passing the
  tolerance at 23 to 26 by the seeds for 26 actual. What is neither rounded
  nor stopped is not seen: `pi` and `e` are their nearest doubles, unmoved
  until an operation takes them, and `pow` is trusted to a unit.

  So MANIFESTO's oracle, which compares "against an error bound", is not
  met, and this entry does not claim it: the verdict stays the tolerance,
  the estimate is no bound, and no bound tried stays finite on the models it
  was for -- the review's ball 1.4e-6 on `gate` at step 10 and NaN by 20,
  and 9.5e-8 on `ball` for terms 2.2e-16 from exact; its narrower one past
  the tolerance on `gate` at step 19 and infinite at 66 (rejected below).

  Reported on each sequence's line, after what it says now, where any
  estimate is not 0: `gate.p: within 0; the interpreter's terms about 1e-11
  from the exact ones`, the largest over the terms compared, and `, past the
  tolerance from 23` where it passes the check's own tolerance, a billionth
  of one plus the term, the first step it does: from there `within` compares
  nothing. A line for a term that parts adds its estimate where it is not 0,
  `; the interpreter's term about 2e-15 from the exact one`. "about" is the
  heuristic said once per line, and the program's first comment says how the
  estimate is made and that it is not a bound. The verdict stays the
  tolerance's: widening it by the estimate would let a model whose terms are
  noise pass in silence, where now it parts and the line says why it may not
  be the step's.

  What stays: every value the interpreter prints, and so every golden and
  every expected header, byte for byte; what a session evaluates, which is
  never disturbed, the perturbation being a thread-local pointer in `Number`
  that only `--check` sets, for its three runs; the tolerance, the hundred
  steps, the first line of a report, the flips, which a disturbed run's other
  clause does not move but shows in its term's estimate, and every line of a
  sequence whose estimate is 0. A disturbed run is asked with the memo
  forgotten before and after it, and the guard hook unset; the unnamed
  instances it makes, keyed by the disturbed values they read, are discarded
  after it with the memo, so a run leaves nothing behind; the step is fed
  the inputs as given, while the runs compute the instance's inputs again, so
  their error is in the estimate. A limit `grad` walks is moved too, its value
  and each derivative by the remainder of its own steps: a check reaches
  `grad` only through an input the runs compute, as in
  `lowpass(x_n = grad_(w = n) f(w))`.
  Each program `--check` writes moves, `hold_` taking
  the estimates; the reports of the twelve whose terms are inexact somewhere
  move, `ball`, `cls`, `fit`, `gate`, `head`, `lap`, `mark`, `rnn`, `rough`,
  `run`, `sized` and `stiff`, by each line of a sequence with an inexact
  term, while `head.K`, `head.Q`, `head.V`, `cls.z`, `stiff.h` and `run.high`
  stay, being exact and chosen by no guard near its threshold. `lap.y` and
  `run.y` are limits of exact geometric terms, whose steps C36 reads
  exactly, so their lines are known to the digit: `lap.y: within 0; the
  interpreter's terms about 5.8e-11 from the exact ones` and `run.y: within
  1.3e-26; the interpreter's terms about 9.4e-11 from the exact ones`, the
  largest estimate being that of `x_7`, -3/4, seven times its remainder, as
  C36's ratio is of steps without their sign; that of `x_1`, 3/4, is its
  remainder, 7.6e-11. The rest are held by their
  form, their digits being the seeds'; simulated, `gate.p` is about 1e-11, a
  limit's remainder, and `ball.p`, `fit.p` and `head`'s `S`, `A` and `O`
  1e-16 to 1e-14, roundings. None of the twelve is expected to pass the
  tolerance. README's paragraph on the check gains the estimate.

  Rejected, each measured first by a simulation in Python against mpmath. The
  review's ball, a radius beside each inexact value propagated through every
  operation: ball arithmetic cannot see that two operands are correlated, so
  a recurrence that subtracts consecutive terms or a step that corrects
  itself grows the radius where the error shrinks. Over `gate`, `lim ex`'s
  series multiplies its argument's radius 2.3e5 fold at |z| = 4, where the
  true factor is e^z, 55 at most, and the radius is 1.4e-6 at step 10 and NaN
  by 20; over `ball`, whose `exp` is the prelude's `e^x`, 9.5e-8
  for terms 2.2e-16 from exact by step 99. The check would have compared
  against enclosures too wide to find anything, on the models it was for.
  The review's own, narrower, ball does no better: on a limit's result
  alone, C36's remainder for radius and the series walked on midpoints, its
  argument's radius carried by the series at its two ends. On `gate` that
  radius grows 1.6 fold a step, the descent correcting itself again, past
  the tolerance at step 19 and infinite at 66, whatever the midpoint's
  precision, as the radius is the remainder's and not a rounding's; with the
  argument's radius dropped, 8.8e-12 for 4e-12, but no longer a bound, and
  `ball`, `fit` and `chaos`, which take no limit, have none. An
  exact midpoint rounded to a bounded precision, Arb's own answer to that
  growth: a second inexact arithmetic over the bignum, with `exp`, `log` and
  powers of its own, some 300 lines, and every operation through the
  prelude's `exp` a bignum one, `bench/`'s `hand` and `grad` among them; and
  on `gate` it buys nothing, the series multiplying the radius at every step
  whatever the precision, while for a model of limits the error is the
  tolerance's 1e-10, not the double's 1e-16. Proven enclosures where a
  remainder is known, as `exp`'s Taylor remainder: the sources are not where
  the radius grows. Higher precision without a radius: `long double` is a
  double under MSVC, `__float128` GCC's with a library, MPFR a dependency
  (`CLAUDE.md`, section 5), and double-double some 150 lines before its `exp`
  and `log`, every inexact value twice as wide, again for models whose error
  is the tolerance's. Showing the estimate in a session, `~x  # about 1e-11`,
  which `sum_(k=20) 1/2^k` would want, its nine digits shown and five of them
  wrong: four evaluations of every line, and Deferred's *Numbers the user
  cannot tune* stands as written.

  About 80 lines: 30 in `number.hpp`, the pointer, splitmix64, the error-free
  tests and the moves; 10 in `convergence.hpp` and its three callers, the
  remainder kept and the limit moved; 40 in the check, the runs, the
  estimates and the report; about 13,580 in all. The last three entries
  overran by 40 to 170 per cent, on the reviews' cases; this one's risk is in
  what keeps a value between evaluations, the memo, a literal's cells and a
  history's terms, which a run must neither read nor fill. `--check`
  evaluates every term four times, `fit`'s 20 ms about 80, and 1.6 s about 6
  under the sanitizers; the inexact
  operations test a thread-local pointer, held under callgrind to one per
  cent on `limit`, whose terms are inexact.

  Specified in `test/compile/estimate.ink`: `tie`, a limit that hides which
  side of its threshold a guard is, `chaos`, the logistic map, `dyad`, the
  same from a value a double holds, so seen by roundings alone, and `flat`, a
  power that is 0, with their reports, wired with the implementation, as are
  the twelve reports above. Nothing in the interpreter is specified, as
  nothing it prints moves.

  Built as specified: `test/compile/estimate.ink`'s four instances are
  checked, `tie` and `flat` to the digit, `chaos` passing the tolerance at
  24 and `dyad` at 27, 0.98 and 0.99 from the exact terms at worst; the
  twelve reports moved as specified, `lap.y` and `run.y` to the digit,
  `gate.p` 9.1e-12, `ball.p` 6.7e-16 and `head`'s `S`, `A` and `O` 1.1e-15
  to 7.1e-15, and the six lines named stayed. No golden moved, nor any
  expected header. Departures: the runs come before the terms compared, not
  after, each apart, the memo, the unnamed instances and the guard hook set
  aside and given back, so the terms compared see what they saw before; a
  complex sum is two real ones, so Two-Sum part by part, not a rounding
  unknown; a matrix limit in the third run draws its way cell by cell; a term before a sequence's start has an estimate as a compared one
  does; and a sequence whose estimates are all 0 passes none to `hold_`, as
  for `why`. Under callgrind every workload is as it was or fewer by half a
  per cent at most, the conversion of an exact number now out of line:
  `limit` 873 million instructions for 877, `grad` 5,715 for 5,734.
  `fit`'s check takes 47 ms for 17. Found on the way: the reports of
  `brink`, `ledge`, `rift`, `sill` and `seam` are held by a regular
  expression with a `;` in it, which CTest reads as two, either passing; the
  new ones write it `\\;`. 212 lines of sources added and 42 removed, 170
  more in all, where about 80 were planned: 64 in `number.hpp`, the
  error-free tests, the moves and the draws; 18 in `convergence.hpp`,
  `matrix.hpp` and the callers; 71 in the check, and 17 in the stack setting
  a run apart. 13,672 lines in all.

  Its review found one defect of its own: an exact number past 64 bits was
  moved whenever it was made inexact, over a power of two too, though the
  double holds it, so `y_n = x_n*~1` over `x_n = x_(n-1)/2` was "about
  2.4e-35 from the exact ones" for terms that are exact; such a number is now
  moved as a small one is, unless its numerator fits 53 bits, and `halved`
  in `test/compile/estimate.ink` holds it. A run is set apart by `Setting`,
  now moving what it sets aside, so it is given back however the run ends; a
  limit is moved by `inexact`'s remainder, not by a function of its own; and
  the check walks a term's cells once: 18 lines fewer and the fix 2 more, 154
  more in all, 13,656 lines in all. By the review's ruling, a limit `grad`
  walks is moved as well, where it was left unmoved: `steep` in
  `test/compile/estimate.ink`, twice the derivative of a series in an input,
  said its terms were about 1.1e-13 from the exact ones where they are
  1.2e-10 from them at 2, and now says 1.2e-10, `Convergence::Limit` going
  with no line more.
- `[done]` **`exp`, `log` and `tanh` accurate, bounded and compiled.** The prelude's
  three are each wrong in a way of their own, measured against mpmath.
  `exp(x) = e^x` raises the double nearest e, whose error x multiplies: 340
  units in the last place at 709. `log` is an open series stopped by a
  limit's tolerance, 1.7e-11 from the exact value, reached by halving or
  doubling a reference deep per factor of two, so `log(2^-1074)` and
  `log(10^300)` run out of depth; the compiler refuses it, the halving 64
  calls deep. `tanh(x) = 1 - 2/(exp(2*x) + 1)` cancels near 0: 5.6e-5 from
  the exact value at 1.9e-12, seven digits at 1e-9.

  Decided, the owner's: no built-in elementary function and no libm. That
  answers *A standard library* (Openings) for the prelude: written in
  inkamath, bounded, compilable, and in the interpreter and the compiled step
  the same operations on the same doubles, `+`, `-`, `*`, `/`, `floor` and
  whole powers of two. The design, measured before it was written down:

      exp(x) = expk(x, floor(x*1.4426950408889634 + 1/2))
      exp(x) | x > 1000 = exp(1000)
      exp(x) | x < -1000 = exp(-1000)
      expk(x, k) = (1 + expp(~(x - k*355/512 + k*2.1219444005469057e-4)))*2^(k - floor(k/2))*2^floor(k/2)
      expp(r) = r*(1 + r*(1/2 + r*(1/6 + r*(1/24 + r*(1/120 + r*(1/720 + r*(1/5040 + r*(1/40320 + r*(1/362880 + r*(1/3628800 + r*(1/39916800 + r*(1/479001600 + r*(1/6227020800 + r/87178291200)))))))))))))
      ilogb(x) = ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, -4096, 4096), 2048), 1024), 512), 256), 128), 64), 32), 16), 8), 4), 2), 1)
      ilogbs(x, k, s) = k
      ilogbs(x, k, s) | x >= 2^(k + s) = k + s
      log(x) = logk(x, ilogb(x))
      log(x) | x <= 0 = 1/0
      logk(x, k) = logm(x/2^k, k)
      logm(m, k) = logs(~((m - 1)/(m + 1)), k)
      logm(m, k) | m*m > 2 = logs(~((m/2 - 1)/(m/2 + 1)), k + 1)
      logs(s, k) = k*355/512 + (2*s*logp(s*s) - k*~2.1219444005469057e-4)
      logp(z) = 1 + z*(1/3 + z*(1/5 + z*(1/7 + z*(1/9 + z*(1/11 + z*(1/13 + z*(1/15 + z*(1/17 + z*(1/19 + z/21)))))))))
      tanh(x) = tanhp(x)
      tanh(x) | x < 0 = -tanhp(-x)
      tanhp(x) = tanhk(-2*x, floor(-2*x*1.4426950408889634 + 1/2))
      tanhp(x) | x > 20 = ~1
      tanhk(y, k) = tanhe(2^k - 1 + 2^k*expp(~(y - k*355/512 + k*2.1219444005469057e-4)))
      tanhe(m) = -m/(m + 2)

  `exp` takes k, the whole number nearest x/ln 2, and e^x = 2^k e^r with
  r = x - k ln 2, ln 2 being 355/512 less 2.1219444005469057e-4 (Cody and
  Waite): k*355/512 is exact for every k below 2^44, and so is x less it, so
  r is rounded only by the correction's product. e^r - 1 is its Taylor series
  to r^14 in Horner's form, its remainder below 2^-62 of e^r on |r| <= ln(2)/2,
  where to r^13 it is 2^-57; its coefficients are reciprocals of whole
  numbers, as the compiler refuses `!`. 2^k is taken in two halves, since 2^k
  is infinite from 709.44 to 709.78, where e^x is not, and 0 from -745.13 to
  -744.79, where it is the least double. Past a thousand either way, its value
  there, inf or 0 as C's exp gives, which also keeps an infinite x from
  becoming NaN.

  `log` reduces by ilogb, C's name for what it is on a double: the power of
  two at or below x, by thirteen guarded steps from 2^-4096, so exact, and
  right for every positive double and every exact number below 2^3072:
  `log(10^400)` is 921.034037. From 2^3072 a threshold it tries has more
  than a thousand digits, is approximated to inf, and the number, a double
  only as inf, passes it, so `ilogb` answers 4095 and `log` refuses, as it
  does inf. Eleven steps from 2^-1075, as first sketched, reach 2^972 and no
  further. Guards rather than comparisons taken as values, so that `grad` at
  a power of two takes the guard's side instead of refusing a jump; a
  threshold past a thousand digits is approximated, which a comparison does
  not mark. Like every name of the prelude it is seen
  everywhere, `?ilogb` shows it, and it means something only of a positive
  number. m = x/2^k, in [1, 2), is folded to [√2/2, √2), so that log just
  below 1 does not subtract log m from ln 2; then s = (m - 1)/(m + 1), |s| <=
  0.1716, and log m = 2s(1 + s^2/3 + ... + s^20/21), whose remainder is below
  2^-60. k ln 2 is split as in `exp`, its correction a double so that an
  exact k times it is C's product: the exact product rounds otherwise for 87
  of the 2,099 k of the doubles.

  `tanh` is -m/(m + 2), m = e^(-2x) - 1 taken for x >= 0 as `exp` takes it,
  (2^k - 1) + 2^k(e^r - 1), so that near 0, where k is 0, m is the series
  itself and nothing cancels. Odd by its guard, exactly; past 20, 1, which it
  is to the double from 19.06 on.

  An exact argument is reduced exactly and rounded once, where its reduction
  ends: the `~` on r and on s. A double's operations are C's. The sketch's
  `~x` first, at the entry, rounds the argument and multiplies its error by
  the function's condition: on 2,800 exact arguments, 472 units for `exp`, at
  613363/1000, and 2.2e10 for `log` near 1, 4.8e-6 of the value, where
  reduced exactly it is 0.83, 1.69 and 2.43 units for the three.
  `log(1 + 1/10^12)` would be 1.0000889e-12, the logarithm of the double
  nearest its argument. Nothing the three give is exact, every path through
  them passing a `~`: `exp(0)`, `log(1)` and `tanh(0)` are a double's 1, 0
  and 0, as `exp(0)` is now. `ilogb`'s answer is exact. A guard making
  `exp(0)` exactly 1 was rejected: `grad` refuses a clause that holds at a
  point alone.

  Accuracy over doubles, against expl, logl and tanhl at 64 bits on 2e7
  points each, and against mpmath at 160 bits on 2e4, which agree to the
  third decimal of a unit:

  | | worst | correctly rounded |
  |---|---|---|
  | `exp` on [-708.39, 709.78] | 1.29 units | 88.6% |
  | `exp` below, to -745.13 | 0.89 units of 2^-1074 | 99.1% |
  | `log`, every positive double | 2.55 units | 99.8% |
  | `log` on [1/2, 2] | 2.87 units | 65.6% |
  | `tanh` on [-20, 20] | 2.77 units | 40.2% |
  | `tanh`, \|x\| < 1 to 2^-40 | 2.83 units | 76.5% |

  So `exp(1)` is a unit above e, which is the double nearest it, and
  `exp(1) == e` is 0 where it was 1. Rejected: correct rounding, by tables or
  double-double, several hundred lines; the shape of fdlibm's log,
  log(1 + f) = f - (f^2/2 - s(f^2/2 + R)), within 1.24 units for 2.87, at a
  helper and two operations more; a high part of ln 2 of 32 bits, 1.18 units
  for 1.29; tanh as m/(m + 2) with m = e^(2x) - 1, 3.14 units; an odd
  polynomial near 0, nineteen terms to reach 0.55.

  Identical, measured: the design emulated in C, built by GCC 13 and Clang 18
  at -O0 and -O2, and in Python with exact fractions wherever the interpreter
  is exact, against an interpreter given this prelude: 9,348 doubles alike to
  the bit in C and the interpreter, the 87 k among them, and 3,500 arguments
  in Python and the interpreter, exact ones among them. They part only where
  a C compiler fuses a multiply and an add, on hardware that has it (AArch64,
  x86 with -mfma or -march=native): each function of the prelude is one C
  expression, which Clang's default fuses within, even as strict C, as GCC's
  GNU modes fuse across, so 18 per cent of results part under either, half
  of `exp`'s over its range, worst errors within 0.05 units of the unfused
  ones; the header already says so (*Fused multiply-adds*). `2^k` is
  `pow(2.0, k)`, as any power compiles, and `exp2(k)` under Clang. The
  interpreter calls `pow` for a double k, in `exp` and `tanh`, but shifts an
  exact one, as `log`'s always is, so the two agree only where `pow` and
  `exp2` are exact for every whole k from -1100 to 1100: assumed, and
  `compile_powers` holds it on every platform CI builds.

  `grad` differentiates the definitions: floor's derivative is 0 and every
  guard takes its side at its threshold, so 0, 1, 2, every power of two and
  the fold at √2 answer; `grad_(x = 2) log(x)` is 0.5. floor's argument is a
  whole number at a few doubles near (j + 1/2) ln 2, where `grad` says floor
  jumps: at no p/q with q <= 1000 and |x| <= 50, searched. Past a thousand
  `exp`'s derivative is 0, a constant's, and past 20 `tanh`'s; `log`'s at
  2^-1074 is NaN, its derivative overflowing. NaN is refused by `exp` and
  `tanh` at their first guard, `a comparison needs a number`, as by `log`
  now; they answered NaN. An infinite x reaches `log`'s fold as NaN and is
  refused so too, where the step answers NaN and libm inf. `tex ?expk`,
  `?logm`, `?logs` and `?tanhk` refuse their `~`, and `tex ?exp` showed its
  constant as `~1.44269504` (C96).

  Compiled where called, as every function is, a definition writes its
  argument again at each reading: a header stepping `exp(a*y_(n-1) - 1)` is
  4.9 KB of C, with `tanh` 15.7 KB, and with `log` it exhausts 4 GB, each of
  ilogb's steps tripling the last. So a call to a function of the prelude
  compiles to a C function of the header's own, `<header>_<name>(double
  arg_x, ...)`, its parameters named as a limit's are, emitted once before
  the step and the limits' functions, the prelude's calls inside it so too,
  and one whose arguments are all constant folded as now: `exp(1000)` is
  `INFINITY`. The step then reads as the
  prelude does, `ball_exp(-(...))`. Rejected: temporaries for an argument
  read twice, as `Shared` gives a matrix's operand, which hoisted before the
  step's expression compute both sides of a guard, and which a limit's
  function does not have; inlining where every argument is a name or a
  number, which moves no expected header but compiles `exp(x_n)` and
  `exp(-x_n)` two ways; a C function for every function, which moves every
  header that calls one.

  What moves, found by running this prelude, each in the implementation's
  commit: no golden in `test/data` and no session of `README.md`, nine digits
  hiding the rest, but README's paragraph on the prelude (section 5);
  `test/compile/expected/kernel.h`, whose `ceil(x_n)` becomes
  `kernel_ceil(m_->x[0])` and its function, and the comment of
  `test/compile/kernel.ink`; `compile_log_refused` in `test/cli.cmake`,
  whose `lg` now compiles, printing nothing; the programs `--check` writes
  for `ball`, `fit`, `head` and `sized`, `exp` a function there, and their
  reports' estimates, `ball.v` 1.1e-15 to 8.3e-16 and the like, and `fit`'s
  `within`s, as its first arguments are exact and now reduced exactly:
  `fit.a` 5.6e-17 to 8.3e-17. No test holds those digits. `gate`, `cls` and
  `rnn` write their own series and do not move. The interpreter is slower:
  `exp` 10 to 18 µs a call for 5, `log` about 50 for 24, a dozen calls of
  the prelude each and log's thirteen exact powers; `bench/grad.ink` 0.81 s to
  3.0 s and `bench/hand.ink` 0.022 s to 0.064 s.

  Rejected besides: built-ins over libm, the owner's refusal, two arithmetics
  where one is wanted and no identity between interpreter and step; an
  `ilogb` built-in, which thirteen guarded steps make unneeded; keeping the
  series, bounded by `lim`'s tolerance, unbounded in its terms and refused by
  the compiler.

  About 70 lines of sources: the prelude from 6 lines to 21 and a comment of
  ten, and some 45 in the compiler for the functions. Specified in
  `test/data/spec/elementary.ink`, 31 of its 72 entries failing, those
  passing being values today's prelude already gives to nine digits and its
  refusals; and in `test/compile/elementary.ink`, whose report, every term
  `within 0`, and whose functions are wired with the compiler's half.

  Built as specified, every entry passing as written, now
  `test/data/elementary.ink`, and the spec suite gone. `els` in
  `test/compile/elementary.ink` is checked, its five terms within 0 of the
  interpreter's, and its program defines `els_exp`, `els_log` and
  `els_tanh` and raises no power of e. A call to a function the built-ins'
  scope holds but `floor`, on single values not all constant, is
  `CompileC::Prelude`: the body compiled once, over `arg_` names, outside
  any limit and with no temporaries, and emitted before the limits'
  functions; a constant call folds, and one on a matrix or by named
  arguments is written where it is called, as before. Moved as specified,
  and nothing else: `kernel.h`, the programs of `ball`, `fit`, `head` and
  `sized` and their estimates, README's paragraph; no golden. The step is
  the interpreter's to the bit without contraction, which the header asks
  of its build (*Fused multiply-adds*) and CI's x86-64 gives; the test
  targets' `-ffp-contract=off` is C97's. By the review's rulings: 2^k is
  built by a shift up to 2^3321, and a ratio of two powers of two converted
  by `ldexp`, the review's 300,000 doubles and 644 exact arguments still
  alike to the bit; `log(~1/0)` and `log(2^3072)` name a NaN never written,
  inf over 2^4095 approximated to inf, a known limit, as a guard saying
  otherwise costs more than three lines and moves two entries; a few doubles
  near (j + 1/2) ln 2 are refused by `grad`,
  `grad_(x = ~0.3465735902799727) exp(x)` saying "floor jumps"; and `grad_(x = 10^400) log(x)` is 0, not NaN as the ruling
  had it, 1/x being below the least double. Departure: `compile_log_refused`
  is `compile_log`, as it refuses nothing. Under callgrind `grad` goes from
  5,750 million instructions to 14,870, 2.59 times, 17,434 before the shift;
  `hand` from 93 to 279, 3.0 times, which the shift does not help, its exact
  arguments' conversions not being of powers of two; `read` from 10 to 11;
  `deep`, `harmonic`, `limit` and `matrix` within 3 million. 94 lines of
  sources added and 7 removed, 87 more in all, where about 70 were planned:
  28 in the prelude and its comment, 45 in the compiler and 14 in `Number`.
  13,743 lines in all.

  Its review held the step to the interpreter through the compiler itself:
  507 instances of a model calling `exp`, `log`, `tanh` and `ilogb` on
  doubles fed by `--check`, some 50,000 arguments over every range, built by
  GCC 13 and Clang 18 at -O2 without contraction, 134 of them at -O0 and
  -O3 too, every term within 0; `exp`'s to its first infinity only, where
  the check stops. An exact argument the step reads as
  its double, whose rounding `exp` multiplies by x: 13 units of e^21 from
  `a = 1/3`, 189 of e^657, which `test/compile/elementary.ink` now says.
  Clang writes `pow(2.0, k)` as `exp2(k)`, and `log`'s k is exact in the
  interpreter, its 2^k a shift, so the step agrees where `pow` and `exp2` are
  exact at whole k, as glibc's are from -4096 to 4096, not because both call
  the same one: a `pow` a unit off there, preloaded into both, parts `els.l`
  and `els.g` alone. By the review's ruling `compile_powers` holds both exact
  from -1100 to 1100. Compiled, `log` takes 235 to 416 ns a call where libm's
  takes 6, ilogb's thirteen steps each calling `pow` twice in a header that
  writes NaN: a later item, by the review's ruling. The review found C99,
  `--check` estimating a term whose runs part by more than 2^512 infinitely
  far, `exp`'s from 390, and registered C100 and C101 open; held `els`'s
  report to its five lines; and joins a prelude call's arguments as they are
  emitted, every header alike: 5 lines fewer, 82 more for the item, and
  C99's 2; 13,740 lines in all.
- `[done]` **A float target, `--float`.** Ranked first of what had not moved by an
  outside review: the oracle is worth what separates the target from the
  reference, and with doubles held to a billionth `--check` checks that the
  code generator agrees with the interpreter, not how far a deployment is
  from the exact terms. `MANIFESTO.md` names float32 first among the number
  types for targets, and asks what carries the annotation.

  Decided: the compilation carries it. `--float`, beside `--compile` or an
  instance's `--check`, writes floats where the header writes doubles:
  `inkamath --compile pid.ink --float -o pid.h`, `inkamath --check drift.ink
  calm --float -o calm.c`. The model is the paper's and the number type the
  deployment's, so one file serves a controller compiled for a desktop in
  double and for a microcontroller in float, and the question the check
  answers is the one asked: how far is this float deployment from the exact
  reference. Elsewhere it is refused, `inkamath: --float takes --compile, or
  --check with an instance`. Rejected: an annotation of the model, which
  writes the deployment into the paper's text and needs a file per target;
  a type per sequence or per parameter, the mixed precision of a float
  filter with a double accumulator, which is a real need, but one no model
  here has, and whose syntax would be a second way to say what a flag says
  for the whole; a `typedef` the host chooses with a macro, which makes the
  arithmetic built one of two that `--check` could not both have checked,
  and needs a macro for every literal and every function, `floor` or
  `floorf`, in a header meant to be read; `long double`, a double under
  MSVC, 80 bits on x86 and 128 on AArch64, three targets under one name, and
  closer to exact than the interpreter's own limits are. Fixed point and int8
  are out of scope: their scaling is information about each quantity, which
  an annotation would carry, and nothing here forecloses one.

  The header, `test/compile/expected/float/pid.h` against `pid.h`: every
  `double` a `float` -- the fields, the step's arguments, `const float x[2]`
  for an input of more than one cell, temporaries, the functions of limits
  and inverses, the prelude's functions where they are compiled as C ones,
  and the index read as a value, `(float)m_->index_`, exact to 2^24 steps;
  `long long index_` stays. Every constant is the float nearest the double
  written today, in the shortest digits that read back as it, with `f`:
  `0.1f`, `1.0f`, `-2.0f`, `1e+06f`, and `INFINITY` past `FLT_MAX`; so is
  every literal the compiler writes of its own, `0.0f` and `1.0f` in a
  guard's test, since one `0.5` left in makes C compute its expression in
  double. `floorf`, `powf` and `fabsf` replace `floor`, `pow` and `fabs`;
  `isnan`, `NAN` and `INFINITY` are float already, so a NaN-aware header is
  aware the same way. The interface comment adds "Every value is a float,
  and every operation rounds to one", the first line names `--compile
  --float`, and a history term no float holds is refused as one no double
  holds is, in a float's words: "where x's history gives a term no float
  holds", 10^39 among them. Built with GCC's and Clang's `-Wdouble-promotion`
  and `-Wfloat-conversion`, both hand-written headers are silent, and a
  `0.5` in an operation of either is not: the tests build every float header
  so, which holds that no operation is done in double. A double constant
  only assigned, `m_->q = 0.25`, passes both silently, and Clang's even
  where it is inexact, `m_->dt = 0.1`; it is the float its `f` would have
  written, so nothing is lost but the reading. Rates, histories, sizes and guards are as they
  are, in floats.

  A limit compiled in float walks the interpreter's rule, and one disjunct
  more: a step no larger than twice `FLT_EPSILON` times its term, a finite
  one, ends the walk, `if (step_ <= 2 * FLT_EPSILON * fabsf(t_) &&
  isfinite(t_) && stepped_) return t_;` before the rule's test, or for a
  limit of matrices every cell's step within twice `FLT_EPSILON` times that
  cell, a finite one, and `<float.h>` included where a limit is. The rule's
  1e-10 is below a float's spacing at any term past 8.4e-4, and Newton's iterates
  close on two floats a unit apart rather than on one: `stiff` in
  `test/compile/newton.ink`, held to the rule alone, is NaN from step 25 on,
  and with the disjunct within 1.4 units of a float at every step, stopping
  after 2 or 3 terms. A series stops where its sum stops moving, as it did,
  `gate`'s at 24 terms at most. The disjunct answers where the interpreter
  refuses a walk that creeps by a unit or two of a float at each term: `crawl` in
  `float.ink`, 10^8 moved by 8, is 100000016 where the interpreter says it
  does not converge, as a double header answers one that creeps by less
  than half a unit, whose step is 0, by the rule alone (C102). "A term the
  interpreter would refuse is NaN" is untrue of either, and `--check` parts
  at both. Rejected: the rule alone, which makes a float
  deployment of any iteration look broken where it is not; a float tolerance
  in place of 1e-10, a second constant where the disjunct is the float's own
  limit; the disjunct in double too, which would answer where the interpreter
  refuses, past 4.5e5 where 1e-10 is a double's spacing, a change to the
  language's rule and not to a target.

  The prelude's `exp`, `log` and `tanh`, written in inkamath (next in line),
  need no version of their own in float, measured on every float, in C with
  floats operation by operation against the double libm: `exp` within 1.16
  units of a float on [-87, 88] and 0.79 of the least subnormal below, `log`
  within 2.85 on [1/2, 2] and 1.56 elsewhere over e^±80, `tanh` within 2.61
  on [-10, 10]. Sampled at 2e4 points in numpy's float32, the farthest were
  1.04, 2.67 and 2.48.
  `k*355/512` is exact in a float for |k| below 47,000, and `exp`'s k never
  passes 1443; its correction, rounded to a float, moves r by 2.5e-10 at most
  where e^x is a float other than 0 and inf; `ilogb`'s thresholds overflow to
  inf and underflow to 0 in a float as in a double, and decide alike. Their
  polynomials' degree is a double's, which costs a float target time and not
  accuracy. One definition, less accurate compiled in float: that is what
  `--check --float` measures.

  `--check --float` writes the program for that header. Its inputs are the
  floats nearest the interpreter's, written as constants are, `static const
  float in_0[100]`, and its terms are kept as floats and read as doubles,
  `(double)got[k]`. A float cannot meet a billionth: a term parts where it is
  more than a thousandth of one plus the interpreter's term from it, three
  of a float's seven digits asked and four, 2^13 units at 1, left to what a
  hundred steps accumulate. Each line then says how far its sequence came in
  units of a float too, the most any term came in the spacing of floats at
  the interpreter's term or at 1 where the term is smaller, so not always
  the distance before it: `calm.v: within 1.1e-06, 1.4 units of a float`,
  whose 1.1e-06 is 1.2 units at its own term. That is the comparison with
  the target's own rounding the billionth never made. The estimate of the
  interpreter's own error is unchanged and follows, so a reader sees the
  reference's 1e-11 beside the step's 1e-7, and says where it passes the
  thousandth. A step's term that parts is shown to nine digits, which tell floats apart; the first
  line says `in float`; flips and margins are as they are. The verdict is
  the tolerance's, as in double: `wild` and `far.d` have lost every digit
  and part. Rejected: a report alone, which would pass them; the billionth,
  which nothing passes; a tolerance in units, a second normalisation beside
  one plus the term; holding the float step to the double one, agreement
  between targets, which `MANIFESTO.md` declines as a goal.

  Known limits. A constant is the float nearest its double, the float
  nearest its exact value but where that double falls halfway between two
  floats, about one constant in 2^29. A target that computes floats in
  double or wider, `FLT_EVAL_METHOD` 1 or x87's 2, parts from the step
  checked, as fused multiply-adds do, and the header's first comment already
  says to build the check as the step is built. Everything refused in
  double, tensors and complex numbers among them, is refused in float; every
  double header and program is byte for byte as now.

  About 50 lines of sources: 25 in the compiler, a flag through `Build`, the
  type's word, the literal's digits and suffix, the functions' names, the
  limit's disjunct, `<float.h>` and the two sentences, the rest of the
  fifty-odd places that write `double`, `0.0` or `floor` changed rather
  than added; 15 in the check, the inputs, the terms, the tolerance, the
  units and the nine digits; 10 on the command line.

  Specified in `test/compile/float.ink`: from earlier files `calm`, `wild`
  and `ledge`, `gate`, `ball`, `stiff` and `fan`, and of its own a
  resonator at two radii, `hum` and `drone`, `far`, large coordinates,
  `tally`, a count past 2^24, `crawl`, a limit that creeps, and `steep`,
  exp past a float's range,
  with their reports, worked out in C with floats and in numpy's float32,
  which agree to the bit, against exact terms; and excerpts of a header
  with a vector input, of a limit and of a check program. Whole headers in
  `test/compile/expected/float`, `pid.h` and `adc.h`, hand-written. All are
  wired with the implementation: those headers compared byte for byte, and
  each instance checked by a program in `compile/float`, built with
  `-Wdouble-promotion` under GCC and Clang, `wild`, `ledge`, `far`,
  `crawl` and `steep` expected to fail.

  Built as specified: every report as `float.ink` gives it, the two headers
  byte for byte, and every double header and check program byte for byte as
  before. The header's own words for a double -- `double`, `fabs(`,
  `floor(`, `pow(`, `0.0` and `1.0` -- are rewritten by one pass over its
  text past the line naming its source, where the fifty-odd places that
  write them would each have changed; each constant is the float `Double`
  writes. Departures: a limit's comment still says it is taken "as the
  interpreter takes it", "within 1e-10f"; an input whose sign bit is set is
  written `-` before its magnitude, so a negative NaN is `-NAN`; `pid.h` and
  `adc.h` in float are built alone, as C, under GCC and Clang only; and
  `--help` names `--float`. The prelude in float, sampled at 2e6 points
  against the double libm: `exp` 1.15 units on [-87, 88], `log` 2.83 on
  [1/2, 2] and 2.46 over e^±80, `tanh` 2.49 on [-10, 10], each within the
  figures above. Under callgrind every benchmark within 0.1 per cent. Found
  on the way and left to a ruling: `mark` in `test/compile/steady.ink`,
  power iteration, closes in float on a cell near 0.52 that alternates
  between floats two units apart, more than `FLT_EPSILON` times the cell,
  and is NaN at 33, so the disjunct does not end every iteration a float
  cannot settle. 66 lines of sources more, where about 50 were planned: 35
  in the compiler, 22 in the check and 9 on the command line. 13,806 lines
  in all.

  The review found the disjunct taking a walk past a float's range for one
  that closes, a step of inf being no larger than `FLT_EPSILON` times an
  infinite term: `run` in `test/compile/nan.ink` was -inf at 14, where the
  interpreter does not converge and a double header is NaN. The term must
  now be finite, `blast` in `float.ink`; a line more, 13,807 in all. By
  its ruling the disjunct takes twice `FLT_EPSILON`: `mark`, and `cls` in
  `softmax.ink`, Newton's method on a series, alternate two units apart and
  were NaN at 33 and at 2; no other report of the 80 instances moves. With
  C103 and C104, found on the way, 13,810 lines in all.
- `[done]` **The prelude called compiled, for a double.** Since `exp`, `log` and `tanh`
  are written in the prelude, the interpreter walks their definitions node by
  node: under callgrind `bench/hand.ink` went from 93 million instructions to
  279 and `bench/grad.ink` from 5,750 to 14,870. The compiler already writes
  each as a C function whose doubles are the interpreter's to the bit.

  Decided, the owner's in principle: the prelude compiled once by
  `--compile`, checked in, and called by the interpreter for a call of the
  prelude's own `exp`, `log`, `tanh` or `ilogb` on a real double. Identical by
  construction, as the step is to the interpreter: no value moves, and two
  kinds of refusal become answers (below).

  The header is `include/inkamath/inkamath_prelude.h`, what `inkamath
  --compile include/inkamath/inkamath_prelude.ink -o inkamath_prelude.h`
  writes. That file's one sequence calls the four on its index, so the header
  holds `inkamath_prelude_exp(double arg_x)`, `_log`, `_tanh` and `_ilogb`
  and their ten helpers, `static inline`, and a struct, an init and a step
  nothing calls, some 20 of its 108 lines. Named so that its guard is
  `INKAMATH_PRELUDE_H` and its names are prefixed: `prelude` is the
  interpreter's array of the prelude's lines, and `compiled` a variable in
  `check.hpp`. `interpreter.hpp` includes it as it is, C compiled as C++:
  operations on doubles, `pow`, `floor`, `isnan`, `NAN` and `INFINITY`, the
  same in either. No `extern "C"`, as nothing static is linked, and no
  namespace, as `<math.h>` cannot be included inside one and the prefix
  scopes already; GCC 13 and Clang 18 build it at `-Wall -Wextra -Wpedantic
  -Werror` without a word. Checked in, so building the interpreter needs no
  interpreter; kept in step as the goldens are, by `record_prelude`, a target
  writing it with the interpreter built, and `prelude_header`, a test
  comparing what that interpreter writes now with the file, `compare_files
  --ignore-eol`. A change to the prelude, or to how the compiler writes it,
  fails until the header is recorded in the same commit, its diff read.
  Generated, so counted apart from the sources.

  Recognised by its home, as `mod` and `floor` are: once the interpreter has
  read its prelude, it gives its stack the four definitions the built-ins'
  scope holds and a function for each, given for `Number` and for no other
  number type (C65). A session's, a file's or a model's definition of one of
  the names is another definition, a guarded clause as much as a plain one:
  a session's clause starts a definition of its own, as a model's or a
  file's does, rather than extending the prelude's.
  Every other call pays one comparison of its home.

  Asked in `Reference::Eval`, once the arguments are evaluated in the caller's
  scope and before the memo, for a call neither indexed nor a limit. Taken
  where the one argument is a single value, inexact, not approximated past a
  thousand digits, with an imaginary part of 0 and a finite real part,
  positive for `log` and `ilogb`, and outside every run of `--check`; and
  where the function's answer is not 0. Anywhere else the definition is
  walked, as now. Answered as `Number(double)`: inexact, not approximated, its
  imaginary part +0. That is what the definition answers there: each power of
  two it meets is within the thousand digits, `ilogb`'s thresholds 2^-2048 to
  2^2048 for a positive double, and every operation of two real values answers
  a real double with an imaginary part of +0, whatever the sign of the
  argument's 0. `ilogb` answers the exact whole number it is in the
  interpreter.

  Each exclusion is where the definition answers otherwise than C:
  - an exact argument, reduced exactly and rounded once where C rounds it
    first: `exp(2/3)` is ~1.947734041054676 and `exp(~(2/3))`
    ~1.9477340410546757;
  - an approximated one, whose answer the definition marks so;
  - a complex value or a matrix, which the definition refuses;
  - inf and NaN, which its guards answer or refuse (`log(~1/0)`, C101) where
    C gives inf or NaN; and for `log` 0 and below, refused as `1/0`, with
    `ilogb` kept to the same domain (C100);
  - an answer of 0. C's minus makes the compiled `tanh(~0)` -0 where the
    definition's is 0 (C98), so the definition is asked rather than a sign
    assumed. Of a finite double the three give 0 only at `tanh(±0)`,
    `log(~1)` and `exp` below -745.13. Gone with C98's fix, which gives
    every such 0 the definition's sign, by a sweep of 144,545 arguments,
    19,176 of them answering 0, each 1/f the walk's;
  - `grad`, whose walk carries parts through the definitions and never
    passes here;
  - every run of `--check`: one estimating the interpreter's own error must
    see the definition's roundings, `check_els_report` failing where those
    runs are compiled, and the main run walks so that `check_els` stays the
    standing proof that compiled and walked agree. Told by the stack's
    `guards`, which `CheckC::Program` sets for the main run, and
    `Number::disturbed`, set for the others.

  The memo is neither read nor filled. A compiled call costs less than a
  key's hash, and the walk filled the memo with each call inside it, `expk`,
  `expp`, thirteen `ilogbs`, so it now turns over more slowly (C69), which
  only keeps more.

  Steps and depth: the call is one step and one reference deep, and its
  arguments cost what they cost, as before the walk; the walk is gone. With
  nothing memoised the walk is some 33 steps for `exp`, 111 for `log` and 84
  for `ilogb`, and nests 3 references deeper for `exp`, 5 for `tanh`, 14 for
  `ilogb` and 16 for `log`. So a line refused for its million steps or its
  256 references because of these walks may now answer:
  `sum_(k=1)^40000 exp(~k/40000)` is ~68732.1323 where it gave up. Nothing
  that answered moves or is refused, as the compiled call never costs more
  than the walk. A refusal by steps was already a function of what the
  session had memoised, a call asked twice costing one step the second
  time. Rejected: charging each call the steps its walk would take, which
  depend on the memo and the path; walking near the depth limit, identity
  of depth for a line where steps cannot have it.

  Contraction. The interpreter's targets build with `-ffp-contract=off`
  under GCC and Clang, set on the `inkamath` interface library: both contract
  a multiply and an add within an expression in C++20 where the target has
  the instruction (`-mfma`, `-march=native`, AArch64), and each function
  here is one expression. Measured with `-mfma` and without the flag, 1,215
  of 160,000 answers of a sweep part from the walk; with it, none. The C
  tests' flag is C97's. MSVC is left as it is: since Visual Studio 2022 its
  default, `/fp:precise`, does not contract whatever `/arch` says, where
  2019's could at `/arch:AVX2`; `/fp:fast` or `/fp:contract` would.
  `pow(2.0, k)` is the interpreter's own `pow` of a double k, held exact by
  `compile_powers`.

  Measured on a prototype that can walk instead: 63,000 doubles over every
  range under GCC 13 and 20,000 more under Clang 18, at -O2, each given to the
  four, and 1 over each answer for the sign of 0, printed at 17 digits, alike
  compiled and walked. Every golden, header and report of the 152 tests as
  before. Under callgrind `hand` from 281 million instructions to 85, below
  the 93 before the prelude was written; `grad` 14,821 to 14,839, its calls
  on parts; `limit` and `read` within 0.1 per cent. A call in a sum: `exp`
  0.16 µs for 8.8, `tanh` 0.20 for 12, `log` 0.62 for 36, some 55 times
  faster. Expected of the implementation: `hand` below 93 million, `grad`
  within 1 per cent, the others within half of one. Every run of
  `--check` still walks.

  `log`'s `pow`: compiled, `ilogb`'s thirteen steps call `pow` twice each,
  0.34 µs of `log`'s 0.39 (the later item the review of `exp`, `log` and
  `tanh` ruled). Not first: it would halve `log` alone, it is the
  compiler's and moves every header that calls `log`, and this header
  follows it by `record_prelude`.

  `grad` keeps its 2.6 times, and no later item is planned: its jets need
  every intermediate of the walk, which a compiled call does not keep.

  Rejected besides: libm's `exp` and `log`, the owner's refusal, and not
  the prelude's values; a hand-written `ilogb` in C++, withdrawn, a second
  definition beside the prelude's; compiling at run time, which needs a C
  compiler where the interpreter runs, a dependency (`CLAUDE.md`, section
  5); the header generated at build time, the interpreter needed to build
  the interpreter; `--compile --prelude`, writing the functions alone,
  lines in the compiler to drop 20 lines of dead C.

  About 30 lines of sources: 20 in `interpreter.hpp`, the include, the test
  of an argument and the four given to the stack; 6 in
  `reference_stack.hpp`, holding them; 3 in `Reference::Eval`, the call.
  Besides, 3 in `CMakeLists.txt`, the flag, and 12 in `test/CMakeLists.txt`,
  the target and the test; README's paragraph on the prelude gains a
  clause.

  Specified in `test/data/spec/fastprelude.ink`, 64 entries, 6 failing:
  values at chosen doubles to 17 digits, of the design's arithmetic
  emulated in Python apart from the interpreter, and its zeros; exact
  arguments beside their doubles; complex, matrix, non-finite and
  out-of-domain refusals; an approximated argument; `grad`; two refusals by
  steps that become answers and one of exact arguments that stays; four by
  depth that become answers, and four that stay, of two exact arguments,
  an approximated one, and an `exp` the session gives a clause, which
  since `trig` starts a definition of its own. Taken out of the
  prototype one at a time, each exclusion fails an entry, or
  `check_els_report` for the estimating runs. `inkamath_prelude.ink` is
  wired with the implementation, as are the header, the target and the
  test.

  Built as specified: every entry passes as written, and the spec is the
  golden `fastprelude.ink`; no other golden, header or report moves.
  Departures: the stack holds one function, `compiled`, not the four
  definitions and a function each; the interpreter gives it, holding the
  four's addresses, and it asks every test of the argument, so that
  `Reference::Eval` asks only the home, one argument, no index and no
  limit. A run of `--check` is told by `guards` and `Number::disturbed`, by
  the review's ruling, and held by `check_walked` in `cli.cmake`: an input
  read through `log` 245 references deep, refused as walked where compiled
  it would answer. `prelude_header` compares the file with what a custom
  command writes on each build: 16 lines in `test/CMakeLists.txt`, not 12,
  and the flag 4 in `CMakeLists.txt`, not 3. Measured on the build: the
  review's sweep, 75,104 calls of the four on doubles over every range, each
  with 1 over its answer, printed at 17 digits, alike compiled and walked
  under GCC 13, Clang 18 and GCC with `-mfma`; under callgrind `hand` from
  280 million instructions to 85, `grad` 14,835 to 14,876, `limit` 876 to
  877, the others as before. 43 lines of sources where about 30 were
  planned: 34 in `interpreter.hpp`, 5 of them includes, 5 in
  `reference_stack.hpp` and 4 in `Reference::Eval`. 13,853 lines in all.
- `[done]` **`grad` compiled.** `--compile` refuses `grad`, "a derivative, for now",
  so every training loop in `test/compile` writes its gradient by hand, and
  the derivative the language checks is not the one deployed. The
  conformance suite asks for both, backpropagation by hand beside the
  gradient construct's.

  Decided: forward, as the interpreter takes it and as differentiation's
  entry foresaw, first derivatives only. Under `grad`, each value compiled
  carries its part: cells of its own shape beside its cells, absent where
  the interpreter's is, of a constant, `floor` or a comparison. A part is
  made by the interpreter's rule for the node, in its order, out of the
  compiler's own sums, products, quotients and powers, a constant part
  folded exactly as the interpreter's is: a product's `a'*b + a*b'`, a
  quotient's `(a' - q*b')/b`, q the quotient, a constant power's
  `c*u^(c-1)*u'` with `pow` as written, `e^w`'s `e^w*w'`; a negation's part,
  and a quotient's whose numerator has none, a subtraction from 0, `0.0 -
  q*b'`, as C98, which lands before this, has every negation. So the gradient
  the step computes is the interpreter's to the bit wherever its values are,
  and `--check` holds it within 0. One with respect to a matrix is a pass
  over the body per cell, its seed a constant, as in the interpreter; a
  matrix's with respect to a single value has the matrix's shape. A grad
  whose point and body read only constants folds, exactly.

  What compiles: a point that is any value the step has, a term, a
  parameter, an input; a body through arithmetic, transposes, literals,
  cells and rows read, sums with constant bounds and functions compiled
  where called, whose guards choose a clause as for the value, each chain of
  values with its chain of parts beside it, so the clause that holds gives
  the slope; and the prelude. A grad inside a limit's terms, Newton's step by its own
  derivative, and one in a guard, a clipped gradient, are compiled as
  anywhere.

  Written where it is called, the prelude's part would write its argument
  again at each reading, as its value did before it was a function of the
  header's (*`exp`, `log` and `tanh` accurate*). So a function of the prelude
  called on a value that moves has a C function for its part beside the one
  for its value, emitted once: `fall_exp_dx(double arg_x, double part_x)`,
  named with `d` and each parameter that moves, its parameters those its
  part reads and then the parts. `ilogb`'s answer is a choice of constants,
  so it has no part and no such function. Where an argument moves, its
  value is a function too where that tests for a jump, `mod_ja`, a
  `floor`'s or an equality guard's, as the interpreter refuses there (C117).
  Taking one function for the
  derivative, times the argument's part, was rejected: the interpreter
  carries the part through each operation, and doubles do not distribute,
  so the step would part from it by a unit here and there.

  Where the interpreter refuses for the point's sake, the step is NaN,
  written in the value where the refusal is made, so that it reaches every
  term reading it as any refusal does: `floor` of a value that moves, where
  it is whole, `(floor(u) == u ? NAN : floor(u))`; a comparison read as a
  value whose sides move, where they meet; a guard's `==` or `<>` whose sides
  move, where they meet, its clause then taken or left at the point alone; a
  power of a moving base whose derivative is infinite there, at a base of 0.
  In a header that writes NaN, a gradient is NaN where the value of what it
  differentiates is, since only its part is kept; a clause refused wherever
  it is taken, `log`'s `1/0`, is NaN in its part too. A refusal the shapes
  decide stays one, in the interpreter's words: a body that does not read the
  name, a definition reading the global of it, a Jacobian, an exponent that
  changes with the name over any base but `e`, a body through an instance. An
  index or a size that moves is refused as a place or a size that is not a
  constant already is. Refused for now, as no model asks: a derivative of a
  derivative, which needs four parts; of a limit whose arguments move, which
  walks the parts too; of a matrix power; of a power whose exponent is
  not a constant, which five lines would compile, its part absent where the
  exponent is 0 by the conditions below, but which no model asks; and through a definition by cells, whose guards
  the compiler took as constants only, until *Guards on cells at run time*.

  `--check` needs nothing new. The interpreter's terms are its grad's,
  walked by `derivative.hpp` in every run, the disturbed ones too, as
  `steep` in `test/compile/estimate.ink` already shows. A flip is followed
  for a sequence's guarded clauses only, so a guard inside the body, a
  ReLU's, parts only by its values, as one inside a function called; a
  guard reading a gradient is followed as any guard. In float the parts are
  floats as the values are, the part functions rewritten with the rest.

  A part that comes through a chain of guarded clauses is there by the
  clauses, 0 where the clause taken has none, and carries beside it where it
  is there at all: the chain's own conditions, 1 for a clause with a part
  and 0 for one without, `relu`'s `z < 0 ? 0 : 1`. A `floor`, a comparison
  or a power reading the value tests for a jump only where that holds, as
  the interpreter, seeing no part, tests none: a dead ReLU unit's mask,
  `t*(relu(t) > 0)` at t = -1, a clamp compared at its level and the floor
  of a clamp answer as interpreted.

  Known limits. Where the clause taken has no part, the part is 0, which an
  infinite value multiplies to NaN where the interpreter has nothing to
  multiply: `grad_(t = 6) f(t)*~(10^400)`, f a clamp at 1. A function of the
  prelude saturated, `tanh` past 20 or `exp` past 1000 either way, has its
  part wherever its argument has, so a `floor`, a comparison or an `==`
  guard reading it tests a jump the interpreter, seeing no part, does not:
  `grad_(t = x) floor(tanh(t))*t` is NaN past 20, where the interpreter
  answers 1. And a division by zero is an infinity, as anywhere in the
  header, which only the value carries: grad keeps the part, so
  `grad_(t = x) (t + 1/(x - 2))` is 1 at 2, where the interpreter refuses.

  Rejected: dual numbers in the header, a struct of value and part and a
  function per operation, which is a second arithmetic to read beside the
  doubles, and one for a float target, past which the cells, guards, NaN and
  temporaries the compiler has would not reach; the derivative definitions
  planned before differentiation, which shapes known while compiling make
  possible here, but each refusal that needs a value, each absent part and
  each guard would still be a node, and the parts over cells are that
  transformation done where the compiler already is; reverse mode, an
  adjoint step, one pass for every weight where forward takes one per
  weight, as `MANIFESTO.md` places in the compiler, but whose roundings are
  not the interpreter's, so that `--check` could hold it to the tolerance
  only, and which needs a tape -- an entry of its own when a model with many
  weights is too slow; finite differences, which are not the derivative and
  suffer the rounding the oracle is for.

  About 350 lines of sources, in `compile.hpp` but for the static refusals,
  made shareable in `derivative.hpp`: the parts through Code, the rules, the
  binding and seeding of the name, the chains of parts and of where they
  are, the tests that are NaN and the prelude's part functions; re-estimated
  by the review with those conditions and without cells, so past 525 the
  implementation stops and reports. Sized against compiled `lim`, 167, and
  the prelude's functions, 45 in the compiler: the interpreter's 830
  include a memo, a fill, a walk of limits and derivatives of every order,
  none of which the step has. 13,915 lines in all before it, after
  `fixes`, about 14,265 after.

  Specified in `test/compile/grad.ink`, its numbers worked out apart from
  the interpreter, by hand, with exact fractions, by the step's operations
  in doubles and floats in C and numpy, and by mpmath: `line`, least
  squares on exact data, its terms exact fractions, `within 4.4e-16`, and in
  float `within 2.7e-07, 1.9 units of a float`, with its step written out;
  `fall`, `test/data/prelude.ink`'s logistic regression on its log loss,
  grad's gradient beside the one written by hand, each within 0, and two of
  its part functions; `hinge`, a ReLU applied to each cell; `edge`, a NaN for each
  refusal the point decides, and a grad folded; `clip`, a clipped gradient;
  `steer`, Newton's method by grad's derivative inside a limit; and the
  refusals, a file of ten. Wired with the implementation: the six checks
  and their reports in `test/CMakeLists.txt`, `line` in float, and the
  refusals in `test/cli.cmake`. Nothing that compiles today reads `grad`, so
  no golden, header or report moves; README's paragraph on what the compiler
  refuses gains a sentence.

  Built as specified: every report as `grad.ink` gives it, `line` in float
  too, the two header excerpts byte for byte, and the ten refusals in their
  words; every other header, check program and report as before. A part is
  a `Code` beside the value's, so it rides through calls, chains and limits'
  terms as the value does; the name is bound as a call binds a parameter,
  and the interpreter's own `Derivative::Names` refuses what the body's names
  decide. Departures. A comparison or a `floor` at a jump at a constant point
  is NaN at every step rather than folded past, `j` and `o` in `edge`, found
  on the way. A function of the prelude's part is there wherever an
  argument's is: the conditions of its own clauses stay inside its part
  function. A clause refused wherever taken is NaN in its part only where
  another clause has one, so `ilogb` has neither. A cell read of a constant
  matrix point is a double, not folded exactly. A body through an instance
  of a model is refused where an argument moves, not wherever it reads the
  name, and through an instance with memory written there as in a call. A
  function of the prelude is written only where something calls it: a part
  a `floor` or a comparison drops leaves one nothing does, which Clang
  would not build.
  Found on the way and registered: C110, an infinite product's 0 part; C111,
  the prelude's part jump lost after a `floor` or a comparison; C112, a
  global read inside a function compiled with the function's parameters,
  which `grad` refuses only where it reads grad's own name; and by its
  review, C113 to C118, fixed, and C111 with C117. 387 lines of sources
  more, where about 350 were planned: 381 in `compile.hpp` and 6 in
  `derivative.hpp`. 14,307 lines in all.
- `[done]` **Guards on cells at run time.** `--compile` decides the guards of a
  matrix or a function by cells while compiling, and refuses one that reads
  a value that moves, "a guard on cells that is not a constant": a ReLU
  written by its cells, `relu(z)[i] | z[i] > 0 = z[i]`, applied in a step.
  Under `grad` it refuses every definition by cells, guarded or not, "a
  derivative through a definition by cells, for now", so a network trained
  by `grad` through its activations has no step. A term by cells already
  tests its guards at run time (*A sequence's terms cell by cell*).

  Decided: the value and the parts, as the interpreter's `grad` gives both
  (*`grad` of a definition by cells*). Each cell is the chain the interpreter
  tries: the clauses for that cell in the order written, then those for
  every cell, guarded before unguarded, then the cell of the matrix written
  whole, else 0, never NaN, a cell no clause gives being 0. It is the chain
  `Chain` makes of a function's clauses, built per cell with the cell's
  places bound as constants, so a guard reading only places folds as now.
  Under `grad` a cell's chain carries its chain of parts as a function's
  does: the clause's part, 0 where the clause taken has none, and the
  definition has a part where any cell's clause has one, as
  `derivative.hpp` stores it, so where none does at the point nothing
  reading it tests a jump. An equality guarding a cell
  whose sides move is NaN where they meet, where the interpreter says
  `pk[1,1] takes a clause at t = 0 that holds only there`, and `z*(z > 0)`
  where its comparison jumps, as anywhere. A guard that reads a parameter is
  a test too, where today it folds the parameter's value in and the header
  says it is "compiled in, as a size, a bound or a lag", which a guard is
  not: the parameter is a field and the definition is computed where the
  parameters are, as any value reading them. No recorded header moves:
  `heat.h`'s are the only guards on cells, and read places only.

  `--check` needs nothing new. Its record of each cell's clause, and the
  flip it reports, follow the clauses of a sequence's terms; a definition by
  cells compiled where it is called or read parts only by its values, as a
  guarded function does. Following it was rejected: its guards are asked
  once per call, `rfit`'s ReLU three times in each pass of its loss, and the
  interpreter's memo asks a repeated call once, so the record would need a
  key per call that neither side has. A ReLU to be watched at its threshold
  is a term by cells, as `rift` is. Nor
  is anything new in a NaN-aware header: a cell's guard tests its operands
  as any guard does there, and a chain writes no NaN that its clauses do
  not, so a ReLU by cells leaves a header unaware. In float the chains are
  floats as everything is.

  Tensors compiled refuse "a tensor's cells under a guard that is not a
  constant" for a term, whose clause `--check` keeps by row and column; that
  refusal stays, being about the record. A tensor's function by cells, a
  batch's ReLU, is the chain here with the slice a place, as tensors
  compiled, which landed first, bind it.

  Stays refused: a guarded clause for one cell of one term, `c_2[1] | x_2 >
  0 = 5`, "a guarded cell of one term", a term whose clause `--check` would
  have to keep and which no model asks; a Jacobian, now reached through
  cells and refused in the interpreter's words; and a place or a size that
  moves, as now. Rejected: the value alone, `grad` through cells refused
  for now, when training through the activation is the case and `Chain`
  already carries the parts; a loop over the cells with an `if` per cell,
  where the step is expressions and the cells are known while compiling;
  and the ReLU rewritten as a function of single values called per cell,
  which leaves the definition by cells refused under `grad`.

  About 30 lines of sources: `CellOf` building a chain with `Condition` in
  place of `GuardHolds`, the fallback passed in, about 15; `Cells` assembling
  the cells' parts and where they are, about 15; the refusal and
  `GuardHolds` 4 fewer. `TermCells` walks a term cell's clauses the same way
  over strings; sharing the walk is looked for, landed first as a refactor,
  every header byte for byte, where it removes lines. Sized against
  `Chain`'s parts in `grad` compiled, about 25 lines and reused whole, and
  sizes inferred's compiler half, 20. Past 45 the implementation stops and
  reports. 14,355 lines in all before it, at cfb0abb; about 14,385 after.

  Specified in `test/compile/cellguards.ink`, its numbers worked out with
  exact fractions in Python and by hand, then seen to be the interpreter's:
  `rfit`, a ReLU network trained by `grad`, a unit reviving at the first step
  and the samples fitted at the second, exact and within 0 in double and in
  float; `tfit`, `rfit` with its samples a tensor's slices and its ReLU of
  three indices, every cell's clause flipping at every step, the same in
  float; `layer`, `net.ink`'s network with `h_n = relu(z_n)`, computing h
  by `net.h`'s two lines; `kink`, each cell's slope either side of its
  threshold, NaN where the interpreter refuses, and a cell moving where
  another cell's clause has a part; `crest`, `rift` by a function, parting
  by its values alone; `mask`, a parameter read by a cell guard become a
  field; `order`, the chain's order told from the others, the whole's cell
  and 0 at its end; and two refusals. Wired with the implementation:
  the checks and their reports in `test/CMakeLists.txt`, `rfit` and `tfit`
  in float, `crest` failing, `layer`'s lines and the refusals in
  `test/cli.cmake`. One
  recorded refusal moves: `q` in `grad.ink`, `grad_(t = x_n) cel(t)[2]`,
  compiles, 2 at every step, and leaves `compile_grad_refused`; README's
  sentence on `grad` compiled gains the network.

  Built as specified: every report as `cellguards.ink` gives it, `rfit` and
  `tfit` in float too, `layer`'s two lines byte for byte and the two
  refusals in their words; every other header, check program and golden as
  before. First a refactor: `Walk`, the order in which a cell's clauses are
  tried, shared by `TermCells` and `CellOf`, every output byte for byte, 10
  lines fewer. Departures. `fit` is `rfit`, as `adam.ink`'s `fit` already
  names the check program, in a commit of its own. `q` compiling is held by
  its line, `compile_cellguards_cel` in `test/cli.cmake`. Specified on the
  way, in a commit of its own, as ruled on its review: `tfit`. Then 15 lines
  of sources more, where about 30 were planned, all in `compile.hpp`. 14,421
  lines in all. `Assembled` stores a constant part at its place among the
  cells, not by row and column, which a tensor's slices need; no header
  moved. Its review fixed C132 and C133, 8 lines, and found the
  interpreter refusing where the step answers when it sizes a clause under
  `grad` or when the value written whole refuses beside cells that each have
  a clause, and constants folded from a call that ignores a moving argument
  refused. 14,431 lines in all, after `fixes5`.
- `[done]` **The prelude's part functions for the interpreter's `grad`.** Under
  `grad` the interpreter walked the prelude's definitions with jets: under
  callgrind `bench/grad.ink` was 16,418 million instructions at 27489db,
  where it was 5,750 before the prelude was written in inkamath.

  Decided: `inkamath_prelude.ink` gains `d_n = grad_(t = n) (exp(t) +
  log(t + 1) + tanh(t) + sin(t) + cos(t))`, so that `record_prelude` writes
  `inkamath_prelude_exp_dx(double arg_x, double part_x)`, `_log_dx`,
  `_tanh_dx`, `_sin_dx` and `_cos_dx`, whose doubles are the walk's to the
  bit, and grad's walk calls them for a call of the prelude's own function.
  Not `ilogb`, which has no part (C139 answers it with none). Taken where:
  - the value is one the fast path takes, outside every run of `--check`;
  - the jet is the value and one part. A grad inside a grad walks; a jet
    with no part is the value alone, as outside grad;
  - the part is a real double, not approximated, or exact for `exp`, `sin`
    and `cos`, whose definitions round it at their `~` first. `tanh` and
    `log` meet it exactly first, in `-2*x` and `x/2^k`, where rounding
    first is another rounding: `grad_(t = ~0) tanh(~0.5 + t*599492/10^318)`
    walks to ~4.7146912432892993e-313, rounded first ~4.7146912433387059e-313,
    and `grad_(t = ~1.5e-300) log(t*10^200*10^200)` to ~6.67e299, rounded
    first inf;
  - the header's part is neither 0 nor NaN. It is 0 where the clause has no
    part, `exp` past 1000 and `tanh` past 20, which an exact 0, a product
    with an infinity and a `floor` tell from a 0 (C110), and NaN where a
    reduction's `floor` jumps (C111, C117), where the walk refuses in its
    own words. Every other 0 or NaN walks too, as ruled on review, a known
    cost.

  Answered as `[value, part]`, each `Number(double)`. Asked in
  `Derivative::Term` once the memo has not answered and the step and the
  reference are taken, and memoised: a call is one step and one reference
  deep, where walked `exp` is 4 steps and 3 references deeper, `tanh` 6 and
  4, `sin` and `cos` 7 and 5, and `log` 19 and 5, so a line refused for its
  steps or depth may now answer; nothing that answered moves or is refused.
  Every run of `--check` walks, so that `test/compile/grad.ink` compares
  the step with the walk, not the header with itself.

  Rejected: asking before the memo, which charges a remembered call a step
  and a reference and refuses both; a part as the derivative times the
  argument's part, which rounds otherwise than the walk; every exact part
  rounded first, the counterexamples above; doubles alone for parts, 3,266
  million instructions, as a weight's part through exact data is exact;
  testing the part in C++ by the prelude's thresholds, a second copy of its
  guards, or by a function the compiler would write, moving every header
  that calls `exp` under grad; the `_jx` value functions, which test the
  jump the part function tests already; one part in any slot of a longer
  jet, a loop no model asks for.

  Specified in `test/data/spec/fastgrad.ink`, 55 entries, 7 failing; each
  test taken out of the prototype fails an entry. Built as specified, one
  departure, of form: the part functions are a table of their own beside
  the values'. Four sweeps, 807,251 lines, the five at and beside their
  limits and jumps, through exact and inexact parts of either sign, signed
  zeros and `1/(3*g)` for a zero's sign, byte for byte the walk's at
  0f96666 under GCC 13 and Clang 18. Under callgrind `grad` from 16,407
  million instructions to 1,358, 12 times fewer, where below 1,500 was
  expected, the others to the million as before. The header grows from 148
  lines to 255. 46 lines of sources where about 45 were planned: 19 in
  `derivative.hpp`, 25 in `interpreter.hpp` and 2 in `reference_stack.hpp`;
  14,534 in all, from 14,488.
- `[done]` **`sin`, `cos`, `abs`, `max` and `min` in the prelude.** A rotation, a
  pendulum or an oscillator needs `sin` and `cos`, and a clip, an L1 loss or
  a hinge `abs`, `max` and `min`; each session writes its own, as README's
  `abs` and `conditional.ink`'s `max` do, and README's `cos` is a series of
  its own `exp`.

  Decided as for `exp`, `log` and `tanh`: written in inkamath, no libm, the
  interpreter and the compiled step the same operations on the same
  doubles. The design, measured before it was written down:

      abs(x) | x < 0 = -x
      abs(x) | x >= 0 = x
      max(a, b) = a
      max(a, b) | a < b = b
      min(a, b) = a
      min(a, b) | b < a = b
      sin(x) = sinr(x, floor(x*0.6366197723675814 + 1/2), 0)
      sin(x) | abs(x) > 2^20 = 1/0
      cos(x) = sinr(x, floor(x*0.6366197723675814 + 1/2), 1)
      cos(x) | abs(x) > 2^20 = 1/0
      sinr(x, k, c) = sink(~(x - k*3217/2048 + k*2391/2^29 + k*8029421003/2^63 + k*1987263209/2^96 - k*7744522442262977/2^156), mod(k + c, 4))
      sink(r, j) = sinp(r)
      sink(r, j) | j > 2 = -cosp(r)
      sink(r, j) | j > 1 = -sinp(r)
      sink(r, j) | j > 0 = cosp(r)
      sinp(r) = r - r*sins(r*r)
      sins(z) = z*(1/6 - z*(1/120 - z*(1/5040 - z*(1/362880 - z*(1/39916800 - z*(1/6227020800 - z*(1/1307674368000 - z/355687428096000)))))))
      cosp(r) = cosw(r*r)
      cosw(z) = 1 - z/2 + z*z*(1/24 - z*(1/720 - z*(1/40320 - z*(1/3628800 - z*(1/479001600 - z*(1/87178291200 - z*(1/20922789888000 - z/6402373705728000)))))))

  k is the whole number nearest x/(pi/2), by the double nearest 2/pi, and r =
  x - k pi/2 with pi/2 in five parts (Cody and Waite, as `exp` takes ln 2):
  two of 12 bits, two of at most 33 and one of 53, within 2^-159 of it.
  Every product k times a part but the last is exact in a double for |k| <
  2^20, the first two in a float for |k| < 2^12, and each subtraction is
  exact where it cancels, its operands within a factor of two (Sterbenz),
  so r is right to a unit or so however small: below 2^20 the double nearest
  a multiple of pi/2 in absolute terms, 45.553093477052, is 6.2e-19 from 29
  pi/2, 66 bits cancelled, and the most cancelled are 73 bits, at
  321307.96. The quadrant j = k + c mod 4 picks sin r, cos r or their
  negatives, c being 1 for `cos`: x + pi/2 would round. sin r is Taylor's to
  r^17 and cos r to r^18 in Horner's form, remainders below 2^-62 and 2^-67
  on |r| <= pi/4, their coefficients reciprocals of whole numbers as `exp`'s
  are. An exact argument is reduced with pi/2 so held, 7.4e-49 off, and
  rounded once, at the `~`: `sin(355)`, 355 being 3.0e-5 from 113 pi, is
  mpmath's, correctly rounded. r is off by k times 7.4e-49, under a unit
  where it is above about k 1.3e-32, so an exact argument is accurate down
  to about 1e-31 from pi and 1.4e-26 from a multiple near 2^20, and nearer
  answers the reduction's error (C123); no double comes so near.

  Past 2^20 either way, `1/0`, and NaN in a header, refused for accuracy:
  the products would round, and the hardest doubles in [2^20, 2^22] be 2^40
  units off, `cos` at 3442254.066112624. README says that a growing phase
  such as `sin(w*t)` stops at 2^20. Rejected: Payne and Hanek's reduction,
  2/pi to some 1,100 bits and a product of many words, which a step in
  doubles cannot take without dozens of parts; answering past 2^20 regardless, 9.7 units off by 2^25 and
  800 by 2^30 on random doubles; four parts of 33, 33, 33 and 53 bits, as
  accurate in a double and rounded in a float, `sin(100)` 40 units of a
  float off; three parts, which lose the doubles nearest a multiple of
  pi/2; a minimax polynomial, two terms shorter, whose constants are no
  reciprocals.

  Accuracy, against sinl and cosl at 64 bits on 1e7 points a range, and
  against mpmath at 200 bits elsewhere:

  | | worst, sin and cos | correctly rounded |
  |---|---|---|
  | on [-pi/4, pi/4] | 0.75 and 1.25 units | 97.7% and 74.0% |
  | on [-2 pi, 2 pi] | 1.56 and 1.45 units | 77% and 73% |
  | on [-1000, 1000] | 2.34 and 2.29 units | 72.7% |
  | on [-2^20, 2^20] | 2.43 and 2.37 units | 70.3% |
  | the 12 doubles nearest a multiple of pi/2 below 2^20 | 0.35 units | all |
  | a sample of 20,000 exact arguments to 10^6 | 1.43 and 1.39 units | 74.5% |
  | in float, on [-6434, 6434] | 2.44 and 2.29 units of a float | 70.7% and 70.6% |

  The float figures are the design emulated with floats operation by
  operation, its constants the floats nearest the header's doubles, 2.20
  units at the 300 floats nearest a multiple of pi/2 there, 1.9 on to 8192;
  past 8192, k times 3217 passes 2^24 and rounds in a float, and the error
  grows to x*2^-24, within the spacing of floats at x, which `--check
  --float` shows.

  Nothing `sin` and `cos` give is exact: `sin(0)` and `cos(0)` are a double's
  0 and 1, as `exp(0)` is a double's 1. A clause for 0 would make them exact,
  and `grad` refuses a clause that holds at a point alone, so a pendulum at
  rest would have no derivative. `pi` is the built-in, the double nearest pi,
  so `sin(pi)` is 1.2246468e-16, the sine of that double as C's, and no
  argument but 0 is exactly a multiple of pi; `pi` in the prelude would be
  the same double. `sinpi(x)`, sin(pi x), exact at whole x, was rejected: a
  sixth function no model asks for. A 0 has the definition's sign: `sin(-0)`
  is +0, its subtraction from 0 (C33), where C's is -0.

  `abs` is README's own (section 3), two guards rather than a default and
  one, so that a session writing it, as README and `conditional.ink` do,
  writes the prelude's clauses again, each replacing its own: `?abs` prints
  as now, where a default would print before them. A complex number is
  refused, "a comparison needs real numbers", as README's `abs` refuses it:
  a modulus needs the real and imaginary parts and a root, none in the
  prelude, and is another function. `abs(-0)` is -0, its argument, where C's
  `fabs` is +0.

  `max` and `min` take two arguments, as C's `fmax`; a matrix's greatest cell
  is a reduction over its cells, which the language writes as `sum_` only,
  and is refused as `exp` refuses a matrix. A NaN on either side is refused
  by the guard, "a comparison needs a number", and NaN in a NaN-aware
  header, where `fmax` answers the other argument and hides it. The answer
  is the argument chosen, exact or not: `max(1/3, 1/4)` is exactly 1/3.

  `grad` differentiates the definitions: the polynomials' derivatives,
  within 2.04 units of cos and 1.62 of -sin at worst on a sample of 2,000
  points, and floor's 0, so a few doubles where x*2/pi + 1/2 is whole are
  refused as floor jumping, as in `exp`. `abs`'s slope at 0 is 1, its `x >=
  0` clause's. At a tie `max` and `min` take the first argument's slope, as
  TensorFlow's maximum and minimum: `max(0, x)` has a ReLU's slope 0 at 0,
  as PyTorch's, and `max(x, 0)` slope 1. Rejected: refusing the tie as a
  jump, where `max` is continuous, which stops training at a bias set to 0;
  half of each slope, as JAX takes it, a clause at the tie alone, which
  `grad` refuses.

  Compiled where called as `exp` is, with no change to the compiler: on a
  prototype, `<header>_sin(double arg_x)`, `_cos`, `_abs`, `_max` and `_min`
  and their helpers, and under `grad` `_sin_dx`, `_abs_dx`, `_max_da` and the
  like; a model of the five, a pendulum and a `grad` through each kink at its
  tie, checked within 0 under GCC 13 and Clang 18.

  Called compiled by the interpreter as `exp` is: `inkamath_prelude.ink`'s
  sequence adds `sin(n) + cos(n)`, `record_prelude` writes them into
  `inkamath_prelude.h`, and a call of the prelude's own `sin` or `cos` on a
  real double within 2^20 of 0 takes the C function under `exp`'s
  exclusions; past it the definition refuses, walked. Walked, each is some
  40 steps and nests 5 references deeper. `abs`, `max` and `min` stay walked:
  a guard and an operation, and `max` and `min` take two arguments where the
  fast path's functions take one.

  What moves: no golden, header or report, measured with the prototype on
  every test of `ctest`. `conditional.ink`'s `abs` and `max`, and the `sin`
  and `cos` of `sequences.ink` and README, are definitions of their own. A
  session's clause on a built-in or a name of the prelude starts a
  definition of its own, as a model's or a file's does: `abs(y) | y < 0 =
  -y` defines `abs`, which extending the prelude's would refuse for its
  parameter, and `abs(3)` then finds no clause. So `floor(2.5)` after
  `floor(x) | x > 10 = 0` finds no clause where it gave 2, `pi(x) | x > 0 =
  1`, refused, defines `pi`, a sequence or another arity, `exp_0 = 1` or
  `mod(a) | a > 0 = 1`, replaces the prelude's name, and a file run or
  compiled does the same: `s_n = exp(~n)` after `exp(x) | x > 5000 = 7`
  compiles to `n > 5000 ? 7.0 : NAN`. The extension was a remnant of the
  built-ins being the session's own, and is deleted before this: in
  `fastprelude.ink`, `exp(~1)` and `dive(250)` after `exp(x) | x > 5000 =
  7` move to "no clause of exp applies".
  `inkamath_prelude.h` gains the functions, and README's paragraph on the
  prelude (section 1) the five names.

  About 32 lines of sources: 22 in the prelude and 6 of its comment, 4 in
  the interpreter's fast path. 14,318 lines in all now, about 14,350 after.

  Specified in `test/data/spec/trig.ink`, 104 entries, 94 failing: values at
  nine digits, mpmath's, and at seventeen, of the design emulated in C and
  in Python with exact fractions, apart from the interpreter, at hard
  arguments; exactness, refusals, `grad` and its ties, the fast path's steps
  and depth, and a session's clause. And in `test/compile/trig.ink`, its
  report every term `within 0`, wired with the implementation as
  `inkamath_prelude.ink` is.

  Built as specified: every entry passes as written, and the spec is the
  golden `trig.ink`; no other golden, header or report moves but the
  prelude's header. `test/compile/trig.ink` is checked as `elementary.ink`
  is: `check_tri_report` every term within 0, its lines in the check's
  order, c, g, q, s, b, l, m, v and w, and `check_tri_functions` the ten
  functions and no libm `sin` or `cos`. Departures, by the review's
  rulings: a session's clause on a built-in or a name of the prelude starts
  a definition of its own, the extension deleted first, 6 lines, so that
  `abs(y) | y < 0 = -y` defines `abs`, and `trig.ink` gains a clause on
  `floor`, written by hand, and C123, recorded wrong on purpose; and the
  refusal past 2^20 is for accuracy, which README says. Measured on the
  build: 200,044 doubles, random on [-2^20, 2^20] and in magnitude from
  10^-320, the nearest a multiple of pi/2 and floor's ties with their
  neighbours, and 22 chosen, `sin` and `cos` of each and 1 over each printed
  at 17 digits, alike compiled and walked under GCC 13 and Clang 18. 30
  lines of sources where about 32 were planned: 22 in the prelude, 4 of its
  comment and 4 in the fast path; 24 with the extension's deletion. 14,379
  lines in all, after `fixes4`.
- `[done]` **Tensors compiled.** `--compile` refuses a tensor by name, "cannot
  compile X: a tensor", so attention per batch and per head, which the
  interpreter answers (*Tensors of rank 3*), has no step: `attention.ink`
  compiles one head of one sample. The conformance suite asks for the paper's
  model, and a minibatch is a tensor wherever one is trained.

  Decided: a tensor is a value the compiler holds as it holds a matrix, its
  cells slice after slice and row by row, as the interpreter stores them, and
  whatever meets one meets it slice by slice, through one function that takes
  it apart into slices, applies itself to each and stacks the results, as the
  interpreter's does. So a single value or a matrix meets every slice and a
  tensor its slices in turn, for `+`, `-`, `/`, `*` and `'`; a product is
  batched over the first index. What carries a value carries a tensor: a
  sequence's term, a parameter, a model's input, a function's argument, a
  term sampled at another rate, an input's history, a part under `grad`.

  In the struct a term is `double O[1][2][3][4]`, window first: C's own layout
  of that array is the interpreter's, so a host copies a batch in and out
  whole. A tensor keeps its three dimensions even of one cell, as it keeps
  `;;` when printed. A parameter is a field, `double s0[2][1][2]`, which init
  writes cell by cell, as a matrix's. An input states its size by three
  bounds, slices first, as the interpreter already reads it (C83),
  `X_n[b<=2, t<=3, c<=4]`, and the step takes a pointer to its cells, `const
  double X[24]`, copied into the window as a vector's is; where an input is a
  tensor, the first comment's sentence on inputs ends "row by row, slice after
  slice", and the last term shown is `m.O[0][b][i][j]`, "slice b+1, row i+1
  and column j+1". Rejected: `[1][6][4]`, the same memory with a slice's rows
  to be counted by the host; a field per slice, several fields for one term;
  the batch last, where no paper puts it; `const double X[2][3][4]`, rejected
  for a matrix in *An input of more than one cell*, as a cast for every host
  under GCC's `-Wpedantic`.

  One constant index reads a slice, three a cell, and a row is chained,
  `T[b][t]`; two are refused in the interpreter's words, "a 2x2x2 tensor
  takes one index or three, not two". A definition by three indices, a
  sequence's term or a function, takes each cell's clause as one by two does,
  the slice a place as the row is, so a guard that reads only places folds,
  and its size is measured by `Reference::Measured` as now, a read's extent
  carrying its slices: softmax's `sm(z)[b,t,s]` is the size of the scores it
  is given. A literal `[a;; b]` is compiled slice by slice. A sum over a
  slice's index unrolls as any sum with constant bounds.

  Refused, in the interpreter's words where it has them: a power, "only a
  matrix has a power, not a 2x2x2 tensor", which the square matrix's test
  would otherwise let through as a matrix; tensors of different numbers of
  slices; a literal whose slices differ. A comparison, a guard, an exponent,
  `and`, `or` and a block of a tensor are refused in the compiler's words for
  a matrix, "a comparison of matrices", a tensor being a stack of them: words
  of their own were five lines for refusals no model reaches. For now: "a
  tensor in a limit", its terms or an argument, as a limit's function fills
  an array of rows and columns and no model iterates a tensor to a fixed
  point; "a derivative with respect to a tensor"; and "a tensor's cells under
  a guard that is not a constant", one reading the index, as `--check` asks
  the interpreter's guards of a cell by its row and column, not its slice. A
  gradient whose body meets tensors, with respect to a single value or a
  matrix, compiles, the parts riding the values slice by slice: a model's
  weights are shared matrices and its batch the tensor. Weights stacked by
  head, a tensor, reach a loss only through softmax by cells, which compiled
  `grad` then refused, so nothing asked for a seed of three indices; since
  *Guards on cells at run time* it does not, and that seed is what stays
  between attention compiled and its training.

  `--check` feeds a tensor input by its cells and holds every cell of a term,
  reading the interpreter's slice by slice, and names a cell that parts from
  it by its slice, row and column, `spread.d[2,1,2]`, as a matrix's by its row
  and column. In a header that writes NaN, a tensor term with a NaN cell is
  NaN in every cell, the loop gaining the slices'. `--float` writes floats, as
  everywhere. Nothing new elsewhere: an instance with memory per cell is named
  after each place it reads, the slice among them, as now. The random models
  gain no tensors in this entry.

  First a refactor: `Code`, `Sequence` and `Parameter` carry an `Extent`
  where they carry rows and columns, and a C subscript and an array's
  dimensions are written from it, every header, check program and golden
  byte for byte; about as many lines out as in, its own commit, its net
  stated, and past about 15 lines more than it removes the implementation
  stops and reports. Then about 110 lines of sources: the arithmetic slice
  by slice 25, reads of slices and cells and the literal 30, definitions by
  three indices 20, the header's dimensions, NaN loop and sentences 15, the
  refusals 6, the check 8 and the rest 6; re-estimated by the review at about
  150, counted without the refactor, so past 225 the implementation stops
  and reports. Sized against the interpreter's tensors, 222 with a literal,
  printing, `tex` and parsing the compiler does not need, inputs of more
  than one cell, 119, and compiled `lim`, 167. 14,355 lines in all before
  it, at cfb0abb, the fixes since `grad` compiled included; about 14,505
  after.

  Specified in `test/compile/tensor.ink`, its numbers worked out apart from
  the interpreter and the compiler: `heads`, multi-head attention over a
  batch, `tensor.ink`'s conformance model with its input scaled by `n/50`, so
  that `O_50` is its `O`, held within 1e-13 where NumPy's doubles come within
  6.7e-15 of mpmath's; `whirl`, a batch of states turned a quarter each step,
  meeting tensors every other way, its every term exact; `batch`, a linear
  layer trained by minibatch descent through `grad`, exact; the last two in
  float as well; `spread`, a cell that drifts, named by its place; the header
  excerpts of each; and eight refusals. Two recorded refusals become headers:
  `compile_tensor_refused`, whose file joins the refusals', and
  `inputs_batch`, with its line in `inputs.ink`'s comment. Wired with the
  implementation; README's paragraph on the compiler gains a sentence,
  `attention.ink`'s comment loses "the compiler refuses for now", and
  `test/data/inputs.ink`'s "Compiled, it is refused".

  Built as specified: every report as `tensor.ink` gives it, `whirl` and
  `batch` in float too, the header excerpts byte for byte and the eight
  refusals in their words; every other header, check program and golden as
  before, and the refactor's byte for byte. A `Code`'s cells are a tensor's
  slice after slice, and one stretched over another's shape is read at a place
  modulo its cells, a matrix over every slice as a single value over every
  cell, so `+`, `-`, `/`, `*` and `'` meet slice by slice and a part rides
  them. A read takes the cells the interpreter's read takes of a value of its
  size whose cells are numbered, so a slice, a row and a cell, and every
  refusal of one, are the interpreter's, as is a cell clause's outside the
  size. Departures. A tensor whose slices are single values, met by a matrix
  of another size, is refused, "a tensor whose slices are single values met
  by a matrix, for now", where the interpreter takes each slice as a single
  value; `whirl`'s `p` is one, and nothing meets it so. An input that states
  no size, read with three indices, `X_n[1,2,1]`, is refused as with one or
  two: "write 'X_n[b<=1, j<=2, k<=1]'". A tensor's term by cells with a guard that folds is
  not followed for flips by `--check`, which could not tell its slices apart;
  one that does not fold is refused, as specified. A matrix stretched over the
  slices is computed once, into temporaries, as a single value stretched over
  a matrix is. `--check`'s `hold_` takes each term's rows too, in a program
  for an instance with a tensor only. Specified on the way, in a commit of its
  own: `spread`, the drift of a tensor's cell, named. Found on the way and
  registered: C122, a cell of one term outside its size written past the
  term's cells. The refactor, 41 lines fewer, every output byte for byte; then
  48 lines of sources more, where about 150 were planned: 40 in `compile.hpp`
  and 8 in `check.hpp`; C122's 1; and by its review, C124 to C127, 15, and
  its rulings, 14. 14,416 lines in all, after `trig`.
- `[done]` **Cases in a paper's order, and `clear`.** A paper writes the cases first
  and "otherwise" last, `clamp(x) | x > 1 = 1` then `clamp(x) = x`, and here
  the second line starts the definition over and drops the guard (C11):
  `clamp(5)` is 5. A sequence written so, `s_n | n > 1 = 1` then `s_n = n`,
  and a definition by cells keep their guards, so one order means two
  things. It bit a model: a ReLU so written in a model's body was 0
  everywhere and its slope 0, and the unit meant to die never lived. The
  restart was the prompt's way to start a definition over, and is as much a
  way to lose one without a word.

  Decided: nothing a name holds is dropped but by `clear`, at the prompt as
  in a file and a model's body, since a transcript, a file run and a file
  used are the same lines and should mean one thing.
  - A clause joins its name's definition in the scope it is written in. An
    unguarded one for every call, plain or general, is the fallback, tried
    after the guards wherever it was written, as now. Written again, a
    clause replaces itself where it stands (C45, C46); a plain one that is
    the whole definition is replaced parameters and all, since nothing else
    goes with it, so `f(x) = x` then `f(x, y) = x + y` needs no `clear`.
  - A clause that cannot join is refused, and says how to start anew, "; write
    'clear f' first", but in a model's body, where `clear` cannot be written:
    other parameters beside other clauses (C51, which a plain clause escaped),
    defaults among them (C154), a clause without an index on a sequence or
    with one on what is not (C70 dropped the value), any clause on a model or
    a file used. A model is one statement and replaces a model only; `use`
    binds a file, or brings in a name, only over nothing or the same.
  - `clear f`, a word at the start of a line as `use` is and not reserved,
    drops what `f` is in the scope it is written in: clauses, a model, an
    instance, a file used or a name brought in. The name then means what it
    means beneath, the prelude's or a built-in, or nothing, and `?f`, `tex
    ?f` and every caller follow, the memo going as with any definition. A
    name defined only beneath is refused, "abs comes with the language, so
    it cannot be cleared", as are a name defined nowhere and anything after
    the name: no single clause is removed. A model's body holds definitions,
    so not `clear`; a file used reads it as the session does. A file is not
    read again for being cleared and used again.
  - Unchanged: a session's clause on a name of the prelude or a built-in
    starts a definition of the session's own (trig), which the clauses after
    it join; one on a name brought in by `use` joins the session's copy,
    never the file's. `--compile` runs a file as the session does and
    compiles the clauses it leaves, with no change: on a prototype the ReLU
    compiles to `b > 0 ? b : 0`, its slope to `b > 0 ? 1 : 0`.

  Rejected: the prompt keeping the restart while files accumulate, since a
  transcript, a file run and `--compile`'s file are run as the prompt is, so
  the trap would stay where models are written and one text mean two
  things. `f(x) := x` to restart: a paper reads `:=` as `=`, so the
  difference would be invisible on the page and in `tex`, and it is a
  second way to write a definition (MANIFESTO, *What it is not for*); `clear`
  says what nothing says now, and gives a prelude's name back. A marker
  word, `new f(x) = x`: the same in more letters. Removing one clause: a
  syntax for an edit nobody has needed, which `clear` and rewriting say.
  Restarting where a clause cannot join: the trap in another shape.
  Reserving `clear`: `use` is not.

  What moves, by a prototype on every test: `matrices.ink`'s `T = [1 2; 3 4]`,
  "starts it over", keeps its cells and gains `clear T` before it;
  `references.ink`'s C11 entries, `m=5` after `m_n` and `p_0=1` after `p=9`,
  are refused and gain a `clear` each; `sequences.ink`'s C70 entry, `rv_n | n
  > 0 = 1` after `rv = 5`, is refused, `?rv` is `rv = 5` and `rv_1` "rv is not
  a sequence"; `conditional.ink`'s two C51 refusals gain the way out;
  `trig.ink`'s "a plain definition starts over" goes, and the comments that
  say the old rule become this one: `conditional.ink`'s "No 'otherwise'
  clause", `references.ink`'s "a plain definition replaces whatever the name
  held" and "a clause turns a plain definition into a sequence", and
  `matrices.ink`'s "starts it over". README's `exp(x)=sum_(k=0) x^k/!k`, after
  the sequence `exp`, is refused and gains `clear exp`; section 3's "a plain
  definition clears the guarded clauses with it" becomes this rule, with
  `clamp`, the operator table gains `clear name`, section 1's "can be defined
  again" adds that `clear` gives it back, and section 5 says a prelude's name
  cleared is the prelude's again. No header, check report or other golden
  moves. The rows of C11, C51 and C70 and phase 10's "the way to start over"
  are history and stay.

  About 30 lines of sources net: the restart and C70's erasure out, about
  13; the refusals 12, `clear` 15, the model's body 2, the plain clause
  alone 2. 14,653 lines in all now, about 14,685 after; past 45 net the
  implementation stops and reports.

  Specified in `test/data/spec/clauses.ink`, 115 entries, 47 failing, with
  `test/data/cases.ink` for `use`: the cases in a paper's order at the
  prompt, in a model's body under `grad` and in a file; sequences and cells
  as they are; a matrix written whole after its cells; each refusal; models,
  files and `use`; the prelude; callers and the memo; and `clear` itself.

  Built as specified, with the review's rulings: the spec is the golden
  `clauses.ink`, 116 entries with `clear` in a file used, which `cases.ink`
  holds; the goldens and README passages listed above moved, and no header or
  report. Departures. A refusal quotes a clause's parameters as written, as
  `?f` shows them, and a line's local, which keeps no text, as the tokens it
  compares, spaced (C158). `use`
  checks every name it brings in before it binds the file, so one refused
  binds nothing. The advice is given where the definition lives, the session
  or a file, and not in a model's body or for a line's local. Found on the way
  and registered: C154, fixed here. 40 lines of sources where about 30 were
  planned, and 45 was the limit: 12 in `reference.hpp`, the restart and C70's
  erasure out against the refusals, the clause on a model or a file, the plain
  clause alone and the advice, with C154's tokens in place of the names, 9
  more than their join; 17 in `reference_stack.hpp`, a model, a file and a
  name brought in only over nothing or their like, in a model's body too, and
  `clear`; 11 in `interpreter.hpp`, the word, and `clear` refused in a model's
  body. 14,727 lines in all, after `budget`. By its review, C158 to C160, 24 lines.
- `[done]` **A truth read from an approximated number.** With `rt_0 = 1`, `rt_n =
  (rt_(n-1) + 2/rt_(n-1))/2` and `c = 14142135623730950488/10^19`, `rt_20 >
  c` answers 0 and says nothing, where rt_20 is above the square root of 2
  and c below it: approximated at the twelfth step, rt_20 is the double one
  unit below the double nearest c. Beside `g(x) = 2` and `g(x) | x > c =
  1`, `g(rt_20)` answers 2, as plainly. C68 decided it: "A comparison is not
  either: it is a truth, exactly." A truth is exact only if what it compares
  is, and one read from a double is not a little off when it is wrong but
  wholly so, and a guard turns it into another clause. The eigenvalue
  research's intervals, counted by Sturm sequences past the bound with
  `sgn(x) = (x > 0) - (x < 0)`, were exact fractions, "certified", and wrong.
  That sentence is reversed, as `MANIFESTO.md` asks of an oracle past its
  horizon: say which it is giving.

  Decided, by one rule, C68's own: what is computed from an approximated
  number is approximated, a choice among it.
  - A comparison of numbers or of whole matrices that reads one answers 1 or
    0 approximated, the double with the mark, as `rt_20*0` is; `and` and
    `or` where a side they read is. A side not read cannot mark them.
  - A guard that reads one chooses by an approximated truth, so the answer
    of the clause chosen is approximated, whether that clause's own guard
    held or one tried before it failed, as `g(rt_20)`'s did. An exact answer
    becomes its double: the mark is an inexact number's, in the half of its
    layout an exact one uses. So beside `bg(x) = 10^400` and `bg(x) | x > c
    = 1`, `bg(rt_20)` is inf and `bg(rt_20)/10^399` -nan, both marked, as
    C68's representation has them: wrong, but not silently. "No clause of h applies" decided so ends ", by
    a guard approximated past a thousand digits". By cells, each cell by the
    guards read for it; under `grad`, the derivative of the clause chosen,
    value and parts, a part it lacks a 0 approximated.
  - So such a truth has no `frac`, is no index or bound, refused as any
    approximated index is, and makes inexact what it multiplies. The mark is
    in the value, so a term remembered is read again marked.
  - Every comparison is marked, however far from its threshold, as nothing
    tells how far the double is from its exact value: `rt_20 > 1` is `1  #
    approximated past a thousand digits`.

  Unchanged: what is inexact by nature or by `~`, unmarked by C68, so
  `~rt_11 > c` is a plain 0, wrong as it is. What such an operation
  computes from an approximated number stays marked, as C68 has it: Reddi's
  Adam transcribed takes `v_n^(1/2)` of exact decimals past the bound and
  is marked, an answer inexact anyway, but `(rt_20 - c)^(1/2)` is
  1.49e-8 imaginary for 4.1e-11 real, and clearing the mark at a root would
  hide it. A limit's stopping rule, in
  doubles always; its answer is a term, marked if the term is. `tex`, which
  prints no value. The compiler, which approximates nothing: a constant it
  folds from a marked truth is the same double, a header with `2^4000 > 1`
  unmoved. `--check` holds the step to an interpreter term chosen by an
  approximated guard as to an inexact one, and says from which index, which
  no report in the tests reaches.

  Two lines of the prelude, so that the rule marks nothing C68 would not.
  `ilogbs(x, k, s) | k + s < -3321 = k + s`, as C101's `k + s > 3321`: every
  exact number is above 2^-3322 and every double too, so a threshold below
  is passed, where now it is 2^-3584 approximated to 0 and passed by x >= 0.
  Without it, ilogb's exact answer chosen there becomes a double, the
  thresholds after it doubles that vanish, and `ilogb(1/10^950)` is -3073
  where it is -3156, `log(1/10^950)` refused. And `logm`'s fold `m*m > 2`
  becomes `m > 2/m`: of an exact argument with parts past 500 digits the
  square passes the bound, and `log(10^999)` would be marked for a choice
  between two reductions equally accurate. `2/m` has m's digits, so it never
  passes, and its truth is the square's for an exact m and for a double,
  whose answers stay as they are; the header's line moves with it.

  Rejected:
  - Marking only an unsafe comparison, its gap below the error's bound. No
    bound is carried, and after a cancellation the double is any distance
    off: `(rt_20 - c)*10^16 > -1` reads -2.22 for about 1.7e-5, 55% of the
    threshold away and on the wrong side. A radius beside each double is
    ball arithmetic, rejected for `--check`'s estimate (*The interpreter's
    own error*); a relative gap would certify that answer.
  - Refusing, as "refuses rather than guesses" would read: a model past its
    horizon -- the epidemic in doubles from its eighth step -- decides most
    guards rightly, and marked it answers and says so.
  - An exact answer kept exact and marked: a mark on exact numbers, carried
    by every fast path of `+`, `-`, `*` and `/`.
  - Kleene's `and` and `or`, an exact side that decides alone clearing the
    mark: a second rule beside arithmetic's, where `0*rt_20` is marked too,
    and reading the exact side first already avoids it.
  - Comparing a double with an exact number exactly: rt_20's double is below
    c too, so `rt_20 > c` stays 0, and `~0.1 == 1/10` moves from 1 to 0.
  - Marking only the clause whose own guard held: `g(rt_20)` is the other
    case.

  What moves, measured with a prototype on every test of `ctest`:
  `bignum.ink`'s `rt_20 > 1`, marked, and the comment above it that states
  C68's sentence; `elementary.ink`'s `tex ?ilogbs`, by its clause;
  `inkamath_prelude.h`'s `ilogbs`, one test more and its answers the same.
  README's paragraph on numbers (section 1) gains the rule, and C68's row a
  line that it is reversed here. Outside the tests, the eigenvalue
  sessions from 8x8 are refused, "an index must be exact, and 9 was
  approximated": the spectral radius and the H-infinity norm at 8x8 and
  10x10 and the 10x10 Hilbert eigenvalues, which answered, and the 12x12
  ones, which ran out of steps or refused already. Their `dim` counts a
  Sturm polynomial's cells by `A[i,1] == A[i,1]`, which past the bound is
  marked, and a size the language has only as a number. Callgrind, prototype against integration:
  `hand` 0.4%, `grad` 1.1% and `limit` 1.3% more.

  About 55 lines of sources: `matrix.hpp` 7, the comparison and the mark;
  `number.hpp` and `numeric_interface.hpp` 4, the mark; the evaluator's
  `and` and `or` 4; `reference.hpp` 18, the guards, cells and refusal;
  `derivative.hpp` 20, the same under `grad`; the prelude 1. 14,653 lines in
  all now, about 14,710 after; past about 83, the implementation stops and
  reports.

  Specified in `test/data/spec/approximated.ink`, 77 entries, 34 failing:
  every comparison and `and`, `or` and matrix `==` against what stays exact
  beside it; arithmetic, `frac`, indices, a sum's bound and a remembered
  count; guards that hold, fail, are not read, or refuse; cells; `grad`;
  the prelude's guards, and the answers its two lines keep. Expected values
  by exact arithmetic and the doubles emulated in Python, apart from the
  interpreter.

  Built as specified: every entry passes as written, and the spec is the
  golden `approximated.ink`. What moved is what the prototype measured:
  `bignum.ink`'s `rt_20 > 1` and its comment, `elementary.ink`'s `tex
  ?ilogbs` and the prelude's header, whose values are unchanged: 159,003
  calls on doubles, 1 over each among them, print alike compiled and
  walked, and 35,032 on exact and double arguments as before; with the
  rule, of those only the calls of an argument already approximated move,
  marked, and C101's. Departures:
  under `grad` a part the clause chosen lacks stays lacking rather than
  becoming a 0 approximated, so that nothing it leaves constant moves, by
  the review's ruling; grad's answer where the body has no derivative is 0
  approximated if the body's value is, so `grad_(x = rt_20) floor(x)` is
  marked too, floor's jump decided by an approximated number; and, by its
  review, grad's whole answer is marked where such a part might reach it
  (C165). An input's argument reached past a guard approximated is marked
  as a clause is.
  C101's `log(2^3321)`, which answered 2^3321, answered inf marked, as
  wrong, until C157. Callgrind against integration: `deep` 0.8%,
  `grad` 0.8% and `limit` 0.7% more. 71 lines of sources where about 55
  were planned: `derivative.hpp` 27, `reference.hpp` 28, `matrix.hpp` 12,
  `number.hpp`, `numeric_interface.hpp`, the evaluator's `and` and `or` and
  the prelude 1 each. 14,836 lines in all, after `fixes10`.
- `[done]` **A size bound by a signature**, the first step towards eigenvalues:
  `tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]`, M in R^(n x n), so that a
  function loops over its argument's size. The eigenvalue prototypes
  measured one, `dim(A) = on(A)'*on(A)` with `on(A)[i] = A[i,1] ==
  A[i,1]`, a comparison of the point that `grad` refuses as a jump and the
  compiler cannot fold. Today the text parses as something else: a parameter
  that is not a name is dropped in silence (C150), so `tr([1 2; 3 4])` is
  "tr takes no arguments" and `tex ?tr` sets `\mathit{tr}`.

  Decided. A parameter is a name, `x` or `x = 1`, or a name and its size,
  by bounds as a model's input states one (C83): `v[j<=n]` a column,
  `M[j<=m, k<=n]` a matrix, `T[b<=p, j<=m, k<=n]` a tensor, slices first. A
  bound is a whole number, at least 1, or a name, a size the call binds to
  the argument's, exact and constant for that call; a name repeated, in one
  parameter or two, is one size, so `M[j<=n, k<=n]` is square and `dot(x[j<=n],
  y[j<=n])` takes two columns of one length. A single value is a 1x1 matrix,
  as a size inferred reads one, and a row is not a column, as for an input.
  A size that disagrees is refused at the call, before the body, naming the
  parameters that state it, as written, and what each was given: `tr takes
  M[j<=n, k<=n], not a 2x3 matrix`, `dot takes x[j<=n] and y[j<=n], not a
  2x1 matrix and a 3x1 matrix`. It is a statement, not a guard: no other
  clause is tried. Anything else in a signature is refused where written, `a
  parameter is a name, as 'x' or 'x = 1', or a name and its size, as
  'v[j<=n]'`, and a bound that is neither, `a size is a whole number or a
  name, as 'v[j<=3]' or 'v[j<=n]'`.

  A size is bound as a parameter is: the body alone sees it, as a sum's
  bound, a number, an index, a cell's bound or an argument, `id(n)`, and it
  hides the session's name. That answers *Sizes inferred*'s objection to
  `[j<=n]`, whose meaning would change the day a global n is defined: n is
  bound here as x is in `f(x)`. A model's input reads the names in its
  bounds and binds none, since the model is compiled before any argument
  exists; a function is compiled where it is called, at its argument's
  shape. The index names, j and k, bind nothing: they say which bound is
  which, and the body may reuse them, as tr's sum and `rev(v[j<=n])[j<=n]`'s
  cells do. Binding them was rejected, a value of j meaning nothing outside
  a cell. A size named as a parameter, the definition's index or one of its
  cells' indices is refused, `bad has a parameter and a size named n`;
  reading the parameter, as a model's bound does, was rejected as a second
  way to state one size, `f(n, x[j<=n])` saying nothing `f(x[j<=n])` does
  not.

  The sizes are the definition's, as its parameters' names are (C51):
  stated in one clause, they hold for every clause and are bound once per
  call, a clause with the names alone taking them, so a base clause need
  not repeat them; two that state different sizes are refused, the
  names of their indices aside, `fm takes A of size n x n, so a clause
  cannot take it of size p x p; write 'clear fm' first`. Each clause's own
  was rejected: a guard would see n in one clause and not the next. A
  parameter binds its sizes as it is bound, so a default reads the sizes of
  the arguments given, `scale(v[j<=n], s = n)`, and a default is held to its
  parameter's size and gives it, `pad(v[j<=n] = [1; 2])`; one that reads a
  size stated by its own parameter or a later one is refused where written,
  `late's default for s reads the size n, not stated before it`. A
  repeated index in a signature binds nothing, so `M[j<=n, j<=n]` means
  `M[j<=n, k<=n]`. Refusing every such repetition, in signatures and model
  inputs alike, is queued as *Refuse a repetition that adds nothing*.

  Nothing new elsewhere. The memo keys a call by its arguments and their
  shapes (C50), the sizes a function of them; `sum`, `lim` and a function's
  sequence read a size as a parameter. Under `grad` a size is a constant,
  the shape never moving. `?name` prints as written; `tex` sets a size as an
  input's, `\operatorname{tr}(M \in \mathbb{R}^{n \times n})`, each clause
  as written. Compiled, a call's sizes are its argument's extent, folded
  where it is called, and a disagreement is refused in the interpreter's
  words; `--check` needs nothing. The prototypes' characteristic polynomial,
  Faddeev-LeVerrier, needs no `dim`:

      id(n)[j<=n, k<=n] = j == k
      fm(A)_0 = 0*A
      fm(A[j<=n, k<=n])_m = A*fm(A)_(m-1) + fc(A)_(m-1)*id(n)
      fc(A)_0 = 1
      fc(A)_m = -tr(A*fm(A)_m)/m
      cp(A[j<=n, k<=n])[j<=n+1] = fc(A)_(j-1)
      det(A[j<=n, k<=n]) = (-1)^n*fc(A)_n

  and `grad_(A = [1 2; 3 4]) det(A)` is the cofactor matrix. It does not
  compile: `fc(A)_n` is a function's sequence read at an index, "a sequence
  with parameters", which *A sequence with parameters read at a constant
  index, compiled* lifts.

  Rejected besides: a type, `M : R^(n x n)`, rejected for inputs as a
  second way to write a size beside bounds; `rows(M)`, which no paper writes
  (*Sizes inferred*); `M[n, n]`, which reads a cell; and sizes for a
  model's parameters, `k[j<=2] = [1; 2]`, refused as now, which nothing has
  asked for.

  About 105 lines: the signature read and its refusals 30, the definition's
  sizes and the clauses' agreement 12, binding and checking with its words
  35, shared by the interpreter's two calls, `grad`'s and the compiler's,
  `tex` 6, the compiler's constants 15, the rest 7. The prototypes' guess of
  40 to 70 counted the binding alone. Past 160 the implementation stops and
  reports. 14,653 lines at eee8d5e, by `wc -l include/inkamath/*.hpp src/*`.

  Specified in `test/data/spec/signatures.ink`, 73 of its 103 entries
  failing, those passing being definitions echoing themselves, `?tr` and
  `n`; and in `test/compile/signatures.ink`, a model calling tr at two
  shapes, whose header is to be the one its sizes written as numbers give,
  byte for byte, and whose `--check` report is exact, with one refusal; both
  wired with the implementation. README's section 3 gains `tr` with the
  implementation.

  Built as specified, but for the overrun's ruling: the spec is the golden
  `signatures.ink`, 102 entries, the refusal of a parameter's repeated
  index queued with its entry (above); `test/compile/signatures.ink`'s
  header is its numbered twin's, byte for byte, `inv`'s report is exact and
  its refusal is a case of `cli.cmake`; README's section 3 gains `tr` and a
  size hiding a session's name, and section 5 a model's bound reading one.
  Departures. A function's parameter and a model's input are read by one
  helper, and `floor` is given its parameter as a list, as the parser gives
  one. The definition binds its defaults and its sizes,
  `Reference::BindDefaults`, for the interpreter's calls and `grad`'s; the
  compiler binds its own, one default and its sizes at a time, by the same
  measure. A bound that is not whole, `x[j<=2.5]`, is refused as an index
  is, "an index must be a whole number, not 2.5". About 105 lines planned,
  164 landed, past the stop line by decision: the first build was 197, a
  review for size took out 29, mostly by sharing what a model's input and
  `Reference::Size` already had, and the ruling 7, the refusal and a line it
  freed, and C166 put 3 back. `reference.hpp` 108, the definition's sizes, their agreement and
  their names, and binding and measuring a call's, with `BindDefaults`
  moved there; `parameters.hpp` 57, reading a parameter; `compile.hpp` 9;
  `latex.hpp` 5; `extent.hpp` 3; `reference_stack.hpp` 2; `interpreter.hpp`
  -20, a model's input read by the shared helper. 15,007 lines in all,
  after `fixes11`.
- `[done]` **The characteristic polynomial and stability, in the prelude**,
  the second step towards eigenvalues, approved by the owner: `charpoly(A)`
  exactly, Routh's and the Schur-Cohn tests, and the spectral radius and
  abscissa by bisection on those tests, certified while they are exact.
  Written in inkamath with the sizes a signature binds, the eigenvalue
  research's `dim(A) = on(A)'*on(A)` gone. The design, measured on a
  prototype:

      charpolym(A)_1 = A^0
      charpolym(A)_m = A*charpolym(A)_(m-1) + charpolyc(A)_(m-1)*A^0
      charpolyc(A)_0 = 1
      charpolyc(A[j<=n, k<=n])_m = -sum_(j=1)^n (A*charpolym(A)_m)[j,j]/m
      charpoly(A[j<=n, k<=n])[j<=n+1] = charpolyc(A)_(j-1)
      hurwitzb(a, b) = !a/(!b*!(a - b))
      hurwitzb(a, b) | b < 0 or b > a = 0
      hurwitzc(q[j<=m], i) = 0
      hurwitzc(q[j<=m], i) | i <= m = q[i]
      hurwitzr(q)_0 = q
      hurwitzr(q[j<=m])_k[j<=m] = hurwitzc(hurwitzr(q)_(k-1), j + 1) - mod(j + 1, 2)*hurwitzr(q)_(k-1)[1]/hurwitzr(q)_(k-1)[2]*hurwitzc(hurwitzr(q)_(k-1), j + 2)
      hurwitzt(q[j<=m])_0 = sum_(j=1)^m abs(q[j]) > 0 and q[1] <> 0
      hurwitzt(q)_k = hurwitzt(q)_(k-1) and hurwitzr(q)_(k-1)[2]*q[1] > 0
      hurwitzs(p[j<=m], s)[t<=m] = sum_(i=1)^t p[i]*hurwitzb(m-i, m-t)*s^(t-i)
      hurwitz(p[j<=m], s = 0) = hurwitzt(hurwitzs(p, s))_(m-1)
      schurcohnm(p[j<=m], r)[t<=m] = sum_(i=1)^m p[i]*r^(m-i)*sum_(a=0)^(m-i) hurwitzb(m-i, a)*hurwitzb(i-1, m-t-a)*(-1)^(m-t-a)
      schurcohn(p[j<=m], r = 1) | r >= 0 and sum_(j=1)^m abs(p[j]) >= 0 = hurwitz(schurcohnm(p, r))
      rhon(A[j<=n, k<=n]) = rhop(sum_(j=1)^n sum_(k=1)^n abs(A[j,k]))
      rhop(s) = 2^(ilogb(s + (s == 0)) + 1) + 0*s
      rhos(b) = b[2] - b[1] <= 2^-53*min(abs(b[1]), abs(b[2]))
      rhob(A)_0 = [0; rhon(A)]
      rhob(A)_m = rhon(A)*rhoh(charpoly(A/rhon(A)), rhob(A)_(m-1)/rhon(A))
      rhob(A)_m | m > 64 and rhos(rhob(A)_(m-1)) = rhob(A)_(m-1)
      rhoh(p, b) = b + (b[2] - b[1])/2*([1; 0] - schurcohn(p, (b[1] + b[2])/2)*[1; 1])
      rho(A[j<=n, k<=n]) = ~rhob(A)_256[1]
      abscissab(A)_0 = [-1; 1]*rhon(A)
      abscissab(A)_m = rhon(A)*abscissah(charpoly(A/rhon(A)), abscissab(A)_(m-1)/rhon(A))
      abscissab(A)_m | m > 64 and rhos(abscissab(A)_(m-1)) = abscissab(A)_(m-1)
      abscissah(p, b) = b + (b[2] - b[1])/2*([1; 0] - hurwitz(p, (b[1] + b[2])/2)*[1; 1])
      abscissa(A[j<=n, k<=n]) = ~abscissab(A)_256[1]

  `charpoly(A)` is det(lambda I - A) as a column, highest power first and
  monic, as MATLAB's and NumPy's `poly` give it: Faddeev and LeVerrier, M_1
  = I, M_m = A M_(m-1) + c_(m-1) I, c_m = -tr(A M_m)/m, exact, of complex
  entries too, and differentiable, so `grad` of its last coefficient is the
  cofactor matrix. `hurwitz(p, s = 0)` is 1 where every root of p is in Re z
  < s, `schurcohn(p, r = 1)` where every one is in |z| < r, both strictly:
  the polynomial a column of m cells, degree m - 1. Routh's test is its
  first column, all of p[1]'s sign and none 0, taken as a recurrence on the
  polynomial rather than on pairs of rows: p_k drops its leading coefficient
  and subtracts a0/a1 times its odd part, as the array's next row does, so
  its leading coefficient is the column's next entry, and the test reads
  that entry before the term that divides by it, `and` not reading further.
  A shift `p(z + s)` gives the half-plane Re z < s; the map
  `z = r(1 + w)/(1 - w)`, the disc |z| < r onto Re w < 0, the Schur-Cohn
  test. A leading 0 is a root at infinity, which neither region holds, so it
  answers 0: the map's image has one exactly where -r is a root, on the
  circle, and needs no case of its own. The zero polynomial answers 0, a
  nonzero constant 1.

  `rho(A)` and `abscissa(A)` bisect a bracket, `rhob(A)_m` and
  `abscissab(A)_m`, [lo; hi] with lo <= rho < hi: the Schur-Cohn test at the
  midpoint is the truth that rho < x, Routh's after the shift that alpha <
  x, and the bracket keeps the half it says. From [0, B) and [-B, B), B the
  power of two above the sum of |A[j,k]|, which bounds rho strictly, 2 for
  the zero matrix; so every midpoint is dyadic, the brackets of 2A are those
  of A doubled, and `frac rhob(A)_m` shows one exactly, as tight as m asks.
  The tests are of A/B, exactly, B being a power of two, and the bracket is
  scaled back by B, so the tests' numbers are near 1 whatever A's scale.
  Unscaled, the prototype's `rho([~1 2; 3 4]/10^100)*10^100` was 18.3, its
  polynomial's rounded coefficients near 10^-200, `rho(10^-300)` was
  1.49e-300 and marked, its tests past a thousand digits, as were exact
  `rho([1 2; 3 4]/10^150)`, 1.96e-149, and times 10^250, which refused a
  NaN.
  The answer is lo, made inexact, after 64 halvings and as many more as
  leave the bracket wider than 2^-53 of its end nearer 0, `rhos`, up to
  256: within a unit of a double of rho wherever rho is above 2^-203 of B.
  After 64 alone, `abscissa([1/10^30 0; 0 -1])`, unstable, was 0 unmarked,
  and `rho([0 1; 1/10^30 0])`, 10^-15, right to 4 digits. A 0 never stops
  it, a nilpotent matrix's rho or a marginal system's abscissa, hence the
  cap; 256, as past it a test of a matrix above 3x3 would read, by the
  estimate below, numbers past a thousand digits, so the halvings would be
  marked, and 2^-203 of B is far below what a double's eigenvalue resolves,
  about 2^-52 of the matrix's norm. An answer the cap ends, wider than
  `rhos` asks, is marked (C192), a 0 always. A bound on the boundary fails
  its strict test and becomes lo, so an eigenvalue a halving reaches is the
  answer exactly: `rho([2 0; 0 -3])` is 3, `abscissa([1/2 1; -1 1/2])` 0.5,
  a nilpotent matrix's rho 0, marked. The research's comment said (lo,
  hi]; its own guards kept [lo, hi).

  What it costs. A test at a midpoint of b bits reads, in Routh's column,
  ratios of Hurwitz determinants of polynomials whose coefficients carry n b
  bits, so about n^2 b bits, and past 3,322 (a thousand digits) they are
  approximated and, by *A truth read from an approximated number*, every
  test after is marked and so is the answer. On random matrices of tenths,
  certified to 64 halvings up to 6x6 for `rho`, whose map doubles the bits,
  and 8x8 for `abscissa`; on the research's, the marks start at the 39th
  halving of `rho` at 8x8, the 26th at 10x10, the 19th at 12x12, and of
  `abscissa` at the 51st at 10x10. Marked, the answers are a double's:
  against NumPy, within 2e-15 relative at 10x10 and 12x12, 1.3e-13 at 32x32.
  0.1 s at 5x5, 0.2 s at 10x10, 0.4 s at 12x12, 9 s at 24x24 and 55 s at
  32x32, every term inside its own million steps: no budget is added.

  Decided besides:
  - **Names.** `charpoly`, as sympy and PARI say it; `poly` is too common a
    word. `hurwitz` names the property, a Hurwitz polynomial, and
    `schurcohn` the test for a Schur polynomial, every root in the disc, by
    whatever method, leaving `schur` free for the decomposition the
    eigenvalue work may want; `routh`, `jury` and `stable` were rejected.
    `rho` is the paper's rho(A); `abscissa`, as `alpha` is every learning
    rate's name. A session may take any of them for itself, `rho = 1000` a
    density, and `clear` gives it back. The helpers are prefixed by their
    function, as `expk` and `sinr` are, `hurwitzb`, the binomial
    coefficient, too: a helper, not a public `binomial`, by the review's
    ruling.
  - **Refused.** A matrix not square, by its signature:
    `rho takes A[j<=n, k<=n], not a 3x2 matrix`. A complex entry by every
    test, in abs's words, as C147 left `abs`:
    `a comparison needs real numbers, not i`, every cell passing through abs
    before Routh reads one, as a complex polynomial can pass the column: the
    map's image of z + i, `(1 - i)w + (1 + i)`, tests (1 + i)(1 - i) = 2 >
    0. The sum of the abs exists for that alone, and a prelude comment says
    so. `schurcohn` checks the polynomial written, not its image, so
    `schurcohn([1; i])` names i, not a cell of the image, 1 - i; and
    `hurwitz` its s, guarded by `abs(s) >= 0` as `schurcohn` by `r >= 0`,
    so `hurwitz([1; 1/2], i)` names i, not 1/2 + i, by the implementation
    review's ruling. Their own
    words would be about 5 lines of C++ beside C147's: kept in abs's, by
    the review's ruling. A negative radius,
    `no clause of schurcohn applies`.
  - **An inexact matrix** is bisected by rounded tests whose truths are
    exact, so its bracket would print exact fractions certifying nothing;
    `+ 0*s` keeps it inexact, and `frac` refuses it. An approximated one
    marks the answer, as any choice it makes.
  - **grad.** A bracket is a staircase in A, so grad's rule, flat where no
    comparison jumps, makes `grad` of `rho` 0 everywhere, exactly the
    staircase's and not rho's. Refused, `grad cannot differentiate rho yet`,
    where the argument moves: a pointer to each of the two in the stack, as
    `stepwise` points to `ilogb`, and one check in `derivative.hpp`'s call,
    about 6 lines of C++. The tests and the brackets keep grad's rule: flat,
    or refused where they jump. Rejected for now: the research's Newton
    polish, one step on p from the certified point, which serves a real
    simple eigenvalue, where both examples here have a dominant pair; and a
    step on D(r), the last Hurwitz determinant of the map's image, whose
    root is rho for a pair too, but whose slope is wrong without a word
    where two eigenvalues share the largest modulus, and which divides by 0
    where the bracket's end is exact. A later item, with a model that trains
    a spectral radius.
  - **Stopping.** 64 halvings at least, then relative, up to 256, by the
    review's ruling, which kept the 64. Rejected: 64 halvings alone, which
    printed an unstable abscissa of 10^-30 as 0; relative alone, which
    never stops at 0, where a nilpotent matrix and many abscissas are, but
    for a cap; absolute, which breaks the scaling and wastes steps on a
    large matrix; `lim`, which stops by a double's tolerance; and stopping
    where the tests stop being exact, which nothing in the language can
    ask. The answer is the lower end, not the midpoint, so 0 prints as 0
    and a dyadic eigenvalue as itself; the bracket as the answer was
    rejected, a column where a number is asked.
  - **Schur by the map, then Routh**, as the research did. The Schur-Cohn
    recursion, (a0 p - an p~)/z, was weighed: no map and no binomials, but
    five lines where the map costs three over Routh, and its fraction-free
    form doubles the bits at each step.
  - **Compiled**: `charpoly` compiles since *A sequence with parameters
    read at a constant index, compiled*; `rho` and `abscissa` do not yet,
    `hurwitzb`'s factorial refused. grad refuses them in `compile.hpp`'s
    call where an argument moves, `Staircase`, as `derivative.hpp` does:
    without it, their refusal under grad came only from the factorial and
    from `A^0`'s derivative, and once those compile grad would answer the
    staircase's 0. A case of `test/cli.cmake` holds both.

  What moves: no golden and no header, as no test names any of them; the
  prelude is read at every start, 33 lines more. README's section 1 lists
  the names with the prelude's others, and section 2 shows `charpoly` and
  `rho`.

  About 45 lines: 31 of the prelude, the stop's 3 and a longer `schurcohn`
  among them, some 8 of its comment, and 6 of C++ for grad. 15,055 lines at
  75c03ae, by `wc -l include/inkamath/*.hpp src/*`. Past 60 the
  implementation stops and reports.

  Specified in `test/data/spec/charpoly.ink`, 88 entries replayed by the
  spec suite, 80 failing by design, those passing being the definitions
  echoing themselves, `digits` and a session's `rho`: values by sympy,
  mpmath at 60 digits and NumPy, and each bracket by bisection in Python's
  fractions with every test decided from the exact eigenvalue, apart from
  the interpreter. The prototype above, defined in a session, gives every
  answer but grad's two refusals and the last, which needs the name to be
  the prelude's.

  Built as specified: the spec is the golden `charpoly.ink`, 88 entries,
  and the compiled refusal a case of `cli.cmake`; README's sections 1 and 5
  list the names, and section 2 shows `charpoly`, `hurwitz`, `rho` and
  `abscissa` and says where the answers stop being certified. The compiled
  prelude header is unchanged, as its file calls none of them. Departures:
  the staircases are a set of the two in the stack, and grad's check sits
  in its call, before a limit or a term; the comment is 8 lines. 0.2 s for
  the 10x10 here, 0.1 s for a nilpotent 2x2 run to 256 halvings; the
  golden takes 31 s under the sanitizers, within the 60 s of every golden
  but fastgrad.ink, its scaled entries a 2x2's, which fails unscaled as the
  10x10 did, at 2 s where the 10x10 took 63. The 10x10's two answers are
  since `bisection_digits.ink`, and the bisections the cap ends
  `bisection_cap.ink`, goldens of their own that run beside it: 11 s, 16 s
  and 13 s under the sanitizers with C192's entries, where the one file
  took 50 s on the build worker. 50 lines landed against about
  45: the prelude 33 and its
  comment 8 in `interpreter.hpp`, with 2 binding the staircases,
  `reference_stack.hpp` 3 and `derivative.hpp` 4. The review's C190 put
  the bisection in A/B's units, `rhou` and `abscissau`, `rhod` scaling by
  B's power in two halves, so that B need not be a double: 4 lines more;
  C192 marks an answer the cap ended, 3 more: 57 in all. Compiled grad's
  refusal, after the merge of `fixedloop`, 9 more in `compile.hpp`: 66 in
  all. 15,256 lines in all, after `charpoly`, on integration's 15,190.
- `[done]` **The real eigenvalues and the largest singular value, in the
  prelude**, the third step towards eigenvalues: `eig(A)`, every eigenvalue
  of a matrix whose characteristic polynomial has only real roots, a
  symmetric one first, as a column, smallest first, each certified by
  bisection as `rho` is; and `smax(A)`, sqrt(lambda_max(A'A)). Written in inkamath on
  `charpoly`, `hurwitzs` and `rho`'s scaling and stop. The design, measured
  on a prototype:

      eigl(q)_1 = q[1]
      eigl(q)_j = eigl(q)_(j-1)
      eigl(q)_j | q[j] <> 0 = q[j]
      eigv(q[j<=m]) = sum_(j=2)^m (q[j]*eigl(q)_(j-1) < 0)
      eigm(A[j<=n, k<=n])[j<=n, k<=n] = sum_(i=1)^n (A^(j+k-2))[i,i]
      eigr(A) = A' == A or eigv(charpoly(-eigm(rhod(A, -rhoe(A))))) == 0
      eigz(A[j<=n, k<=n], i) = eigv(charpoly(-A)) < i and eigv(charpoly(A)) <= n - i
      eigh(p, i, b) = b + (b[2] - b[1])/2*([1; 0] - (eigv(hurwitzs(p, -(b[1] + b[2])/2)) >= i)*[1; 1])
      eigu(A, i)_0 | eigz(rhod(A, -rhoe(A)), i) = [0; 0]
      eigu(A, i)_0 = [-1; 1]
      eigu(A, i)_m = eigh(charpoly(rhod(-A, -rhoe(A))), i, eigu(A, i)_(m-1))
      eigu(A, i)_m | m > 64 and rhos(eigu(A, i)_(m-1)) = eigu(A, i)_(m-1)
      eigb(A, i)_m = rhod(eigu(A, i)_m, rhoe(A))
      eigk(A, i) = rhoa(eigu(A, i)_256, eigb(A, i)_256)
      eigc(A[j<=n, k<=n])[i<=n] = eigk(A, i)
      eig(A[j<=n, k<=n]) | eigr(A) = eigc(A)
      smax(A[j<=m, k<=n]) = rhod(eigk(rhod(A, -rhoe(A))'*rhod(A, -rhoe(A)), n)^(1/2), rhoe(A))

  with `rhoe(A[j<=m, k<=n])`, any matrix, rho's own signature still
  refusing one not square first.

  **The count.** Descartes' rule of signs is exact for a polynomial whose
  roots are all real: the sign changes of its coefficients, zeros skipped,
  are its roots above 0 with their multiplicity. So the eigenvalues of A
  below x are the sign changes of charpoly(-A)'s shift by -x, `hurwitzs`,
  which Routh's test already has, and lambda_k < x exactly where there are
  k of them or more. `eigv` counts, `eigl` carrying the last sign that is
  not 0. The bracket of the k-th, `eigb(A, k)_m`, is bisected as `rho`'s
  is: of A/B from [-1, 1), B = 2^e above the sum of |A[j,k]| (C190), the
  test at the midpoint keeping the half it says, [lo, hi) with lo <=
  lambda_k < hi, the answer lo made inexact, so a dyadic eigenvalue is
  itself. Each k is bisected on its own count, so equal eigenvalues are
  never parted, each copy the same answer, and distinct ones are parted at
  the first midpoint between them, 1 -+ 10^-12 within 64 halvings; the memo
  shares every midpoint two bisections have in common, so a repeated
  eigenvalue costs one. Ascending, as MATLAB's `eig` of a symmetric matrix
  and NumPy's `eigvalsh` give them.

  **All real, or refused.** The count is exact only if every root is real;
  of x^2 + 1 it would answer 0 twice. A symmetric matrix's are, so `A' ==
  A` asks nothing more. Another's are tested by Hermite's criterion: the
  Hankel matrix of power sums, H[j,k] = tr(A^(j+k-2)), is positive
  semidefinite exactly where every root is real, its rank the distinct
  roots and its signature the distinct real ones; H being symmetric, that
  is charpoly(-H) without a sign change, Descartes again. Of A/B, so that
  its powers stay near 1. A matrix with a complex eigenvalue is refused,
  `eig needs a matrix whose eigenvalues are all real`, its real ones not
  given alone: a column whose length depends on the values is no column a
  paper writes. The prelude cannot word a refusal, so the guard's failure
  on eig is given these words in C++, about 5 lines inside `Unapplied`,
  which keeps its suffix where the guard was approximated past a thousand
  digits: `eig needs a matrix whose eigenvalues are all real, by a guard
  approximated past a thousand digits` is what an exact matrix whose test
  passes them may get, its eigenvalues real or not.
  `no clause of eig applies`, the words with none, says nothing of why,
  and `abs(i)`'s refusal as the words was rejected as a lie that reads
  well.

  **Zero, certified.** 0 is the one value the relative stop never reaches,
  so `rho` of a nilpotent matrix runs to the cap and is marked (C192).
  Here it is certified where it is exact, by the counts at 0, which need no
  shift: lambda_k = 0 where fewer than k eigenvalues are below 0 and at
  most n - k above, and its bracket is [0; 0] from the start, `rhos` true
  of it, so the answer is 0 unmarked: a singular symmetric matrix, the
  zero matrix, a nilpotent one. One line. Any other exact eigenvalue that
  is not dyadic stops by the relative rule; one below 2^-203 of B, not 0,
  is the bracket the cap leaves, marked, `eig([1/10^100 0; 0 1])` [0; 1].
  One past a double's range is the double `~` makes of it, inf or a 0
  that is not one, marked though every test is exact, by `rhoa` as
  fixes14 marks rho's and abscissa's: `eig([2 1; 1 3]/10^400)`, [0; 0].

  **What it costs.** A test is one shift, about m^2 products, and a count;
  no Routh column and no map, so a test at a midpoint of b bits reads
  numbers of about n b bits, where `rho`'s read n^2 b. On the research's
  symmetric matrices of tenths, certified up to 20x20, marked at 24x24
  and then within a double of NumPy's, measured on symmetric matrices
  alone: a marked repeated eigenvalue of another can be off at the ninth
  digit, as an inexact one's is; 0.03 s for a 2x2, 0.05 s for
  Hilbert's 5x5, 0.3 s for its 8x8, 0.4 s at 10x10, 1.1 s at 12x12, 4 s at
  16x16, 14 s at 20x20 and 36 s at 24x24. The spec replays in 10 s under
  the sanitizers. Hermite's test is one `charpoly` more, of an n x n.

  Decided besides:
  - **Names.** `eig`, MATLAB's and Octave's; `eigvals` and `eigenvals`,
    NumPy's and sympy's, are longer for nothing. `eigb` is the bracket, as
    `rhob` is, the helpers prefixed `eig`. `smax`, the paper's sigma_max;
    `norm` is a vector's and Frobenius's too, `sigma` every activation's,
    `svd` a decomposition, `norm2` read as a squared norm.
  - **smax.** sqrt of the largest eigenvalue of S'S, S = A/2^e with
    rho's e, scaled back by 2^e: A'A unscaled would be 10^400 of
    `[~1 2; 3 4]*10^200`, inf, where sigma is a double. S'S is n x n, n
    A's columns; the smaller of S'S and SS' would spare a wide matrix's
    zero eigenvalues, certified at once: a line for nothing. Its
    eigenvalue is `eigk`'s at k = n, by the same count, not `abscissa`'s,
    whose Routh column reads n^2 b bits and whose 0 is marked. Past a
    double's range its answer is marked as eig's, by its own scaling back,
    which comes after `rhoa`: a line.
  - **Refused besides.** A matrix not square, by eig's signature. A complex
    entry in abs's words, `a comparison needs real numbers, not i`, `rhoe`
    reading every cell before a test does, so a Hermitian matrix too, as
    `rho` refuses it; of smax, A's cell, not one of A'A.
  - **An inexact matrix** is bisected by rounded tests, its bracket inexact
    and `frac` refusing it, as rho's. Hermite's test is rounded too, and
    H, a Hankel matrix of power sums, is ill-conditioned, so the review
    found most nonsymmetric inexact matrices refused from about 5x5 even
    with their eigenvalues well apart, and symmetric ones but for rounding,
    `Q*D*Q'`, from 5x5 or 6x6; `(A + A')/2` is the way round. An exact
    one whose test passes a thousand digits may be refused,
    `[1/3 + 1/7^600 1; 0 1/3 + 1/7^600]`, its refusal saying so, and
    nonsymmetric 6x6 to 8x8 matrices whose cells have a few dozen digits
    reach that horizon: H's entries are powers up to 2n - 2, and its
    characteristic polynomial their products. Or it may pass, the answer
    marked, as any an approximated guard chose: `[1 1; -1/10^500 1]`, whose
    eigenvalues are complex, is [1; 1] marked. A k-fold eigenvalue of an
    inexact matrix is good to about 16/k digits, `eig([~1 1; 0 1])` [1;
    ~1.00000001] and the inexact 3x3 identity's third ~1.0000069, and a
    complex pair within about 8 digits of the real axis may be accepted,
    `[~3 10^-7 0; -10^-7 3 0; 0 0 1]` as about [1; 3; 3]. README says each.
  - **grad.** A bracket is a staircase: `eig` and `smax` join `rho` and
    `abscissa` in the stack's staircases, `grad cannot differentiate eig
    yet` in the interpreter and the compiler alike, one line. A simple
    eigenvalue's derivative, v'dA v of a symmetric matrix, is a later item
    with rho's.
  - **Compiled**: neither, as `rho`: `eig` stops at `A' == A`, "a
    comparison of matrices", and `smax` at `hurwitzb`'s factorial. A case
    of `test/cli.cmake`, `g_n = grad_(a = n) eig([a 1; 1 2])[1]`, holds
    compiled grad's refusal.

  Rejected:
  - **Sturm's sequence**, the research's `sturm.ink` and `eig.ink`: it
    counts distinct real roots of any polynomial, so it tells real roots
    from complex ones by itself, but it needs polynomial division in
    inkamath, a leading coefficient, a shift and a remainder recurrence,
    and a tower of gcds for the multiplicities, about 30 lines against
    `eigl`, `eigv`, `eigm` and `eigr`'s 6, and its remainders'
    coefficients grow where a shift's do not.
  - **Budan and Fourier**, the research's `symeig.ink`: the same count as
    the shift, by n derivatives evaluated at x, each a helper the shift
    does not need.
  - **Sylvester's inertia**, the negative pivots of A - xI: a pivot is 0
    at a dyadic x where a leading minor has that eigenvalue, which
    exact elimination cannot step over, and it serves a symmetric matrix
    alone.
  - **Refusing every matrix not symmetric**: `[1 2; 3 4]`, a companion
    matrix and a triangular one have real eigenvalues, and Hermite's test
    is a line.
  - **Certifying every dyadic eigenvalue exactly**, by a second count at
    each midpoint: a nonzero dyadic one is reached and stops; 0 alone
    needed it.
  - **The research's `lim`** stop and its `ilogb(nb(A)) + 50` halvings,
    replaced by rho's, as rho's were.

  **H-infinity, later.** ||G||_inf of C(sI - A)^-1 B, A stable, is the
  least gamma such that the Hamiltonian [A, BB'/gamma^2; -C'C, -A'] has no
  eigenvalue on the imaginary axis, bisected on gamma as the research's
  `hinf.ink` did. Its characteristic polynomial is even, q(s^2), and the
  test is whether q has a real root at or below 0: q's roots are not all
  real, so Descartes cannot count them. It needs a count of real roots of
  any polynomial in a half-line: Sturm's sequence, or Hermite's forms,
  whose signatures count distinct real roots, weighted by -u for those
  below 0, read by Descartes on their characteristic polynomials, which
  are real-rooted, so with what this item adds. Then an upper bound to
  start from, `hurwitz` for A's stability, a D term's gamma^2 I - D'D, and
  the staircase. Not specified here.

  What moves: no golden and no header, as no test names any of them; the
  prelude gains 17 definitions, read at every start. README's section 1 lists
  `eig`, `eigb` and `smax` with the prelude's others, and section 2 shows
  `eig` and `smax` and says what is refused.

  About 31 lines: 20 of the prelude, two of them wrapped, some 6 of its
  comment, 5 of C++ for the refusal's words and 1 for the staircases.
  15,256 lines at d3c2149, by `wc -l include/inkamath/*.hpp src/*`. Past
  46 the implementation stops and reports.

  Specified in `test/data/spec/eig.ink`, 54 entries replayed by the spec
  suite, 47 failing by design, those passing being definitions echoing
  themselves, `digits` and a session's `eig`: eigenvalues exactly with
  sympy, its real roots compared exactly, Hilbert's with mpmath at 60
  digits, the 6x6 and `smax` against NumPy, and each bracket by bisection
  in Python's fractions with every test decided from the exact
  eigenvalue, printed by a transcription of `Number::Shown`, apart from
  the interpreter. The prototype above, defined in a session, gives every
  answer but the three refusals' words, grad's two, the marks past a
  double's range and a session's `eig`, which needs the name to be the
  prelude's.

  Built as specified: the spec is the golden `eig.ink`, 54 entries, and the
  compiled refusals a case of `cli.cmake`; README's sections 1 and 5 list
  the names, and section 2 shows `eig` and `smax` and says what is refused.
  Departures: smax's mark is `smaxd`, a definition more, which marks where
  scaling back by 2^e does not come back to the same double, so a
  subnormal it loses digits of too, and a subnormal it does not, of
  `~1e-320`, unmarked where `eig`'s is marked; the refusal's words are
  given to the prelude's `eig`, known by its name and its scope, as
  `mod`'s are; `rhoe` of any matrix takes fixes14's clause for a sum past
  a double with it, over 2^(m+n). The golden replays in 5 s under the
  sanitizers. 29 lines landed against about 31: the prelude 19 and its
  comment 6, 1 binding the staircases, and 3 in `reference.hpp`. 15,324
  lines in all, after `eig`.
- `[done]` **The H-infinity norm, in the prelude**, the fourth step towards
  eigenvalues, which *The real eigenvalues* left for later: `hinf(A, B, C,
  D)`, the largest singular value of G(iw) = C(iwI - A)^-1 B + D over every
  frequency, A stable, and `dhinf(A, B, C, D)`, the same over the unit
  circle, each bisected and certified as `rho` is. Written in inkamath on
  `charpoly`, `hurwitz`, `schurcohn`, `eigv` and rho's stop and marks. The
  design, measured on a prototype:

      hinfc(p[j<=m], k) = 0
      hinfc(p[j<=m], k) | k < m = p[m-k]
      hinfz(f[k<=m], w)[i<=m-1, j<=m-1] = sum_(b=0)^(min(i, j)-1) (hinfc(f, i+j-1-b)*(b+w)*hinfc(f, b+w) - hinfc(f, b)*(i+j-1-b+w)*hinfc(f, i+j-1-b+w))
      hinfa(p[j<=m])[j<=m] = (-1)^j*p[j]
      hinfs(X) = eigv(charpoly(X)) - eigv(hinfa(charpoly(X)))
      hinfq(c[j<=m])[j<=(m+1)/2] = c[2*j-1]
      hinft(q) = hinfs(hinfz(q, 1)) <> hinfs(hinfz(q, 0))
      hinfh(A, B, C, K, R) = [A + B*R*K, B*R*B'; -C'*C - K'*R*K, -(A + B*R*K)']
      hinfr(D, g) = g*(D'*D)^0 - D'*D
      hinfx(A[j<=n, k<=n], B, C, D[j<=p, k<=m], e)[i<=m+1, j<=2*n+1] = (-1)^m*charpoly(hinfr(D, i*2^e))[m+1]*charpoly(hinfh(A, B, C, D'*C, hinfr(D, i*2^e)^-1))[j]
      hinfn(m)[i<=m+1, k<=m+1] = i^(k-1)
      hinfo(u, m)[i<=1, k<=m+1] = u^(k-1)
      hinfl(A, B, C, D[j<=p, k<=m], e, u) = hurwitz(charpoly(D'*D), u*2^e) == 0 or hinft(hinfq((hinfo(u, m)*hinfn(m)^-1*hinfx(A, B, C, D, e))'))
      hinfk(A[j<=n, k<=n])[r<=n*n, c<=n*n] = A[floor((r-1)/n)+1, floor((c-1)/n)+1]*(mod(r-1, n) == mod(c-1, n)) + (floor((r-1)/n) == floor((c-1)/n))*A[mod(r-1, n)+1, mod(c-1, n)+1]
      hinfv(M[j<=n, k<=n])[r<=n*n] = M[floor((r-1)/n)+1, mod(r-1, n)+1]
      hinfw(A[j<=n, k<=n], B, C, D[j<=p, k<=m]) = 2*sum_(j=1)^p sum_(k=1)^m D[j,k]^2 + 8*n*((hinfk(A)^-1*hinfv(B*B'))'*hinfk(A')^-1*hinfv(C'*C))[1,1]
      hinfu(A, B, C, D, e)_0 = [0; 1]
      hinfu(A, B, C, D, e)_m = hinfp(A, B, C, D, e, hinfu(A, B, C, D, e)_(m-1))
      hinfp(A, B, C, D, e, b) = b + (b[2] - b[1])/2*([1; 0] - (hinfl(A, B, C, D, e, (b[1] + b[2])/2) == 0)*[1; 1])
      hinfu(A, B, C, D, e)_m | m > 64 and rhos(hinfu(A, B, C, D, e)_(m-1)) = hinfu(A, B, C, D, e)_(m-1)
      hinfm(M)_0 = 0
      hinfm(M[j<=p, k<=q])_t = max(hinfm(M)_(t-1), max(M[floor((t-1)/q)+1, mod(t-1, q)+1], -M[floor((t-1)/q)+1, mod(t-1, q)+1]))
      hinfj(M[j<=p, k<=q]) = hinfm(M)_(p*q) + (M == 0*M)
      hinfd(x, f) = x*f
      hinfd(x, f) | x <> 0 and (abs(x*f) < 2^-1022 or abs(x*f) >= 2^1024) = x*f + 0*10^-1000
      hinfg(A, B, C, D) = 2*ceil(rhop(hinfw(A, B, C, D))/2)
      hinfe(A, B, C, D, f) = hinfd(rhod(rhoa(hinfu(A, B, C, D, hinfg(A, B, C, D))_256, hinfu(A, B, C, D, hinfg(A, B, C, D))_256)^(1/2), hinfg(A, B, C, D)/2), f)
      hinfe(A, B, C, D, f) | hinfw(A, B, C, D) == 0 = ~0
      hinfb(A, B, C, D, f) = hinfe(A, B/hinfj([B; D]), C, D/hinfj([B; D]), f*hinfj([B; D]))
      hinf(A[j<=n, k<=n], B[j<=n, k<=m], C[j<=p, k<=n], D[j<=p, k<=m] = 0*C*B) | hurwitz(charpoly(A/hinfj(A))) = hinfb(A/hinfj(A), B/hinfj(A), C/hinfj([C, D]), D/hinfj([C, D]), hinfj([C, D]))
      dhinf(A[j<=n, k<=n], B[j<=n, k<=m], C[j<=p, k<=n], D[j<=p, k<=m] = 0*C*B) | schurcohn(charpoly(A)) = hinf((A + A^0)^-1*(A - A^0), 2*(A + A^0)^-1*B, C*(A + A^0)^-1, D - C*(A + A^0)^-1*B)

  **The test.** Above D's largest singular value, gamma is a singular value
  of G(iw) exactly where iw is an eigenvalue of the Hamiltonian H = [A + B
  R^-1 D'C, B R^-1 B'; -C'(I + D R^-1 D')C, -(A + B R^-1 D'C)'], R = gamma^2
  I - D'D (Boyd, Balakrishnan and Kabamba, 1989), and the largest singular
  value is continuous and tends to D's as w grows; so gamma <= ||G|| exactly
  where H has an eigenvalue on the axis. The bisection is of g = gamma^2,
  which is all R reads, and a g at or below D's largest squared singular
  value is below the norm by `hurwitz(charpoly(D'D), g)`, which spares R's
  inverse where it has none. H being Hamiltonian, det(sI - H) = q(s^2), and
  an eigenvalue iw is a root -w^2 of q at or below 0. q's roots are not all
  real, so Descartes cannot count them, as the eig entry found; Hermite's
  forms can, through Bezoutians (Hermite and Hurwitz): the Bezoutian of q
  and q' has for signature the number of q's distinct real roots, and that
  of q and x q' those above 0 less those below, so their difference, twice
  the roots below 0 and one for a root at 0, is nonzero exactly where q has
  a root at or below 0. `hinfz(q, 1)` and `hinfz(q, 0)` are the two, one
  line by the formula B[i,j] = sum_b (f_(i+j-1-b) g_b - f_b g_(i+j-1-b))
  with g's coefficients read from f's. A Bezoutian is symmetric, so its
  signature is Descartes on its characteristic polynomial, `eigv` as eig
  counts, less the same with every other sign changed. At the norm, where it
  is attained, the peak's two imaginary eigenvalues meet, and a double root
  counts once: the test says at or below, so a norm a midpoint reaches is
  the bracket's lower end and the answer itself, as a dyadic eigenvalue is
  rho's.

  **One polynomial for every test.** det(R) det(sI - H) is the determinant
  of [sI - H0, U; V, gI - D'D], H0 = [A, 0; -C'C, -A'], U = [B; -C'D] and V
  = [D'C, B'], so a polynomial of degree m in g, m the inputs: it is
  interpolated once from its values at g = 2^e, 2 2^e, ..., (m + 1) 2^e,
  `hinfx`, each a characteristic polynomial of 2n x 2n, and each test reads
  it at its midpoint as a row of powers: before the normalisation below, 11
  s for a 6x6 of one input, where a characteristic polynomial of H at every
  test took 44.

  **The bracket.** [lo, hi) on g, lo <= ||G||^2 < hi, from [0, U), in units
  of U = 2^e above 2 tr(D'D) + 8n tr(PQ), e even, `hinfg`, P and Q the
  Gramians, A P + P A' = -BB' and A'Q + QA = -C'C, each solved as one n^2 x
  n^2 system, `hinfk` the Kronecker sum. It bounds the norm: ||G|| <= ||D||
  + 2 times the sum of the Hankel singular values (Enns; Glover, 1984), at
  most ||D||_F + 2 (n tr PQ)^(1/2), whose square is at most twice the sum of
  the two's. Each Hankel singular value being at most ||G|| (Nehari), U is
  within a factor 8 min(p, m) + 32 n^2 of ||G||^2, so a certified bisection
  stops by the relative rule within 65 halvings up to about 10x10, and never
  near the cap. And 2 tr(D'D) + 8n tr(PQ) is 0 exactly where G is, every
  Hankel singular value and D being 0, so 0 is answered at once, certified,
  as rho's nilpotent 0 is. The research's bound, `up(A, B, C)_40`, doubled
  until the test said above: rejected, as a mode near the axis needs as many
  doublings as its distance has bits, and a cap would leave a bracket that
  certifies nothing.

  **Normalised by the largest cells, exactly.** The tests' numbers are
  exact, so a power of two buys them range alone and costs digits: dividing
  [1 2; 3 4] by 8 puts an 8 in every denominator, and 10^200 by a power of
  two leaves its 5^200. The system is divided instead by its largest cells,
  rational: time by A's, A and B, which leaves the norm; the output by [C,
  D]'s; the input by [B; D]'s, the answer multiplied back by the two. A
  common factor goes entirely and cells whose largest is 1 do not move.
  Measured on an 8x8 of whole numbers and one input: certified so, marked by
  powers of two and off at the eighth digit; on 1/(s^2 + s/2 + 1) with C
  times 10^200: certified so, marked by powers of two and 6.1e200 for
  2.07e200; unnormalised, the 8x8 certified and the resonance refused with a
  NaN. The sum of the cells, rho's measure but exact, was tried: as good at
  the extremes, but it moves every cell of a textbook system, so a dyadic
  norm, s/(s + 1)'s 1, is no bracket's end in its units, and the 8x8 is
  marked where the largest cell certifies it.

  **Stop and marks**, rho's: 64 halvings at least, then while the bracket is
  wider than 2^-53 of its end nearer 0, up to 256; the answer is the root of
  lo, then scaled back by 2^(e/2), inexact: scaled first, g's double would
  leave a double's range at half the norm's exponent, 1e400 for a norm of
  1e200. Marked where a test was approximated past a thousand digits, and
  where the answer's double is not normal, inf, 0 or subnormal, `hinfd`, as
  eig's (C197, C199). The cap never ends a certified bisection (above).

  **What it costs.** A test at a midpoint of b bits reads q's coefficients,
  of about m b bits more than H's data, then two Bezoutians of their
  products and their characteristic polynomials, and `eigv` multiplies two
  coefficients to read a sign, doubling their digits, where comparing the
  signs would not: eig's horizon too, an item of its own. Certified: of
  small whole numbers, up to 8x8 with one input and 5x5 with two inputs and
  two outputs, not 6x6 as first measured, which the review found past a
  thousand digits in 4 random ones of 4; of cells with three digits 5x5,
  with six 4x4, with twelve 3x3. Past that the tests are approximated and
  the answer marked, refused since the review (below): a 4x4 of
  twelve-digit cells within 2e-16, a 5x5 within 3e-11, a 4x4 of 16-digit
  cells within 1.2e-7, against mpmath. 0.05 s for a 2x2, 0.6 s at 4x4, 7 s
  at 6x6 and 50 s at 8x8 with one input; 0.2 s at 3x3, 4 s at 5x5 and 17 s
  at 6x6 with two. The spec replays in 14 s under the sanitizers.

  **An inexact system** is read exactly: each inexact cell of hinf's and
  dhinf's arguments is the rational its double is, so an answer is certified
  for the data as stored, or marked, never silently wrong. In C++, about 15
  lines on the loop that C206's check needs anyway, a cell already
  approximated past a thousand digits kept as it is, its mark with it.
  Measured on cells k/7, each a full mantissa: certified up to 3x3 and some
  5x5, marked from 4x4, within 1e-9 at 5x5 and 5e-6 at 6x6; whole numbers
  made inexact read back exactly and are certified as they are. 1.1 s for a
  2x2 and 19 s at 3x3 under the sanitizers with one input, 57 s at 5x5,
  where rounded tests take 0.6, 0.9 and 2.6 s; in Release 0.1, 0.5 and 7 s.
  Rejected: rounded tests, as rho's matrix has, which misjudge as n grows,
  at the peak's double root and away from it: on random systems of small
  whole numbers made inexact, 20 with one input and 20 with two at each size
  up to 5x5, within 3e-11 up to 3x3, 3e-7 at 4x4 and 1e-2 at 5x5; at 6x6, of
  8 with two inputs the median is 1e-2, and of 12 with one, one is answered
  13.5 for 2.87, unmarked. And refusing an inexact cell, which costs 7 lines
  of C++ and every discretised plant whose pole is an `exp`. rho, abscissa
  and eig keep their rounded tests for now (next in line). A cell that is
  itself inf or NaN is refused as C206 refuses rho's, `hinf needs finite
  cells, not inf`, on the same loop over every argument. A complex cell is
  refused in max's words, as rho's, the normalisation reading every cell.

  **Unstable, refused.** A stable A is the norm's hypothesis: `hinf needs
  every eigenvalue of A left of the imaginary axis`, by `hurwitz`, words
  given in C++ as eig's guard's are. A pole on the axis makes the norm
  infinite, and is refused alike rather than answered inf: it may cancel,
  and telling which is a controllability test this item does not need. An
  unstable mode that cancels is refused too; the norm is of the system
  written. Sizes by the signature: D left out is `0*C*B`, p x m zeros, and a
  0 written for a D of two columns is a single value, refused.

  **Time scales far apart**: 1/(s + 1) + 1/(s + 10^-200) is answered 1e200,
  marked, its tests past a thousand digits, which the prototype's take to a
  NaN, refused in a comparison's words. The implementation may refuse in
  plain words instead where the NaN's cause costs more than about 5 lines, a
  departure it reports. Known limit: a norm small beside its normalised
  cells, a coupling of 2^-500 in a 2x2, is answered 0 marked, its tests past
  a thousand digits before its double is reached.

  **Discrete time**, `dhinf`, the supremum over |z| = 1, which is the norm
  the language's own models have (MANIFESTO.md, not continuous time), A in
  the unit circle by `schurcohn`: the bilinear map z = (1 + s)/(1 - s) takes
  the circle onto the axis and keeps the norm, so dhinf is hinf of ((A +
  I)^-1 (A - I), 2 (A + I)^-1 B, C (A + I)^-1, D - C (A + I)^-1 B), the
  map's sqrt(2) on B and on C made one 2 on B, exactly. A + I is invertible,
  -1 being outside the open disc, and the refusal, `dhinf needs every
  eigenvalue of A inside the unit circle`, comes first. A Schur-Cohn
  analogue on the circle, a count of the symplectic pencil's roots on |z| =
  1, was rejected: the map is one line and reuses every test.

  Decided besides:
  - **Two inputs and outputs, now.** The Hamiltonian serves any number at
    no line more; the interpolation's degree is m. One input alone was
    rejected as a restriction that saves nothing.
  - **Names.** `hinf`, as the paper writes H-infinity; `hinfnorm`, MATLAB's
    and python-control's, is longer for nothing, and `norm` a vector's.
    `dhinf`, as MATLAB's `dlqr` and `dlyap` are the discrete `lqr` and
    `lyap`; `hinfz` reads as a helper. Helpers prefixed `hinf`. No public
    bracket: one of gamma^2 is a column no paper writes, and smax has none.
  - **grad.** A bracket is a staircase: hinf and dhinf join rho's set,
    `grad cannot differentiate hinf yet`, in the interpreter and the
    compiler, a line. The norm's derivative at a simple peak, Re(u' dG v)
    at the peak's frequency, is a later item with rho's.
  - **Compiled**: neither, as rho: `hurwitz` stops at hurwitzb's factorial.
    A case of `test/cli.cmake` holds it and compiled grad's refusal, with
    the implementation.

  Rejected:
  - **Sturm's sequence**, the research's `hinf.ink`: q's remainders at 0
    and at -inf count its roots below 0, but polynomial division is some
    12 lines where the Bezoutians are 4, as the eig entry found for eig.
  - **Hermite's matrix of power sums**, which the eig entry proposed, from
    tr(H^(2k)): powers up to H^(4n-2), whose entries carry four times a
    Bezoutian's digits.
  - **Routh's array on the even polynomial**, with the singular case's
    auxiliary polynomial: a zero row at once, and further singular cases
    exactly where the test matters, at a double root.
  - **The bounded real lemma**, a Riccati equation solvable exactly where
    gamma is above the norm: no exact solver. **Sampling |G(iw)|**:
    certifies nothing.
  - **The quadratically convergent iteration** (Bruinsma and Steinbuch,
    1990): it needs the imaginary eigenvalues themselves and G's singular
    values between them, complex and inexact.
  - **Bisecting gamma**: R reads gamma^2 alone, and a norm whose square is
    dyadic, sqrt(2), would be no bracket's end.

  What moves: no golden and no header, as no test names either; the prelude
  gains 30 definitions, read at every start. README's sections 1 and 5 list
  `hinf` and `dhinf`, and section 2 shows them beside |T(iw)| and says what
  is certified, with the implementation.

  About 81 lines: the prelude 42, eight of them wrapped, some 12 of its
  comment, and 27 of C++: the two refusals' words 4, C206's check over every
  argument and the exact reading on its loop 20, the staircases 1 and its
  wrapping. 15,447 lines at f898d7e, by `wc -l include/inkamath/*.hpp
  src/*`. Past 122 the implementation stops and reports.

  Specified in `test/data/spec/hinf.ink`, 52 entries replayed by the spec
  suite, 43 failing by design, those passing being definitions and `digits`
  echoing themselves, |T(iw)| at the peak and a session's `hinf`: the norm
  exactly with sympy for one input and one output, an inexact cell's as its
  double's rational, from the critical points of |G(iw)|^2 in w^2 or of
  |G(e^(iw))|^2 in cos w, with mpmath at 60 digits for two, a sweep and a
  golden section, python-control's linfnorm agreeing within 1e-14 but at the
  extreme scales, where it fails; each bracket by bisection in Python's
  fractions with every test decided from that norm, and its double printed
  by a transcription of `Number::Shown`, apart from the interpreter. The
  prototype above, defined in a session, gives every answer but the
  refusals' words, the inf cell's, grad's two, a session's `hinf`, which
  need the C++ and the name to be the prelude's, the inexact entries, which
  it gives on the doubles written as rationals, and the time scales far
  apart.

  Built as specified but for the time scales far apart: the spec is the
  goldens `hinf.ink`, 13 entries, the norm's values, and `hinfdata.ink`, 40,
  what it reads and refuses and the discrete norm, two so that each replays
  well within a test's minute under the sanitizers, in 10 s and 9 s; the
  compiled refusals are a case of `cli.cmake`; README's sections 1 and 5
  list the names, and section 2 shows `hinf` and `dhinf` and says what is
  certified and how an inexact cell is read. Departures: 1/(s + 1) + 1/(s + 10^-200) is refused,
  `hinf needs tests within a double's range`, rather than answered 1e200:
  its tests pass a thousand digits holding 10^-400 beside 1, which no double
  holds, so none of them is decided, and what the prototype took to a NaN
  is refused in words. `hinfy`, a definition more, divides the interpolated
  polynomials by a power of two before the tests, and refuses them where a
  cell is inf or below 2^-1022 of the largest: undivided, 10^100 apart was
  a NaN too, its Bezoutians past a double, and from 10^160 an approximated
  bracket met the exact polynomials past it; divided, it is answered
  1e100, marked, up to some 10^152 apart, and refused from 10^154, where an
  answer would be marked but off by a factor, 2.4 at 10^170. The exact
  reading and C206's check are one hook, `checked`, which evaluation and grad
  call on each argument of a staircase, where the check had run on a single
  argument alone; the refusals' words are a table in `Unapplied` with
  eig's. 80 lines landed against about 81: the prelude 46 and its comment
  10, and 24 of C++. 15,527 lines in all, after `hinf`.

  With the review's ruling: a test approximated past a thousand digits is
  refused, `hinf needs tests within a thousand digits`, rather than marked,
  as a marked answer was off by any factor: a damping of 10^-100 answered
  2.47e100 for 1e100, a 4x4 of `dhinf` of small rationals 2608 for 1077, a
  3x3 whose states are scaled 10^10 apart in doubles 2 for 0.636, and cells
  of 200 digits 10^10 apart 0 for 1e9. `hinfi(x)`, a definition more, is
  whether x is exact: x == x is a double where x was approximated, and
  2^-1100 cannot move it. hinfy, hinfg and hinfp ask it of the polynomials,
  the bound and each test, in words that say no guard of a double, as
  hinfi's answer is never wrong; hinfy's guard below 2^-1022 goes, the
  polynomials past a thousand digits refused before it. hinfy still divides
  them, without which an approximated test is a NaN before it is refused.
  So time scales are certified up to some 10^50 apart and refused from
  10^60, a mode B cannot reach up to 10^80 slower; full doubles up to 2x2
  and 3x3 of one scale, cells k/7 (10 of 10), where 3x3 of several scales
  are refused in 6 of 7 and 4x4 of k/7 in 7 of 8; and a 3x3 whose states
  are scaled 10^10 apart, 10^4 in doubles, but not 10^15 or 10^5. The
  known limit above, a coupling of 2^-500, is certified now. Under
  `dhinf` the refusal names hinf: naming dhinf would need the caller in
  `Unapplied`, more than 2 lines. 8 lines more and 1 for C208: 89 lines,
  15,536 in all.
- **Doubles read exactly by rho, abscissa and eig**, as hinf reads them:
  each inexact cell the rational its double is, an answer certified for the
  data as stored or marked, where rounded tests misjudge unmarked. To weigh
  against the inertia count below, by its cost in time under the
  sanitizers.
- **An inertia count for inexact symmetric matrices**: the negative pivots
  of LDL' of A - xI count the eigenvalues below x, backward stable, where the
  rounded Descartes count of `eig` loses a k-fold eigenvalue to about 16/k
  digits and Hermite's test refuses symmetric matrices but for rounding.
  Rejected for `eig` only for an exact dyadic midpoint that zeroes a pivot,
  which an inexact matrix does not meet.

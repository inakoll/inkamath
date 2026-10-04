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
| C68 `[fixed]` | **Exactness ran out without a word.** Past a thousand digits a number is approximated, by design, and phase 13 made `~` say only that the digits shown are not the whole value, whatever the kind, leaving `frac` to tell the kinds apart. That leaves nothing to tell a user who does not ask that an answer from exact inputs is now a double: an epidemic model is computed in doubles from its eighth step, and an exact third looks the same as a value whose exactness ran out. An inexact number now carries whether it was approximated past the bound, in the half of its layout an inexact number did not use, so a `Number` is no larger; what is computed from it carries it on, and an answer that has it ends in `# approximated past a thousand digits`, a comment, so that it still reads back as what it says. What was inexact by nature, or made so by `~`, is not marked: it never was exact. A comparison is not either: it is a truth, exactly. `frac` says why it refuses. Specified in `bignum.ink` first; five goldens that crossed the bound moved with it, `!(10^20)`, `!1e400`, `2^2147483648`, `0.5^3000000000` and `(0-2)^(2^70+1)`, each exact until it passed the thousand digits. |
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
| C83 `[open]` | **A model's input is compiled as a single value, whatever it is.** The step takes one double for each input and the compiler reads the input as 1x1, so with `mm(x_n) = { y_n = [1 2]*x_n }` the header computes a 1x2 term where the interpreter, given `x_n = [n; 1]`, answers a single value. `--check` of that instance crashed, reading a second cell of each term the interpreter gave one of; it now refuses the instance by name, "v.x_(0) has 2 cells, where the compiled step takes a single value", and `check_matrix_input` in `test/cli.cmake` holds it. Found specifying a model's history of its inputs (next in line). What stays open is the header: an input wants a shape, taken as an array as a matrix parameter is, or a refusal by name. |
| C84 `[open]` | **`--check` compares nothing of an instance a model writes unnamed.** With `bare(u_n) = { c_n = u_(n-1) }` and `wrap(x_n) = { c_n = bare(u_n = x_n).c_n }` checked as `nest = wrap(x_n = n)`, the step keeps the instance as `c_bare`, and the check asks the interpreter for `nest.c_bare.c_0`, which it cannot name, "a term has no names", so every term of `c_bare.c` and `c_bare.u` is skipped and each line reads `within 0`. No checked instance writes one. Found reviewing the specification of a model's history of its inputs (next in line), whose check reports a term the interpreter cannot give instead of skipping it, which would turn each such line red. The check wants to ask for such a term through the definition that writes the instance, or to say by name that it cannot. It now says so, `veiled.c_bare.c: not asked, as the interpreter cannot name it`, where each line read `within 0`; asking stays open. |
| C85 `[fixed]` | **`tex` sets a base term apart from the guarded clauses written before it.** Clauses are tried in the order written (C45): after `y_n \| n < 0 = 0`, `y_(-1) = 3` and `y_n = n`, `y_(-1)` is 0, but `tex ?y` sets `y_{-1} = 3` on a line of its own above the cases, as if it held. Found reviewing the specification of a model's history of its inputs (next in line), whose clauses on an input may come in either order. `tex` wants the clauses in the order they are tried, or the base term left out where an earlier clause covers it. Now a term written after a guard is a case in its place, `3 & \text{if } n = -1`, and one written before every guard keeps its line, which holds; the C85 entries at the end of `test/data/tex.ink` hold it. |
| C86 `[open]` | **A clause whose value is a constant that fails refuses the whole model when compiled.** The compiler folds a constant value exactly, and a failure there is taken as the model's: in a model `gd(a = 2)` defining `h(x) = 1`, `h(x) \| x <= 0 = 1/0` and `y_n = h(a + n)`, `--compile` says "cannot compile y: division by zero" where the interpreter answers 1 for every term and never takes the clause, and `[1 2]*[1 2]` there says a product's sizes. `log`'s `log(x) \| x <= 0 = 1/0` is one, so a model calling `log` is refused in those words instead of "a sum or a product with no upper bound", which its series earns. Found specifying a function applied to each cell, `f.(x)`, which sizes inferred replaced (next in line). A clause written to refuse wants compiling as what a step does where the interpreter refuses, NaN as for a limit that does not converge, or a refusal naming the function and its clause. |

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

- **Sizes inferred in a definition by cells.** A network applies every
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
  gives nothing. A single value is read as a 1x1 matrix is, so `rl(5)` is 5.
  Every read of an index agrees, or it is refused naming two that do not,
  `a[i,j] and b[i,j] give j different sizes, 2 and 3`, or one, `z[i,i] gives
  i different sizes, 1 and 3`. The first read winning was rejected: of
  `a[i,j] + b[i,j]` it would cut a longer `b` short in silence, read past a
  shorter one, and answer by the order the operands were written; the
  smallest winning, as a zip does, is the same silence. A bound written is
  the bound and is not checked against the reads, so `top(z)[i<=2, j] =
  z[i,j]` takes two rows of any `z`. That `matrices.ink` says a size is
  written, not guessed from the cells a clause happens to give, stands: a
  read is not a guess, the size being the matrix's that every cell reads.

  The size is the definition's, as now: the matrix written whole and each
  clause for all cells, by its bounds or its reads, agree, else "the clauses
  of V give it different sizes"; a clause that gives an index none takes the
  others', so a causal mask, `cm(s)[i,j] | j > i = 0`, needs `cm(s)[i,j] =
  s[i,j]` beside it and nothing more. Where nothing gives one, it is refused
  when read, as a definition of one-cell clauses is today: `M has no size,
  as nothing reads a matrix at r alone; write it as M[r<=rows, c<=cols]`,
  the bounds written kept. So `M[r,c] = r + c`, refused where written in
  `matrices.ink`, is refused where read: a size may come from a clause
  written after, and refusing where written would depend on the order the
  clauses come in. A clause for one cell gives no size, as now, and one
  outside the size inferred says so as reading it would. A term's size is
  read at its index from the clauses at that index, as now, so a base term
  by cells, `q_0[i] = 1`, has none unless it reads one, and a term may read
  the one before, `cn_n[i,j] = cn_(n-1)[i,j] + j`.

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

  Specified in `test/data/spec/sizes.ink`, 116 of its 159 entries failing,
  those passing being definitions echoing themselves and four answers that
  stay: `e^A`, `[1 2] < 3`, a comparison of a shape no call was given, and
  `mod` by a single value. A ReLU network and a softmax classifier are each
  trained one step by `grad` through functions of cells with no size written,
  held to the gradient written by hand, the first exactly, with the values
  the dot's specification had. The compiled half is `test/compile/sized.ink`,
  `logistic.ink`'s regression with a sigmoid by cells and no bound, whose
  header is to be the one its bounds written give, byte for byte; wired with
  the implementation.

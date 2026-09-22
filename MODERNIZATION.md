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
| C20 `[fixed]` | **The evaluation budget covers reference lookups and nothing else.** Both the recursive-descent parser and the AST fold are unbounded C++ recursion: `(` ×8000 segfaults while parsing (4000 is fine), and a flat `1+1+…` of 50000 terms segfaults while folding. `README.md` §6 claimed "a runaway recursion is reported rather than crashing the process" and C1 read as though the whole class was closed; both were true only of recursion through a name. Fixed with a limit on the token count of one line, which bounds every recursion a line can provoke — the parser's, the evaluator's and the destructor's — since the tree has at most one node per token. 1000, against a measured overflow at about 2000 nested parentheses under the sanitizer and about 8000 without it. A crude bound, but one check covers the class, where a parser depth limit would leave the flat case building a tree too deep to destroy. |
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
| D15 `[fixed]` | **`CLAUDE.md` promises a list that does not exist.** §3 says some recorded outputs are deliberately wrong and "are listed in `MODERNIZATION.md`". The only list there was phase 0's, naming C5 and C6; both are fixed and both goldens have moved. `references.ink:98` (C26) was the first entry a restored list would have needed, and C26 is fixed, so the list would be empty. §3 now asks for the comment on the entry itself plus a register entry, which is what the goldens already do and what does not rot. |
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

Found by sitting at the prompt and writing what a student would write:
factorials, Fibonacci, Newton's method for a square root, a sum of a series,
compound interest, binomial coefficients, the roots of a quadratic. Most of it
works, and two of the three defects below came out of the quadratic.

| | |
|---|---|
| C54 `[fixed]` | **`lim` did not go through clause dispatch, so guarded clauses did not exist for it.** `Converge` called the general clause's expression directly, and `General()` is an unguarded-only predicate, so a limit and the terms it claims to be the limit of were two different sequences: a definition whose guarded clause halves each term had its limit computed from the unguarded clause that multiplies by ten, and a definition whose *only* general clause was guarded was told it had none. Every term now goes through `EvalImp`, the seed included, and a guarded general clause counts as one. What this cannot fix, and what cost me a golden: a sequence that is flat for five terms and then jumps is *correctly* called converged at the flat value, because no finite step test can see the jump coming. |
| C53 `[fixed]` | **A guard that did not hold left its index bound.** `Selects` wrote the index into the callee's frame before evaluating the guard and never removed it, so a clause that did not answer still shadowed a global (`rg_0 = nn` read `0`, not the global `7`), clobbered an argument the call had just bound, poisoned the guards of the clauses after it, and could give a name a value it never had anywhere. The index is bound on trial now -- a small RAII on `ReferenceStack` that restores the slot unless the clause is the one that answers. |
| C52 `[fixed]` | **A printed matrix read back as a different value.** `1-[1 2;3 4]` printed `0 -1 / -2 -3`, and typing that back gave `-1 / -5`, because a space before a signed element does not separate: `0 -1` reads as `0-1`. `README.md` section 2's own worked example was not re-enterable, and neither was any matrix with a negative cell. Commas alone would not have been enough -- the prompt reads one line and a matrix prints on several -- so the two halves are: a matrix prints as **the literal that would produce it**, columns right-aligned so the grid survives, and the prompt **continues a line whose brackets are open**, asking `..` for the rest. What is printed can now be pasted back over the lines it printed on. A 1x1 still prints as its bare value, which is what every scalar answer and every diagnostic quoting one is. Every recorded matrix moved, in this commit; the `matrix` unit tests and `README.md` with them. |
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
| C41 `[kept]` | **A block that does not fill its band continues with its own last cell.** `[a, [3 4]]`, with `a` 2x2, gives `1 2 3 4 / 3 4 4 4`; `[[1 2 3], a]` fills a whole row with `3 3 3`. The code says so in as many words -- *"extend the previous (up and left) evaluated cell result"* -- so it is deliberate, and read charitably the rule is **a block continues with its last value**, which is an ellipsis: `[a, 0]` pads the band with zeros and `[a, 1]` with ones, and that is a notation a matrix wants. The author does not remember the use case and suspects exactly this: an attempt at the `...` of matrix notation. Where it stops being statable is the multi-cell block, where continuing `[3 4]` with `4 4` is a corner repeated rather than a continuation anybody wrote. It is the third answer to one question -- a literal's short row pads with zeros (README.md section 2), a single value stretches, a short block repeats a corner -- and only the first two can be said out loud. **Decision: kept, and recorded as it is** in `matrices.ink` with the entry saying so. Not turned into a diagnostic while it may still be the residue of an idea; when the idea is found or ruled out, this goes and an explicit notation for the continuation replaces it, rather than a fourth fill rule. |
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
each measured rather than assumed.

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
is part of the number.

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

**Slices.** `a[1]` as a whole row, which would make a reduction natural rather
than a recurrence over cells, and would give chained indexing a reason to exist
-- today `a[1,2][1,1]` is a no-op precisely because every index yields a 1x1.

If they were mine to order: the conditional, because it is the one missing
primitive rather than a convenience; then the number systems, because the seam
is already open and the experiment above took an afternoon; then the tolerance,
and a prelude behind it.

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

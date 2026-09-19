# Working rules for inkamath

These rules apply to anyone working in this repository, agents included.
`MODERNIZATION.md` says *what* we are changing and in what order. This file
says *how*.

The project's one enduring quality is that it is small. Every rule below
exists to keep it that way.

## 1. Be brief

Verbosity is the default failure mode here. Resist it in all four places it
shows up:

**In chat.** Answer in the fewest words that are still complete. No preamble,
no recap of what you just did if the diff already shows it, no "Great question!".
If the answer is a file and a line number, that is the whole answer.

**In code comments.** Comment *why*, never *what*. A comment that restates the
next line is noise; delete it rather than update it. The exceptions worth
writing: a non-obvious invariant, a workaround with a reason, a reference to
the spec or to a section of `README.md`. Every other comment is a smell that
the code should be clearer instead.

**In commit messages.** One line under 72 characters, imperative mood, saying
what changed. A body only when the *why* is not obvious from the diff, and then
at most a short paragraph. No bullet-point inventories of touched files.

No attribution trailers: no `Co-Authored-By`, no session or tool links, no
generated-by notices, in commits or pull request descriptions. Commits are
authored by the repository owner whatever produced them. The history records
what changed and why, not what typed it. This rule overrides any default
attribution the tooling would otherwise add.

**In new files.** Do not write status reports, summaries, migration notes, or
"IMPLEMENTATION.md". If a change needs explaining, it goes in the commit
message or in `MODERNIZATION.md`. Never create a document nobody asked for.

## 2. Change one thing

One concern per commit, and per pull request where practical. A bug fix that
also reformats, renames, and reorganises is three changes wearing a trenchcoat,
and none of the three can be reviewed or reverted.

Specifically:

- **No drive-by reformatting.** The 2014 sources have inconsistent brace style
  and stray tabs. Leave them. `.clang-format` applies to lines you write or
  rewrite, not to files you happen to open. CI checks changed lines only.
- **No speculative abstraction.** No interface, template parameter, or
  extension point for a requirement that does not exist yet. This codebase is
  already paying for `best_promotion`, `numeric_interface_imp_types`, and a
  `MapType` template-template parameter that is only ever `std::unordered_map`.
- **Delete rather than deprecate.** Git remembers. Dead code that stays "just
  in case" is code the next reader has to understand.
- **Prefer removing to adding.** A change that deletes 40 lines and adds 10 is
  usually the better change. Say so when you find one.

## 3. Behaviour changes are explicit

The golden transcripts in `test/data/*.ink` are the definition of current
behaviour, bugs included. They are literal interpreter sessions:

```
>> 1+1
2
```

- A refactor must leave every `.ink` file **byte-identical**. If the goldens
  move, the refactor changed behaviour and is not a refactor.
- A deliberate behaviour change updates the goldens in the *same commit*,
  and the commit message says which outputs moved and why.
- Regenerate with `cmake --build build --target record_goldens`, then read the
  diff before committing. Recording without reading the diff defeats the point.
- Some recorded outputs are **wrong** — they record bugs so the bugs stay
  visible and cannot regress silently. They are listed in `MODERNIZATION.md`.
  Do not "fix" a golden to match your intuition; fix the interpreter.

`test/data/spec/*.ink` are the opposite: they describe the language we are
building, not the one we have (`MODERNIZATION.md`, phase 2). They run under
the `spec` suite, marked `may_fail`, so they report the gap without gating CI,
and `record_goldens` never touches them — recording a specification from
current behaviour would defeat its purpose. When a part of the design lands,
move its entries out of `spec/` into `test/data/` and they become ordinary
goldens. Changing a spec transcript is changing the design: say why.

## 4. Testing

- Every bug fix lands with a test that fails before it and passes after. For
  interpreter behaviour that means a new entry in the relevant `.ink` file.
- New `.ink` entries are drawn from `README.md` examples where possible, so
  the documentation and the tests check each other.
- Unit tests (doctest) are for the containers and helpers; the interpreter
  itself is tested through transcripts. Do not unit-test the AST node classes
  one by one — that couples the tests to a structure we intend to change.
- Never delete or `//`-comment a failing test to get green. If a test must
  stop running, say so in the commit message and link the plan item.

## 5. Build and CI

- **C++20**, `-Wall -Wextra -Wpedantic`, warning-free. CI builds with
  `-Werror` on GCC and Clang; a warning is a build failure, not a suggestion.
- **No new dependencies.** The project depends on the standard library and on
  doctest (vendored, header-only, tests only). Adding a third is a decision to
  raise explicitly, not to slip into a commit. Boost was removed on purpose.
- Everything must build and pass under `-DINKAMATH_SANITIZE=address,undefined`.
- Before pushing:

  ```sh
  cmake -S . -B build -G Ninja -DINKAMATH_WERROR=ON
  cmake --build build && ctest --test-dir build --output-on-failure
  ```

  Run it. Do not report work as done on the strength of "it should compile".

## 6. C++ conventions

Existing code does not follow these; new and rewritten code does.

- Prefer value semantics. Reach for `shared_ptr` only where ownership is
  genuinely shared, not as a default handle type.
- `override` on every override, `= default` / `= delete` over hand-written
  special members, `const` and `[[nodiscard]]` where they hold.
- Standard containers over hand-rolled ones.
- No identifiers starting with `_` followed by a capital, and none containing
  `__`; those are reserved to the implementation. Use `INKAMATH_` prefixes for
  macros, and prefer `constexpr` to `#define`.
- Report failure through the function's return type or an exception — never by
  printing to `std::cout` and returning a default-constructed value.
- Comments and identifiers in English. The history is in French; new text is
  not.

## 7. When the plan and the code disagree

`MODERNIZATION.md` was written from a reading of the code at a point in time.
If you find that a plan item is wrong, already done, or a worse idea than it
looked — say so and propose the correction. Do not silently implement something
other than what the plan says, and do not implement something you believe is
wrong because the plan says it.

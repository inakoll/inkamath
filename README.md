Inkamath
========

Inkamath is a small mathematical interpreter written in standard C++. It
evaluates complex arithmetic, matrices of expressions, user-defined functions,
and sequences defined by recurrence. It was written around 2014; see the
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
3.14159265 2.71828183

>> 1+2*3^3*2+1
110

>> a=[1 2;3 4]
a=[1 2;3 4]

>> [a, a; a, a]
1 2 1 2
3 4 3 4
1 2 1 2
3 4 3 4

>> exp(x)_0=1
exp(x)_0=1

>> exp(x)_n=exp(x)_(n-1)+x^n/!n
exp(x)_n=exp(x)_(n-1)+x^n/!n

>> lim exp(1)
2.71828183

>> cos(x)=(lim exp(i*x)+lim exp(-i*x))/2
cos(x)=(lim exp(i*x)+lim exp(-i*x))/2

>> cos(pi/3)
0.5
```

Building
--------

Requires a C++20 compiler and CMake 3.20 or newer. No external dependencies:
doctest is vendored under `third_party/` and is used by the tests only.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/inkamath            # the REPL; 'q' or end of input quits
ctest --test-dir build --output-on-failure
```

Useful options: `-DINKAMATH_WERROR=ON` (warnings are errors, as in CI) and
`-DINKAMATH_SANITIZE=address,undefined`.

Interpreter behaviour is pinned by golden transcripts in `test/data/*.ink`,
which are literal sessions — every example in this file is one of them, so the
documentation and the tests check each other. Regenerate with
`cmake --build build --target record_goldens` and read the diff: it is the
record of what your change did.

`MODERNIZATION.md` tracks the work in progress and the known defects.
`CLAUDE.md` has the working rules.

Language
--------

### 1. Numbers and operators

Numbers are complex; `i` is the imaginary unit. `e` and `pi` are the only
other built-ins, and there are no built-in functions.

| | |
|---|---|
| `+expr` `-expr` | unary plus and minus |
| `!expr` | factorial, written as a prefix |
| `expr+expr` `expr-expr` | addition, subtraction |
| `expr*expr` `expr/expr` | multiplication, division |
| `expr^expr` | power |
| `name = expr` | definition (section 3) |
| `lim name` | the limit of a sequence (section 4) |
| `?name` | print a definition back (section 5) |

`^` associates to the right, so `2^3^2` is `512`. `#` starts a comment and
runs to the end of the line.

```
>> !5
120

>> 2+3*i
2+i*3

>> (1+i)*(1-i)
2
```

### 2. Matrices

`[a b; c d]` is a matrix literal: `,` or a space separates columns, `;`
separates rows. A short row is padded with zeros.

```
>> [1 2;3 4]
1 2
3 4

>> [1 2 3;4 5 6]
1 2 3
4 5 6

>> [1; 2, 3]
1 0
2 3
```

The cells are expressions, and the literal expands to the size of what they
evaluate to. If `a` is the 2x2 matrix above, then `[a, a; a, a]` is 4x4:

```
>> [a, a; a, a]
1 2 1 2
3 4 3 4
1 2 1 2
3 4 3 4
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

A name has one definition. Defining it again replaces what was there.

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
9.53674316e-07
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
1e-10, and reports non-convergence rather than handing back the term it
stopped on:

```
>> lim s
5.82076609e-11

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
error: w did not converge within 100 terms (last term 1.63508193)
```

That is a mathematical problem, not a limitation of `lim`, and the language is
enough to solve it. What remains after `n` terms is `1/n - 1/(2n^2) +
1/(6n^3) - ...`, and a clause may add it:

```
>> y_n=w_n+1/n-1/(2*n^2)+1/(6*n^3)-1/(30*n^5)
y_n=w_n+1/n-1/(2*n^2)+1/(6*n^3)-1/(30*n^5)

>> y_20
1.64493407

>> pi^2/6
1.64493407
```

Twenty corrected terms give every digit that is printed. An acceleration is a
sequence like any other, so which one to use stays the user's decision — it
is the mathematics, and no built-in choice would be right for every series.

An index must be a whole number, and `lim` is a reserved word.

A term is evaluated once per *context* — which definition, which index, which
argument values — and the answer is remembered until a definition changes. It
makes no difference to what an expression means; it makes the difference
between a chain of terms and a tree of them for a recurrence whose general
clause names itself twice, as a pair of mutually recursive sequences does.

### 5. Printing a definition back

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
2 4
6 8
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

### 6. Diagnostics

An expression either produces a value or says why it cannot. Nothing evaluates
to zero by default.

```
>> undefined
error: undefined is not defined

>> 1+
error: unexpected end of input after '+'

>> [1 2
error: missing ']' after '2'
```

Evaluation is bounded: a runaway recursion is reported rather than crashing the
process.

# `fn` and `def`: inputs stay on the stack by default

Status: **implemented**, 2026-11-01. See `CHANGELOG.md` for the shipped description; this
document is the reasoning behind it.

A `fn`'s inputs stay on the stack for its body to work on, and the names in its signature are
documentation. A `def` binds them as locals instead.

```qd
fn double(x:i64 -- r:i64) {
	2 *
}

def area(w:f64 h:f64 -- a:f64) {
	w h *
}
```

This is step 5 of making Quadrate more concatenative, after option A's fallible calls, stack
variables, inferred effects (`fn { }`) and builtin combinators.

## Why

Quadrate always had both dialects: named locals, and words that work on the stack. Which one was
unmarked decided which one code was written in. Binding was the default and `stack fn` the
exception, and the corpus followed: about 1,600 functions bound their inputs against fewer than
200 that left them on the stack, and named locals outnumbered every stack word put together by
nearly twenty to one (R55).

Forth and Factor make the stack the default and names the thing you ask for. Factor is the exact
precedent: `:` defines a stack word, `::` the same word with its inputs bound as locals, and the
quotations follow with `[ ]` and `[| a b | ]`. `fn` and `def` are those two, spelled as words.

The earlier steps made the plain form practical to write. Stack variables let one combinator
work at every arity; `fn { }` made function values short; builtin combinators made `dip`,
`keep` and `bi` cheap.

## Meaning

- **`fn`.** The inputs are on the stack at entry, top last. Names in the signature are
  documentation, are not in scope, and are optional; naming some and not others is allowed. The
  body may bind a local of the same name with `->`.
- **`def`.** Each input is taken off the stack at entry and bound as a local; the body starts on
  an empty stack. Every input must be named. A `def` with no inputs is an error: there is nothing
  to bind, and it should be a `fn`.
- **A receiver** (`fn (v:Vec) push(...)`) is bound either way, as before.
- **Lambdas** follow the same rule: `fn (i64 -- i64) { 2 * }`, `def (x:i64 -- r:i64) { x 2 * }`.
  `fn { }` has no inputs to bind, and `def { }` is an error.
- **`pub` and `inline`** go before either keyword.
- **An input named in the body** of a `fn` is reported as such, with the hint to write `def`,
  rather than as an undefined identifier.

## The keyword

The first spelling was a modifier, `let fn`. `def` replaced it before release: a second
declaration keyword makes the choice the first word of the declaration rather than a prefix to
it, it is as short as `fn` and unlike it to the eye, and in Python, Ruby and Scala it already
means a function whose parameters are named. `bind`, `named`, `defn`, `fun`, `proc` and `func`
were considered; `fun` and `func` are too close to `fn`, and `proc` suggests no results.

A keyword this cheap does not push code toward the stack on its own: `def` costs nothing over
`fn`, as `::` costs one character over `:` in Factor. What keeps Factor code concatenative is the
library, the examples and the combinators, and that is where Quadrate's has to come from too. So
the switch came with a lint.

## The `needless-binding` lint

A `def` whose body starts by pushing all its inputs back, in the order they were declared, and
names none of them again is a `fn` with extra steps -- the inputs were already on the stack in
that order:

```
def sum(a:i64 b:i64 -- s:i64) { a b + }   // quadlint: needless-binding
fn sum(a:i64 b:i64 -- s:i64) { + }
```

`quadlint` reports it by default (`--no-needless-binding` turns it off, `//nolint:needless-binding`
silences one). The condition is exact rather than heuristic, so the advice is always a rewrite
that means the same thing:

- every input is pushed, by name, as the first statements of the body, in declaration order;
- each is then named exactly once in the whole body -- a second use, a `->` that rebinds it, or a
  `{name}` in an interpolated string counts;
- methods are skipped, since their receiver is bound either way.

When it was added, 85 functions in the standard library, the examples, `quadmcp`, qdos's programs
and quadrate-floppy matched it, and all were rewritten as it advises.

## Migration

The same source means one thing before the flip and another after, so a formatter pass cannot
tell old code from new. `quadfmt --migrate` is a one-off: every function and lambda that has an
input before its `--` becomes `def`, and `stack fn` becomes `fn`. It scans tokens instead of
parsing, because the old code no longer parses; it skips comments, strings, `import` blocks
(whose declarations have no body to bind in) and function types (`f:fn(i64 -- i64)`,
`[]fn(...)`). It writes a file even when the result does not parse, so the compile-error tests
migrate too.

## What does not change

- **Code generation and the interpreter.** Both already had the two behaviours; only which one
  is the default moved.
- **Inferred effects and combinators.** A `fn { }` worked on the stack already, and a combinator
  inlines a body only when it binds nothing.

## Not in scope

- Rewriting the remaining `def`s, where names may still earn their place. That is a per-function
  judgement, best made module by module, as `hof` was.

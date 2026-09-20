# TODO

What is open. Finished work is not kept here: `CHANGELOG.md` has the prose and git has the
history. The `R<n>` identifiers come from a full review of the language surface on 2026-09-17
and are stable, so they can be referred to while working through them.

## Direction: two dialects, spelled explicitly (decided 2026-09-19)

The goal was the concatenative property: **juxtaposition is composition**, so any contiguous run
of words can be lifted into a named word and replaced by its name without changing meaning. What
was decided instead is narrower and is now shipped: **the difference must not be silent.** Binding
is the unmarked default, `stack fn` marks the function whose inputs stay on the stack, and the
choice per function is a readability judgement rather than a property of a colon.

```qd
fn sq(n:i64 -- r:i64) { n n * }         // default: each input bound as a local
stack fn sq(i64 -- r:i64) { dup * }     // inputs stay on the stack; names, if any, are docs
6 fn (i64 -- r:i64) { dup * } call      // inline quotation applied: 36
```

So the language carries both dialects, and nothing forces the corpus to move. Which code is
*better* stack-direct was answered by porting `stdlib/hof` both ways (R47, closed): **a body
whose parameters are used once, in the order the caller pushed them, is ceremony** -- `apply`
became the single word `call` -- while a body that uses a value twice or takes two functions
becomes a puzzle: `bi` stack-direct is `pick rot call rot rot call`, which is correct, passes
its tests, and tells a reader nothing. 48 of 452 functions that take parameters are `stack fn`.

## Language design

- [ ] **Sum types / tagged unions.** `enum` gives bare ints and `struct` gives records, but there
      is no "one of these". That absence is why `Ok`/`Err` are conflated with `true`/`false` and
      why `null` is `0` — four spellings each of 0 and 1 — and a `Result<T, E>`-shaped variant
      type would let much of that be deleted rather than maintained.
      **Both arguments filed for it have since been withdrawn, each by an implementation that
      turned out not to need the feature.** The first was `stdlib/json`, which had no `parse` at
      all: it has one as of 2026-09-19. A JSON value is a tagged union in the abstract, but a
      `struct Value { kind:i64 … }` with a payload field per kind is what every C and Go parser
      writes, it costs 80 bytes a node, and the `kind` tag is checked in exactly the places a
      `match` would have been. The `(value, found)` pair *was* the real cost, and it was paid off
      without the feature: the accessors are fallible (`as_int!`, `get!`) and report `ErrType`,
      `ErrKey`, `ErrIndex`, so nothing returns a bare flag beside its value.
      The second was the error channel — R24, closed 2026-09-20, see the `error` entry in
      `CHANGELOG.md` — which `stdlib/error` answered the same way and in forty lines of Quadrate:
      the control flow already exists and is enforced, and what was missing was a payload type,
      which a refcounted struct already is.
      So what is left is the spelling of 0 and 1, not a blocked use case.
      Deliberately **not** adding: interfaces/traits (generics see 5 uses, not under strain),
      slices/iterators (`len`/`nth`/`append`/`set` over `ptr` arrays is the right level),
      `comptime`/const-generics. Labeled `break` will bite eventually with nested `for`, but not yet.
      **Open question**: twice the concrete case dissolved on contact with a real implementation,
      and both times what replaced it was a struct with a tag field. What would a third candidate
      have to look like to survive that — and if none presents itself, is this an item or a
      preference?

- [ ] **R13. `and`/`or` do not short-circuit.** Both operands are evaluated before either
      runs, so the guarded form `i xs len < xs i nth … and` reads `xs[i]` whether or not the
      bound check passed. There is no way to write a guard.
      **Half of this closed 2026-09-20.** The other complaint was that `and`/`or` were
      bitwise and used as logical, so `2 1 and` was `0`. They are the logical operators now
      and the bitwise forms are `bits::and`/`bits::or`/`bits::xor`/`bits::not`; see the
      `and`, `or` and `not` entry in `CHANGELOG.md`. That fixes the wrong-answer hazard and
      does nothing for this one, which is a property of the evaluation order rather than of
      the operators.
      **Reframed 2026-09-18, and it still holds: do not design this separately.**
      Short-circuiting *is* deferred evaluation, and the concatenative answer to deferred
      evaluation is a quotation — which already works (`6 fn (i64 -- r:i64) { dup * } call`
      evaluates to 36). Adding `land`/`lor` as a third pair of boolean primitives would
      spend the language's budget on a special case of the general mechanism, and would have
      to be unpicked later.
      **Nothing in the corpus needs it.** Searched 2026-09-20 for the shape this describes —
      a bounds or null guard combined with an index or dereference: zero sites, in stdlib,
      examples, tests and cmd alike. It is a trap that is set and has caught nobody, which is
      why the hazard is documented rather than worked around.
      **Open question**: what is the spelling — `[ … ] [ … ] and` over quotations, or
      combinators in `hof` beside `when`/`unless`? And given no call site wants it, is the
      answer to wait for one?

- [ ] **R21. `>>field` is specced as returning an updated struct; it mutates in place.** Structs are
      reference values: `P { x = 1 } -> a  a -> b  b 99 >>x drop` leaves `a <<x` as 99, and passing
      a struct to a function lets that function mutate the caller's value. The spec's "sets field,
      pushes modified struct back (for chaining)" and the idiom `p 42 >>x -> p` both read as a
      functional update.
      **Inverted 2026-09-18: the spec is the one that is right.** The item first read as
      "almost certainly documentation only". "Sets field, pushes
      modified struct back (for chaining)" is the composable reading, and chaining is what makes
      `>>field` a word like any other rather than a statement; mutating in place and returning the
      same reference is what breaks it. So the implementation is the odd one out, not the wording.
      The blast radius was 12 `>>` sites when this was written; it is **38 in `stdlib` +
      `examples` and 125 across the whole corpus** as of 2026-09-20, most of the growth being
      `stdlib/json`, which builds its tree by assigning fields. Still small enough to change
      rather than document around, but no longer negligible -- and the ownership rules written
      into specification 11.2.2 since then are what a copying `>>field` would have to respect.
      **Open question**: does the functional update mean copying the struct — and if so, does that
      make structs value types on assignment, which is a far larger change than the call sites?

## Upstream

- [ ] **libu8t 1.4.0 does not lex a negative hex or binary literal.** `-0x10` comes back as
      the integer `-0` followed by the identifier `x10`, and `-0b101` as `-0` and `b101`.
      The scanner takes a leading `-` as part of the number (`scanner.c`, the `cp == U'-'`
      branch) but that branch duplicates only the decimal path -- the `0x`/`0b` prefix
      handling in the positive branch above it is missing. Found 2026-09-20 by
      `fuzz_formatter`. In an expression the stray identifier is undefined and `quadc`
      says so, badly (`Undefined identifier 'x10'`); in an enum value it was taken as the
      next variant and the formatter wrote the enum back split in two, which
      `parseEnumDeclaration` now rejects as a malformed literal. Both are papering over
      the lexer. The fix belongs in `github.com/klahr/libu8t`, followed by a revision bump
      in `subprojects/u8t.wrap`; the guard in `parseEnumDeclaration` and the wrong error
      message in expressions can both go once that lands.

## Interpreter tier (lib/interp)

- [ ] Not planned for this tier: structs, `defer`, `import`/`use`, anonymous functions. Imports in
      particular cannot mean anything on a device with no package resolution — qdos takes its
      scopes from `lib<name>.so` filenames — so they should keep refusing clearly.

## Deferred

- [ ] Package registry — searchable index instead of raw Git URLs.
- [ ] (Stretch) Inline asm — `asm("cli; hlt")` style. Today everything privileged lives in a `.S`
      file called via FFI, which works fine; this is quality-of-life for short sequences (port I/O,
      halt) so kernel code can stay in `.qd`.

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

- [ ] **Sum types / tagged unions** — the one addition worth arguing for, and no longer a longer
      horizon: an error channel that cannot be put on the stack costs this language more than it
      would an ALGOL one (see R24). `enum` gives bare ints and `struct` gives records, but there is
      no "one of these". That absence is *why* errors are out-of-band int codes plus a message, why
      `Ok`/`Err` are conflated with `true`/`false`, and why `null` is `0` (four spellings each of 0
      and 1). A `Result<T, E>`-shaped variant type would let most of the error-handling surface be
      deleted rather than maintained.
      The evidence used to be `stdlib/json`, which had no `parse` at all. **It has one now
      (2026-09-19), and writing it did not need sum types** — so that argument is withdrawn and
      this item stands on R24 alone. A JSON value is a tagged union in the abstract, but a
      `struct Value { kind:i64 … }` with a payload field per kind is what every C and Go parser
      writes, it costs 80 bytes a node, and the `kind` tag is checked in exactly the places a
      `match` would have been. The `(value, found)` pair *was* the real cost, and it was paid off
      without the feature: the accessors are fallible (`as_int!`, `get!`) and report `ErrType`,
      `ErrKey`, `ErrIndex`, so nothing returns a bare flag beside its value.
      Deliberately **not** adding: interfaces/traits (generics see 5 uses, not under strain),
      slices/iterators (`len`/`nth`/`append`/`set` over `ptr` arrays is the right level),
      `comptime`/const-generics. Labeled `break` will bite eventually with nested `for`, but not yet.
      **Open question**: with JSON out of the argument and `stdlib/error` now supplying the
      purpose-built error value (see R24), is anything left of the case for the feature?

- [ ] **R13. `and`/`or` are bitwise and are used throughout as logical.** There is no short-circuit
      operator; `lnot` exists but has no binary counterpart. Verified: `2 1 and` → `0`, so any
      operand not already 0/1 gives the wrong answer. Both sides always evaluate, so the guarded
      form `i xs len < xs i nth … and` is unsafe. Live in the corpus:
      `t 0.0 > best_t 0.0 < t best_t < or and if` (`examples/raytracer/raytracer.qd`) is correct
      only because every operand happens to be a comparison result.
      **Reframed 2026-09-18: do not design this separately.** Short-circuiting *is* deferred
      evaluation, and the concatenative answer to deferred evaluation is a quotation — which already
      works (`6 fn (i64 -- r:i64) { dup * } call` evaluates to 36). Adding `land`/`lor` as a third
      pair of boolean primitives would spend the language's budget on a special case of the general
      mechanism, and would have to be unpicked later. The correctness hazard is real and stands:
      `2 1 and` is `0`, and the guarded form `i xs len < xs i nth … and` evaluates both sides. Until
      the quotation-based form exists, document the hazard.
      **Open question**: what is the spelling — `[ … ] [ … ] and` over quotations, or combinators in
      `hof` beside `when`/`unless`?

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

- [ ] **R24. `err` is global state, not a value.** Set by `panic`, cleared on read, survives
      intervening non-fallible calls (all verified). It cannot be stored, returned, wrapped, or
      chained, so there is no way to build "failed to open config: no such file". It is also the
      main reason stdlib authors reach for `!` rather than propagating.
      A value that cannot be put on the stack cannot be composed, which is why this is filed as a
      blocker for the error surface generally rather than an ergonomic wish.
      **Designed 2026-09-19, not built.** Two designs were compared. *Error as a value handed
      out on request*: `panic` also builds an `Error` struct (code, message, `*Error` cause), the
      context holds a reference, and a reader pushes it. It is the cheaper one, but it does not
      answer the complaint -- the error stays global and you get a copy -- and it needs more than
      it looks: the runtime cannot build a struct whose layout belongs to a stdlib module, so it
      needs an opaque refcounted slot plus a `raise` primitive that takes a value, and clearing
      at every fallible call site stops being two inline stores and becomes a release.
      *Error on the stack*: the failure path of a fallible call pushes the `Error`, and the
      context slot disappears. This is the answer to the complaint as written -- "a value that
      cannot be put on the stack cannot be composed" -- and most of the machinery exists: the
      validator already models the failure arm as a different stack shape
      (`bareFallibleCallBefore` + `removeProducedValues`), and what to push on failure is decided
      in one place (`generateFallibleCallEpilogue`). Pushing `(Error, code)` keeps `switch`
      matching codes and leaves the error under it for the arm.
      **Measured 2026-09-19, and it reversed the conclusion.** The corpus has **394 failure arms
      after bare fallible calls; 330 ignore the error and 64 read it** (249 switch arms, 145
      if/else arms, plus 6 sites with no failure arm at all). The stack-passing design taxes the
      330 to improve the 64, which is the wrong way round for a language that charges for what
      you do not use.

      **Done instead 2026-09-19: `stdlib/error`.** The reader returns one value rather than two.
      It needed no compiler change at all -- `error::last` packs what `err` already returns into
      an `Error` struct, `wrap` composes context and keeps the code, `root` walks the cause chain
      -- so it is forty lines of Quadrate, no migration is forced, and the 64 sites that read
      `err` today can move when someone touches them. That answers the parts of this item that
      were actually blocking: an error can now be stored, returned, collected into an array, and
      given context by the frame that has it.

      **Still open**, and why this is not closed: the channel is still global. `err` empties on
      read, the state survives intervening non-fallible calls, and `error::last` has to run first
      in the failure arm. If that turns out to matter in practice, the stack-passing design is
      the fix and `Error` is already the type it would push, so nothing here is wasted. The
      evidence to watch for is whether anyone reaches for the module. Sum types do not appear to
      subsume any of this: the control flow already exists and is enforced, and what was missing
      was a payload type, which a refcounted struct already is.

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

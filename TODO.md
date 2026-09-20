# TODO

What is open. Finished work is not kept here: `CHANGELOG.md` has the prose and git has the
history. The `R<n>` identifiers come from a full review of the language surface on 2026-09-17
and are stable, so they can be referred to while working through them.

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

- [ ] **No way to copy a struct.** A struct is a mutable reference: `a -> b` binds a second
      name to the same struct, and `b 42 >>x drop` is visible through `a`. That is the design
      (R21, settled 2026-09-20 — the specification wording was the bug, not the behaviour),
      but it leaves no way to ask for an independent one. The only copy available is
      rebuilding it by hand, `P { x = a <<x  y = a <<y }`, which is verbose and has to be
      revisited every time a field is added.
      Shallow is the tractable meaning: a new struct of the same type, fields copied, the
      refcounted ones retained. `sizeof<P>` already reports the layout, so the machinery is
      there. `clone` is the better name than `copy` — a builtin shadows user functions of
      the same name, and `copy` is a word a program is likely to want for itself.

      ```qd
      a clone -> b
      b 42 >>x drop        // a untouched
      ```

      **Open question**: what happens to the fields that are themselves references — a
      `str`, an array, another struct? Retaining them is the cheap answer and the one that
      matches "shallow", but it means `clone` gives independence at one level only, and the
      name should not suggest more than it does.

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

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

- [ ] **A count beside an array, in the `strings` FFI.** `sort::*` takes `[]T` and nothing
      else — the array carries its length. `strings::sort`, `sort_desc`, `join` and `column`
      still take `arr:[]str count:i64`, and `split`, `lines`, `words` and `split_n` still
      *return* a count beside the array they built. The two spellings now sit side by side for
      the same job: `strs sort::strings` against `strs count strings::sort`.
      These are FFI, so the count comes off the stack in C rather than out of the signature;
      dropping it means reading `qd_array_length` in `stdlib/strings/src/strings.c` and
      revisiting what the four producers return. Left out of the sort conversion on purpose —
      it is a separate change to a separate module, and C rather than Quadrate.
      Found 2026-09-21 on finishing the sort conversion.

## Interpreter tier (lib/interp)

- [ ] Not planned for this tier: structs, `defer`, `import`/`use`, anonymous functions. Imports in
      particular cannot mean anything on a device with no package resolution — qdos takes its
      scopes from `lib<name>.so` filenames — so they should keep refusing clearly.

      Three of the four do. `struct` and `use` are refused where they are written ("nothing
      declared here can be interpreted yet"), and a `defer` body is refused when the function
      holding it is called ("this construct cannot be interpreted yet"). An anonymous function
      is not: `fn (x:i64 -- r:i64) { x 1 + } -> g` reports `Expected ')' after receiver type in
      method declaration`, which is the parser reading `fn (` as the start of a method rather
      than a refusal of anything. It is the one of the four that does not say what is wrong.

## Deferred

- [ ] Package registry — searchable index instead of raw Git URLs.
- [ ] (Stretch) Inline asm — `asm("cli; hlt")` style. Today everything privileged lives in a `.S`
      file called via FFI, which works fine; this is quality-of-life for short sequences (port I/O,
      halt) so kernel code can stay in `.qd`.

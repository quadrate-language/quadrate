# TODO

## Open

### Direction: two dialects, spelled explicitly (decided 2026-09-19)

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

So the language carries both dialects, and nothing forces the corpus to move. What remains open is
which code is *better* written stack-direct — R47, now answerable one function at a time with no
compiler change behind it. `stdlib/hof` is the sharpest case: it carries a Factor-shaped combinator
vocabulary (`bi`, `tri`, `keep`, `dip`, `both`, `bi_star`, `when`, `unless`, `times`, `fold`) while
every one of its own signatures is named.

Two items were deleted rather than answered, and are recoverable from git: *"Decide the fate of the
live shufflers"* and **R18** (multi-return selection "has no syntax" — `drop`/`nip`/`swap` are that
syntax). R13 and R21 were briefly justified by the concatenative property; they now stand on their
own, and both still do.

### Language design / scope

From a feature-scope review (2026-08-13). Counts are whole-corpus greps over `lib/` + `examples/`
(82 `.qd` files, 18,891 lines) and **exclude the doom port**, whose sources aren't in the working
tree — `packed` and `enum` score much higher there.

Overall read: the core is the right size — comparable to Go. The problem isn't count, it's
redundancy. Subtractive work, with one exception: the shufflers are vocabulary rather than
redundancy, and the item proposing to cut them was deleted. Re-counted 2026-09-18:
**21 keywords, 86 `BUILTIN_INSTRUCTIONS` entries** (17 of them `__` freestanding internals), **66
documented in `reference.def`** — the review's 87/69 predate the `read` and `ctx` removals.

- [ ] **Make `pick` fixed-depth; drop `roll`.** *(Rewritten 2026-09-18 — was "drop `pick`/`roll`
      from the user-facing surface".)* The technical objection stands and is the reason to change
      them rather than keep them as they are: both take a **runtime** index, which defeats static
      stack tracking, and codegen already refuses them in compile-time-stack functions
      (`generator_nodes_instructions.cc`, "not supported in compile-time-stack functions"). But
      deleting them outright was the wrong conclusion now that `stack fn` makes stack-direct code a
      first-class form — Factor keeps `pick` as a fixed-depth word, `( x y z -- x y z x )`, and has
      no `roll` at all. So: respell `pick` as third-item copy, which is statically checkable and
      needs no index, and drop
      `roll` from the user-facing surface while keeping it as an internal op (method receiver
      rotation). Blast radius is nil either way — `pick` is at **0** corpus uses and `roll` at 2.
- [ ] **Sum types / tagged unions** — the one addition worth arguing for, and no longer a longer
      horizon: an error channel that cannot be put on the stack costs this language more than it
      would an ALGOL one (see R24). `enum` gives bare ints and `struct` gives records, but there is
      no "one of these". That absence is *why* errors are out-of-band int codes plus a message, why
      `Ok`/`Err` are conflated with `true`/`false`, and why `null` is `0` (four spellings each of 0
      and 1). A `Result<T, E>`-shaped variant type would let most of the error-handling surface be
      deleted rather than maintained.
      The sharpest evidence is `stdlib/json`: it has **no `parse`**, and its 27 public functions
      are string scanners that re-scan the document on every key lookup
      (`get_int(json:str key:str -- value:i64 found:i64)`), because there is no value type to parse
      into and no dispatch to walk one with. That `(value, found)` pair is an ad-hoc `Option`
      repeated across dozens of stdlib signatures.
      Deliberately **not** adding: interfaces/traits (generics see 5 uses, not under strain),
      slices/iterators (`len`/`nth`/`append`/`set` over `ptr` arrays is the right level),
      `comptime`/const-generics. Labeled `break` will bite eventually with nested `for`, but not yet.
      **Open question**: does the JSON case make this the next thing to do, or is a real
      `json::parse` simply not a goal?

### Language review 2026-09-17

Findings from a full review of the language surface. Method: the specification read end to end;
`semantic_validator_typecheck.cc` / `_instructions.cc` read at the points where checking is
suspended; ~40 probe programs compiled and run against `quadc 0.5.0-58-gd963049e`; and a feature
census over the `stdlib` + `examples` corpus (82 `.qd` files, 18,918 lines then and 19,079 now,
doom port absent as before).

Nothing here is an accepted decision. Each item records **what was measured**, and **the open
question** — several may well come out as "working as intended, close it". IDs are stable so they
can be referred to while working through them; they are ordered by leverage, not by effort.

Counts below were re-measured on 2026-09-18 with a tokenizer that strips string literals, comments
and parenthesised signatures. The original figures came from `grep -rhoE "(^|[^:a-zA-Z_])word\b"`,
and in GNU grep a `^` alternation inside a group silently matches nothing under `-o` — it reports
0 for a word a plain `grep -ow` finds 22 times. Anything here claiming "zero uses" was checked
that way.

#### What the compiler does and does not check

- [ ] **R9. `docscheck` covers 16% of the documented examples.** 219 blocks checked and passing,
      1,120 skipped as "not programs", out of 1,337 fenced Quadrate blocks (218/1,117/1,335 at the
      review; the three added are this session's). The floor is honestly scoped and documented in
      `tools/check_docs.py` — but the front-page example is in the 84%:
      `fn double(x:i64 -- result:i64) { 2 * }` in `about.md` does not compile: naming `x` consumes
      it, so the body underflows. It wants `x 2 *` or `stack fn double(i64 -- result:i64)`.
      **Open question**: can the harness synthesise a `fn main` wrapper for fragment blocks and lift
      coverage materially, or is the current floor the right stopping point and the fix is only to
      hand-audit the handful of blocks that are whole declarations?

#### Language design decisions to settle

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

- [ ] **R15. The sized-integer divergence the spec already documents — decide it.** `300 cast<u8>`
      yields `300`, and a sized type on a parameter or return annotation is inert. Only struct
      fields and the `mem` accessors honour the width; those were verified correct (`packed struct
      { a:u32 … }` with `-1 >>a` loads back `4294967295`). The spec §3.1.1 says a conforming
      implementation "SHOULD either apply the width consistently or reject sized types in positions
      where it does not" — the reference implementation does neither.
      **Open question**: truncate in `cast`, or reject the annotation where it carries no meaning?
      Rejecting is smaller and matches the subtractive precedent.

#### Spec and implementation disagree

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
      12 `>>` sites remains the whole blast radius, which is why this is worth doing rather than
      documenting around.
      **Open question**: does the functional update mean copying the struct — and if so, does that
      make structs value types on assignment, which is a much larger change than 12 call sites?

#### Ergonomics and gaps

- [ ] **R24. `err` is global state, not a value.** Set by `panic`, cleared on read, survives
      intervening non-fallible calls (all verified). It cannot be stored, returned, wrapped, or
      chained, so there is no way to build "failed to open config: no such file". It is also the
      main reason stdlib authors reach for `!` rather than propagating.
      A value that cannot be put on the stack cannot be composed, which is why this is filed as a
      blocker for the error surface generally rather than an ergonomic wish.
      **Open question**: subsumed by the sum-types item above, or worth an independent
      error-value type first?

- [ ] **R27. Threads get a hardcoded 1,024-element stack.** `qd_create_context(1024)` in
      `stdlib/thread/src/thread.c:44` and `lib/rt/src/runtime.c:856`; `-s` does not reach it.
      Verified: a thread body that pushes 5,000 values dies with *"Stack overflow (use -s to
      increase stack size)"* — advice that does not work for this case. Thread contexts are properly
      isolated otherwise, which is the important part.
      **Open question**: plumb `-s` through, or make it a `thread::spawn` parameter?

#### Cuts with corpus evidence

- [ ] **R30. Two genuinely unused debug builtins, not four.** Re-measured 2026-09-18 over
      `stdlib`, `examples`, `tests` and `cmd` (the last was missing from the original count, which
      is where `quadmcp`'s Quadrate sources live):

    - `printsv` — **0 uses anywhere.**
    - `printv` — **5, all in its own tests** (`errors/invalid_type`, `errors/invalid_type_output`,
      `control_flow/function_pointers`). `prints` is likewise 0 outside tests.
    - `free` — **109 uses. Not removable**: `stdlib/mem/qd/mem/mem.qd` implements `mem::free`
      *with* it (`address free`), so the suggested replacement was circular, and `buf free` on a
      raw allocation is the ordinary idiom.
    - `dec` — 3 uses, all in tests, but `--` is the same instruction and has **37 real decrement
      uses** (`examples/tak`, `examples/bf`). Removing the word form alone would delete one
      spelling of an instruction that is in use.

      So the only defensible cut is `printsv`, and possibly `printv`. Both are debug aids, and
      "no committed uses" is a weak argument for that category — nobody commits debug prints. The
      three print forms (`prints`, `printv`, `printsv`) are all unused in real code by that
      measure, which argues about the trio rather than the two.
      **Open question**: cut `printsv` alone, cut all three type-aware/stack debug forms, or keep
      them as the debugging affordance they are? Unlike the earlier removals this one is not
      settled by corpus counts.

#### Positioning and scope

- [ ] **R47. Almost the whole corpus is written in the bound dialect.** The gap is not a missing
      feature — `stack fn` makes the other form first-class — it is that almost nothing uses it.
      Measured over `stdlib` + `examples`, 2026-09-19:

      - **Of 497 functions that take parameters, 27 are `stack fn`.** A bound parameter is consumed
        off the stack, so the other 470 bodies re-push their arguments by name instead of operating
        on what the caller left.
      - **2,896 `-> ` named locals against 36 shufflers combined** (`swap` 18, `nip` 7, `rot` 5,
        `over` 4, `dup2` 2, `roll` 2, `pick` 0). `<<field` is 1,150 and `>>field` 12.

      The 27 are not spread evenly, and where they cluster is the useful part: **`math` 8, `bits` 7,
      `fuzzy` 6**, then `bytes` 2, `crypto` 1, and three in `examples`. Every other module is at
      zero — **including `hof`**, which is Factor-shaped in its *vocabulary* (`bi`, `tri`, `keep`,
      `dip`) while every one of its own signatures binds. So the combinator library that reads most
      stack-direct is not written that way either.

      It is deliberately *not* a sweep: a mechanical `-> x` elimination would produce unreadable
      stack gymnastics and prove the wrong thing. The question is which code is *better*
      stack-direct, and that has to be answered by porting and reading.

      Suggested first cut: **`bits.qd` then `fuzzy.qd`** — they already hold 13 of the 27 `stack
      fn` declarations and the heaviest shuffler use in the corpus, so they should be where the
      style fits with least forcing. If the diffs read worse *there*, the answer is "keep the bound
      dialect", and this is where to find that out cheaply. `bits.qd` is also freestanding-eligible,
      so it settles whether bound locals and stack juggling lower to the same code. Then `hof.qd`,
      which should be pure gain. Then `dc.qd`, the honest test — a calculator that currently
      hand-rolls its own stack over `mem::alloc`.
      **Open question**: is the target every module, or is the honest answer that `hof`-style code
      is stack-direct and data-heavy modules keep named locals? Answering "every module" without
      porting three first would be a guess — and is `dc.qd` the acceptance test?

### Interpreter tier (lib/interp)

- [ ] Not planned for this tier: structs, `defer`, `import`/`use`, anonymous functions. Imports in
      particular cannot mean anything on a device with no package resolution — qdos takes its
      scopes from `lib<name>.so` filenames — so they should keep refusing clearly.

### Formatter

- [ ] Remaining edge cases (low priority — no valid Quadrate program triggers them):
    - Unclosed nested block comments (`/* a/* b/* c`): the formatter wraps with a synthetic `*/`.
      `fuzz_formatter` skips inputs with an unclosed `/*`.
    - Content loss on dense malformed input with control chars (e.g.
      `fn main() {ed \tp <<_fncx!+!\t {\n\t\035if {}}\n}`) — pass 1 produces output that pass 2 fails
      to re-parse. The normalization step is lossy on these inputs.

### Deferred

- [ ] Package registry — searchable index instead of raw Git URLs.
- [ ] (Stretch) Inline asm — `asm("cli; hlt")` style. Today everything privileged lives in a `.S`
      file called via FFI, which works fine; this is quality-of-life for short sequences (port I/O,
      halt) so kernel code can stay in `.qd`.

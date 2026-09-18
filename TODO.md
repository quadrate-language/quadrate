# TODO

## Open

### Direction: Quadrate is a concatenative stack language (decided 2026-09-18)

The goal is the concatenative property: **juxtaposition is composition**, so any contiguous run of
words can be lifted into a named word and replaced by its name without changing meaning. That is a
testable property, not a style preference, and it is what the items below are now judged against.

The language already admits it. Verified against this build:

```qd
stack fn sq(i64 -- r:i64) { dup * }    // the concatenative form (spelled `fn sq(i64 …)` until R11)
fn sq(n:i64 -- r:i64) { dup * }        // naming CONSUMES it: "Stack underflow (requires 1 value)"
6 fn (i64 -- r:i64) { dup * } call     // inline quotation applied: 36
```

and `stdlib/hof` already carries a Factor-shaped combinator *vocabulary* — `bi`, `tri`, `keep`,
`dip`, `both`, `bi_star`, `when`, `unless`, `times`, `fold` — though not yet the dialect: all of its
own signatures take named parameters (R47). Enforced stack effects are not an obstacle; Factor
enforces them too. So this is not a rewrite. It is picking a side in a disagreement the tree is
already having with itself, and then moving the corpus (R47), which is where the real cost is:
**of 816 functions that take parameters, 784 name them** — and naming consumes.

What this decision closes, and where it lands, is recorded on each item. Two were deleted outright
rather than answered: *"Decide the fate of the live shufflers"* (its proposed experiment was to port
`bits.qd` and `fuzzy.qd` **to named locals** and read the diff) and **R18** (multi-return selection
"has no syntax" — `drop`/`nip`/`swap` are that syntax). Both are recoverable from git if the
direction is ever reversed.

*Amended 2026-09-19 — the half of this that said which form is "the real one" is withdrawn; the
half that said the difference must not be silent is shipped.* R11 landed as `stack fn` (see Done):
binding is the unmarked default and the modifier marks the stack form, rather than naming becoming
documentary everywhere. So the language now carries **both dialects explicitly**, and the choice
per function is a readability judgement rather than a property of the colon. Two consequences for
the items below. **R47 is no longer a migration**: the 456 bodies that name their parameters are
correct as written, and what is left of the item is the open question it always had — which code
reads better stack-direct — now answerable one function at a time with no compiler change behind
it. **R13 and R21 lose the justification they were given on 2026-09-18** (that the concatenative
property demanded them) and stand or fall on their own: the `and`/`or` hazard is real regardless,
and `>>field` chaining is a spec-versus-implementation disagreement regardless.

### Language design / scope

From a feature-scope review (2026-08-13). Counts are whole-corpus greps over `lib/` + `examples/`
(82 `.qd` files, 18,891 lines) and **exclude the doom port**, whose sources aren't in the working
tree — `packed` and `enum` score much higher there.

Overall read: the core is the right size — comparable to Go. The problem isn't count, it's
redundancy. Subtractive work, with one exception the Direction above carves out: the shufflers are
vocabulary, not redundancy, and the item proposing to cut them is deleted. Re-counted 2026-09-18:
**21 keywords, 86 `BUILTIN_INSTRUCTIONS` entries** (17 of them `__` freestanding internals), **66
documented in `reference.def`** — the review's 87/69 predate the `read` removal (R41) and the
`ctx` removal.

- [ ] **Make `pick` fixed-depth; drop `roll`.** *(Rewritten 2026-09-18 — was "drop `pick`/`roll`
      from the user-facing surface".)* The technical objection stands and is the reason to change
      them rather than keep them as they are: both take a **runtime** index, which defeats static
      stack tracking, and codegen already refuses them in compile-time-stack functions
      (`generator_nodes_instructions.cc`, "not supported in compile-time-stack functions"). But
      deleting them outright was the wrong conclusion once the language is concatenative — Factor
      keeps `pick` as a fixed-depth word, `( x y z -- x y z x )`, and has no `roll` at all. So:
      respell `pick` as third-item copy, which is statically checkable and needs no index, and drop
      `roll` from the user-facing surface while keeping it as an internal op (method receiver
      rotation). Blast radius is nil either way — `pick` is at **0** corpus uses and `roll` at 2.
- [ ] **Sum types / tagged unions** — the one addition worth arguing for, and no longer a longer
      horizon: a concatenative language is worse off than an ALGOL one with an error channel that
      cannot be put on the stack (see R24), so the Direction above promotes this. `enum`
      gives bare ints and `struct` gives records, but there's no "one of these". That absence is *why*
      errors are out-of-band int codes plus a message, why `Ok`/`Err` are conflated with `true`/`false`,
      and why `null` is `0` (four spellings each of 0 and 1). A `Result<T, E>`-shaped variant type
      would let most of the error-handling surface be deleted rather than maintained.
      Deliberately **not** adding: interfaces/traits (generics see 5 uses, not under strain),
      slices/iterators (`len`/`nth`/`append`/`set` over `ptr` arrays is the right level),
      `comptime`/const-generics. Labeled `break` will bite eventually with nested `for`, but not yet.
- [ ] Watch list, no action yet — features barely carrying their weight in-corpus: `type` aliases 0,
      `var` globals 0, string interpolation 1, `enum` 1 decl, generics 5, `sizeof` 5. (`packed` 0 and
      `enum` 1 are load-bearing in the doom port, so they stay regardless.)
- [ ] Scope-vs-depth note, not a task: 39 stdlib modules, 506 public `.qd` functions plus the
      C-implemented modules (`strings`, `io`, `os`, `fmt`, `net`, `http`, `tls`), 10 CLI tools, 67k
      lines of C++. The library and tooling surface has outrun the compiler's reliability — the
      two `quadc` segfaults that stood here are fixed (see Done), but both had been open and
      unbisected for months while modules and tools kept landing. Breadth is fine; depth under the
      breadth is thin. Worth weighing before the next module or tool lands.

### Language review 2026-09-17

Findings from a full review of the language surface. Method: the specification read end to end;
`semantic_validator_typecheck.cc` / `_instructions.cc` read at the points where checking is
suspended; ~40 probe programs compiled and run against `quadc 0.5.0-58-gd963049e`; and a feature
census over the `stdlib` + `examples` corpus (82 `.qd` files, 18,918 lines then and 19,079 now,
doom port absent as
before).

Nothing here is an accepted decision. Each item records **what was measured**, and **the open
question** — several may well come out as "working as intended, close it". IDs are stable so they
can be referred to while working through them; they are ordered by leverage, not by effort.

Where an item restates something already open above (`R3`), it is kept only for the new evidence,
not as a second copy of the task.

*Amended 2026-09-17*: R2 corrected (its headline claim was wrong); R34 and R35 added and fixed the
same day, R34 taking R6 with it. R36–R38 were found while fixing those and have since been resolved
or dropped. Everything from this review that is finished is in Done, condensed.

*Re-verified 2026-09-18.* Every number in this section was re-measured; corrections are inline
below and summarised here. **Wrong**: R4's abort-site counts (inflated ~65% by counting fallible
*declarations* as call sites, plus two per-module figures that were simply not there) and R29/R30's
zero-use claims. **Confirmed**: the corpus size (82 files), json's 27 public functions, R9's doc
coverage ratio, R16's single `enum`, R20's byte-string behaviour, R27's 1,024-element thread stack,
R33's LOC figures, R31's ratio. **Drifted since the review, by this session's own work rather than
by error**: `loop` 177 → 182, docscheck 218 → 219 blocks, `lib/llvmgen` 12,695 → 12,414 lines, the
suite 2,064 → 2,112 tests. Line-number references have been replaced with function names, which do
not drift.

*Amended 2026-09-18 — the usage counts in this section were wrong.* They were produced with
`grep -rhoE "(^|[^:a-zA-Z_])word\b"`, and in GNU grep a `^` alternation inside a group silently
matches nothing under `-o`: the pattern reports 0 for a word that a plain `grep -ow` finds 22
times. Every "zero uses" claim below was re-measured with a tokenizer that strips string
literals, comments and parenthesised signatures (`/tmp/census.py` shape; worth committing if this
recurs). The ratios and the headline conclusions survive — named binding still outnumbers every
shuffler combined by roughly 40:1, and `>>field` really is 12 uses — but three specific cut
candidates did not: R29 was withdrawn outright (`read` was load-bearing, and has since been
removed for a different reason — see Done), and R30 was cut from four unused builtins to two,
where it remains open.

#### Abstraction — the cluster that shapes everything else

- [ ] **R3. Evidence for the sum-types item above, not a second copy of it.** Three measurements
      that were not in the original note, all consequences of "there is no *one of these*":
      `stdlib/json` has **no `parse`** — its 27 public functions are string scanners that re-scan the
      document on every key lookup (`get_int(json:str key:str -- value:i64 found:i64)`), because
      there is no value type to parse into and no dispatch to walk one with; the `(value, found)`
      multi-return convention it uses is an ad-hoc `Option` repeated across dozens of stdlib
      signatures; and see R4 for the error-handling cost.
      **Open question**: does the JSON case move this from "longer horizon" to the next thing after
      R1/R2, or is a real `json::parse` simply not a goal?

#### What the compiler does and does not check

- [ ] **R9. `docscheck` covers 16% of the documented examples.** 219 blocks checked and passing,
      1,120 skipped as "not programs", out of 1,337 fenced Quadrate blocks (218/1,117/1,335 at the
      review; the three added are this session's). The floor is honestly
      scoped and documented in `tools/check_docs.py` — but the front-page example is in the 84%:
      `fn double(x:i64 -- result:i64) { 2 * }` in `about.md` does not compile (R11: naming `x`
      consumes it, so the body underflows).
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
      **Inverted 2026-09-18: the spec is the one that is right.** The item's parenthetical said
      "almost certainly documentation only" — that was under the old direction. "Sets field, pushes
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
      main reason R4's authors reach for `!`.
      **Raised 2026-09-18 by the Direction above.** A value that cannot be put on the stack cannot
      be composed, so global `err` costs a concatenative language more than it cost an ALGOL one —
      this moves from "ergonomics and gaps" to a blocker for the error surface generally.
      **Open question**: subsumed by R3, or worth an independent error-value type first?

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

- [ ] **R31. The migration baseline.** *(Repurposed 2026-09-18: these were the supporting numbers
      for the `pick`/`roll` and live-shuffler cuts. The shuffler item is deleted and the ratio below
      now reads the other way — it is the problem statement, not the verdict, and the number to
      drive down under R47.)* Re-measured
      2026-09-18 with the tokenizer, over `stdlib` + `examples` (the original scope): `-> ` locals
      **2,896**; `<<field` 1,150; `>>field` **12**; `drop` 54, `dup` 23, `swap` 18, `nip` 7, `rot`
      5, `over` 4, `dup2` 2, `roll` 2, `pick` **0**. Named binding outnumbers every shuffler
      combined by roughly 40:1 (the earlier 46:1 was from the broken pattern).

      Under the Direction above none of this argues for cutting anything, and each figure now reads
      differently. **40:1** is the gap to close, not evidence the shufflers are dead weight — it is
      R47's headline. **`pick` 0 and `roll` 2** still single those two out, but for respelling
      rather than removal: they are the only two that cannot be statically checked. **`>>field` at
      12** is the one figure that keeps its original reading — structs are read-mostly in practice,
      which is also why R21's blast radius is small.
      **Open question**: is 40:1 the right metric to track, or is "functions whose parameters are
      unnamed" the more honest one, since that is what R47 actually changes?

#### Positioning and scope

- [ ] **R32. The stack is no longer the programming model — parent item for closing that gap.**
      *(Its open question is answered: see Direction at the top of this file. The other branch, a
      docs restructure that leads with functions and locals and demotes the stack to "how calls
      work", is dropped — it would have documented the drift as the design.)*

      The diagnosis stands unchanged and is the reason the decision was needed. Given R31's ratios,
      real Quadrate is an ALGOL-family language with named parameters, named locals, structs and
      methods, that uses postfix syntax and a stack calling convention. `examples/dc/dc.qd` — a
      calculator, the most stack-shaped program there is — opens with
      `fn stack_push(s:ptr val:f64 -- )` over a hand-rolled `mem::alloc` array, because the
      language's stack is not where data lives. Meanwhile `learn/2-stack/` is four pages against
      three for functions and two for error handling. *(Corrected 2026-09-18: the review said "more
      than functions and error handling together", which is 4 against 5 — it is not. Four pages on
      the stack, more than on either subject alone, is the accurate form and still the point.)*

      What hangs off this: R47 (the corpus), R13 (short-circuit via
      quotations), R21 (`>>field` chains), the `pick`/`roll` respelling, and R3/R24 gaining
      priority. The docs work is still real but it is now the *last* step and points the other way:
      `learn/2-stack/` becomes correct rather than demoted.
      **Open question**: does `dc.qd` get rewritten onto the language's own stack as the proof, and
      is that the acceptance test for R47?

- [ ] **R47. The corpus is written in the other dialect, and nothing in this file said so.**
      The gap is not a missing feature — the concatenative style compiles today (see Direction) —
      it is that almost nothing is written in it. Two measurements over `stdlib` + `examples`,
      2026-09-18:

      - **2,896 `-> ` named locals against 36 shufflers combined** (`swap` 18, `nip` 7, `rot` 5,
        `over` 4, `dup2` 2, `roll` 2, `pick` 0) — R31.
      - **Of 816 functions that take parameters, 784 name them and 32 do not.** A named parameter is
        consumed off the stack, so those 784 bodies re-push their arguments by name instead of
        operating on what the caller left. This is the measurement that was missing from this file.

      The 32 are not spread evenly, and where they cluster is the useful part: **`bits` 7 of 18 and
      `fuzzy` 6 of 11**, then `math` 8 of 93, `examples` 8 of 83, `bytes` 2 of 24, `crypto` 1 of 41.
      Every other module is at zero — **including `hof`**, which is Factor-shaped in its *vocabulary*
      (`bi`, `tri`, `keep`, `dip`) while every one of its own signatures is named. So the combinator
      library that reads most concatenative is not written concatenatively either.

      This is the item that decides whether the direction is real, and it is deliberately *not* a
      sweep: a mechanical `-> x` elimination would produce unreadable stack gymnastics and prove the
      wrong thing. The question is which code is *better* concatenative, and that has to be answered
      by porting and reading.

      Suggested first cut: **`bits.qd` then `fuzzy.qd`** — they are already the two most
      concatenative modules by the ratio above, which is the same reason the deleted shuffler item
      named them as its heaviest shuffler users, so they should be the ones where the style fits
      with least forcing. If the diffs read worse *there*, the direction is wrong and this is where
      to find that out cheaply. `bits.qd` is also freestanding-eligible, so it settles whether named
      locals and stack juggling lower to the same code. Then `hof.qd`, which is the one that should
      be pure gain. Then `dc.qd`, the honest test — a calculator that currently hand-rolls its own
      stack over `mem::alloc`.
      **Open question**: is the target every module, or is the honest answer a two-dialect language
      where `hof`-style code is concatenative and data-heavy modules keep named locals? Answering
      "every module" without porting three first would be a guess.

- [ ] **R33. Supporting numbers for the scope-vs-depth note above.** Re-verified 2026-09-18:
      `cmd/` is **20,841 lines, larger than `lib/llvmgen/` at 12,414** — ten CLI tools against one
      code generator (`lib/qc` is 22,880, `lib/rt` 12,360). The suite now passes 2,112: 914 stdlib,
      337 MCP server, 714 language, 25 C++ compiler tests; it was 2,064 at the review and the
      growth is this session's regression tests. The whole
      `[Unreleased]` changelog is completions, `--help` layout, `quadrepl`, `quaddoc` and diagnostic
      prefixes — **no language changes except removals** — while the two `quadc` segfaults that
      stood at the top of this file went unbisected for want of the doom sources. (They are fixed
      now; the sources were in git history at `01e766b6` the whole time.)
      **Open question**: is a tooling freeze for one release worth doing explicitly, or does the
      existing note already cover it?

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

## Done

Condensed to the decision and anything that would be re-litigated without it. The full write-ups
are in this file's git history; the user-facing versions are in `CHANGELOG.md`.

### 2026-09-18 — language review items

- [x] **R4 — `strings::char_at` is total.** The 158 abort sites were three unrelated populations,
      not one, and **all sat inside non-fallible functions**, so none was the local `!` → `?` edit
      the item proposed. Fixed by removing the need for the operator: an index outside the string
      gives `strings::NotAChar` (-1), as `index_of` already returns -1 and `slice` already clamps.
      Fuzzing (8,070 malformed inputs) found 10 of json's 18 entry points killing the process and
      `array_len "[}"` hanging; `uri`, `path`, `hex`, `base64`, `fuzzy` were clean before and after,
      so the "not shippable" charge was true of `json` specifically. `strings::substring` was the
      worse bug and was not in the item at all: declared fallible, documented `@error
      ErrOutOfBounds`, and it called `abort()` — so every caller's `switch` was dead code.
      Remaining: the 33 `substring!` sites, guarded at every site the fuzzing reached.

- [x] **R10 — exactly one of them was really unasserted; the skip line could not say which.**
      Generating `.out` siblings was the wrong answer: none of these files is a program with a
      `main` whose output was forgotten. Of the 93 (the suite has grown to 834 files since the 92
      of 803 above), 42 are asserted by a suite other than `qd` — 37 `stdlib/*_test.qd` through
      `quad test`, 3 `args/*.qd` through the args suite, which pins the output per argument list,
      2 blocking http servers — and 50 are helper modules, asserted through the `.out`/`.err` of
      the test that imports them. The exception was `tests/qd/test_constants/module.qd`: a stale
      duplicate of
      `constants/test_constants/module.qd` with non-`pub` constants and an older `Pi`, which
      `use test_constants` never reached because the copy beside the test is nearer. Deleted.
      What was actually wrong is that `SKIP:no expected output` read identically for a helper and
      for a forgotten `.out`. `tools/check_test_expectations.py` (reference suite) now fails unless
      every file without an expectation is reached from one that has one — by relative-path `use`,
      by module name resolved outwards as the compiler resolves it, or by a symbol it defines named
      inside its own module — and `run_all.sh` states which of the four reasons each skip is.

- [x] **R11 — the colon no longer decides; `stack fn` does.** The complaint was that
      `fn f(x:i64 -- r:i64)` binds and consumes `x` while `fn f(i64 -- r:i64)` leaves it on the
      stack, so an annotation that reads as documentation changes what the body means. The fix is
      a declaration modifier rather than a change of default: **binding stays the unmarked
      behaviour, and `stack fn` marks the function whose inputs stay on the stack.** Under it,
      names are optional and documentary — not in scope, visible to `quaddoc`, the LSP and
      diagnostics, and a body may bind a local of the same name with `->`. Without it every input
      must be named, since there would be nothing to bind, and the *"Cannot mix named and unnamed
      input parameters"* rejection is gone: it was a symptom of the ambiguity, and under `stack`
      naming only some inputs is merely partial documentation. Blast radius was 27 declarations in
      `stdlib`/`examples`/`cmd` and 96 in tests, none of them body changes. Two adjacent bugs fell
      out: a bare identifier in a parameter list was stored as the *name* with an empty type, so
      `fn f(P -- r:i64)` over a struct reported *"Invalid type ''"* and then a cascade; and the
      interpreter's declaration sniffer did not know `pub`/`inline`/`stack`, so `stack fn …` at
      the REPL was wrapped in a `main` and parsed as an expression.

- [x] **R12 — `while` is back, condition written once.** The `while` removed earlier was
      `cond while { body … cond }`, the same line count as the `loop { cond if { break } … }` that
      replaced it, so restoring it verbatim would have delivered nothing. Now `cond while { body }`,
      the condition re-evaluated at the head of each iteration. The condition is the shortest run of
      preceding words whose net effect is `( -- flag )`: the parser takes the preceding expression
      run, the validator trims it. The trim is load-bearing — `"Processing..." print nl` nets zero,
      so a net-effect check alone accepts it and it printed every iteration until the trim landed.
      The compile-time-stack path shipped untested (`main` is never native-eligible, so every test
      put its loop there); `control_flow/while_native` closes that.

- [x] **R17 — a closure can capture a `for` iterator, by value.** Two bugs. The validator tracks the
      iterator in `iteratorNames`, separate from `localVariables`, and passed only the latter as the
      capture scope. Fixing that alone would have produced a wild read: the closure environment
      holds *pointers* to each capture's slot, and an iterator has none — it is an SSA value
      replaced each iteration. So it is captured by value, each closure getting a refcounted block
      of its own. By-reference has no distinct meaning here: assigning to the iterator's name
      declares a shadowing local rather than moving the loop on.

- [x] **R25 — the `error { … }` literal is removed.** Two spellings of `msg code panic`, four uses,
      three of them its own tests. It was never a value: the parser rewrote it to a struct
      construction named `__error__` with special cases in the validator and generator.
      `skipBracedGroup` was added so a removed `name { … }` construct reports one error rather than
      also emitting a bogus "Unmatched '}' at top level".

- [x] **R39 — an anonymous function is a value, and parses like one.** `parseSimpleToken` now
      recognises `fn (`, which the array-literal loop already routes through, so `[fn (…) {…}]`
      came free. The struct-literal field parser needed its own check *before* its `:` test, which
      was the cause: the lambda's own `x:i64` was read as a field written with a colon, so the
      error advised `x = value` about a field nobody wrote. Specification 8.2 now states the
      general rule — a field initializer accepts anything accepted as a value elsewhere.

- [x] **R46 — the allocation sites were not aborting; `!` was inert.** The item said 50 sites abort
      on OOM. They did not: `mem::alloc` set neither `error_code` nor a message, and those are what
      `!` and `?` read, so the call fell through and the status was popped as the result — the next
      binding reported *"Stack underflow when assigning to local variable"*. An audit found three
      modules that never set it: `net.c` (33 paths, which also returned success from every
      failure), `mem_heap.c` (12), `http_server.c` (5). All fixed. With the mechanism working,
      "abort with a message naming the allocation" is a policy that is now true rather than
      asserted; specification 11.1 states it and distinguishes it from malformed input, which a
      library must not abort on. Specification 10.5 gains the rule the three modules broke.
      Converting the sites to `?` remains R3's cascade.

- [x] **R48 — a loop body gives a captured local one binding per iteration.** The capture block was
      allocated once in the entry block, so every iteration's closure pointed at the same one and
      escaping closures all read the last value. Allocated at the declaration now, previous released
      first. The pointer slot starts null so a declaration in an untaken branch leaves nothing to
      release, and release became one helper shared with function-exit cleanup, which had the logic
      inline and without a null check. Capture by reference is unchanged within a binding.
      `heapCapturePointers` went with it: written everywhere, read nowhere.

- [x] **R41 — `read` removed, `os::args` replaces it.** It splayed argv across the operand stack,
      which is not an expressible stack effect: the validator pushed sixteen synthetic `str` values
      and set `mHasUnpredictableStack`, so every function containing it lost its arity, if-arm and
      defer checks. Removing the padding exposed a real bug the fiction had hidden — the
      module-method call path pushed a fallible call's results but not its status.

- [x] **R5 / R7 / R8 — loops have a checked stack effect.** One hole, not three. A loop body must
      leave the stack as it found it; `break` arms must agree; `continue` must leave it as an
      iteration starts.

- [x] **Use-after-free on every struct global.** Reading a field off a `var` struct global freed it
      — five `globals/var_struct_*` valgrind failures, one bug.

- [x] **The two `quadc` doom segfaults.** Both in codegen, surfacing through `llvm::verifyModule`;
      neither file was special. Open for months for want of sources that were in git history at
      `01e766b6` the whole time.

### 2026-09-17 — language review items

- [x] **R1 — generic type parameters unify structurally.** `structTypesMatch` compared types as
      literal strings, so `[]T` never matched `[]i64`.
- [x] **R2 — `call` after `<<field` or on a typed parameter was never modelled.** Nothing converted
      a `fn(...)` type string back into a signature.
- [x] **R16 — `switch` over an enum warns about unhandled variants**, read off the case labels; a
      warning rather than an error, because the intent is inferred rather than typed.
- [x] **R28 — comparator `sort` and generic `hof`**, the payoff from R1: six entry points that each
      fixed element type *and* direction collapse to comparators.
- [x] **R34 — the declared-effect check ran only on literal-only bodies**, a misplaced
      `mHasUnpredictableStack = true`. Took R6 with it.
- [x] **R35 — `ident <<field` folded the identifier into the field-access node**, so the parser
      deleted the preceding node and codegen reconstructed it.
- [x] **R14 — `cast<T>` no longer converts a string to a number.** Every other direction `cast`
      offers is total; parsing is not, and it reported failure as `0`. `strconv` already had the
      honest version. The docs were teaching the unsafe form, including reading user input.
- [x] **R19 — nested array literals implemented**, which uncovered a silent miscompilation:
      codegen's element loop ignored non-literals without a word, so `7 -> x  [x 2 3]` built `[2 3]`.
- [x] **R20 — `str` made properly UTF-8** (a),(b), and `unicode` made to deserve its name (c)
      rather than renamed to `ascii`: the two layers disagreed about the same character.
- [x] **R22 — float division by zero gives inf/nan**, because the two code paths disagreed: an
      inline `fdiv` returned inf while the runtime path trapped.
- [x] **R23 — `print` renders arrays.** `qd_print` had no pointer case at all.
- [x] **R26 — reference cycles documented**, not detected; the measurement was redone after `drop`'s
      own leak (R42) swamped the first one.
- [x] **R29 — withdrawn; `read` is load-bearing.** The "zero bare uses" claim came from a broken
      grep: `^` inside a group matches nothing under `-o`, reporting 0 for a word `grep -ow` finds
      22 times. The same artefact cut R30 from four unused builtins to two, where it remains open.
- [x] **R40 — truncated signatures were `gen_docs.sh`, not `quaddoc`.** `\(([^)]*)\)` stops at the
      first `)`, which in `pred:fn(T -- i64)` is the inner one.
- [x] **R42 — a stack slot owns what it holds, pointers included.** `drop` leaked every array and
      struct it discarded; the item's scope estimate was wrong, because the codebase already had
      the right convention for strings behind a helper the survey missed.
- [x] **R43 — an unqualified call to a function in the module's own import block is rejected.** It
      used to compile clean and leave an extra value, so an `if` consumed that instead of the
      result and always took the true branch.
- [x] **R45 — an assertion failing inside a helper now fails its test.** 103 assertion calls across
      the corpus were inert.
- [x] **R44 — every character-index conversion goes through the memo.** Making `str` UTF-8 turned
      finding character *i* into a walk from the start, so a loop reading a string one character at
      a time became quadratic: a 20,000-character scan took 2.9 s and `json::get_array` over a 60 KB
      file never finished. Two memos on `qd_string_t` — the codepoint count, and a packed
      (char index, byte offset) scan cursor so a forward scan resumes rather than restarting. The
      cursor is what mattered, since the file contains `π` and so misses the one-byte shortcut.
      `char_at` already carried a comment warning about exactly this, from the last time it was
      fixed.

### Language design / scope — cuts

- [x] Cut `>>field!` (two forms for one operation, split on what `drop` already says), `ctx` (zero
      uses, ~340 lines, unmodellable by the static checker), and the eight zero-use shufflers
      `dupd`, `swapd`, `swap2`, `drop2`, `over2`, `overd`, `nipd`, `tuck`.

### Compiler & runtime

- [x] **A `for` loop with float bounds never terminated** — the runtime-stack path converted the
      bounds with `CreateFPToSI`, so a step of `0.5` became `0`. The type is not known at compile
      time there, so the loop now carries both iterators and selects on the runtime tag.
- [x] **`return` was a silent no-op in `main`**, top level included.
- [x] **Module bodies were never semantically validated when imported** — `use sb` compiled clean
      when `sb.qd` called a function that does not exist.
- [x] **The JIT could pair new codegen with an old runtime**, resolving an installed
      `libqdrt.so` ahead of the build's.
- [x] **`panic` with code 0 reported success** — sentinel collision with `error_code`'s "no error";
      fixed with a separate `has_error` flag.
- [x] **Fallible calls used two incompatible protocols** — FFI imports pushed the real code, user
      functions pushed 1/0, so `switch { Ok … }` always hit `_` for user calls.
- [x] **`defer` ignored the control flow it was written under** — a `defer` in an untaken branch
      still ran.
- [x] **Branch stack effects unified**, and the diagnostic promoted to an error.
- [x] **Build cache did not include compiler identity**, so a rebuilt `quadc` served executables
      from the old one (7,670 stale entries).
- [x] **`shr` was arithmetic in the constant folder and logical everywhere else**; `not` is bitwise
      and silently wrong for boolean use, so `lnot` was added rather than changing `not`.
- [x] **The parser accepted an unbalanced `}`**, closing the function early and compiling the
      truncated result cleanly.
- [x] Smaller: `//` comments on struct-field lines, the formatter deleting comments inside
      declaration bodies, `<` versus generic ambiguity, method dispatch colliding with builtin
      names, `switch` native-path codegen, `spawn`/`wait`/`detach` type models, sized integers
      reconciled with the spec, `create_test_context` hand-rolling the context.

### Interpreter tier, docs, CI, freestanding, examples

- [x] **Interpreter tier**: the six remaining AST-walker gaps closed — `return`, `const`/`enum`,
      named locals, `for`, `cast<T>`, array literals — plus `switch`.
- [x] **`make docscheck`** compiles and runs the fenced blocks in the docs; it immediately found
      dot-syntax that does not exist, `make<ptr>` for `make<Point>`, `drop`s in error arms, and
      `-> x` on already-bound parameters. Error-handling docs and the freestanding subset written.
- [x] **CI/quality**: the release build was broken at `-O3 -Werror` (a real null-deref);
      `fmtcheck` wired in; formatter and LSP fuzzers; two formatter idempotency bugs; function-entry
      coverage for `quad test`; dependency conflict detection across the graph.
- [x] **Freestanding mode**: `quadc --freestanding`, `libqdrt-freestanding.a`, validator subset
      enforcement, the `mem.c` / `mem_heap.c` split, raw `st*`/`ld*` builtins, and
      `examples/kernel/` — now x86_64 long mode with GDT, IDT, PIC, PIT and PS/2.
- [x] **Examples**: `wc` (matches GNU wc), `csvcut`.

### Earlier

- [x] Embedding API (`qd_pop_*`, `qd_load_file`, error accessors), go- and python-quadrate bindings.
- [x] Build cache (~200x on repeat builds), package manager semver `update`, formatter and LSP
      polish, the whole stdlib and examples formatted.

# TODO

## Open

### Compiler correctness (high priority)

- [ ] **quadc segfaults on a cross-module `pub fn` call from deeply-nested control flow.**
      `examples/doom/qd/r_perspective.qd` calls `R_PointToAngle2` (from `r_main.qd`) inside the
      R_DrawThings iteration, ~5 `if`/`else` levels deep. `quadc` dies with "Segmentation fault (core
      dumped)" during the final `doom.qd` compile — no diagnostic, just the signal. Signature is
      ordinary: `pub fn R_PointToAngle2(x1:i64 y1:i64 x2:i64 y2:i64 -- ang:i64)`. Defining the
      function in the same file works; adding `use "r_main.qd"` doesn't help. Likely a missing bounds
      check on a type-stack/local table past some nesting depth. Bisect under gdb for a backtrace.
      **Not reproducible from this tree** —
      `examples/doom/` currently contains only `build/`, `ffi/sdl_shim.o` and `wads/`; the `.qd`
      sources are absent, so this needs the port restored before it can be bisected.

- [ ] **quadc segfaults when `examples/doom/qd/d_main.qd` adds `use "info.qd"`.** `info.qd` is a
      ~1400-line auto-generated const table that already imports fine from `p_mobj`, `p_pspr`, etc.
      Pulling it into `d_main` to seed per-thing state-machine fields (`mobj_spawnstate`,
      `state_nextstate`, …) crashes the compiler with no message. Probably the same class as the item
      above — a module-graph shape crossing a limit in symbol resolution or type-stack state.

### Language design / scope

From a feature-scope review (2026-08-13). Counts are whole-corpus greps over `lib/` + `examples/`
(82 `.qd` files, 18,891 lines) and **exclude the doom port**, whose sources aren't in the working
tree — `packed` and `enum` score much higher there.

Overall read: the core is the right size — comparable to Go. The problem isn't count, it's
redundancy. Subtractive work. After the shuffler and `ctx` removals below: 21 keywords, 87
`BUILTIN_INSTRUCTIONS` entries (57 user-facing word-named, 13 symbol operators, 17 `__` freestanding
internals), 69 documented in `reference.def`.

- [ ] **Drop `pick`/`roll` from the user-facing surface.** Both take a *runtime* index, which defeats
      static stack tracking — codegen already refuses them in compile-time-stack functions
      (`generator_nodes_instructions.cc`, "not supported in compile-time-stack functions"). Keep
      `roll` as an internal op (method receiver rotation). Needs the 3 call sites rewritten first
      (`pick` 1, `roll` 2), so it is not the freebie the shuffler removal was.
- [ ] **Decide the fate of the live shufflers: `swap` 18, `over` 7, `nip` 7, `rot` 5, `dup2` 2.**
      Not obviously wrong, but two signals say they are papering over missing expressiveness rather
      than earning their place: **`rot` never appears alone** — 2 of its 3 sites are `rot rot`, i.e.
      the inverse spelled as two forwards — and **`over over` (math.qd) and `dup2` (fuzzy.qd) spell
      the same min/max idiom two ways**. The other cluster, `nip nip` / `drop nip` in `time.qd`, is
      not shuffling at all: it is *selecting one of a multi-return*, a real need with no syntax.
      Cheap way to settle it with evidence instead of argument: port `bits.qd` and `fuzzy.qd` to
      named locals and read the diff. They are the heaviest users and exercise both failure modes
      (`1 swap shl` = fixed operand order, `nip nip` = multi-return selection). Check first that
      named locals lower with no runtime call — `bits.qd` is freestanding-eligible.
      `dup` (26) and `drop` (48) are **not** in scope: `drop` is result-discarding (`io::write! drop`),
      the job of Go's `_ =`, and `dup *` for squaring is genuinely clearer than naming the value.
- [ ] (Longer horizon) **Sum types / tagged unions** — the one addition worth arguing for. `enum`
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
      lines of C++. The library and tooling surface has outrun the compiler's reliability — two
      documented `quadc` segfaults on ordinary programs, above. Breadth is fine; depth under the
      breadth is thin. Worth weighing before the next module or tool lands.

### Language review 2026-09-17

Findings from a full review of the language surface. Method: the specification read end to end;
`semantic_validator_typecheck.cc` / `_instructions.cc` read at the points where checking is
suspended; ~40 probe programs compiled and run against `quadc 0.5.0-58-gd963049e`; and a feature
census over the `stdlib` + `examples` corpus (82 `.qd` files, 18,918 lines, doom port absent as
before).

Nothing here is an accepted decision. Each item records **what was measured**, and **the open
question** — several may well come out as "working as intended, close it". IDs are stable so they
can be referred to while working through them; they are ordered by leverage, not by effort.

Where an item restates something already open above (`R3`, parts of `R23`), it is kept only for the
new evidence, not as a second copy of the task.

#### Abstraction — the cluster that shapes everything else

- [ ] **R1. Generic type parameters unify on bare `T` only.** `[]T` and `fn(...)` appear to be
      compared as literal type strings, so every generic over an aggregate is rejected at the call
      site. Measured:
      `fn count<T>(a:[]T -- n:i64)` called as `[1 2 3] count` → *"Parameter 1 expects type '[]T' but
      got '[]i64'"*; `fn apply<T>(x:T f:fn(T -- T) -- r:T)` called as `5 &dbl apply` → *"expects
      'fn(T -- T)' but got 'fn(i64 -- i64)'"*. Bare `T` works (`fn id<T>(x:T -- y:T)` accepts i64,
      str and f64), and generic structs work. The workaround in the corpus is to declare the
      parameter `ptr`, which discards checking entirely.
      **Open question**: is this a substitution bug in the signature comparison (likely small), or
      does `[]T`/`fn(...)` unification need type-variable machinery that does not exist yet? The
      answer decides whether R28 and the `type`-alias watch item resolve themselves.

- [ ] **R2. A function pointer does not survive a struct field.** The declared `fn(...)` type is
      lost through `<<field`, so the following `call` is modelled as producing nothing:
      `struct Shape { area:fn( -- f64) }` then `s <<area call print` → *"Type error in 'print':
      Stack underflow (requires 1 value)"*. Binding first (`s <<area -> f  f call -> a`) fails the
      same way, and declaring the field `ptr` does not help. There are **zero fn-typed struct fields
      in the whole corpus**, which is consistent with the feature never having worked.
      **Open question**: fix the type propagation, or decide that fn-pointer fields are out of scope
      and reject them at the declaration instead of at the use site? This is the minimum viable form
      of dynamic dispatch, so the answer also bears on the interfaces/traits decision above.

- [ ] **R3. Evidence for the sum-types item above, not a second copy of it.** Three measurements
      that were not in the original note, all consequences of "there is no *one of these*":
      `stdlib/json` has **no `parse`** — its 27 public functions are string scanners that re-scan the
      document on every key lookup (`get_int(json:str key:str -- value:i64 found:i64)`), because
      there is no value type to parse into and no dispatch to walk one with; the `(value, found)`
      multi-return convention it uses is an ad-hoc `Option` repeated across dozens of stdlib
      signatures; and see R4 for the error-handling cost.
      **Open question**: does the JSON case move this from "longer horizon" to the next thing after
      R1/R2, or is a real `json::parse` simply not a goal?

- [ ] **R4. 315 abort-on-error call sites, 263 of them inside the standard library.** Counts are
      `[a-z_]!` call sites over the corpus: 263 in `stdlib`, 52 in `examples`. Heaviest: `json` 38,
      `thread` 30, `uri` 28, `os` 19, `regex` 18, `path` 18. `json.qd` calls `strings::char_at!`
      20+ times while scanning *untrusted input*, and line 376 already carries the comment
      *"`!` aborted here, which killed the process on untrusted input"* against a site that was
      converted to a `switch`. A library that aborts the host process on malformed input is not
      shippable, and this reads as the design pushing authors there — `?` requires the enclosing
      function to be fallible, and threading a bare `(code, msg)` through recursive descent is
      miserable — rather than as carelessness.
      **Open question**: is this a stdlib-hygiene sweep to do now (convert library `!` to `?`), or
      does it wait for R3 because the sweep would otherwise be done twice?

#### What the compiler does and does not check

- [ ] **R5. Any function containing a `loop` gets no declared-effect verification.**
      `LOOP_STATEMENT` sets `mHasUnpredictableStack = true` unconditionally
      (`semantic_validator_typecheck.cc:1302`), and the arity check at :682 is gated on that flag.
      Verified: a `( -- r:i64)` function whose `loop` body pushes an unconsumed `42` each iteration
      compiles clean and returns with three junk values on the stack. With `loop` at 177 uses and
      `while` removed (R12), this exemption covers most non-trivial functions in the corpus.
      The other exemptions, for the record: `read` (R14), `fmt::printf`, `fmt::sprintf`,
      `flag::parse`, every FFI-imported function, and any unresolved symbol.
      **Open question**: model loop bodies properly (fixpoint over the body, `break` edges joined),
      or accept the gap and stop claiming *"validates all stack operations at compile time"*
      (`index.md`)? A middle option is to check only loops with no `break`/`continue`.

- [ ] **R6. `switch` arms are not balance-checked.** The `if`/`else` rule at :1057 — with its
      well-reasoned diverging-arm and post-fallible-call exemptions — has no `switch` counterpart.
      Verified: `c switch { 1 { 10 }  2 { 20 30 }  _ { 0 } }` in a `( -- r:i64)` function compiles
      and runs. The same argument the `if` rule makes applies verbatim: whatever follows reads a
      value whose identity depends on which arm ran.
      **Open question**: extend the existing check to `switch`, including the `Ok`-arm exemption
      that mirrors the post-fallible-call case?

- [ ] **R7. An unbalanced `if` inside a loop is caught by codegen, as an internal error.**
      `quadc: error: internal: if/else arms leave the stack at different depths (then: 2, else: 3)
      at line 3` followed by `quadc: error: LLVM generation failed` — no source excerpt, no
      `file:line:column:`, and the word "internal" tells the user they hit a compiler bug when they
      wrote ordinary bad code. The frontend misses it precisely because of R5.
      **Open question**: this is arguably fixed for free by R5. If R5 is declined, is it worth
      promoting the codegen check to a located frontend diagnostic on its own?

- [ ] **R8. `for` bodies are checked against the pre-loop stack, so the model is unsound in both
      directions.** The body is type-checked in a *copy* and the parent stack is left unchanged
      ("loops don't have consistent stack effects", :1296). Verified rejecting a valid program:
      `0 1000 1 for i { i }  0 1000 1 for i { drop }` → *"Type error in 'drop': Stack underflow"*,
      though it is balanced. And verified accepting an invalid one: a `for` body leaving values per
      iteration passes silently (`0 5 1 for i { i }` leaves depth 5 at runtime).
      **Open question**: is accumulate-on-the-stack inside `for` an idiom worth keeping (it is the
      only map-like form available today), in which case the check can only ever be a warning — or
      is it a mistake that R1 would make unnecessary?

- [ ] **R9. `docscheck` covers 16% of the documented examples.** 218 blocks checked and passing,
      1,117 skipped as "not programs", out of 1,335 fenced Quadrate blocks. The floor is honestly
      scoped and documented in `tools/check_docs.py` — but the front-page example is in the 84%:
      `fn double(x:i64 -- result:i64) { 2 * }` in `about.md` does not compile (R10).
      **Open question**: can the harness synthesise a `fn main` wrapper for fragment blocks and lift
      coverage materially, or is the current floor the right stopping point and the fix is only to
      hand-audit the handful of blocks that are whole declarations?

- [ ] **R10. 87 language tests have no asserted output.** `run_all.sh` reports them as
      `SKIP:no expected output` — they compile and run but nothing is pinned, ~11% of the 682
      language tests.
      **Open question**: generate the missing `.out` siblings from current behaviour and review the
      diff, or are these deliberately output-free?

#### Language design decisions to settle

- [ ] **R11. Naming a parameter silently changes the calling convention.** `fn f(x:i64 -- r:i64)`
      binds and consumes `x`; `fn f(i64 -- r:i64)` leaves it on the stack. An annotation that looks
      purely documentary changes what the body means, and mixing the two forms is rejected outright
      (*"Cannot mix named and unnamed input parameters"*). The rule is stated in the spec (§4.4) and
      the interpreter tier already had eleven tests encoding it wrongly.
      **Open question**: leave it (it is at least consistent and diagnosed), or make naming
      non-consuming, or bind *and* leave the value? This is the wart most likely to cost a newcomer
      their first hour, so "leave it, document harder" is a legitimate answer but should be a
      decision rather than the default.

- [ ] **R12. The cost of removing `while`, now measurable.** `loop` is at 177 uses against `for`'s
      104, and the overwhelming majority are `loop { cond if { break } … }` — three lines and a
      nesting level where `while` was one. `examples/kernel/kernel.qd` is wall-to-wall with it.
      Compounding: `loop` is exactly the construct that suspends effect checking (R5), so the
      removal pushed the dominant loop form into the unchecked path.
      **Open question**: reinstate `while` as sugar that lowers to the same `loop`, or does the
      subtractive argument still hold? If R5 is fixed, most of this item evaporates.

- [ ] **R13. `and`/`or` are bitwise and are used throughout as logical.** There is no short-circuit
      operator; `lnot` exists but has no binary counterpart. Verified: `2 1 and` → `0`, so any
      operand not already 0/1 gives the wrong answer. Both sides always evaluate, so the guarded
      form `i xs len < xs i nth … and` is unsafe. Live in the corpus:
      `t 0.0 > best_t 0.0 < t best_t < or and if` (`examples/raytracer/raytracer.qd`) is correct
      only because every operand happens to be a comparison result.
      **Open question**: add short-circuit `land`/`lor` (named, per the no-symbolic-bitwise rule),
      or rely on comparisons always yielding 0/1 and document the hazard? Note this is a correctness
      question, not ergonomics.

- [ ] **R14. `cast<T>` fails silently.** `"42abc" cast<i64>` → `42`; `"notanumber" cast<i64>` → `0`,
      indistinguishable from parsing the string `"0"`. This sits oddly in a language that otherwise
      *forces* the caller to handle failure (R16 is the same shape). `strconv::parse_int` is the
      fallible sibling that already exists.
      **Open question**: make string→numeric `cast` fallible, or remove that direction of `cast`
      entirely and point at `strconv`?

- [ ] **R15. The sized-integer divergence the spec already documents — decide it.** `300 cast<u8>`
      yields `300`, and a sized type on a parameter or return annotation is inert. Only struct
      fields and the `mem` accessors honour the width; those were verified correct (`packed struct
      { a:u32 … }` with `-1 >>a` loads back `4294967295`). The spec §3.1.1 says a conforming
      implementation "SHOULD either apply the width consistently or reject sized types in positions
      where it does not" — the reference implementation does neither.
      **Open question**: truncate in `cast`, or reject the annotation where it carries no meaning?
      Rejecting is smaller and matches the subtractive precedent.

- [ ] **R16. No exhaustiveness check on `switch` over an enum.** A missing variant with no `_` arm
      is caught only incidentally, and only when the function's declared outputs happen to expose it
      (*"declares 1 output(s) but body leaves 0"*). The enum metadata needed is already collected.
      **Open question**: warn, error, or leave alone given `enum` has 1 decl in-corpus (though it is
      load-bearing in the doom port)?

- [ ] **R17. Closures cannot capture a `for` iterator.** `0 3 1 for i { fn ( -- r:i64) { i } … }` →
      *"Undefined identifier 'i'"*. The iterator is saved and restored around the loop as an
      ordinary frame entry, so it is presumably not in the capture set.
      **Open question**: fix, or is building closures in a loop out of scope? Worth noting the
      classic capture-by-reference-vs-value question has to be answered either way.

- [ ] **R18. Multi-return selection still has no syntax.** Already flagged above as the real need
      hiding inside the `nip nip` / `drop nip` cluster in `time.qd`; adding that the `(value, found)`
      convention makes it pervasive — every one of `json::get_*`, and much of `strings`, returns two
      values of which callers usually want one, spelled `-> found -> value` and then ignoring one.
      **Open question**: a discard binding (`-> _`), destructuring, or does R3 delete the need?

#### Spec and implementation disagree

- [ ] **R19. Nested array literals do not parse.** `[[1 2] [3 4]]` is documented in specification
      §3.2.2 as valid; it produces *"Unexpected character '['"*. Struct literals inside array
      literals do not parse either: `[ P { x = 1 } P { x = 2 } ]` fails the same way. `make<P>` plus
      `set` does work for arrays of structs.
      **Open question**: implement, or narrow the spec and the array-literal grammar to scalars?

- [ ] **R20. `str` is a byte string; the spec says "Immutable UTF-8 string".** Verified on
      `"héllo wörld"` (11 characters, 13 bytes): `strings::len` → 13, `strings::char_at 1` → 195
      (a continuation byte), `strings::upper` leaves `ö` unchanged, `index_of` returns a byte index.
      Separately, the `unicode` module is not a Unicode module — it is a table of ASCII character
      codes (`unicode::lbrace`, `unicode::digit0`).
      **Open question**: three separate decisions — (a) correct the spec's wording to "byte string";
      (b) add codepoint-level operations or explicitly decline them; (c) rename `unicode` to
      something that says what it is (`ascii`?), which is a breaking change across the corpus.

- [ ] **R21. `>>field` is specced as returning an updated struct; it mutates in place.** Structs are
      reference values: `P { x = 1 } -> a  a -> b  b 99 >>x drop` leaves `a <<x` as 99, and passing
      a struct to a function lets that function mutate the caller's value. The spec's "sets field,
      pushes modified struct back (for chaining)" and the idiom `p 42 >>x -> p` both read as a
      functional update.
      **Open question**: documentation only, or is there an argument for value semantics on
      assignment? (Almost certainly documentation only — but the current wording is actively
      misleading and 12 `>>` sites is a small blast radius if anything does change.)

- [ ] **R22. Float division by a literal `0.0` is a compile error.** *"Division by zero in '/': the
      divisor is the literal 0.0"*. For `f64` this is a well-defined IEEE 754 operation yielding
      infinity; the integer rule appears to have been applied to floats.
      **Open question**: allow it for `f64` (and let `math` expose the infinity constants), or is
      rejecting it a deliberate safety choice?

#### Ergonomics and gaps

- [ ] **R23. There is no way to print an array or a struct.** `[1 2 3] print` prints an empty line;
      `printv` gives `ptr:0x…`. Debug output for aggregates has to be hand-written per type.
      **Open question**: teach `print`/`printv` the array element type and the struct layout (both
      are known at compile time), or is this what `fmt` is for?

- [ ] **R24. `err` is global state, not a value.** Set by `panic`, cleared on read, survives
      intervening non-fallible calls (all verified). It cannot be stored, returned, wrapped, or
      chained, so there is no way to build "failed to open config: no such file". It is also the
      main reason R4's authors reach for `!`.
      **Open question**: subsumed by R3, or worth an independent error-value type first?

- [ ] **R25. `error { code = … message = … }` literal has near-zero use.** Listed in the grammar and
      §10.2 as the alternative to `msg code panic`.
      **Open question**: cut it now on the `>>field!` precedent (two spellings of one operation), or
      hold because R3 would replace both?

- [ ] **R26. Reference cycles leak, silently.** Verified with two `*Node` structs pointing at each
      other. Expected for refcounting and entirely defensible — but `specification.md` §11 does not
      mention it, and `struct Node { next:*Node }` is the spec's own example.
      **Open question**: document the limitation, add weak references, or add a cycle detector for
      debug builds? Documenting is nearly free and probably sufficient.

- [ ] **R27. Threads get a hardcoded 1,024-element stack.** `qd_create_context(1024)` in
      `stdlib/thread/src/thread.c:44` and `lib/rt/src/runtime.c:856`; `-s` does not reach it.
      Verified: a thread body that pushes 5,000 values dies with *"Stack overflow (use -s to
      increase stack size)"* — advice that does not work for this case. Thread contexts are properly
      isolated otherwise, which is the important part.
      **Open question**: plumb `-s` through, or make it a `thread::spawn` parameter?

- [ ] **R28. No comparator sort, and `hof` is `i64`-only — both downstream of R1.** `sort` exposes
      six monomorphic entry points (`ints`, `ints_desc`, `floats`, `floats_desc`, `strings`,
      `strings_desc`), all taking `(arr:ptr count:i64)`, and every combinator in `hof` — `apply`,
      `bi`, `tri`, `keep`, `dip`, `both` — is hardcoded to `fn(i64 -- i64)`. Recorded here so that
      when R1 lands there is a list of APIs to revisit rather than a fresh survey.
      **Open question**: none yet — this is a follow-on task, not a decision.

#### Cuts with corpus evidence

- [ ] **R29. `read` has zero bare uses and actively degrades checking.** It clears the type stack and
      sets `mHasUnpredictableStack` (`semantic_validator_instructions.cc:119`), pushing 16 synthetic
      `str` values because argc is unknown at compile time — so any function containing it loses its
      declared-effect check. `flag` and `os` cover the job properly.
      **Open question**: straight cut with a `REMOVED_INSTRUCTIONS` entry pointing at `flag`/`os`?

- [ ] **R30. Four more zero-use builtins.** Bare-use counts over the corpus, excluding `module::`
      hits: `free` 0 (all 120 hits are `mem::free`/`thread::free`), `printv` 0, `printsv` 0,
      `dec`/`--` 0. `wait` is also 0 bare, against `spawn` 4 and `detach` 5 — that trio duplicates
      the `thread` module and wants a decision about which layer owns threading.
      **Open question**: cut the four outright; separately, do the `spawn`/`detach`/`wait` builtins
      have a reason to exist alongside `thread::`?

- [ ] **R31. Supporting numbers for the `pick`/`roll` and live-shuffler items above.** The full
      census, so the two open items can be settled against one table: `-> ` locals **3,213**;
      `<<field` 1,154; `>>field` **12**; `drop` 63, `dup` 26, `swap` 18, `over` 8, `nip` 7, `rot` 5,
      `roll` 3, `dup2` 2, `pick` 1. Named binding outnumbers every shuffler combined by roughly
      46:1. Separately worth noting: `>>field`, one of the language's two custom sigils, has twelve
      uses in 18,918 lines — structs are read-mostly in practice.
      **Open question**: does the 46:1 ratio settle the live-shuffler item, or is frequency the
      wrong test for `swap`/`over`?

#### Positioning and scope

- [ ] **R32. The stack is no longer the programming model, and the documentation still says it is.**
      Given R31's ratios, real Quadrate is an ALGOL-family language with named parameters, named
      locals, structs and methods, that uses postfix syntax and a stack calling convention.
      `examples/dc/dc.qd` — a calculator, the most stack-shaped program there is — opens with
      `fn stack_push(s:ptr val:f64 -- )` over a hand-rolled `mem::alloc` array, because the
      language's stack is not where data lives. Meanwhile `learn/2-stack/` is four pages, more than
      functions and error handling together.
      **Open question**: is this a docs restructure (lead with functions and locals, demote the
      stack to "how calls work"), or is the concatenative framing something to keep pushing toward
      in the language itself?

- [ ] **R33. Supporting numbers for the scope-vs-depth note above.** `cmd/` is **20,828 lines,
      larger than `lib/llvmgen/` at 12,695** — ten CLI tools against one code generator. Of 2,064
      passing tests: 898 stdlib, 337 MCP server, 682 language, 25 C++ compiler tests. The whole
      `[Unreleased]` changelog is completions, `--help` layout, `quadrepl`, `quaddoc` and diagnostic
      prefixes — **no language changes except removals** — while the two `quadc` segfaults at the
      top of this file remain unbisectable for want of the doom sources.
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

### Language design / scope

- [x] **Cut `>>field!`.** Two forms for one operation, split purely on "does it leave the
      struct behind" — which `drop` already says. `<<`, `>>` and `as` remain; `>>!` is gone.

    43 call sites rewritten to `>>field drop` (12 in `lib/` + `examples/`, 31 in `tests/`).
    Verified the two forms produce byte-identical output before starting. The `noReturn` flag
    is removed from `AstNodeFieldSet`, all three parse sites (`ast_statements.cc`,
    `ast_expressions.cc`, `ast_types.cc`), both validator branches and `generator_structs.cc`,
    which now always pushes the struct back.

    Following the `while` and shuffler precedent, a use reports what happened rather than
    looking like a syntax error:

    ```
    '>>field!' has been removed; it only differed from '>>field' by discarding the struct,
    so write '>>field drop' instead
    ```

    The diagnostic is load-bearing, not a courtesy: `parseSimpleToken`'s `'!'` branch returns
    nullptr without reporting, so a bare trailing `!` would otherwise be silently dropped and
    `p 42 >>x!` would compile as `p 42 >>x` — leaving the struct on the stack and changing the
    program's meaning rather than rejecting it.

    Also touched: 4 doc files (11 code examples plus the prose in `dc-walkthrough.md`
    explaining the `!` suffix), `StructFieldSetNoReturn` in both
    `test_semantic_validator_extended.cc` and `test_llvmgen.cc` (renamed and rewritten, with a
    third test pinning that `>>x!` is now rejected), and
    `tests/qd/structs/struct_field_write_noreturn` renamed to `..._discard`. Regression test
    `tests/qd/compile_errors/removed_field_set_bang`. Suite 2033 passed, 0 failed; `docscheck`
    218 blocks clean.

- [x] **Cut `ctx`.** Zero corpus uses, ~340 lines of implementation, and a static checker that
      could not model it. 22 keywords → 21.

    Removed: the parse site in `ast_statements.cc`, `ast_node_ctx.h` (deleted) and its
    `CTX_STATEMENT` enum member plus seven stale `#include`s, the validator's typecheck case,
    `generateCtxBlock` (81 lines in `generator_control.cc`), both codegen dispatch sites, the
    `generator_impl.h` declaration, and the then-orphaned `cloneContextFn`. Eight `.qd` tests with
    their `.out` siblings, the `CtxStatement` llvmgen test, the 253-line
    `learn/7-advanced/context.md` and its mkdocs nav entry (`defer.md`'s "What's next?" repointed at
    function-pointers), `keywords.md`, `specification.md`, `reference.def`, the playground keyword
    list and the pygments lexer.

    **Kept deliberately, diverging from how the shufflers were handled:**
    - `qd_clone_context` stays in the runtime. Unlike `qd_tuck`, it is an advertised embedding API
      (`docs/docs/embedding.md:675`, "Deep copy a context") with plausible standalone use, so
      deleting it is a separate ABI decision this task does not imply. Its doc comment claimed
      "This is used by the ctx keyword" — corrected.
    - `ctx` stays in `isReservedKeyword`. `while` is not, but the parser diagnostic fires on any
      `ctx` in a function body, so permitting `-> ctx` as a variable would only produce a more
      confusing error at the use site. `ctx` also stays in `synchronize()`'s recovery list, which
      does match the `while` precedent.

    **The diagnostic skips the block rather than calling `synchronize()`.** `synchronize()` stops at
    the `ctx` body's own `}`, which then closes the enclosing function — turning one error into four,
    three of them bogus "unexpected identifier at top level". Since `ctx { … }` is brace-balanced,
    consuming it exactly leaves the function intact and yields a single error:

    ```
    'ctx' has been removed; the block's values were appended to the parent stack anyway,
    so write the body inline
    ```

    **Four sources described `ctx` four different ways** — which is the "semantics are non-obvious"
    strike, caught concretely: `ast_node_ctx.h` said "exactly one value is returned to the parent";
    `reference.def` said "results are appended to parent stack"; `keywords.md`'s table said "Context
    variable access" (not even close) while its body said "cannot modify parent variables"; and the
    validator ignored the body entirely, pushing exactly one `INT` unconditionally. That last one is
    the documented net-zero-body bug, and it was three lines of code.

- [x] **Cut the eight zero-use stack shufflers** — `dupd`, `swapd`, `swap2`, `drop2`, `over2`,
      `overd`, `nipd`, `tuck`. Removed from the instruction table, the validator's type rules, the
      compile-time-stack codegen, and the runtime (`qd_dupd` … `qd_tuck` are gone from
      `runtime_stack.c` and `runtime.h` — **an `libqdrt.so` ABI break**, safe because nothing emitted
      calls to them and the build cache keys on compiler identity).

    Counts, measured rather than the "81 builtins" this file previously asserted:
    `BUILTIN_INSTRUCTIONS` went 95 → 87 entries, of which the user-facing word-named ones went
    65 → 57 (the rest are 13 symbol operators and 17 `__`-prefixed freestanding internals).
    The documented surface in `reference.def` went 76 → 68 `BUILTIN(...)` entries, plus one
    `KEYWORD(...)` for `ctx` (22 → 21).

    Following the `while` precedent in `parseBlockStatement` (`ast_expressions.cc`), use now reports what happened rather
    than a generic "undefined identifier", and carries the old stack effect because that is what a
    reader porting old code needs:

    ```
    'tuck' has been removed (it was ( a b -- b a b )); bind the values with named locals ('-> a -> b') instead
    ```

    A `REMOVED_INSTRUCTIONS` table in `instructions.h` holds the eight names and their effects;
    `semantic_validator_collect.cc` consults it just before the undefined-identifier fallback.

    **The scope was wider than "zero uses" suggested.** That count was over `lib/` + `examples/`;
    `tests/` had seven dedicated `.qd` tests (`stack/{dupd,swapd,nipd,overd}.qd` deleted,
    `stack/{pairs,advanced}.qd` and `documentation/stack_notation.qd` trimmed) and twelve C runtime
    tests. Also touched: `reference.def` (the source `gen_docs.sh` generates `reference.md` from),
    `builtins.json`, the pygments lexer, quadrepl and quadlsp completion lists, quadmcp's two help
    texts, and the playground's quick reference — 24 files.

    Regression test `tests/qd/compile_errors/removed_stack_shufflers` with its `.err` sibling;
    `TuckValid` in `test_semantic_validator_extended.cc` flipped to `RemovedShufflerRejected`.

    **Two doc bugs surfaced, both in the dead builtins.** `docs/api/builtins.json` had `overd` as
    `(a b c -- a b c a)` and `runtime.h` had `qd_dupd` as `( a b -- a b a )`; the runtime does
    `( a b c -- a b a c )` and `( a b -- a a b )`. Wrong signatures had been published for builtins
    nobody used, and nothing caught it — `docscheck` can't, since these are table rows rather than
    fenced blocks. Evidence for the removal, not against it.

    Regenerating `reference.md` also picked up `lnot`, added earlier but never regenerated. Left in.
    Unrelated drift in `os`/`rand`/`signal` docs was reverted to keep the change focused — note
    `gen_docs.sh` drops the `<!-- doccheck: compile-only -->` directive from `signal.md`, so
    regenerating it breaks `docscheck`. Pre-existing bug, not fixed here.

### Interpreter tier (lib/interp)

- [x] **The six remaining gaps in the AST walker are closed: `return`, `const`/`enum`, named
      locals, `for`, `cast<T>` and array literals.** Each had been probed construct by construct;
      all six landed together because four of them turned out to be one feature.

    **Named locals were the load-bearing one.** `qd_interp` grows a `std::vector<Frame>`, one
    frame per active call, and `-> x` binds into the innermost. `for`'s iterator is an ordinary
    entry in the same frame (saved and restored around the loop, as `iteratorVars` is in
    `generateFor`), so implementing locals implemented most of `for` with it. The top-level frame
    persists across evaluations, the way the stack already does — a prompt that forgot its names
    between lines would be the wrong shape for what this tier is for.

    **The tier had been binding parameters wrongly, and the tests recorded it.** A signature whose
    inputs are *all* named binds them on entry and takes them off the stack —
    `semantic_validator_typecheck.cc` states the rule and the compiled tier follows it, so
    `fn double(x:i64 -- r:i64) { 2 * }` is a stack-underflow *error* in a real build. Eleven test
    declarations spelled it that way and passed here; they now read `{ x 2 * }` and mean the same
    thing in both tiers. Mixed or unnamed inputs still stay on the stack, which is the exemption
    the validator makes.

    **Arrays needed reference counting, not just construction.** `len` and `nth` each consume the
    reference the stack carries, so a literal bound with `-> a` and read twice was freed under the
    second read — `a 1 nth` returned garbage from freed memory. Reading a pointer local now
    retains first and a frame releases what it holds when it is dropped, which is precisely what
    `generateIdentifier` and `generateLocalCleanup` do. The interpreter now ends its own test run
    with zero bytes in use.

    **`const` was the worst failure of the set and the smallest fix**: it parsed, and then the name
    reported as undefined, which reads like a typo rather than a missing feature. Constants and
    enum variants share one table keyed by the written form, so `Colour::Red` resolves as a value
    *and* as a `switch` case label — the latter matching `compareAgainstNamedConstant`, including
    its refusal to let a label that resolves to nothing quietly never match.

    Also: `evalSwitch` was leaking the string it popped as its subject. `qd_interp_destroy` now
    drops its frames rather than leaving what they own to the allocator.

    Fourteen tests added (`Return`, `Constants`, `Enums`, `NamedLocals`, `NamedParameters`,
    `ForLoops`, `Casts`, `ArrayLiterals` among them); 754 assertions pass under valgrind with no
    errors and nothing in use at exit. Full suite 2064 passed, 0 failed.

    **Writing `for` here found a compiler bug**, since a float loop worked in this tier and hung
    in a compiled build. Fixed below rather than matched.

    **`-> a b c` is not a thing.** `AstNodeLocal` carries a name list and the compiled tier loops
    over it, but both parse sites build a one-element vector and the syntax is a single name —
    so the multiple-binding spelling is dead in the parser. The walk handles the list anyway,
    because the node can express it; `-> a -> b -> c` is what a program writes.

### Compiler & runtime

- [x] FIXED — **A `for` loop with float bounds never terminated.** `0.0 2.0 0.5 for x { … }`
      compiled clean and hung, printing the iterator as 0 forever. `generateFor`'s runtime-stack
      path converted start, end and step with `CreateFPToSI` before building the loop, so a step
      of 0.5 became 0. Only functions that miss `analyzeIsBodyNativeEligible` — `main` among them —
      take that path; the compile-time-stack path already had a float branch and was correct.

    **The type is not known at compile time on that path, which is the whole difficulty.** The
    bounds arrive as `qd_stack_element_t`s carrying a runtime tag, so there is no IR type to
    branch on. The loop therefore carries *both* iterators — an i64 PHI and a double PHI, each
    advanced by its own step — and selects between them with the tag: the condition is
    `select(isFloatLoop, floatCmp, intCmp)` and the body reads whichever the same flag picks. The
    start element decides, matching the compile-time path and the interpreter. LLVM deletes the
    dead half whenever the tag folds to a constant, so an integer loop is unchanged in the
    optimised output.

    **Reading the iterator had to become type-aware too**, or a float loop would push its value
    under an INT tag. `generateInlinePushIntValue` and `generateInlinePushFloatValue` differed only
    in the tag they stored, so both now delegate to one `generateInlinePushTaggedValue` that takes
    the tag as a value; the iterator passes a `select` of 0 and 1. No branch, one extra select per
    read, and 40 lines of duplication gone.

    **A second bug fell out of the same code.** All three bounds were converted according to the
    *start's* tag, so `0.0 10 1 for` read the integer end and step as the bits of a double —
    garbage. Each bound is now read in both domains from its own tag, so mixed bounds mean what
    they look like.

    Regression test `tests/qd/control_flow/for_loop_float_bounds` covers ascending and descending
    fractional steps, mixed bounds, an iterator bound with `->`, an integer loop nested in a float
    one sharing the name, and a native-eligible function for the other path. Verified red: on the
    unfixed compiler it prints 0 forever. Its output is byte-identical to the same programs under
    `lib/interp`. Full suite 2064 passed, 0 failed; `docscheck` 218 blocks clean.

- [x] **`switch` in the interpreter tier.** Matches integers, floats, strings and bools against
      literal cases, with `_` as the default, and takes its value off the stack the way `if` takes
      its condition. The compiled tier has one further rule — where the value is the status of a
      user-defined fallible call, `Ok` means "no error" rather than "equals 1" — which is
      inexpressible here, this tier having no fallible calls, so the two cannot disagree.

    Also fixed `parseSwitchStatement`, which stopped at the first comment between two cases. A
    dispatch table is the one place that most wants a comment per line, so annotating one made it
    fail to parse; it affected the compiled tier equally.

- [x] FIXED — **`return` was a silent no-op in `main`.** Not just inside an `if` — at the top
      level too. Found while restructuring `wc.qd` for the branch-arity work below.

    **Cause.** Every other function-generation path assigns `currentFunctionReturnBlock`, but
    main's did not: `generateFunction`'s `isMain` branch created its `returnBB` and used it as the
    fall-through target without ever setting the member. `generateNode`'s `RETURN_STATEMENT` case
    is guarded on that being non-null, so it emitted nothing at all. One assignment (plus clearing
    it at the end of the branch, as the other paths do) fixes it.

    Worth noting what the silence cost beyond the obvious: reaching `returnBB` is also what runs
    main's defers and local cleanup, so an early `return` skipped those too — and the semantic
    validator models `return` as diverging, so a `main` written with guard clauses validated
    clean and then did the wrong thing at runtime.

    Regression test `tests/qd/control_flow/return_in_main` covers a top-level return, a guard
    whose condition is false, a return out of a `for` loop, defer execution on the early path, and
    a non-main function for contrast. Verified red by reverting just the assignment: the loop runs
    to completion and `unreachable` prints.

    **The reference documented the bug as a language limitation, and over-generalised it.**
    `keywords.md` claimed *"`return` only works at the function body's top level. It cannot be
    used inside `if`, `else`, `loop`, or other blocks."* That was never true outside `main` —
    verified `return` working from inside `if`, `for`, `loop` and `switch` arms in ordinary
    functions, on the pre-fix build. Replaced with an accurate description plus a guard-clause
    example that `make docscheck` compiles and runs.

- [x] FIXED — **Branch stack effects are unified, and the diagnostic is now an error.** Both
      halves landed together, so codegen and the validator agree instead of one warning while the
      other silently discarded:

    - **Validator** (`semantic_validator_typecheck.cc`, IF_STATEMENT case): mismatched arms are
      rejected. The message now says what the rule is rather than describing the old miscompile:
      *"both arms must leave the same number, since whatever follows reads a value whose identity
      would otherwise depend on which arm ran"*.
    - **Codegen** (`generateIf`, `generator_control.cc`): unequal arms set `compilationFailed`
      with an internal-error diagnostic. The `std::min` merge stays, because bottom alignment is
      correct — the arms share the pre-`if` stack as a prefix — but it is no longer reached
      silently. What was wrong was the silence, not the alignment.

    Exemptions unchanged and still load-bearing: a diverging arm (`return`/`panic`/`break`/
    `continue`) contributes no effect, and an `if` directly after a fallible call is skipped
    because the success arm receives the call's result and the failure arm does not.

    **The two `wc.qd` sites turned out to be false positives, not benign mismatches — and finding
    that is what made the flip safe.** `f flag::destroy` is net zero at runtime (verified with
    `depth`), but the validator modelled it as +1, so *any* module-qualified method call on a
    receiver inside one arm produced a spurious mismatch. Root cause: `mHasUnpredictableStack` —
    set when a function calls a variadic, an imported C function, or `flag::parse`, meaning the
    validator has admitted its stack model is unreliable — is consulted by the function-level
    arity check (`:628`) and the defer-effect check (`:1049`), but this diagnostic ignored it.
    It now gates on the same flag. `wc.qd` needed no change and is untouched; had this been
    promoted to an error without the gate, it would have rejected correct programs.

    Acceptance test restored as `tests/qd/compile_errors/if_branch_arity_mismatch` with its `.err`
    sibling. Spec gains **§6.1.1 Branch stack effects**, stating the rule normatively with both
    exemptions and an `expect-error` example that `make docscheck` compiles. Re-swept after the
    flip: 38 stdlib modules and 24 example programs, zero errors and zero warnings. Full suite:
    2024 passed, 0 failed.

- [x] FIXED — **Module bodies were never semantically validated when imported.** A program doing
      `use sb` compiled clean even when `sb.qd`'s body called a function that does not exist, so
      every stdlib and third-party module body went unchecked during a normal build. The module
      loop in `main.cc` did call `validate(..., isModuleFile=true, ...)`, but under
      `if (!fromCache)` — and the validator parses module files to collect their signatures and
      hands those ASTs to `AstCache` via `importFromValidator`, so by the time the loop ran every
      module was a cache hit and validation was skipped for all of them. Four parts:

    1. **`CachedAst` gains a `validated` flag**, and `getOrParse`'s `outFromCache` becomes
       `outAlreadyValidated`. Presence in the cache means parsed, never checked; the loop now marks
       entries validated after they pass.
    2. **Files in the main file's own directory are excluded** (the first of the two designs the
       item proposed). They are directory-namespace siblings already owned by the main-file pass,
       and only that pass has the context to resolve them — `getSiblingQdFiles` deliberately skips
       the file containing `main()`, so a sibling referencing a constant defined there cannot be
       resolved from the module loop at all.
    3. **Intra-module references resolve via the module's own file list**, not
       `getSiblingQdFiles` — that function scans a directory for the directory-namespace feature
       and returns nothing under `lib/` or `tests/`, i.e. for every module that matters here. Files
       already linked by an explicit `use "other.qd"` in either direction are excluded, or every
       function in them is reported as a duplicate definition.
    4. **Unqualified references inside a module file now resolve against the current package's
       module functions.** Functions pulled in by an intra-module `use "helper.qd"` land in
       `mModuleFunctions` under the package, not in `mDefinedFunctions`, so referencing them
       without a prefix — which is the documented behaviour inside a module — was rejected. Guarded
       on `mIsModuleFile` so a main file cannot silently reach into a module.

    Regression test `tests/qd/compile_errors/module_body_unvalidated` with its
    `unchecked_module_body/` helper module, whose uncalled function references an undefined name.
    Full suite: 2022 passed, 0 failed — the same count as before the change.

    **It immediately found a real stdlib bug.** `crypto`'s `build_hmac_input` declared
    `-- result:ptr result_len:i64` while its body ends `total result`, pushing the length first and
    the pointer on top. Every caller binds `-> buf -> len`, matching the body, so HMAC was correct
    at runtime — the *signature* was backwards, and the type checker believed it, flagging four
    `sha256_bytes`/`sha512_bytes` calls. Fixed by correcting the declaration to match the body
    (no runtime change); verified `hmac_sha256` and `hmac_sha512` still match `openssl dgst` exactly.

- [x] FIXED — **The JIT could pair new codegen with an old runtime.** `quad run` resolved
      `libqdrt.so` through the loader's search path first, so an installed `/usr/lib/libqdrt.so`
      from an older release won over the one the binary was built with — reading struct fields at
      offsets the old ABI doesn't have, and presenting as "my compiler change had no effect" while
      `quad build` produced a correct binary. The order is now explicit override
      (`QUADRATE_LIBDIR`) → the runtime beside the executable (`dist/bin/quad` → `dist/lib`, then a
      flat layout) → the loader search path as a last resort.

- [x] FIXED — **`create_test_context` in `lib/rt/tests` and `stdlib/mem/tests` hand-rolled the
      context**, mallocing it and initialising only `->st`, so any test that tripped a fatal
      runtime path walked an uninitialised call stack and died of SIGSEGV instead of the SIGABRT it
      was asserting. Both now go through `qd_create_context`/`qd_free_context` like the other
      thirteen test files, and the workaround comment on the death tests is gone.

- [x] Reaped the dead `while` implementation — the keyword has been rejected at parse time for a
      while, but `AstNodeWhileStatement`, `generateWhile` (~225 lines in `generator_control.cc`),
      the `WHILE_STATEMENT` branches in `semantic_validator_{collect,typecheck}.cc`, the codegen
      dispatch case, the LSP folding case, the enum member and seven `#include`s were all still
      carried. The parse-time diagnostic and `while`'s entry in the parser's `synchronize()`
      keyword list stay — the latter is error recovery, not an implementation.

- [x] `panic` with code 0 reported success — sentinel collision between the panic code and
      `error_code`'s "no error" value. Added `int64_t has_error` to `qd_context`; codegen now tests
      `has_error != 0 || error_code != 0` via `generateClearErrorState`/`generateReadErrorState`
      (7 sites). `tests/qd/errors/panic_code_zero`.
- [x] Fallible calls used two incompatible protocols — FFI-imported functions pushed the real error
      code, user-defined ones pushed only 1/0, so `switch { Ok … }` always hit `_` for user code and
      `if`/`else` crashed for FFI code. Codegen now shapes the status for whichever consumer reads it
      (`fallibleConsumerOf`): boolean before `if`, error code before `switch`. `Ok` is generated as a
      test of error *state*, so a `panic` carrying code 1 still reads as failure.
      `tests/qd/errors/fallible_protocol_unified`; spec §10.3 states both shapes normatively.
- [x] Fallible-call arity invisible in the signature — folded into the protocol unification above.
- [x] Runtime stack-underflow handling was inconsistent — `drop` was fatal but `print` returned `-2`
      silently, and codegen discards those return values. 11 genuinely silent functions found (not the
      91 first claimed — that grep missed ops that hand-roll the fatal sequence). All instruction-level
      stack failures are now fatal, 61 sites. The embedding API (`qd_push_i`, `qd_pop_s`, …) and
      `qd_spawn`'s allocation/thread failures deliberately keep returning codes; both conventions are
      documented at the top of `runtime.h`. Two tests were pinning the old behaviour, one of which hid
      a real bug in `tests/embed/native-functions-test.cc`.
- [x] `defer` ignored the control flow it was written under — registration was lexical, so a `defer`
      in an untaken branch still ran, and a `defer` after a `?` ran on the propagate path with its
      local unbound. Each `DeferEntry` now carries an i1 `reached` alloca created in the entry block;
      scope exit emits `if (reached) { body }`. Six emission sites collapsed into `emitDeferScope`.
      `tests/qd/control_flow/defer_registration`; spec gains §6.6.1 Registration.
- [x] `switch` had no native-path codegen — two defects: `generateSwitchStatement` popped the
      scrutinee off the runtime stack unconditionally (now takes it from `compileTimeStack`, with PHI
      reconciliation at the merge block), and bare `const` case labels resolved to nothing, leaving
      the block unterminated → invalid IR (now an `IDENTIFIER` branch sharing
      `compareAgainstNamedConstant`, plus a hard diagnostic when a label resolves to nothing).
      Four tests in `tests/qd/control_flow/switch_value_native_*`.
- [x] `spawn`/`wait`/`detach` had no type model in the validator, so `&worker spawn` left a `ptr` on
      the type stack while the runtime pushed an `i64` handle — thread handles couldn't be stored in
      an array. Branches added matching the runtime. Not added: a check that the spawned function
      consumes nothing (`mPendingFnSignature` has subtle lifetime rules).
      `tests/qd/threading/spawn_handle_type`.
- [x] Build cache did not include compiler identity — `computeKey` hashed only sources and a few
      options, so a rebuilt `quadc` kept serving executables from the old one (7670 stale entries).
      `BuildCache::addCompilerIdentity()` mixes in `QUADRATE_VERSION`, `QUADRATE_GIT_COMMIT`, and the
      size + mtime of `/proc/self/exe`.
- [x] `shr` was arithmetic in the constant folder and logical everywhere else — the folder now emits
      `CreateLShr`, so all four paths agree. `tests/qd/bitwise/shr_logical_all_paths`. A separate
      `sar` for arithmetic shift is a language addition, deliberately left out.
- [x] `not` is bitwise and silently wrong for boolean use (`flag not and other` — `~0 = -1`, and
      `-1 and X = X`). Added a new `lnot` builtin (`x == 0 ? 1 : 0`) and left `not` untouched, wired
      through every path. `tests/qd/bitwise/lnot_logical_negation`.
- [x] Parser silently accepted an unbalanced `}` in a function body, closing the function early and
      compiling the truncated result cleanly. The top-level loop's `default:` case discarded
      unrecognised tokens; now diagnoses "Unmatched '}' at top level".
      `tests/qd/compile_errors/stray_close_brace_module_scope`.
- [x] `//` comment on a struct field line was a parse error, and cascaded to every following field.
      The struct-declaration field loop was the only one of three not calling `parseComment`.
      `tests/qd/structs/field_trailing_comment`.
- [x] Formatter deleted comments inside struct/enum declaration bodies. Both declaration nodes gain a
      `BodyComment` list, kept out of `child()`/`childCount()` so generic tree walkers are unaffected.
      `tests/formatter/51_declaration_body_comments`.
- [x] `<` operator vs generic ambiguity — generics now require `<` immediately adjacent, so `Vec3<T>`
      is generic and `COUNT < x` is comparison.
- [x] Method dispatch silently failed when the method name collided with a builtin instruction
      (`depth`, `clear`, `len`) — the instruction path built the function name without the module
      prefix. Now resolves via `userFunctions` like the identifier path.
- [x] Runtime `read` inferred types on argv and pushed numeric-looking args as i64, crashing
      `flag::parse`. `qd_read` now always pushes strings, matching what the validator already declared.
- [x] Sized integer types reconciled with the spec — §3.1 rewritten (stack values are 64-bit; narrower
      widths are memory-layout annotations only), new §3.1.1 covering the width table, store
      truncation, load widening, packed vs 8-byte-slot layout, and the `u64`-above-2^63 caveat.
      `packed` documented in §5.6/§8.1 and added to the grammar; §2.3.1's keyword list corrected.
      Recorded as a known divergence: sized types are accepted in positions where they're inert
      (`300 cast<u8>` yields `300`; parameters and returns ignore the width).

### Documentation

- [x] Freestanding subset documented — `docs/docs/learn/7-advanced/freestanding.md`, in the nav
      after FFI. Covers what changes versus a hosted build, the `_start` entry contract, the allowed
      modules (`bits`, `limits`, `mem`, `sys`) and the `mem.c` / `mem_heap.c` split, the rejected
      builtins, the whole `sys` surface (raw `st*`/`ld*`, x86 port I/O, `cli`/`sti`/`hlt`), what does
      *not* work (division and modulo halt via `qd_div`/`qd_mod`; strings are a stub; no error
      reporting at all), overriding the weak `qd_freestanding_halt` and `QD_FREESTANDING_STACK_CAP`,
      and the `boot.S` + `linker.ld` + Makefile pattern. Every claim was checked against a real
      `--freestanding` compile rather than taken from the source comments — including confirming
      that a `/` really does emit a reference to `qd_div`. Note the page describes the *current*
      `examples/kernel/`, which has moved well past the VGA hello-world this item was written for:
      it is now x86_64 long mode with GDT, IDT, PIC remapping, PIT timer and PS/2 keyboard.

- [x] `make docscheck` — `tools/check_docs.py` extracts fenced `quadrate`/`qd` blocks, compiles each,
      and runs the complete programs; wired into all three `.builds/*.yml` after `fmtcheck`. 224
      blocks compile and run clean; 21 were failing when it first ran. The other 1118 blocks are
      fragments with no sound way to synthesise context, so this is a floor. Opt out with
      `// doccheck: skip|compile-only|expect-error <reason>`.
- [x] Error-handling docs fixed by hand (spec §10.1–§10.6, `reference/errors.md`,
      `learn/6-error-handling/patterns.md`): the `drop`s in error arms are gone (both `if` and
      `switch` consume the status), §10.1's `divide` never compiled, §10.2 now states that `panic`
      reports failure whatever code it carries including 0, and §10.6 adds a complete worked example
      covered by `docscheck`. The stale caveat about FFI-only `switch` matching is deleted.
- [x] Doc examples calling `-> x` on an already-bound parameter — swept across `reference/errors.md`,
      `types.md`, `keywords.md` and spec §4.4, which now states that named parameters are bound on
      entry and MUST NOT be re-bound.
- [x] Other bugs `docscheck` found: `sb.data`/`pool.used` dot syntax that doesn't exist
      (`learn/7-advanced/memory.md`), `make<ptr>` where `make<Point>` was meant (`structs.md`).
- [x] Getting-started guide, "Thinking in stack-based" tutorial
      (`learn/2-stack/thinking-in-stack.md`), and the annotated `dc-walkthrough.md`.
- [x] All 36 stdlib modules documented; cross-module `Calls:`/`Called by:` links in quaddoc output via
      a `buildCallGraph` pass; quadmcp's module list generated from `docs/api/modules.json` instead of
      four hardcoded chains (36 modules vs 25).

### CI / quality

- [x] The release build was broken and CI could not have been passing. Two failures at `-O3 -Werror`:
      a genuine null-deref in `lib/qc/src/ast_parse.h:386` (`expandAllStringInterpolations`
      dereferenced `block->child(i-1)` unchecked), and `-Wnull-dereference` firing inside LLVM's own
      inlined internals, now scoped to `-Wno-error=null-dereference` on the `llvmgen` target only.
      Worth checking what GCC/LLVM the CI images pin.
- [x] `fmtcheck` target (`quadfmt -c lib examples`) wired into all three `.builds/*.yml`; `tests/` is
      excluded because `tests/formatter/` inputs are intentionally unformatted.
- [x] Formatter fuzzer (`tests/fuzz/fuzz_formatter.cc`, crash-freedom + idempotency) and LSP fuzzer
      (`fuzz_lsp_text.cc`, 200K iterations clean; `lspGetWordAtPosition` extracted into
      `cmd/quadlsp/src/lsp_text.cc`). Also fixed `tests/fuzz/meson.build`, whose `fuzz_parser` target
      had been broken since the AST split. A full JSON-RPC LSP fuzzer is still pending —
      `handleMessage` is stateful with many side effects.
- [x] Two formatter idempotency bugs — `findBlockEndLine` falling back to `startLine` on unbalanced
      braces, and the inline-body extractor using `rfind('}')` instead of the matching close brace.
      Both `emitBlockBody` paths now track brace depth.
- [x] Function-entry coverage for `quad test` — `--coverage` on `quad test` and `quadc --test`,
      instrumenting every user function in the main module with `qd_coverage_mark(idx)`. Stdlib
      functions excluded.
- [x] Dependency conflict detection across the graph — `rangesHaveCommonVersion` in
      `cmd/quadpm/src/semver.cc` probes boundary candidates, replacing the caret-vs-caret-only check.
      Catches `^1 vs ^2`, `~1.2 vs ~1.3`, `>=2 vs <1.5`, `=1.2.3 vs =1.2.4`, three-way unsatisfiable.

### Freestanding mode (kernels, embedded, no-OS targets)

Goal: compile a Quadrate program with no libc / hosted-OS dependency.

- [x] `quadc --freestanding` — skips the auto-emitted `int main(int, char**)` wrapper, accepts
      `pub fn _start( -- )` (or `main`) as the entry, emits a `void _start(void)` shim calling the
      user fn with a runtime-provided static `qd_context`, then `qd_freestanding_halt()`. Output is
      `.o`, so the user runs their own linker.
- [x] `libqdrt-freestanding.a` (`lib/rt/src/freestanding.c`) — statically allocated stack
      (`QD_FREESTANDING_STACK_CAP`, default 1024), `qd_freestanding_ctx` global, weak
      `qd_freestanding_halt` (`cli; hlt` on x86, `wfi` on ARM). Stubs for call-stack tracking,
      `free`/retain/release, closures, and inline arithmetic/comparison/bitwise stack ops.
      Division/modulo halt rather than pulling in libgcc's `__divdi3` on 32-bit.
- [x] Validator subset enforcement — `setFreestandingMode(bool)` rejects `use <module>` for anything
      but `bits`, `limits`, `mem`, and rejects `print`/`prints`/`printv`/`printsv`/`nl`/`read`/
      `panic`/`err`/`spawn`/`wait`/`detach`. User `.qd` imports are still allowed.
- [x] Freestanding-safe `mem` — split into `mem.c` (raw ops) and `mem_heap.c` (alloc family), no
      `#ifdef`s; the build picks sources. `libmem-freestanding.a` = `mem.c` only.
- [x] Raw memory builtins `st8`/`st16`/`st32`/`st64` and `ld8`/`ld16`/`ld32`/`ld64`, lowering directly
      to LLVM `store`/`load`. No runtime call, no libc, allowed in `--freestanding` — a kernel can
      touch MMIO without an FFI shim.
- [x] `examples/kernel/` — "Hello, Quadrate kernel!" to VGA at 0xB8000 from pure Quadrate (the `vga.c`
      shim was removed once `st16` landed). `kernel.qd`, `boot.S` (multiboot1), `linker.ld`,
      `grub.cfg`, `Makefile` (`make` → 33KB i386 multiboot ELF, `make iso`, `make run`).

### Examples

- [x] `examples/csvcut/csvcut.qd` — `cut`-like CSV column extractor with RFC-4180-ish quote handling,
      file or stdin. Demonstrates manual `argv` parsing, `sb::StringBuilder`, byte scanning.
- [x] `examples/wc/wc.qd` — `wc(1)` clone; output matches GNU wc.
- [x] `links-api` — HTTP JSON API example, including `http::request_body(c)` (named to avoid colliding
      with client-side `http::body(req, body)`), demonstrated by its POST /echo endpoint.

### Earlier

- [x] Embedding API: `qd_pop_i`/`qd_pop_f`/`qd_pop_s`/`qd_pop_p`, `qd_load_file`,
      `qd_error_code`/`qd_error_message`/`qd_clear_error`, `qd_context_stack_size`; go-quadrate and
      python-quadrate bindings, C embed tests, FFI example and docs updated.
- [x] Build cache — ~200x speedup on repeat builds, 16 integration tests.
- [x] Linter `//nolint` inline suppression, opt-in rule tests, `.flags` test runner.
- [x] Package manager: smart semver-based `update`.
- [x] Formatter: removed dead `--line-width`, brace-in-comment fix, blank-line preservation, struct
      default-value preservation, inline struct construction detection, import-block doc comment
      preservation, `--` rule for void functions and receiver-only methods, idempotency.
- [x] LSP: `--` separator conditional on inputs/outputs across 11 signature sites.
- [x] Formatted the entire stdlib (48 files) and all examples (29 files).
- [x] Replaced raw `new`/`delete` with `std::unique_ptr` — 17 container classes, zero leaks in 345K+
      fuzz runs.
- [x] Split `parseFunctionDeclaration` (~990 lines of duplication removed); split `ast.cc` into 5
      compilation units.
- [x] Fixed cross-module method call resolution (3 compiler bugs).
- [x] Typed function pointers with type aliases; `flag` and `regex` converted to method syntax.
- [x] Pre-built binaries via Docker (`make docker-x64`, `make docker-arm64`).
- [x] Playground with quick reference, share, format, auto-save.
- [x] Fixed the 5 broken benchmarks.

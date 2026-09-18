# TODO

## Open

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

Where an item restates something already open above (`R3`, parts of `R23`), it is kept only for the
new evidence, not as a second copy of the task.

*Amended 2026-09-17*: R2 corrected (its headline claim was wrong); R34 and R35 added and fixed
the same day, R34 taking R6 with it; R36–R38 found while fixing them. R1, R2 and R28 are done — see Done.

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
candidates did not, and R29 and R30 were withdrawn because of it.

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

- [ ] **R4. 158 abort-on-error call sites inside the standard library.** *(Corrected 2026-09-18:
      "315, of which 263 in stdlib" came from `grep -E '[a-z_]!'`, which counts the `!` in a
      fallible **declaration** — `pub fn substring(…)!` — as a call site; stdlib has 145 of those.
      `thread` 30 and `os` 19 were wrong outright: neither module is in the top ten.)* Re-measured
      with a tokenizer that skips declarations, strings and comments: **158 in `stdlib`, 56 in
      `examples`**. Heaviest: `json` 38, `uri` 28, `regex` 18, `path` 18, `hex` 11, `uuid` 10,
      `crypto` 10, `fuzzy` 9. `json.qd` calls `strings::char_at!`
      20+ times while scanning *untrusted input*, and line 376 already carries the comment
      *"`!` aborted here, which killed the process on untrusted input"* against a site that was
      converted to a `switch`. A library that aborts the host process on malformed input is not
      shippable, and this reads as the design pushing authors there — `?` requires the enclosing
      function to be fallible, and threading a bare `(code, msg)` through recursive descent is
      miserable — rather than as carelessness.
      **Open question**: is this a stdlib-hygiene sweep to do now (convert library `!` to `?`), or
      does it wait for R3 because the sweep would otherwise be done twice?

#### What the compiler does and does not check

- [ ] **R9. `docscheck` covers 16% of the documented examples.** 219 blocks checked and passing,
      1,120 skipped as "not programs", out of 1,337 fenced Quadrate blocks (218/1,117/1,335 at the
      review; the three added are this session's). The floor is honestly
      scoped and documented in `tools/check_docs.py` — but the front-page example is in the 84%:
      `fn double(x:i64 -- result:i64) { 2 * }` in `about.md` does not compile (R10).
      **Open question**: can the harness synthesise a `fn main` wrapper for fragment blocks and lift
      coverage materially, or is the current floor the right stopping point and the fix is only to
      hand-audit the handful of blocks that are whole declarations?

- [ ] **R10. 92 test files have no asserted output.** Re-verified 2026-09-18: of 803 `.qd` files
      under `tests/qd`, 549 have a `.out` sibling and 162 a `.err`; **92 have neither**, so they
      compile and run with nothing pinned. `run_all.sh` reports 89 skips, the difference being
      skips for other reasons (an unavailable external module, a blocking server).
      **Open question**: generate the missing `.out` siblings from current behaviour and review the
      diff, or are these deliberately output-free?

- [ ] **R39. A lambda cannot appear inline in a struct literal.** `H { f = fn (x:i64 -- r:i64) {
      x 2 * } }` is rejected by the struct-literal field parser with *"Use '=' instead of ':' for
      struct field initializers"* — it reads the lambda's `x:i64` as the start of another field.
      The working form is to bind it first (`fn (…) {…} -> cb  H { f = cb }`), which is what the
      corpus does. Found while adding the construction-time check in R2; predates it.
      **Open question**: teach the field-initializer parser to recognise `fn (` and parse a
      lambda, or leave it and say so in the docs? Same family as R19 (nested array literals and
      struct literals inside array literals do not parse either) — probably one fix to the
      "what can appear as a value here" question rather than three.


- [x] **R40 — the truncated signatures were `gen_docs.sh`, not `quaddoc`.** The renderer was never
      at fault: `docs/gen_docs.sh` parsed each declaration with `\(([^)]*)\)`, whose `[^)]*` stops
      at the *first* `)` — which, in `pred:fn(T -- i64)`, is the inner one closing the function
      pointer. Every signature with a `fn(...)` parameter lost everything after it and was left
      with an unbalanced paren. Fixed with a one-level-nesting pattern,
      `\(([^()]*(\([^()]*\)[^()]*)*)\)`, which is enough for the one nesting depth a
      function-pointer parameter can have; `fn_failable` moved from `BASH_REMATCH[5]` to `[6]` to
      account for the added group.

      All stdlib signatures now balance (**0 unbalanced**, was 17). `hof::all` publishes in full as
      `(arr:[]T pred:fn(T -- i64) -- result:i64)`. The `[Unreleased]` note claiming this was already
      fixed was about the renderer and was correct about the renderer — the generator was the
      remaining half.

      The related `signal.md` defect went with it: `gen_docs.sh` dropped `/// doccheck:` directives,
      so the one module whose example cannot run under docscheck (it loops until Ctrl+C) lost its
      opt-out on every regeneration. The generator now passes `doccheck:` through as an
      `<!-- doccheck: … -->` comment immediately before the fence, and `signal.qd` carries the
      directive at source. Regenerated `signal.md` is byte-identical to the checked-in page.
      docscheck: 219 checked, 0 failed.


- [x] **R42 — a stack slot owns what it holds, pointers included.** `qd_drop` released strings and
      deliberately skipped `QD_STACK_TYPE_PTR`, on the reasoning that "a raw pointer on the stack
      is not necessarily a counted object". `qd_ptr_release` answers exactly that question — array
      magic, then the struct registry, and anything else untouched — so the objection was already
      handled. `drop` leaked every array and struct it discarded: 128,000 bytes over a thousand
      iterations for an array where binding it instead was zero.

      **The scope estimate in this item was wrong.** It said the fix meant touching every
      `qd_stack_push_ptr` caller, all 66 of them. It did not, because the codebase already had the
      right convention for strings and applied it through a helper — `qdrt_release_if_string` —
      that the original survey missed by grepping for `qd_string_release`. That is also why `swap`
      looked like it leaked strings and does not. The change is to make pointers follow the rule
      strings already followed, in three places: `push_element` takes a reference of its own for
      pointers as it did for strings; the helper (now `qdrt_release_element`) drops a pointer's
      reference as it drops a string's; and the six discard sites that inlined `qd_string_release`
      go through it. Creation sites are untouched — `qd_push_p` goes straight to
      `qd_stack_push_ptr`, so a newly built array still transfers its single reference into the
      slot.

      **The ordering is the safety argument.** Releasing on `drop` without retaining on copy would
      free a value another slot still holds, which is why this was not done earlier; retaining
      first is what makes `dup` safe. Verified both directions: the three leaks go to zero, and
      `dup`/`over`/`swap`/`rot`/`dup2`/`nip` followed by drops are valgrind-clean with no errors,
      as are nested arrays and strings.

      R26's documented behaviour is preserved exactly: a two-node cycle still leaks its own nodes
      (112,000 bytes / 4,000 blocks) and a one-way link is still zero, so the specification text
      stays true. Test: `memory/drop_releases`.


- [ ] **R36. A user function named like a builtin is accepted, and the builtin wins.** Found by
      accident: a test function called `pick` compiled, and `1 pick!` ran the *builtin* `pick` at
      runtime (*"Index 1 out of range (stack has 0 elements)"*). The plain-call form `pick print`
      is rejected only because the builtin's stack effect happens not to fit. Locals shadowing a
      function already get *"Local variable 'x' shadows function with same name"*; declarations
      shadowing a builtin get nothing.
      **Open question**: reject at the declaration (`fn pick` → error naming the builtin), which is
      the only reading under which the call site is unambiguous?

- [ ] **R37. Four hand-rolled signature builders.** `analyzeFunctionSignatures` in `collect.cc`
      (main-module functions), the import-block loop in `collectDefinitions` (same file),
      `analyzeModuleFunctionSignatures` in `modules.cc` (module functions) and the import-block
      loop beside it — each with its own type-string → stack-type
      mapping and its own struct-qualification rules, plus the receiver-insertion logic repeated
      in two of them. R34's fallout included one of them substituting a body residual for
      declared outputs and another mapping sized integers to `any`; both were divergences the
      others did not have. Also of this family: the struct-construction field-initializer check
      (`typecheck.cc`, "Process the field's expression nodes") is a hand-rolled mini type checker
      with its own `switch` over node kinds; R35 found it had no `<<` case at all. It should call
      the real one. R1 and R2 factored the call-site *application* into `bindCallTypeParams` and
      `pushCallResults`, shared by both call paths, so what remains duplicated is the builders.
      `parseFnTypeString` is the inverse of `buildFnTypeString`; those two should stay adjacent,
      since a change to either silently breaks the round trip.

      **Measured 2026-09-18** with `tools/signature_baseline.sh`, driven by the new
      `QUADC_DUMP_SIGNATURES` env var: **773 compilation units, 2,224 distinct signatures, and
      zero module-qualified names computed inconsistently.** The premise of this item — that the
      builders are currently diverging — does not hold any more. The divergences it was written
      from were real, and were fixed: the body-residual substitution and the sized-ints-as-`any`
      mapping both went in R34, and R1 gave every builder the declared type names.

      A first reading of the dump reported 293 inconsistent names. That was an artefact of the
      harness: `quadc` validates the program and then each module file *as its own main file*, and
      a struct that is `thread::Barrier` to an importer is plainly `Barrier` inside its own module.
      Comparing across those passes compares different contexts. The harness now keeps pass 1 only
      — worth remembering before trusting a future run of it. The 47 names that still differ are
      all unqualified and genuinely different functions sharing a name across test files (`abs` is
      `f64 -- f64` in one and `i64 -- i64` in another).

      So what is left is duplication, not a live defect: four builders of ~200 near-identical
      lines that have produced three separate bugs historically. The case for consolidating is
      maintenance, and the baseline now makes it **provable** — it must stay byte-identical.

      One asymmetry does remain, and it is small: **`parameterFieldAccess` is populated only by
      `collect.cc`**, so of 61 pass-1 signatures carrying field requirements just 3 are
      module-qualified and no stdlib function has any. That mechanism infers a `ptr` parameter's
      struct type from the fields the body reads; since R1 the declared struct type is checked
      structurally anyway (`fp::takes` rejects a `Wrong` for a `Need` with no field data at all),
      so the gap is an inference nicety rather than a missing check.
      **Open question**: is a pure de-duplication worth the churn now that it is not fixing
      anything? The baseline makes it safe, and the history argues for it, but it is no longer
      urgent — R9 or R29/R30 may be better uses of a long run.


- [ ] **R38. `analyzeBlockInIsolation` still runs on every function body at collection time, and
      its result is no longer used for anything declared.** Both builders call it before building
      the signature; after R34 neither reads its residual. It may still be load-bearing for side
      effects — method-call marking on identifier nodes, captured-variable collection — and it
      may be pure cost.
      **Open question**: find out which, then either delete the call or rename it for what it
      actually does.

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

- [ ] **R12. The cost of removing `while`, now measurable.** `loop` is at 182 uses against `for`'s
      104 (re-verified 2026-09-18), and the overwhelming majority are `loop { cond if { break } … }` — three lines and a
      nesting level where `while` was one. `examples/kernel/kernel.qd` is wall-to-wall with it.
      What used to compound this — `loop` being the construct that suspended effect checking —
      is gone: R5 landed, and a loop body is now checked like anything else (see Done). So this is
      purely the ergonomic argument now, with no correctness cost attached.
      **Open question**: reinstate `while` as sugar that lowers to the same `loop`, or does the
      subtractive argument still hold?

- [ ] **R13. `and`/`or` are bitwise and are used throughout as logical.** There is no short-circuit
      operator; `lnot` exists but has no binary counterpart. Verified: `2 1 and` → `0`, so any
      operand not already 0/1 gives the wrong answer. Both sides always evaluate, so the guarded
      form `i xs len < xs i nth … and` is unsafe. Live in the corpus:
      `t 0.0 > best_t 0.0 < t best_t < or and if` (`examples/raytracer/raytracer.qd`) is correct
      only because every operand happens to be a comparison result.
      **Open question**: add short-circuit `land`/`lor` (named, per the no-symbolic-bitwise rule),
      or rely on comparisons always yielding 0/1 and document the hazard? Note this is a correctness
      question, not ergonomics.

- [x] **R14 — removed the direction rather than making `cast` fallible.** The open question offered
      both. Removal wins on the property that makes `cast` worth having: every other direction it
      offers is *total* — int to float, float to int, anything to string — so its result never
      needs checking. Making it fallible would have put a failure arm on conversions that cannot
      fail, to accommodate the one that can. And `strconv` already had the honest version of every
      case (`atoi`, `parse_int`, `parse_float`, `parse_bool`, all `!`), so this was a silent
      duplicate of an existing API, not a capability being taken away.

      The compiler rejects a string operand for `cast<i64>`/`cast<f64>` and names the replacement
      in the hint. The interpreter tier rejects it too, and can be exact where the compiler is
      static — the operand's type is on the stack.

      **Corpus fallout: one program.** The sweep compiled all 1,061 `.qd` files; 4 reported the new
      error and 3 of those were the cast tests themselves. The 33 files that fail to parse (all
      intentional `compile_errors`/`syntax` tests) were checked separately and contain no `cast<`
      at all, so nothing is hidden behind them. The one real site, `examples/dc/dc.qd`, guarded
      with its own `is_number` before casting — exactly the shape this item describes — and now
      calls `strconv::parse_float!`, matching the `!` style it already used for
      `strings::substring!`.

      Two escape hatches were checked and are closed: `nth` off a `[]str` propagates `str` and is
      caught, and laundering through `cast<ptr>` already fails at run time (*"Cannot cast type 3 to
      pointer"*). So the silent parse is unreachable from Quadrate in both tiers.

      **Deliberately left**: `qd_casti`/`qd_castf` in `lib/rt` still parse a string with `atoll`.
      They are declared in the public embedding header with that documented behaviour, and no
      Quadrate code can reach the branch any more; turning it into a fatal error is a C-API
      decision, separate from what the language allows.

      Tests: `compile_errors/cast_string_to_number` (literal and local operands, with negatives
      pinning that the total directions still compile), `casting/str_numeric` rewritten to show the
      failure actually being *detected* — `"notanumber"` and `"0"` now give different answers, which
      was the whole complaint — plus `casting/basic` and `casting/edge_cases` moved to `strconv`
      with byte-identical output, and `CastIsTotalSoStringsAreRefused` in `test_interp.cc`.
      Specification §3.7 states the totality rule and §12.6 points at it.

      **The docs were teaching the unsafe pattern**, which the test suite could not see because
      docscheck runs separately from `run_all.sh` — it went from 0 failures to 4 on this change.
      Two were reference listings (`learn/1-basics/values-types`, `learn/7-advanced/generics`,
      whose conversion table advertised `str -> i64` as "Parses integer"). The other two were
      worse: `learn/8-examples/user-input` read a line from the user and cast it straight to a
      number, in both of its examples — the one input source guaranteed to contain arbitrary text,
      demonstrated with the conversion that cannot report failure. All four now use `strconv` and
      show the failure arm. `reference/types.md` listed the two string rows as supported and now
      states the totality rule. docscheck: 221 checked, 0 failed.

- [ ] **R15. The sized-integer divergence the spec already documents — decide it.** `300 cast<u8>`
      yields `300`, and a sized type on a parameter or return annotation is inert. Only struct
      fields and the `mem` accessors honour the width; those were verified correct (`packed struct
      { a:u32 … }` with `-1 >>a` loads back `4294967295`). The spec §3.1.1 says a conforming
      implementation "SHOULD either apply the width consistently or reject sized types in positions
      where it does not" — the reference implementation does neither.
      **Open question**: truncate in `cast`, or reject the annotation where it carries no meaning?
      Rejecting is smaller and matches the subtractive precedent.

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

- [x] **R19 — implemented, and it uncovered a silent miscompilation.** Implementing was the right
      answer to the open question, not narrowing the spec: §3.2.2 documents `[[1 2] [3 4]]`, R23
      had just made nested arrays printable, and the adaptive element type from R28 does the
      typing work at run time.

      Two separate bugs. **The parser** had a nested-array branch, but it sat in the `else` of
      `parseSimpleToken` returning null — and that function reports its own diagnostic before
      returning null, so the recovery ran only after *"Unexpected character '['"* was already
      emitted. Nested `[` and identifiers (which is how `Name { … }` is reached) are now handled
      before that call.

      **Codegen was worse.** Its element loop emitted `LITERAL` nodes and ignored everything else
      without a word, so `7 -> x  [x 2 3]` compiled to the two-element array `[2 3]` — wrong data,
      no diagnostic, and reachable without any of the syntax this item is about. A literal whose
      elements are not all scalar literals is now built through the stack: the array is created
      with no element type and each element generates itself and appends, which is the `[] x
      append` path and gives struct elements and arbitrary nesting depth for free. All-literal
      arrays keep the constant path unchanged.

      **Typing** was also only ever inferred from element zero, and only when it was a scalar
      literal, so `fn mk(x:i64 -- r:[]i64) { [x 2 3] }` was rejected as `[]any`. It now considers
      every element — literals, locals, nested literals, struct literals — requires them to agree,
      and reports a genuine mismatch (`[x "a"]` against `[]i64` says `[]str`). Undecidable stays
      `[]any`.

      Found alongside: `--dump-ast` printed eleven node types as `Unknown`, including
      `ArrayLiteral` itself — fixed. Test: `arrays/nested_and_computed_literals`, valgrind-clean.

- [x] **R20(a)(b) — `str` was made properly UTF-8, rather than the spec relabelled.** The item
      offered "correct the spec's wording to byte string" as (a); the decision went the other way,
      on the instruction that the language shall be proper UTF-8. The spec's description was the
      one that was right and the library was what had to change.

      **What was actually wrong** was worse than the item recorded. Re-verifying turned up two
      things it did not mention: `strings::reverse` reversed *bytes*, so `"héllo wörld"` came back
      as mojibake with every multi-byte character shredded, and `substring` could cut inside a
      sequence — `"héllo" 0 2 substring` produced the bytes `h 0xC3`, a truncated character. Those
      are not "byte semantics", they are output that is not UTF-8 at all, and no amount of
      relabelling the spec fixes them. `from_char` truncated every codepoint above 127 to one
      byte, and the padding functions emitted `pad_ch[0]` — the first byte of the pad character on
      its own.

      **What was done.** A UTF-8 layer in `stdlib/strings/src/strings.c` (decode, encode, count,
      codepoint index to byte offset and back) and every index- or length-sensitive function moved
      onto it: `len`, `char_at`, `from_char`, `substring`, `slice`, `insert`, `remove_range`,
      `truncate`, `index_of`, `index_of_from`, `last_index_of`, `pad_left`, `pad_right`, `center`,
      `reverse`. Case mapping (`upper`, `lower`, `capitalize`, `title`, `equals_ignore_case`,
      `is_lowercase`, `is_uppercase`) and classification (`is_alpha`, `is_alphanumeric`) map per
      codepoint, and the trim family plus `words` use a Unicode whitespace test. No live `ctype`
      call is left in the module.

      **Case mapping is algorithmic, not a UCD table.** Latin-1, Latin Extended-A and Additional,
      Greek and Cyrillic are laid out regularly enough to compute — either a fixed offset between
      the upper and lower runs, or alternating pairs. **Deliberately not handled**: mappings that
      change length (U+00DF sharp s uppercases to "SS"), which a one-to-one API cannot express and
      which would invalidate every index the caller holds; locale-dependent mappings (Turkish
      dotless i); and Latin Extended-B, whose layout is genuinely irregular. Scripts without case
      come back unchanged, which is correct rather than a gap.

      **`char_count` became an exact synonym for `len` and was replaced by `byte_len`.** It had no
      uses outside its own tests. Byte extent still matters for sizing a buffer, and after this it
      was reachable only through the raw pointer from `strings::data`, so it keeps a name.

      Corpus fallout was four tests, every one of which was *asserting the bug* — `strings/utf8_special`
      ("note: strlen counts bytes, not characters"), `unicode/multibyte_chars` ("1+2+3+4 = 10 bytes"),
      `unicode/emoji_handling` and `unicode/mixed_scripts`. Their comments were rewritten rather
      than just their expected output regenerated, so they no longer document the old behaviour as
      intended. New test: `strings/utf8`, valgrind-clean. Specification §3.1 states the codepoint
      rule and the well-formed-output requirement.

- [x] **R20(c) — `unicode` was made to deserve its name, instead of renamed to `ascii`.** The
      earlier recommendation here was the rename. That was wrong, and the measurement is what
      changed it: `unicode` is not a duplicate of `strings`, it is the *per-codepoint* layer
      (`strings::is_alpha` takes a string, `unicode::is_alpha` takes one character), and now that
      `char_at` returns codepoints it is needed more than before, not less. Renaming to `ascii`
      would have made the label honest while enshrining the real problem — that the two layers
      disagreed:

    | | `strings` | `unicode` (before) |
    |---|---|---|
    | is `é` a letter? | 1 | **0** |
    | uppercase `é` | `É` | **`é`** |
    | lowercase `Ä` | `ä` | **`Ä`** |

      The 11 functions that were bounded by ASCII arithmetic (`is_upper`, `is_lower`, `is_alpha`,
      `is_alnum`, `is_space`, `is_digit`, `is_print`, `is_control`, `is_punct`, `to_lower`,
      `to_upper`) moved to a new C backend that shares R20(a)(b)'s tables through
      `stdlib/unicode/include/quadrate/unicode/codepoint.h`, so one implementation answers both
      layers and they cannot drift. **Zero call-site churn**: the 118 function calls are unchanged,
      ASCII results identical, non-ASCII now correct — against the 502 edits the rename needed.

      **What deliberately did not widen.** The 357 constant references needed nothing: `space = 32`
      is an ordinary Unicode codepoint, ASCII being a subset. `is_hex_digit`, `is_ascii`,
      `digit_value`, `hex_digit_value` and the five UTF-8 mechanics helpers stay pure Quadrate,
      because they are ASCII notation or byte arithmetic by definition. `is_ident_start` and
      `is_ident_cont` were defined in terms of `is_alpha`/`is_alnum` and would have silently
      followed them into the rest of Unicode — specification §2 restricts identifiers to ASCII, so
      they are now written against explicit ASCII ranges.

      Two build-level facts this turned up. `isStdlibImport` in `generator.cc` is a hardcoded list
      of stdlib archives, and a library missing from it gets bare symbol names instead of
      `usr_<module>_<name>` plus a forwarding wrapper, which fails to link; `libunicode.a` had to
      be added. And see R43 — the silent miscompilation that cost the most time here.

- [x] **R43 — rejected with a compile error.** An unqualified call to a function in the module's
      own `import` block used to compile clean and leave an extra value on the stack, so an `if`
      consumed that instead of the result and always took the true branch: in `unicode.qd`,
      `c is_digit if { ... }` made `digit_value` answer **17** for `'A'` rather than -1.

      Rejecting beat making it work: the qualified form is already what every module in the
      corpus writes (`math::log` is `x math::ln base math::ln /`), so a diagnostic is cheaper and
      more certain than a second name-resolution path.

      **The first attempt was in the wrong place** and never fired. Putting it in the type
      checker's unresolved-identifier branch covers only a *consumer* writing the bare name, and
      that is already caught by the existing "Undefined identifier" check. The module's own bodies
      never go through `typeCheckBlock` when a program imports it, which is exactly why the bug
      existed. The check now runs where the module's imports are collected
      (`checkModuleUnqualifiedImportCalls`), as a syntactic walk of the module AST: only names
      from that module's import block match, and the qualified form is a `SCOPED_IDENTIFIER` so it
      never does.

      Reported without a source position on purpose. The offending line belongs to the module, but
      the reporter names the file being compiled, so anchoring the caret would point it at an
      unrelated line of the importing program; the module and line go in the message text instead.

      Test: `compile_errors/unqualified_module_import`, with `badimport_mod.qd` — a local module
      that calls its own imported `upper` unqualified.

- [x] **R45 — assertion failures are recorded on the context, so depth no longer matters.** The
      open question offered general propagation of a callee's exec result or a failure flag the
      harness checks. The flag won: propagating would change the calling convention across the
      whole language to duplicate a mechanism Quadrate already has — fallible functions with `!`
      and `error_code`. `testing` simply was not using it; it returns a code and relies on the
      caller looking, which only the frame that emitted the call can do.

      Three parts. The runtime gained `qd_assertion_failed` / `qd_assertion_failures` /
      `qd_assertion_reset` over a counter on `qd_context`; all 19 failure paths in `testing.c`
      record alongside their existing return; and `generateTest` resets at the start of each test
      and folds the count into the result, so a direct failure still returns its own code and a
      deep one returns 1.

      The counter lives on the context rather than at file scope in libtesting for the reason the
      context already documents for its recovery state — `lib/qd` may link the shared runtime
      while `lib/interp` links the static one, so a static would exist twice in one process. It is
      appended, so codegen's GEP of field 0 is unaffected.

      **Verified by mutation, not just by the suite going green.** `1 testing::assert_positive`
      in `testing/numeric_assertions` was changed to `-1`: it now fails, where before the fix that
      assertion was one of the 103 inert ones and would have passed. Reverted after. Regression
      test `testing/assert_depth` covers the passing direction at one frame, two frames and
      through all eleven wrappers; the failing direction cannot live in the suite, since a failing
      test fails the run.

- [x] **R44 — every character-index conversion now goes through the memo, not just the hot ones.**
      Making `str` UTF-8 turned finding character *i* into a walk from the start, so any loop
      reading a string one character at a time became quadratic: a 20,000-character scan took
      **2.9 s**, and `json::get_array` over the 60 KB `docs/api/math.json` never finished —
      `quadrate_list_modules` span at 99.6% CPU for 16 minutes and wedged two full test runs before
      the cause was found. The original `char_at` carried a comment warning about precisely this,
      from the last time it was fixed.

      Two memos on `qd_string_t`: the codepoint count (so `len` is O(1) after the first call, and
      when it equals the byte length every character is one byte and an index *is* an offset), and
      a packed (char index, byte offset) scan cursor so a forward scan resumes rather than
      restarting. The cursor is what actually mattered, since `math.json` contains `π` and so
      misses the one-byte shortcut entirely. Both halves live in one atomic word because strings
      are shared between threads and two separate fields could be read from different updates.

      The first pass fixed only `len`, `char_at`, `substring`, `slice` and the `index_of` family
      and left seven functions counting for themselves — which was the actual finding here, since
      that asymmetry is exactly the trap that had just cost hours. `insert`, `remove_range`,
      `truncate`, `pad_left`, `pad_right`, `center` and `last_index_of` now use the same helpers,
      and `qd_string_char_index` was added as the inverse (byte offset to character index) so the
      search functions share the cursor too. **No hand-rolled conversion is left in the module**:
      `utf8_offset`, `utf8_count`, `utf8_char_index` and `strings_is_flat` all became unused and
      were deleted, which is the check that the conversion is complete.

      Measured on identical work — 30,000 non-ASCII characters, 5,000 `index_of_from` calls and a
      full forward slice scan: **non-ASCII 32 ms against ASCII 27 ms**. The two paths now cost
      about the same, where non-ASCII previously fell off a cliff. The earlier headline figures
      hold: the 20,000-character scan is 26 ms (was 2.9 s), `get_array` 32 ms (was unbounded), the
      MCP call 374 ms (was hung).


- [ ] **R21. `>>field` is specced as returning an updated struct; it mutates in place.** Structs are
      reference values: `P { x = 1 } -> a  a -> b  b 99 >>x drop` leaves `a <<x` as 99, and passing
      a struct to a function lets that function mutate the caller's value. The spec's "sets field,
      pushes modified struct back (for chaining)" and the idiom `p 42 >>x -> p` both read as a
      functional update.
      **Open question**: documentation only, or is there an argument for value semantics on
      assignment? (Almost certainly documentation only — but the current wording is actively
      misleading and 12 `>>` sites is a small blast radius if anything does change.)

- [x] **R22 — allowed, because the language was already contradicting itself.** The open question
      asked whether rejecting it was a deliberate safety choice. It was not, and the evidence is
      that the two code paths disagreed: `generator_nodes_instructions.cc` emits a bare `fdiv` for
      two doubles, so `fn d(a:f64 b:f64 -- r:f64) { a b / }` called with `1.0 0.0` returned `inf`,
      while `0.0 -> z  1.0 z /` went through the runtime's `qd_div`, which trapped, and killed the
      process. Same expression, two behaviours, decided by whether the operands reached the inline
      path. A safety property that holds half the time is not one.

      The integer rule had simply been applied to floats in two places — the validator's literal
      check and `qd_div` — and both are now integer-only. `1.0 0.0 /` is `inf`, `-1.0 0.0 /` is
      `-inf`, `0.0 0.0 /` is NaN. Integer division and modulo by zero are untouched; `mod`/`%` is
      integer-only anyway (the validator types it `int` and the runtime rejects float operands),
      with `math::fmod` as the float remainder.

      `math` gained the second half of the open question: `inf`, `nan`, `is_nan`, `is_inf`,
      `is_finite`. Infinity has no literal spelling, so without them a program could produce one
      only by dividing and could not test for one at all — NaN is unequal to itself, so `x x ==`
      was the only NaN check available and it reads as a bug.

      Specification §12.1 gains a **Float semantics** paragraph beside the existing integer one.
      Tests: `arithmetic/float_div_by_zero`, three in `stdlib/math_test.qd`, the float line dropped
      from `compile_errors/division_by_literal_zero` and pinned negative so it cannot come back,
      and `test_interp.cc` updated — it had been asserting the old trap.

#### Ergonomics and gaps

- [x] **R23 — `print` renders arrays.** `[1 2 3] print` was printing an empty line because
      `qd_print` had no `QD_STACK_TYPE_PTR` case at all and fell through. It now formats arrays by
      element type — `[1 2 3]`, `[1.5 2.5]`, `[a bb]`, `[]` — and recurses into nested arrays
      (`[[1 2] [1 2]]`) under a `QD_PRINT_MAX_DEPTH` of 8, so a self-referential structure prints
      `…` rather than running off the stack. `printv` gets the same treatment.

      Structs print as `<struct 0x…>`. Rendering fields would need the layout at runtime and the
      runtime has only the registry — answering the item's open question in the direction of
      "arrays yes, structs are what `fmt` is for", which is also where the type information lives.

      `print` releases the pointer it consumed (`qd_ptr_release`), matching the stack's
      transfer-on-push convention for pointers. Verifying that turned up a real leak in the R28
      `QD_ARRAY_TYPE_ANY` work — `case QD_ARRAY_TYPE_ANY: break;` skipped `free(arr->data.p)`, so an
      array that never received an element leaked its buffer — now fixed. All the new paths are
      valgrind-clean.

      It also exposed R42: `drop` releases nothing but strings, which is a much larger leak than
      this item and is filed separately.

- [ ] **R24. `err` is global state, not a value.** Set by `panic`, cleared on read, survives
      intervening non-fallible calls (all verified). It cannot be stored, returned, wrapped, or
      chained, so there is no way to build "failed to open config: no such file". It is also the
      main reason R4's authors reach for `!`.
      **Open question**: subsumed by R3, or worth an independent error-value type first?

- [ ] **R25. `error { code = … message = … }` literal has near-zero use.** Listed in the grammar and
      §10.2 as the alternative to `msg code panic`.
      **Open question**: cut it now on the `>>field!` precedent (two spellings of one operation), or
      hold because R3 would replace both?

- [x] **R26 — documented, after the measurement was redone properly.** New §11.2.1 "Reference
      Cycles" in `specification.md`: refcounting alone MUST NOT be expected to reclaim a cycle, an
      implementation is NOT required to detect one, this implementation provides neither a detector
      nor weak references, and a program that builds cyclic structures must break the cycle itself.
      It uses the spec's own `struct Node { next:*Node }` shape, as the item asked.

      The first round of verification contradicted itself — cycle and no-cycle controls leaked
      identically — because every arm discarded the field-write result with `drop`, which leaks on
      its own (R42) and swamped the signal. Re-measured over 1,000 iterations with the results
      bound instead, `in use at exit`:

    - no link — **0**
    - one-way link `a.next = c` — **0**
    - two-node cycle — **112,000 bytes / 4,000 blocks** (1,000 × 2 nodes)
    - self-cycle `a.next = a` — **56,000 bytes / 2,000 blocks** (1,000 × 1 node)

      So the original claim holds exactly, and the one-way result is worth as much as the cycle
      one: nested release works, and the spec now says so — only cycles are affected, a non-cyclic
      chain of pointer fields is reclaimed in full. Documenting was indeed sufficient; no detector.
      Everything leaked is precisely the cycle's own nodes and nothing more.

- [ ] **R27. Threads get a hardcoded 1,024-element stack.** `qd_create_context(1024)` in
      `stdlib/thread/src/thread.c:44` and `lib/rt/src/runtime.c:856`; `-s` does not reach it.
      Verified: a thread body that pushes 5,000 values dies with *"Stack overflow (use -s to
      increase stack size)"* — advice that does not work for this case. Thread contexts are properly
      isolated otherwise, which is the important part.
      **Open question**: plumb `-s` through, or make it a `thread::spawn` parameter?

#### Cuts with corpus evidence

- [x] **R29 — withdrawn. `read` is load-bearing.** The claim of "zero bare uses" was the grep
      bug above. It has 10 uses, and they are not incidental: `read` is the only way to get
      command-line arguments onto the stack, and `flag::parse(argc)` is documented as taking the
      "Argument count from read". `examples/wc`, `examples/dc`, `examples/sha256sum` and
      `cmd/quadmcp/server.qd` all open with `read flag::parse -> f`; `examples/csvcut` uses
      `read -> argc` directly. Removing it was attempted and immediately broke the quadmcp build.
      What remained true was the second half of the original item, recorded as R41: `read`
      cleared the whole type stack and set `mHasUnpredictableStack`. **R41 has since removed
      `read` outright** — see Done. The load-bearing part was real and is what `os::args`
      now carries; what was never load-bearing was the *shape*.

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

- [ ] **R31. Supporting numbers for the `pick`/`roll` and live-shuffler items above.** Re-measured
      2026-09-18 with the tokenizer, over `stdlib` + `examples` (the original scope): `-> ` locals
      **2,896**; `<<field` 1,150; `>>field` **12**; `drop` 54, `dup` 23, `swap` 18, `nip` 7, `rot`
      5, `over` 4, `dup2` 2, `roll` 2, `pick` **0**. Named binding outnumbers every shuffler
      combined by roughly 40:1 — the earlier 46:1 was from the broken pattern, and the conclusion
      is unchanged. `pick` is now at zero and `roll` at two, which strengthens the separate
      `pick`/`roll` item above. Still worth noting: `>>field`, one of the language's two custom
      sigils, has twelve uses in the whole corpus — structs are read-mostly in practice.
      **Open question**: does the 40:1 ratio settle the live-shuffler item, or is frequency the
      wrong test for `swap`/`over`?

#### Positioning and scope

- [ ] **R32. The stack is no longer the programming model, and the documentation still says it is.**
      Given R31's ratios, real Quadrate is an ALGOL-family language with named parameters, named
      locals, structs and methods, that uses postfix syntax and a stack calling convention.
      `examples/dc/dc.qd` — a calculator, the most stack-shaped program there is — opens with
      `fn stack_push(s:ptr val:f64 -- )` over a hand-rolled `mem::alloc` array, because the
      language's stack is not where data lives. Meanwhile `learn/2-stack/` is four pages against
      three for functions and two for error handling. *(Corrected 2026-09-18: the review said "more
      than functions and error handling together", which is 4 against 5 — it is not. Four pages on
      the stack, more than on either subject alone, is the accurate form and still the point.)*
      **Open question**: is this a docs restructure (lead with functions and locals, demote the
      stack to "how calls work"), or is the concatenative framing something to keep pushing toward
      in the language itself?

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

### R41 — `read` removed, arguments are an array (2026-09-18)

- [x] **`read` is gone; `os::args` replaces it.** It splayed the command-line arguments across
      the operand stack, which is not an expressible stack effect: the validator modelled it by
      clearing the type stack, pushing **sixteen synthetic `str` values** and setting
      `mHasUnpredictableStack`, so every function containing it lost its arity, if-arm and defer
      checks. `flag::parse(argc:i64)` then read that pile back off the stack, which is why it
      needed an exemption of its own on both the call and definition sides.

      `os::args( -- args:ptr)` returns them as a Quadrate string array — the convention
      CLAUDE.md already states for anything returning a list of strings — and `os::program_name`
      exposes argv[0], which `read` used to stash in `ctx->program_name` as a side effect that
      nothing ever read back. `flag::parse(args:ptr)` takes the array. All 10 call sites
      rewritten; `read` is in `REMOVED_INSTRUCTIONS`, so using it reports what happened and the
      rewrite. Both exemptions are deleted: `fmt::printf`/`sprintf` remain, since they really do
      pop one value per `%` in the format string.

      **The fiction was hiding a real bug.** With the type stack no longer padded with sixteen
      strings, `f "--name" flag::string if { ... }` stopped compiling — and correctly so. The
      module-method call path pushed a fallible call's *results* but not the **status** the
      following `if` tests, unlike the explicit `Type::method` path, which has always pushed
      both. So the `if` was consuming the result as its condition and the success arm read an
      empty stack. Every caller sat downstream of `read`, whose synthetic strings absorbed it.
      Fixed in the SCOPED_IDENTIFIER module-method branch.

      Smaller things the same removal turned up, all in examples that are
      `build_by_default: false` and so had gone unbuilt since the R5 checks landed:
      `examples/sha256sum` had `= =` where it meant `==`, `> =` where it meant `>=`, and a bare
      `crypto::sha256_bytes` missing its `!`; `examples/csvcut`'s `parse_columns` returned a bare
      `0` for a declared `ptr`; `examples/kernel`'s `print_prompt` pushed three arguments to a
      two-argument `cursor_set` and leaked one per prompt. All four now compile, and sha256sum
      agrees with GNU `sha256sum`.

      `tools/check_builtin_lists.py` found every stale mention, including a `read` left in the
      playground's highlighter. Its prose scan needed a `PROSE_EXEMPT` set: the tool's own note
      says removed names "like `tuck` and `dupd` are distinctive enough not to collide", and
      `read` is an ordinary English word — it flagged "Receiver is read-only" and "Use << to read
      fields", prose that is correct and should not be reworded to satisfy a grep.

      Tests: `compile_errors/removed_read` pins the diagnostic, `os/args` pins the new API, and
      the three `tests/qd/args/` programs plus `flag/basic` and `modules/flag_module` now build
      their argument arrays explicitly. Note `echo_args` changed expected output: arguments come
      back in **input order** now, where popping the stack yielded them reversed.

### Use-after-free on every struct global (2026-09-18)

- [x] **Reading a field off a `var` struct global freed it.** The five
      `tests/qd/globals/var_struct_*` valgrind failures were all one bug, and it was not a leak:
      each reported `in use at exit: 0 bytes in 0 blocks` alongside three invalid reads.

      A pointer on the stack owns a reference. Reading a *local* retains before pushing
      (`generator_nodes.cc`, the PTR block of the local switch), and every consumer releases —
      `generateFieldAccess` pops and calls `qd_ptr_release` with the comment "was retained when
      pushed". Reading a *global* loaded and pushed without retaining, so that release cancelled
      the one reference the global itself owns. `var origin = Point { ... }` was therefore freed
      by its **first** field read, and every read after it touched freed memory. `qd_push_s_ref`
      retains internally, which is why the `str` case was fine and only the pointer case was
      wrong; the compile-time-stack path pushes an SSA value no runtime release ever sees.

      Not just a valgrind finding: with the block reused by the next allocation the corruption is
      plainly visible — `origin <<x` then `Point { x = 99 y = 98 }` then `origin <<x` printed
      `99`, and `origin <<y` printed `98`. Pinned without valgrind as
      `tests/qd/globals/var_struct_use_after_read`, alongside the five that catch it under
      valgrind. `make valgrind` is green.

### Language review — R5, R7, R8: loop stack effects (2026-09-18)

- [x] **R5 / R7 / R8 — loops now have a checked stack effect.** All three were one hole. The
      model that landed: **a loop body must leave the stack as it found it**, because the next
      iteration starts where the last one ended and the trip count is a runtime value. The exits
      are the other half — a `for` can also leave by running off the end of its range, at the head
      depth, so each of its `break`s has to agree with that; a `loop` has no fall-through exit, so
      its `break`s are the only way out and they are what *define* the depth after it. A
      `continue` jumps back to the head in both forms. That last point matters: the
      counter-on-the-stack idiom (`0 loop { ... dup 10 gt if { drop break } ... }`) is a net
      effect of -1 and is perfectly well defined, so the first rule tried — "every jump must be
      neutral too" — was wrong and rejected it. `checkLoopStackEffect` applies the effect to the
      parent stack, which is also what makes the *declared-effect* check work rather than merely
      reject: `fn count_to(limit:i64 -- total:i64)` accumulating on the stack and breaking out now
      verifies.

      With that in place `LOOP_STATEMENT` no longer sets `mHasUnpredictableStack`, so R5's
      declared-effect check, R7's if-arm balance check and the defer-effect check come back on for
      every function containing a loop. R7 needed one more thing: errors inside a loop body were
      suppressed *wholesale* (`reportError` consulted `mInLoopBody`), which is why the unbalanced
      `if` only ever surfaced from codegen as `error: internal:` with no `file:line`. The
      suppression now applies only where the model is genuinely broken —
      `mInLoopBody && mHasUnpredictableStack` — which is `read`, FFI, an unresolved name, or one
      of the variadic entry points. Those needed the exemption on the definition side too, not
      just at call sites: `flag::parse`'s own `for` consumes the argument pile `read` left below
      its frame, which no signature can describe.

      **Corpus cost of the strict rule: three real bugs and one pinned test.** That is the
      argument for taking it rather than the "only loops with no `break`/`continue`" middle option.
      - `examples/fibonacci` leaked a value per iteration — `it dup fib print nl`, 20 values left
        on the stack.
      - `tests/qd/control_flow/complex_control_flow.qd` leaked on `break`: the break arm left `it`
        where falling through left nothing.
      - `fuzzy::best` wrote `... set -> results`, but `set` is `( arr index value -- )` and pushes
        nothing. It compiled clean and **died at runtime** — "Fatal error: Stack underflow when
        assigning to local variable" — verified against the pre-fix compiler. `docs/api/builtins.json`
        was the source of the mistake: it alone claimed `(arr i val -- arr)`, against `reference.def`,
        `reference.md` and the spec. Fixed.
      - `for_loop_stack_accumulation.qd` pinned the old behaviour and its own note asked for this
        to change "as a conscious decision, not silently" if strict loop checking ever landed. It
        is now a `.err` test pinning the rejection.

      Found and not fixed, since nothing calls it and it is a naming defect rather than a
      behavioural one: `regex::find` and `find_from` declare `-- start:i64 end:i64` but leave
      `start` on top, so the names are in the wrong order. Both are i64, so no check can see it.
      `find_all` had the same reversal with `ptr`/`i64`, where the check *could* see it, and that
      one is fixed.

      Regression tests: `loop_body_not_neutral` (R5), `loop_unbalanced_if` (R7, with negative
      patterns pinning the absence of `internal:`), `loop_break_depth_mismatch` (R8), and
      `loop_break_defines_effect` pinning the counter-on-the-stack idiom that must keep compiling.

      Still not modelled, and worth a note rather than a task: a neutral body that *rewrites* a
      slot's type in place. Only the depth is tracked across a loop, not a type fixpoint.

### Compiler correctness — the two doom segfaults (2026-09-18)

Both were reproduced and fixed. The port's sources are not in the working tree, but they are in
git history at `01e766b6` (`git archive 01e766b6 examples/doom | tar -x -C <dir>`), which is what
made this bisectable. Neither cause was what the notes guessed: nesting depth is bounded (the
parser reports "Block nesting too deep"), and neither bug was in the type stack.

- [x] **quadc segfaults when `d_main.qd` adds `use "info.qd"`.** The crash was in
      `llvm::verifyModule` — and in `Module::print`, so the module could not even be dumped.
      `generateFunction` dropped a function's native (compile-time-stack) version on the way past
      its body, the moment the body turned out to call a non-native function. Bodies are generated
      in module order, so a caller could already have emitted a call to `callee_native`;
      `eraseFromParent()` then freed a `Function` those call instructions still pointed at, and the
      module was left holding dangling operands. Importing `info.qd` into `d_main` is what put
      `info::mobj_doomednum` (2 live uses at erase time), `mobj_spawnstate` and `mobj_painstate` in
      that position. The map purge was also incomplete — it guessed two alias keys where the
      function was registered under three.
      Fix: `demoteNonNativeFunctions` settles the native set to a fixpoint after all declarations
      and before any body is generated, so no call to a demoted native version is ever emitted; the
      purge walks the map by pointer; and the remaining in-generation path only erases when
      `use_empty()`. Regression test `tests/qd/regression/native_demotion/` (segfaults without the
      fix).

- [x] **quadc segfaults on a cross-module `pub fn` call from deeply-nested control flow.** Same
      codegen bug — `r_perspective.qd` is not special, and neither is the `if`/`else` depth. What
      that note did capture was real, though: the call was unresolved, because quadc re-validates
      every imported `.qd` outside the main file's directory in a second, isolated pass, and that
      pass could not see what the file imported. Three defects there, each reported as an undefined
      identifier or a bogus stack underflow rather than as the missing import it was:
      a relative `use` was resolved against the *main* file's directory instead of the importing
      file's; a `use` written in the file being validated was treated as an intra-module import, so
      its names never came into scope unqualified and `use "../ffi/sdl.qd"` left `sdl::Init` with
      no known stack effect; and the derived namespace was not registered at all, so every
      qualified call through such a file failed with "Module 'sdl' not imported". A file declaring
      a constant that one of its imports also declares is now a shadow, not a duplicate — the main
      pass already allowed it. Regression test
      `tests/qd/file_imports/import_subdir_nested.qd`.

      Effect on the port: **690 semantic errors → 7 on the pristine tree, from the compiler fix
      alone**. The remaining ones are the port's own rot against five months of language drift
      (`type` became a reserved word; two block-scoped locals read after their block; a dead call
      to a helper that never existed; `netgame` never declared; `NUMSPRITES`/`NUMSTATES`/
      `NUMMOBJTYPES` emitted twice in `info.qd`). With those patched in a scratch copy the whole
      62 kLOC port compiles, codegens and **links** — `use "info.qd"` in `d_main.qd` included.

      Two things found on the way and deliberately left open, since neither blocks the port:
      `0xFFFFFFFFFFFF0000` is rejected as "out of range for i64" (hex and binary literals are bit
      patterns and should parse as `uint64_t` then reinterpret — `integerLiteralProblem` in
      `semantic_validator_typecheck.cc` and `safeParseInt64` in `generator_impl.h` both use
      `from_chars` into `int64_t`), and the same file reached under two path spellings is loaded
      twice, since `mLoadedModuleFiles` is keyed on the literal `use` string rather than the
      resolved path.

### Language review 2026-09-17

- [x] **R2 — `call` after `<<field` (or on a typed parameter) was never modelled.** The
      corrected diagnosis stood: nothing converted a `fn(...)` type *string* back into a
      signature, so `call` always took its unknown-effect branch outside the `&f`/lambda case.
      `parseFnTypeString` is the inverse of `buildFnTypeString`, and `call` falls back to it
      using the type the value already carries — which the parameter registration and the field
      access had been putting on the struct-type stack all along. **Dynamic dispatch through a
      struct field now works**, including multi-result, zero-result and struct-returning fields
      that chain straight into `<<`. Found while fixing it:

    - **`call` was a third instance of the R34 bug.** Its branch fell out of the chain into the
      trailing `mHasUnpredictableStack = true` — a plain statement, not an `else` — so *every*
      function containing a `call` lost its arity, if-arm and defer checks. It is the only
      non-alias branch in `typeCheckInstructionInternal` that did not `return`. The flag is now
      set only when the effect is genuinely unknown (an untyped `ptr`).
    - **Nothing checked a function pointer against the field it initialises**: the field
      initializer evaluator had no case for `&f` or a lambda, so `H { f = &shout }` with
      `f:fn(i64 -- i64)` was accepted. Added both cases, and replaced the raw string compare it
      then reaches with the structural one from R1.
    - **`buildFnTypeString` rendered coarse stack types**, so a struct-returning function came
      out as `fn( -- ptr)` and did not match a field declared `fn( -- Point)`. It now uses the
      declared type names R1 put on the signature.
    - **`ptr` vs `any` in unification**: `ptr` is the untyped escape hatch and stays compatible
      at any depth; `any` means "unknown" only at the top level, which is what keeps the empty
      `[]` literal failing against `[]i64`.

    R39 (a lambda cannot appear inline in a struct literal) was found here and is filed above.
    Sweep clean, docscheck clean, stdlib unit tests clean.


- [x] **R28 — comparator sort and generic `hof`, the payoff from R1.** Both APIs were shaped by
      the missing unification: `sort` had six entry points each fixing element type *and*
      direction, and every `hof` combinator was `fn(i64 -- i64)`.

    - **`hof` is generic**: all seventeen combinators take type parameters, `map` is `map<T, U>`
      so it can change the element type. The seventeen existing i64 tests pass unchanged; five
      new tests cover floats and strings.
    - **`sort::by`, `is_sorted_by`, `lower_bound_by`**: comparator-ordered, C `qsort` convention.
    - **Found a shipped correctness bug in the existing quicksort.** All four of `ints`,
      `ints_desc`, `floats`, `floats_desc` returned *unsorted data* for reverse-sorted input of
      even length 18 or more, silently. The left scan was bounded by `j` instead of `hi`, so it
      could stop on an element sorting before the pivot, which the following pivot swap then
      jumped over. Found only because `sort::by` mirrors the same partition and failed its test.
      Fixed in all five partitions; regression tests cover lengths 2..60 for `ints`, `floats` and
      `by`, and a stress run of ~1,000 sorts over random, duplicate-heavy, reverse and all-equal
      input reports no failures.
    - **Arrays now decide their element type on first use** (runtime change). An empty `[]`
      literal was an *int* array, so `[] "x" append` failed at run time and a generic `map<T, U>`
      could not build its result. Arrays start untyped and adopt from the first `append` or
      `set`; `make<T>` for a type parameter uses the new `qd_makea`, since generics are erased
      and T is unknown at run time.
    - **The function-level output check is structural too**, with the function's own type
      parameters as wildcards — the last place still doing a string comparison. It also skipped
      struct results entirely, so returning a `B` where `A` was declared went unreported.

    R40 (quaddoc truncates fn-pointer signatures) was found here and is filed above. `hof.md` and
    `sort.md` regenerated; sweep clean, docscheck clean.


- [x] **R16 — a `switch` over an enum warns about variants it does not handle.** The check reads
      the case labels: if every arm is a `ScopedIdentifier` whose scope is one known enum, there is
      no `_` arm, and some variant goes unnamed, it says which. Needed one new piece of metadata —
      `mEnumVariants`, the variant list per enum, recorded for both the bare and the
      module-qualified spelling; `mConstantValues` held the variants' *values* but could not answer
      "which variants does this enum have".

      **A warning, not an error** — the open question this item carried. An enum variant is an
      `i64` and nothing tracks that the subject came from that enum, so the check infers intent
      from the labels rather than reading a type, and inferred intent should not be fatal. If a
      real sum type lands (R3) the subject would have a type and this could be promoted.

      It also turned out to sit well beside R6's rule: a switch with no `_` that *produces* a value
      is already an error, because if nothing matches, nothing runs. So covering every variant is
      not enough when the switch yields something — the two diagnostics together push you to the
      `_` arm, which is right, since the subject can hold any integer. A stack-neutral switch over
      every variant needs no `_` and stays clean.

      Nothing in the corpus trips it. Tests: seven in `test_semantic_validator_extended.cc` (missing
      one variant, missing several, exhaustive, wildcard, mixed enum-and-literal arms, two enums,
      literal-only) plus `enums/enum_switch_exhaustive` end to end.


- [x] **R1 — generic type parameters unify structurally.** `structTypesMatch` compared declared
      and actual types as literal strings, so `[]T` never matched `[]i64` and `fn(T -- T)` never
      matched `fn(i64 -- i64)`; only a bare `T` worked, via a coarse `TYPEVAR` on the stack-type
      level. `FunctionSignature` now carries the function's `typeParams` and the declared type
      *names* of parameters and results (the coarse types cannot express this: `[]T` and `[]i64`
      are both PTR). `unifyTypeName` recurses through `[]X`, `fn(A -- B)` and `Name<X>`, binding
      parameters into a map; `pushCallResults` substitutes those bindings into the results.
      Fallout and findings:

    - **The consistency hole closed for free.** `fn same<T>(a:T b:T -- r:T)` accepted `1 "s"`
      because no binding environment existed. Now an error naming both types.
    - **Module-qualified calls never checked struct/array/fn argument types at all** — the check
      lived only in the unqualified path. Both now share `bindCallTypeParams`/`pushCallResults`.
      This surfaced `Flag` vs `flag::Flag` (quadmcp declares a parameter with the unqualified name
      after `use flag`), fixed by canonicalising an unqualified struct name to its module's.
    - **Instantiated generics lost their arguments.** `Box<i64> { value = 5 }` pushed `Box`, so
      `<<value` reported `T`. The struct-type stack now carries `Box<i64>`,
      `lookupStructFieldTypes` strips the arguments, and `resolveFieldType` maps a field declared
      as a type parameter to the matching argument. This also replaced the ad-hoc generic-field
      exemption added for `>>field` during R35.
    - **`hof_test.qd` declared seven lambdas as `fn ( -- )` and passed them where `fn(i64 -- i64)`
      was expected** — untruthful signatures the old string compare could not see (the *strings*
      differed, but the check never ran on a lambda whose type came from the anonymous-fn node).
      Corrected to the effect they actually have.

    `sort`'s six monomorphic entry points and `hof`'s `i64`-only combinators are now fixable
    (R28), and `type` aliases for fn-pointer types become usable. Sweep clean, docscheck 219/219,
    stdlib unit tests clean.


- [x] **R34 — the declared-effect check ran only on literal-only bodies.** `mHasUnpredictableStack
      = true` sat after the closing brace of the "signature found" block in both the `IDENTIFIER`
      (`typecheck.cc:2632`) and `SCOPED_IDENTIFIER` (`:3631`) call paths, so every call switched off
      the arity check, the `if`-arm rule and the `defer` rule for the rest of the function. Since
      `3267700f` (2026-03-24). Moved into the not-found branch of both. Also closed here, because
      turning the checks on exposed each of them within the hour:

    - **R6** — `switch` arms are now depth-checked like `if` arms, with the no-`_` rule that arms
      must leave the stack as found; `switch_arms_unbalanced`, `switch_no_default_changes_stack`.
    - **Failure arms were modelled from the success stack.** `drop` in a failure arm type-checked
      and, at runtime, ate the caller's value (sentinel probe: caller's final `drop` underflowed).
      `flag::int`/`float` and `examples/errors` did exactly this. The arm now starts from the
      pre-call stack; `drop` there is a compile-time underflow. `Ok` seeding follows the same rule
      in `switch`, and a literal `1` arm is an error-code arm (spec 10.3), not `Ok`.
    - **Fallible-call `if` arms are no longer exempt from the depth rule**, and a fallible `if`
      with no `else` merges to the pre-call stack. The spec's §10.6 example needed fixing.
    - **The failure exit of a fallible function truncates the stack** to entry depth minus inputs
      (`qd_stack_truncate`, emitted in the return block when `has_error` is set). Probe:
      `fn g(i64 -- i64)!` panicking before consuming its input left depth 1 in the caller's failure
      arm; now 0. This is what makes the validator's model true for every callee body.
    - **Balanced arms kept the pre-`if` types** (no `== 0` branch in the merge) — the
      `flag::float ... expects float but got string` report.
    - **`blockEndsDiverging` looked only at the last statement**, so `panic  0.0` was not
      diverging; `break`/`continue` (spec 6.1.1) were never checked.
    - **Fallible method calls before `if`/`switch` were not recognised** (bare-name lookup; methods
      are keyed mangled). One helper, `bareFallibleCallBefore`, now serves both.
    - **Module functions with no declared outputs took their produces from the isolated body
      residual** (`modules.cc:1361`), so `bytes::fill` "produced" four values and every `( -- )`
      module function with a loop leaked its inputs into the caller's model. Declared outputs only,
      and the module builder's scalar mapping now goes through `stringToStackValueType` (sized
      integers were `any`).
    - **Method-call argument checks did not skip `TYPEVAR`**, unlike function calls.

    Fallout across stdlib + examples was three functions (`flag::int`, `flag::float`,
    `regex::get_cclass`) plus `examples/errors`; nine tests and four doc pages encoded the old
    conventions. Sweep clean, `docscheck` 218/218, stdlib unit tests clean. Spec §6.1.1, §6.4.1
    and §10.3 rewritten to say what the implementation now enforces.

- [x] **R35 — `ident <<field` folded the identifier into the field-access node.** The parser
      deleted the preceding `IDENTIFIER` and stored its name as `varName`; codegen reconstructed
      the meaning (struct type → function → global → captured → local), the validator had no
      function branch, so a call with arguments before `<<` left phantom values equal to its
      parameter count. Fixed by not folding: `<<field` always reads the struct on top of the stack
      (option B), as `>>field` already did. `AstNodeFieldAccess`/`AstNodeFieldSet` carry only the
      field name; `__global_error__` (`error <<code`, one use — inside `run_tools_test.sh`, now `err`) removed; validator, codegen, LSP
      (`precedingIdentifierName`) and linter updated. Three silent dependents surfaced and were
      made stack-based: the construction-time field evaluator (no `<<` case — `math.qd` reported
      `Vec2` as a float field), the isolated analysis (pushed without popping), and closure
      codegen (captured variables lost their struct type, so `v <<x` in a closure hit the
      ambiguity fallback). Zero-argument calls before `<<` on an ambiguous field name now resolve
      too. `>>field` gained a value-type check. Tests: `structs/field_access_operand_forms`,
      `compile_errors/field_access_on_scalar`, `field_access_unknown_field_typed`. 135 test files
      exercise the form; suite green, valgrind clean on the changed retain/release path.

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

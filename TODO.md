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

- [x] **Done 2026-09-19: `pick` is fixed-depth and `roll` is gone.** Both took a **runtime**
      index, which defeats static stack tracking: nothing could say what a body containing one
      leaves, and code generation refused them outright in compile-time-stack functions.
      `pick` is now the third-item copy Factor keeps, `( x y z -- x y z x )` — checkable, and
      handled on the compile-time stack like the other shufflers rather than hard-erroring.
      `roll` is no longer a word; it stays as the internal op code generation emits to bring a
      method's receiver to the top, and a use of the word reports the removal with the rewrite,
      the way the other removed shufflers do. Corpus blast radius was as measured: `pick` 0 uses,
      `roll` 2 (both in `tests/qd/stack/advanced.qd`).

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
      **Open question**: with JSON out of the argument, is the error channel (R24) enough on its
      own to justify the feature, or does it want a purpose-built error value instead?

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

- [x] **Fixed 2026-09-19: R9. `docscheck` covered 16% of the documented examples.** It checked
      only blocks defining `fn main`, so a page that shows no whole program went unchecked —
      which is how the front page came to open with `fn double(x:i64 -- result:i64) { 2 * }`,
      a body that underflows because naming `x` consumes it.

      Three classifiers were added to `tools/check_docs.py`:

      - a block whose every top-level line opens a declaration is a compilation unit already,
        so it is compiled with an empty `fn main` appended;
      - `doccheck: continues` compiles a block after the nearest declaration-only block above it
        in the same file, which is how a tutorial page is written — declare `divide` in one
        block, use it in the next four;
      - `doccheck: page-context <preamble>`, a file-level marker: every fragment on the page is
        compiled with that preamble and wrapped in `fn main`, and if the fragment leaves values
        on the stack it is compiled again with as many `drop`s as the diagnostic says it left.
        This is what makes a generated stdlib page checkable, where every block is a one-line
        `@example` from the module's source.

      **Coverage went from 219 to 485 blocks**, and every failure it surfaced is resolved: the
      broken examples are fixed, and 54 blocks carry a `doccheck: skip <reason>` naming why they
      cannot be checked. The largest class of real breakage was one mistake repeated across nine
      pages — a body written stack-direct under a signature with named parameters — and the
      second was `-> x  // bind parameter` lines left from before parameters bound themselves.
      One of them was in `reference.def`, so the compiler's own reference table shipped it.

- [x] **Fixed 2026-09-19: every stdlib module's examples are compiled.** All thirty-six are
      opted in. `docscheck` now checks **1,010 blocks**, up from 219 when this started. What is
      left unchecked is 385 prose fragments on the hand-written pages (`point <<x`, a line of
      shuffling, a stack trace) plus the two hand-written math pages, whose examples use values
      the prose supplies; checking those needs a way to say what context a fragment assumes,
      which none of the classifiers answers.

      Three generator bugs surfaced while doing it, each of which had been quietly mangling
      pages: a `|` in a doc comment split the packed record (`math::abs` documents its return as
      `|x|`, and lost its description, grew a phantom error table and rendered its example as
      `||-5.0 math::abs print`); only the first `page-context` line of a module was emitted; and
      the `-> result` annotation was matched greedily, so an example that binds a value
      mid-line had the rest of the line swallowed into a comment -- which is how
      `io::readline`'s example was unparseable and no one could see it. The annotation is now
      recognised only when what follows the last arrow is a list of names or literals.

- [x] **Fixed 2026-09-19: an anonymous function's body is stack-checked.** It was not, and the
      answer to the open question was that the validator walked past it: the case in
      `typeCheckBlock` pushed a PTR and moved on, under a comment saying code generation would
      validate the body, which it does not. `5 fn (x:i64 -- r:i64) { 2 * } call` compiled and
      died at run time with *"Fatal error in mul: Stack underflow"* where the identical body in
      a named function is a compile error. `typeCheckAnonymousFunction` now checks the body the
      way a named function's body is checked -- on its own empty stack, with named parameters
      bound and captures typed from the scope the lambda was written in -- from all three places
      a lambda can appear: a statement, a struct literal's field, an array literal's element.

      It found three more broken `hof` examples that the earlier pass had missed
      (`fn (x:i64 -- r:i64) { dup * }`, which underflows for the same reason), two callbacks in
      `os_test.qd` that never consumed their argument, and two C++ fixtures that asserted zero
      errors for bodies that underflow. All fixed.

- [x] **Fixed 2026-09-19: an anonymous function follows a named function's rules, all of them.**
      The two had drifted apart in seven places, each one silent. A bare type name in a lambda's
      parameter list was read as a parameter *name* with no type, so `fn (Str -- ) { drop }` bound
      a local called `Str` and underflowed; a named function's parameter list had always read it
      as a type. Unnamed inputs were accepted and quietly left on the stack, where a named
      function rejects them and points at `stack fn` -- which a lambda had no way to spell.
      Parameter type names were not checked, so `fn (x:Nope -- )` compiled. Unused parameters were
      not reported. Fallibility was inherited from the enclosing function, so a `panic` inside a
      plain lambda in a fallible function passed validation and was then compiled by a generator
      that knew the lambda could not throw. Outputs were compared by primitive type only, never
      structurally. And a lambda could not be marked `!` at all.

      Now: `stack fn (...)` and `fn (...)!` parse wherever a lambda may appear, binding is decided
      by `stack` in the parser, the validator and the generator alike (all three used to apply
      their own "all parameters named" heuristic), and a fallible lambda's call site behaves like
      a call to a fallible name -- `call!` aborts, `call?` propagates, a bare `call` is read by
      `if` or `switch`. Fixing the last one fixed a bug that had nothing to do with lambdas:
      `call!` on a pointer to a *named* fallible function never checked for the error, so the
      program ran on past a panic with nothing on the stack.

      The 65 lambdas in the corpus written with unnamed parameters now say `stack fn`.

      **Known limit, shared with named functions**: an `fn(...)` type string cannot say `!`, so a
      fallible lambda stored in a field or returned as a `ptr` loses its fallibility on the way
      out. A named function's pointer loses it the same way. Fixing it means giving the type
      syntax somewhere to put the mark.

- [x] **Fixed 2026-09-19: an `if` without an `else` has its arm checked.** It had no check at
      all. An arm that left values behind was not applied to the stack model either, so the
      function-level arity check then ran against a stack that pretended the arm did nothing:
      `fn main() { 1 0 == if { 42 } }` was accepted with no diagnostic anywhere. Specification
      6.1.1 *said* a bare `if` was "governed by the enclosing function's declared effect", which
      could not be true for that reason; the rule now matches the two that were already there --
      an `if`/`else` whose arms disagree is rejected, and a loop body must be neutral -- because
      a bare `if` is an `if` whose other arm is empty. A diverging arm is exempt, as before, and
      the separate rule for an `if` after a bare fallible call is unchanged.

      **Fallout over all 1,111 `.qd` files in the repository: 15 sites, 14 of them real bugs.**
      Every guard in `ct`'s `Queue`, `Deque`, `Map` and `Set` was written `"..." 1 err` -- which
      *reads* the last error rather than raising one, so the message went onto the stack as data
      and the function fell through into the code it was guarding. `q dequeue` on an empty queue
      died in `mod` with "Division by zero" instead of reporting. `Vec` used `panic` and worked,
      which is why the module looked fine. All fourteen are `panic` now, and the five error paths
      that had no test have one. The fifteenth was a linter fixture whose deliberately-nested
      innermost arm left a value; it gained a `drop`.

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

- [x] **Decided and done 2026-09-19: a sized integer type is rejected where it has no width to
      describe.** The specification already answered this in its own normative sentence — these
      types "are valid as struct field types and as the width selector on raw memory accessors" —
      and the divergence note under it recorded that the implementation accepted them elsewhere
      and silently did nothing. So the fix was to make the implementation say what the
      specification says, rather than to invent width semantics for positions that have no width:
      a parameter, a return value, a module-level `var` and a `cast` target are all 64-bit stack
      slots.

      The probe turned up a fourth inert position the item had not listed: a module-level
      `var count:u32 = 1000` stored a `u32` in name only, and its own test said so in a comment.
      All four are rejected now, with a message that names the two places a width does mean
      something and suggests masking (`value 255 and`) for the truncation `cast<u8>` looked like
      it offered. Corpus cost: **zero** — no signature, cast or global in `stdlib`, `examples`,
      `tests` or `cmd` used one, and the eight `sized_ints` tests are all struct fields and are
      untouched. The specification's divergence note is gone.

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

      **Still open**, and the reason this item is not closed: the channel is still global. `err`
      empties on read, the state survives intervening non-fallible calls, and `error::last` has
      to run first in the failure arm. If that turns out to matter in practice, the stack-passing
      design is the fix and `Error` is already the type it would push -- so nothing here is
      wasted. The evidence to watch for is whether anyone uses the module.
      **Still open**: whether sum types subsume it. They do not appear to -- the control flow
      already exists and is enforced, and what is missing is a payload type, which a refcounted
      struct already is.

- [x] **Fixed 2026-09-19: a thread's stack is as big as the context that spawned it.** It was a
      hardcoded 1,024 elements in both spawn paths (`stdlib/thread/src/thread.c`,
      `lib/rt/src/runtime.c`), so a thread body that pushed more died telling the user to *"use -s
      to increase stack size"* — advice that could not reach it. Both now take the spawning
      context's capacity, which is what `-s` sets, and `QD_DEFAULT_STACK_SIZE` in `rt/stack.h`
      names the fallback that was a bare 1024 in three places. Verified: a thread that pushes
      3,000 values overflows at the default and runs under `-s 8192`.

#### Cuts with corpus evidence

- [x] **Done 2026-09-19: `printsv` is cut; `prints` and `printv` stay.** Re-counted over
      `stdlib`, `examples`, `tests` and `cmd`: `printsv` **0 uses anywhere**, `printv` 5 (all in
      its own tests), `prints` **38** — it is how a good many tests print their result, so the
      original count ("0 outside tests") understated it. `free` (109 uses) and `dec` were never
      removable: `mem::free` is implemented *with* `free`, and `dec` is the same instruction as
      `--`, which has 37 real uses.

      So the cut is the one form that nothing has ever used, in tests or out. What is lost is
      "the stack, with types", which is the cross-product of two aids that both remain; what is
      kept is the debugging affordance the item was worried about. Writing `printsv` now reports
      the removal and points at `prints`, the way the removed shufflers do.

- [x] **Answered 2026-09-19 by porting: the bound dialect is right for almost all of it, and
      the exceptions have a shape.** `hof` was the sharpest case and it was ported both ways
      before choosing. What came out is a rule rather than a preference:

      **A body whose parameters are used once, in the order the caller pushed them, is
      ceremony.** Those bodies re-push what the caller already arranged. `apply` was
      `x f call`; as a `stack fn` it is the single word `call`, which is what the function
      *is*. `when` and `unless` go the same way, and the stack form says something the bound
      one hid: `swap 0 != if { call } else { drop }` shows that the function is thrown away
      when the condition fails.

      **A body that uses a value twice, or takes two functions, becomes a puzzle.** `bi`
      stack-direct is `pick rot call rot rot call` and `both` is
      `dup rot swap call rot rot call swap`. Both were written, both pass the tests, and
      neither tells a reader anything. `bi`, `tri`, `keep`, `dip`, `both`, `bi_star`, `times`
      and the seven array combinators keep their names. Deep access is capped at three now
      that `roll` is gone, which is the same reasoning arriving from the other side.

      Ported on that rule: `hof::apply`, `hof::when`, `hof::unless`, and the eighteen pure
      pass-through wrappers whose body was exactly the word being wrapped -- the fourteen
      `sys` port/load/store wrappers, `mem::free`, `path::parent`, `thread::sleep`,
      `regex::unwrap_re`. **Stack fn count: 27 -> 48 of 452 functions that take parameters.**

      Deliberately *not* ported: the other 41 bodies that are mechanically convertible by the
      same scan. `time::add` would become `+` and `term::up` would become
      `"\x1b[%dA" fmt::sprintf`; in both the bound form is where the argument order is
      visible, and the scan cannot tell "the name carries nothing" from "the name is the
      documentation". That is the line, and it is a judgement per function, which is what this
      item said it would be.

### Found while writing `json::parse` (2026-09-19)

Eight things the parser ran into. **Seven are fixed**: the memory bug, and six smaller ones that
all failed without a diagnostic, or with the wrong one — a formatter that broke working files, a
method call that resolved to nothing, a cast that quietly produced a string, a StringBuilder
that cut multi-byte characters in half, a container library that only held integers, and an
equality test that killed the process on two pointers. **One was withdrawn**: a struct from
another module can be named in a signature, and the message that said otherwise is fixed.
Nothing from the list is left open; what the work turned up on the way is in the section below.

- [x] **Fixed 2026-09-19: a struct stored in a field was never freed.** Three defects, each found
      by counting `qd_struct_alloc` against `qd_struct_release` on a loop that parses `[1,1]`:
      `>>field` overwrote a `str`/`ptr`/struct field without releasing the old value, though the
      destructor releases the field and the push that supplied the value retained it; `==`/`!=`
      released their operands only on the string path, so `node null ==` -- the test at the head
      of every walk over a linked structure -- leaked a reference per comparison; and the
      integer-only body scan let a call to a struct-returning function through, which put the
      caller on fast paths that store a local with no retain and rebind it with no release. Fixed
      in `generator_structs.cc`, `runtime_ops.c` and `generator.cc` + `generator_impl.h`
      respectively, along with the `nm`-derived symbol-map collision in `lib/qd/src/qd.cc` that
      surfaced with them. Parsing a 200-element document 100, 400 and 1,600 times now holds RSS
      at 13.5 MB; it was linear in the number of parses. `fib 30` and `tak` are unchanged, so the
      integer fast paths are still taken where they belong.
      A struct held in a field of a *stack-allocated* struct was the obvious next suspect --
      `generateLocalCleanup` releases only the `str` fields of one -- but 400,000 iterations of
      that shape hold RSS flat, so whatever balances it, it is not leaking.

- [x] **Fixed 2026-09-19: `quadfmt` split `==` and `!=` when it expanded a `switch` arm.** An
      arm labelled with a constant reads exactly like a struct construction — an uppercase
      name, a brace, an `=` inside — so a long one was expanded as a field list and re-emitted
      as `field = value`, turning `0 != if` into `0 ! = if`. `make format` could break a
      working file while `make fmtcheck` passed on the result. The field-initializer scan in
      `source_formatter.cc` now tells a lone `=` from the one inside `==`, `!=`, `<=` and `>=`;
      pinned by `tests/formatter/56_switch_arm_operators.qd`.

- [x] **Fixed 2026-09-19: a method call on a `*T`-typed local resolves.** The field's type was
      recorded as a bare pointer with the struct name discarded, so the call fell through to a
      builtin of the same name or to nothing at all. The name now reaches the validator
      (`semantic_validator_collect.cc`, `_modules.cc`) and codegen (`generator_structs.cc`),
      and the six `as Value` annotations in `stdlib/json` are gone. A `*T` field still accepts
      `null`, tracked separately from the struct name, since that is how a linked structure
      starts; pinned by `tests/qd/structs/pointer_field_methods.qd`.

- [x] **Fixed 2026-09-19: `cast<T>` to an unknown or struct type is an error.** It used to fall
      through to a STRING default, so `p cast<Node>` produced a string and the mistake surfaced
      far away. The struct case now points at `as`; `cast<T>` over a generic type parameter is
      unaffected. Pinned by `tests/qd/compile_errors/cast_to_struct.qd` and
      `cast_to_unknown_type.qd`.

- [x] **Fixed 2026-09-19: `ptr == ptr` was a runtime type error.** Two pointers now compare by
      identity, which is also how two struct values are asked whether they are the same struct.
      Ordering stays numeric-only on purpose — `<` on two addresses is still a type error.
      Specification 12.2 gained the operand rules for `==`/`!=`, which it had never stated;
      pinned by `tests/qd/structs/struct_identity_compare.qd`.

- [x] **Withdrawn 2026-09-19: a struct from another module *can* be named in a signature.**
      The finding was wrong. `fn f(b:sb::StringBuilder -- b2:sb::StringBuilder)` compiles and
      runs, in a program and in a module alike; what produced *"Invalid type
      'sb::StringBuilder'. Valid types are: i64, f64, str, ptr, any, or a struct name"* was a
      missing `use sb` in a scratch file that the compiler picked up as a sibling of the one
      being built. The message named the wrong problem, which is what made the mistake look
      like a language limitation — and cost a day's detour building around it. So the fix is to
      the diagnostic: a qualified name now says *"module 'sb' not imported. Add 'use sb' to name
      its types"*, or *"module 'sb' has no type named 'Nope'"* when the module is imported.
      `json`'s own byte buffer stays: it appends raw byte ranges, which is what its serializer
      wants, and it no longer stands for a limitation that was never there.

- [x] **Fixed 2026-09-19: a generic method's result was pushed as a bare type parameter.** Found
      on the way to the above, and the real obstacle in that area. `Box<i64> unwrap` calls
      `fn (b:Box<T>) unwrap<T>( -- v:T)`, and the result arrived as `typevar`, so `b unwrap 2 *`
      was *"Expected numeric types, got typevar and int"* — a container could only be read into
      something taking `any`, which is why nothing in the corpus passed one to a function of its
      own. The machinery existed for plain calls (`bindCallTypeParams` / `pushCallResults`) and
      the four method-call paths all bypassed it. They now bind the struct's type parameters
      from the receiver's type arguments. `tests/qd/generics/generic_method_result.qd` pins it.

- [x] **Fixed 2026-09-19: `ct::Vec` and `ct::Map` only worked for `i64`.** `push`/`get` went
      through `mem::set_i64`/`get_i64` whatever `T` was, so `Vec<f64>` and `Vec<str>` read back
      `0` and `Vec<SomeStruct>` segfaulted; the module's tests only ever instantiated `i64`.
      Elements now live in tagged slots — `mem::set_any`/`get_any`/`clear_any`, eight bytes of
      value and eight of type, the shape the compiler already gives a generic struct field —
      which also makes the container the owner of a string or a struct it holds. `Vec`, `Queue`,
      `Deque` and `Map` are covered for i64, f64, str and struct elements, and the memory suite
      has a `containers` case that catches a missing release. Two more found in passing: a
      module struct's own type parameters were being qualified with the module name (`U` became
      `ct::U`), so `Pair<i64, str>` could not be constructed; and `Map`/`Set` sized the key copy
      with `strings::len`, so `"héllo"` and `"héll"` were the same key.

  Also found on the way and **fixed 2026-09-19**: `sb::append` sized its copy with
  `strings::len`, which counts codepoints, so appending any non-ASCII string cut it inside a
  character, and `sb::append_char` wrote a codepoint as a single byte. Both go by bytes now,
  `append_char` encodes UTF-8, and `sb::len` is documented as the byte count it always was.

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

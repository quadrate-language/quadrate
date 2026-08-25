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
- [ ] (Lower priority) **Reconsider the `>>field` / `>>field!` split.** Three sigils for two
      operations (`<<`, `>>`, `>>!`, plus `as` for disambiguation), where the split is purely "does it
      leave the struct behind" — inferable, or expressible as `>>field drop`. Two forms doubles
      formatter, LSP, linter and doc surface for a modest ergonomic win.
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

### Compiler & runtime

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

- [x] FIXED — **`create_test_context` in `lib/rt/tests` and `lib/mem/tests` hand-rolled the
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

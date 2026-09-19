# Changelog

All notable changes to the Quadrate programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **`error` — errors as values.**
- **`docscheck` checks 1,010 of the documented examples, up from 219.**
- **Specification §11.2.2, "Ownership"**: who retains and who releases, which §11 never said.
- **`memory` suite case `holders`**: an array of strings, an array of structs, a closure's captured block, a deferred block, a generic struct field and a plain string field, each built and dropped 20,000 times.
- **`mem::set_any`, `mem::get_any`, `mem::clear_any` and `mem::AnySize`**: a slot that carries a value's type with it — eight bytes of payload and eight of tag, the shape the compiler already gives a generic struct field.
- **`json::parse` — a real JSON parser.**
- **`tests/run_memory_test.sh`** (suite `memory`, test `memory_test`): three programs that build and drop the same structure at two iteration counts a hundredfold apart, failing if peak RSS grows by more than 4 MB between them.
- **`stack fn` — a function whose arguments stay on the stack.**
- **`while` is back, and the condition is written once.**
- **`strings::byte_len`**, the size of a string's UTF-8 encoding.
- **`math::inf`, `math::nan`, `math::is_nan`, `math::is_inf`, `math::is_finite`.**
- **`print` renders arrays.**
- **Exhaustiveness warning for a `switch` over an enum.**
- **`sort::by`, `sort::is_sorted_by` and `sort::lower_bound_by`**: sorting, checking and binary search ordered by a caller-supplied comparator, using the same negative/zero/positive convention as C's `qsort`.
- **Function-pointer tests** for dispatch through a struct field and through a declared parameter, plus rejections for a signature mismatch and an unconsumed result.
- **Generics tests** for `[]T`, `fn(T -- T)` and a returned `Box<T>`, through plain and module-qualified calls, plus rejections for a binding conflict and a parameter mismatch.
- **`switch` arm stack-effect rule** (spec §6.4.1).
- **`qd_stack_truncate(ctx, target)`** in `libqdrt`, and a matching freestanding stub.
- Regression tests pinning the checks that a call in the body used to switch off — arity, `if`-arm balance and `defer` effect — and the stack rules for the arms of a fallible call.
- **fish completions** (`completions/quad.fish`), covering all ten tools with per-option descriptions, subcommand dispatch for `quad` and `quadpm`, and passthrough so `quad lint --<TAB>` offers what `quadlint` accepts.
- **zsh completions** (`completions/_quad`), covering all ten tools with per-option descriptions, `.qd`-filtered file arguments, and subcommand dispatch for `quad` and `quadpm` — including passthrough, so `quad lint --no-<TAB>` offers what `quadlint` accepts.
- **`quaddoc` reports files it could not parse**, as `file:line:column: warning: ...`, the located form `quadc` and `quadlint` use, and carries on documenting what did parse.
- **`qdcli::Help`** (`lib/cli/src/help.cc`): a shared renderer for the tools' `--help` pages.
- **`tests/run_cli_surface_test.sh`** (suite `tools`, test `cli_surface_test`): 277 checks pinning what all ten tools share — the shape of `--help`, the format of `--version`, how an unrecognised option is reported, and that the bash completions still list every option each tool documents.
- **`completions/quad.bash` rewritten** against the tools' actual flags, and covered by `cli_surface_test`, which now fails if an option in any tool's `--help` is not completable.
- **Long forms for `quadc`'s short options**: `--output`, `--debug`, `--stack-size`, `--include` and `--module` alongside `-o`, `-g`, `-s`, `-I` and `-l`, and `--output` for `quaddoc`'s `-o`.
- **`--no-color` on `quad` and `quadmcp`**, the last two tools that rejected it.
- **`BaseOptions::optionError`**: lets a tool's option handler report what was actually wrong with a flag instead of `parseArgs` falling back to "unknown option", which blamed the flag rather than its missing value.
- **`SemanticValidator::finalStackTypes()`, `finalStackStructTypes()` and `finalStackFunction()`**: what the most recently type-checked function body leaves on the stack, including the struct type of each pointer.
- **`quadrepl` `:doc <name>`**: shows a name's stack effect — builtins and keywords from the compiler's own reference table, standard library functions from their declaration in the installed `.qd` sources, and functions defined earlier in the session.
- **`quadrepl` command aliases**: `:quit` and `:exit` alongside `exit`/`quit`/`:q`, and `:save`/`:load` alongside `.save`/`.load`.
### Changed

- **Twenty-one functions are written in the stack-direct dialect, and the rest deliberately are not.**
- **A sized integer type is rejected where it has no width to describe.**
- **`printsv` is removed.**
- **`pick` copies the third value and no longer takes an index; `roll` is gone.**
- **`strings::char_at` is total: an index outside the string is `strings::NotAChar` (-1), not an error.**
- **`unicode` is now actually Unicode, and keeps its name.**
- **`unicode::is_ident_start` and `is_ident_cont` are pinned to ASCII.**
- **`str` is genuinely UTF-8, and its unit is the codepoint.**
- **No string operation can produce invalid UTF-8 any more.**
- **Case mapping covers the cased scripts, not just ASCII.**
- **`is_alpha`, `is_alphanumeric` and the trim family are per codepoint.**
- **`cast<T>` no longer converts a string to a number; that direction is a compile error.**
- **Float division by zero follows IEEE 754 instead of aborting.**
- **Array literal elements may be any single-value expression.**
- **The element type of an array literal is inferred from all of its elements**, not just from element zero and only when that was a scalar literal.
- **Specification §11.2.1, "Reference Cycles"**: reference counting alone must not be expected to reclaim a cycle, an implementation is not required to detect one, and this one provides neither a cycle detector nor weak references — a program that builds cyclic structures must break them itself.
- **`hof` is generic.**
- **Arrays decide their element type on first use.**
- **The function-level output check compares pointer types structurally**, with the function's own type parameters as wildcards — inside `fn map<T, U>( … -- r:[]U)` the result is built from an empty literal, which is `[]any`, and U is unbound in the body.
- **A function pointer can be called through a struct field or a declared parameter.**
- **`call` no longer switches off the function-level checks.**
- **A function pointer is checked against the field it initialises.**
- **`buildFnTypeString` renders declared type names**, so a function returning a struct is `fn( -- Point)` rather than `fn( -- ptr)` — the coarse stack types cannot tell those apart, and a field declared `fn( -- Point)` would not have matched.
- **Generic type parameters unify structurally, so `[]T` and `fn(T -- T)` work.**
- **A type parameter used twice must bind to one type.**
- **Module-qualified calls check their argument types.**
- **An instantiated generic struct keeps its type arguments.**
- **`ident <<field` is no longer folded by the parser.**
- **The declared-effect, `if`-arm and `defer` checks now run in functions that call other functions.**
- **A failure arm is modelled from the pre-call stack, and a `drop` in it is a compile error.**
- **The success arm of a fallible `if` must leave the same depth as the failure arm.**
- **A fallible function's failure exit truncates the stack to what the caller expects**— the depth at entry minus the declared inputs and receiver.
- **Balanced `if`/`else` arms now yield the arms' types, not the pre-`if` types.**
- **A block diverges when any top-level statement in it is `panic`, `return`, `break` or `continue`**— not only when the last one is.
- **A fallible *method* call before an `if`/`switch` is recognised as one.**
- **Module functions with no declared outputs produce nothing.**
- **Generic type parameters are skipped in method-call argument checks**, as they already were in function-call checks.
- **Diagnostics** for the `if` rule report the arms' resulting depths (`then: 2 value(s), else: 1 value(s)`) rather than effects measured from different bases, and carry a hint after a fallible call.
- **`quaddoc` reads declarations from the compiler's AST.**
- **Parameter tables describe the parameters the function actually has.**
- **Every tool's `--help` follows one layout**: `<tool> - <summary>`, a description paragraph, `Usage:`, an `Options:` block opening with `-h`/`-v`/`--no-color` in that order and worded identically, any tool-specific sections, then `Examples:`.
- **Compiler diagnostics drop the `quadc: ` prefix** when they carry a source location: `main.qd:7:2: error: ...` rather than `quadc: main.qd:7:2: error: ...`.
- **`quadpm`'s diagnostics match the other tools.**
- **`quadmcp --version`** reports `quadmcp <version> (<commit>, built <date>)` like its siblings instead of a bare `quadmcp 0.5.0`.
- **`quadlint` reports a bad option value once.**
- **`quadc` with no arguments** reports `quadc: no input files` on stderr and exits 1, rather than printing its whole help page to stdout with a failure status.
- **`quadrepl` keeps values on the stack.**
- **`quadrepl` tab completion reads the standard library from disk.**
- **`quadrepl` compiles and runs each line once.**
- **`quadrepl` accepts `struct`, `enum`, `const`, `type` and `var` declarations.**
- **u8t tokenizer updated to 1.4.0.**
### Fixed

- **An `if` without an `else` has its arm checked.**
- **Every error path in `ct`'s `Queue`, `Deque`, `Map` and `Set` was dead.**
- **A thread's stack is as big as the context that spawned it.**
- **An anonymous function follows a named function's rules.**
- **An anonymous function's body is stack-checked.**
- **The `-> result` annotation in an `@example` no longer swallows the line.**
- **`hof`'s examples pass their argument to the function they demonstrate.**
- **The math module's examples show the arguments in the order the language takes them.**
- **A doc comment can contain a `|` again.**
- **The front-page example compiles.**
- **Two pointers compare by identity instead of killing the process.**
- **A generic method's result takes the type its receiver's type arguments give it.**
- **Naming a type from a module that is not imported says so.**
- **`ct::Vec`, `ct::Queue`, `ct::Deque` and `ct::Map` hold any element type, not just `i64`.**
- **`Pair<T, U>` can be constructed.**
- **A map key is copied by bytes.**
- **A value read out of a `*T` field knows it is a `T`.**
- **`cast<T>` rejects a target it does not convert to.**
- **`sb::append` copies bytes, `sb::append_char` writes UTF-8.**
- **`quadfmt` no longer drives a space through `!=` and `==`.**
- **A struct stored in a field is freed when the field stops pointing at it.**
- **`==` and `!=` give back the references they were handed.**
- **A function that calls one taking or returning something other than `i64` is no longer compiled as integer-only.**
- **A script's own function is no longer shadowed by a used module's function of the same name.**
- **A local declared in a loop body gets one binding per iteration.**
- **A closure can capture a `for` iterator, and captures it by value.**
- **An anonymous function can appear wherever a value can — a struct-literal field, an array-literal element, a nested literal of either.**
- **`!` and `?` now fire for `mem`, `net` and `http::run`, which they did not at all.**
- **`strings::substring` returns the error it declares instead of aborting.**
- **`json` no longer aborts or hangs on a malformed document.**
- **`drop` releases what it discards.**
- **An assertion that fails inside a helper now fails its test.**
- **Character indexing is no longer O(n) per access.**
- **Calling a module's imported function by its bare name is now a compile error instead of silently wrong code**, including inside the module that declares the import — which is the case that was wrong, since a module's own bodies are not type-checked when a program imports it.
- **Nested array literals parse.**
- **`--dump-ast` names eleven node types it was printing as `Unknown`**: `ArrayLiteral`, `ArrayIndex`, `FieldSet`, `LoopStatement`, `ImportStatement`, `GlobalVarDeclaration`, `FunctionPointerReference`, `Comment`, `TestDeclaration`, `AnonymousFunction` and `TypeAliasDeclaration`.
- **`quaddoc` signatures with a function-pointer parameter are no longer truncated**— the fault was in `docs/gen_docs.sh`, not the renderer.
- **`gen_docs.sh` preserves `/// doccheck:` directives**, emitted as an `<!-- doccheck: … -->` comment before the example fence.
- **An array that never received an element leaked its buffer.**
- **`sort::ints`, `sort::ints_desc`, `sort::floats` and `sort::floats_desc` returned unsorted data.**
- **`flag::int` and `flag::float` corrupted the caller's stack when the flag was missing.**
- **`regex::get_cclass` pushed the integer `0` where the signature promised a `ptr`** on the not-found path; it now pushes `null`.
- **`examples/errors/errors.qd`** dropped a non-existent error value in both failure arms and pushed a dummy result before `panic`; it now reads the error with `err` and matches the spec.
- **Documentation examples that left values on the stack or read the wrong one**, in `reference/generics.md`, `learn/8-examples/user-input.md` and the specification's §10.6 worked example.
- Nine language tests encoded the pre-spec convention of pushing an error value before `panic`, or declared no outputs for a function that returns one, or relied on the checks being off.
- **`quaddoc` and `quadmcp` had no shell completion in practice.**
- **The `sys` module was missing from the documentation entirely**-- all 17 functions and the module page.
- **The `ct` container library was missing its functions**-- `vec`, `deque`, `hashmap`, `queue`, `pair` and `set` published a struct and nothing else, and `ct` itself showed 4 of its 22 functions.
- **Signatures with a function-pointer parameter were truncated mid-type.**
- **Receiver methods were rendered as syntax that does not exist**: `fn f:Flag destroy()` rather than `fn (f:Flag) destroy()`.
- **The cross-reference graph counted words in comments, builtins and field accesses as calls.**
- **`ImportedFunction::line` and `::column` were wrong in any file containing non-ASCII before the declaration.**
- **`tests/quadpm/test_quadpm.sh` blocked for a minute and then failed tests 36 and 37** on any machine with `commit.gpgsign` set globally.
- **`quadmcp` ignored an unrecognised option and started serving anyway**, exiting 0. A mistyped flag therefore looked like a hang to whoever typed it, since the process sat waiting for JSON-RPC on stdin.
- **`quadpm` printed its entire help page to stdout on an unknown command**, burying the one line that said what was wrong.
- **`echo 'code' | quadc` could not reach the stdin path.**
- **`quaddoc -o`, `--title` and `--css` with no value** were reported as unknown options rather than as options missing an argument.
- **`use` of any standard library module failed from a build tree.**
- **`quadrepl` rewrote the word `print` inside string literals.**
- **`quadrepl` prompt escape sequences** are wrapped in `RL_PROMPT_START_IGNORE`/`RL_PROMPT_END_IGNORE`, so readline no longer counts the colour codes as visible columns and misplaces the cursor on a wrapped line.
- **`quadrepl` `reset`** left the runtime stack in place while clearing the history behind it.
- **`quadrepl` history file** grew without bound; it is now capped at 5000 entries.
- Stack floats print as `2` and `1.41421` rather than `2.000000`; long strings are elided in the prompt but shown whole by `stack`.
- `qd_clone_context` leaked `error_context` when the `program_name` copy failed.
### Removed

- **The `error { code = … message = … }` literal.**
- **`ctx` keyword**: `ctx { ... }` ran its body on a copy of the stack and appended only the body's top value to the parent.
- **Eight stack shufflers**: `drop2`, `dupd`, `nipd`, `over2`, `overd`, `swap2`, `swapd`, `tuck`.
- **Eight C runtime entry points** (`qd_drop2`, `qd_dupd`, `qd_nipd`, `qd_over2`, `qd_overd`, `qd_swap2`, `qd_swapd`, `qd_tuck`) from `<quadrate/rt/runtime.h>` and `libqdrt`.
- Phantom `error` builtin from the language reference.
## [0.5.0] - 2026-04-16

### Added

- **`inline` function modifier**: functions declared with `pub inline fn` are inlined at every call site via LLVM's `AlwaysInline` attribute. Works at all optimization levels including `-O0`.
- **`sys` standard library module**: zero-overhead inline wrappers for raw memory access (`sys::st8`/`st16`/`st32`/`st64`, `sys::ld8`/`ld16`/`ld32`/`ld64`), x86 I/O ports (`sys::port_in8`/`port_in16`/`port_in32`, `sys::port_out8`/`port_out16`/`port_out32`), and CPU control (`sys::cli`, `sys::sti`, `sys::hlt`).
- **Freestanding compilation** (`quadc --freestanding`): compile without libc for bare-metal targets. Emits `.o` with a `_start` shim using a static runtime context. Rejects hosted-only builtins and most stdlib modules at compile time.
- **Freestanding runtime** (`lib/rt/src/freestanding.c`): minimal no-libc runtime with static context, halt hooks (x86/ARM/AArch64), and value-stack operations. No heap, no I/O.
- **Freestanding-safe `mem` module**: split into `mem.c` (safe subset: `set_byte`, `get_byte`, `copy`, `zero`, `fill`) and `mem_heap.c` (heap-using: `alloc`, `realloc`, `free`). Freestanding builds link only the safe subset.
- **Raw memory builtins** (`__st8`/`__st16`/`__st32`/`__st64`, `__ld8`/`__ld16`/`__ld32`/`__ld64`): zero-overhead store/load lowering directly to LLVM instructions. Internal — use `sys::st8`/`sys::ld8` instead.
- **x86 I/O port builtins** (`__port_in8`/`__port_in16`/`__port_in32`, `__port_out8`/`__port_out16`/`__port_out32`): lower to inline assembly. x86/x86_64 targets only.
- **CPU control builtins** (`__cli`, `__sti`, `__hlt`): lower to single inline assembly instructions.
- **Type alias declarations** (`type` / `pub type`): compile-time type aliases resolved during semantic validation.
- **String interpolation** (`$"hello {name}"`): desugars to `sb::new`/`sb::append`/`sb::finish`.
- **Boolean literal type**: `true`, `false`, `Ok`, `Err` parsed as boolean literals.
- **Build cache**: content-based incremental compilation using FNV-1a hashing. Skips codegen when output is up-to-date.
- **Test coverage** (`quadc --coverage`): function-entry instrumentation with coverage report after test runs.
- **`quaddoc` cross-references**: call graph with `calls`/`calledBy` arrays and hyperlinks.
- **`quadlint` `//nolint` directive**: per-line or per-rule lint suppression.
- **Typed C embedding API**: `qd_pop_i()`, `qd_pop_f()`, `qd_pop_s()`, `qd_pop_p()`, `qd_error_code()`, `qd_error_message()`, `qd_clear_error()`, `qd_context_stack_size()`.
- **`quadpm` semver-aware update**: resolves constraints from `qd.json`, checks git tags, installs newest compatible version.
- **Improved semver conflict detection**: `rangesHaveCommonVersion()` probing across caret/tilde/explicit ranges.
- **`quadmcp` dynamic module list**: loaded from `docs/api/modules.json` instead of hardcoded.
- **Array syntax in signatures**: `[]i64`, `[]f64`, `[]str`, `[]ptr` types.
- **Function pointer syntax in signatures**: `fn(i64 -- i64)` types in struct fields and parameters.
- **Automated AUR package publishing** via SourceHut CI.
- **`cast<i64>`** now supports pointer-to-integer conversion via `uintptr_t`.
- **Bare-metal x86_64 kernel example** (`examples/kernel/`) with GDT, IDT, PIC remapping, PIT timer interrupts, PS/2 keyboard driver, VGA text output, screen scrolling, backspace, and a `> ` shell prompt. Written in pure Quadrate with assembly bootstrap.
- **New examples**: `csvcut`, `mandelbrot`, `brainfuck`.
- **New test suites**: build cache, freestanding compilation, raw memory, type aliases, string interpolation, inline functions (10 test cases).
- **Documentation**: `inline` keyword in specification, keyword reference, quick reference, and function tutorial. AUR installation instructions.

### Changed

- **`quadfmt` rewritten**: AST-based formatting replacing line-based approach. Formatter now preserves the `inline` keyword.
- **Version numbering** derived from git tags instead of a static `VERSION` file.
- **Deduplicated compiler errors**: parsing errors deduplicated by `line:column:message`.
- **Method call resolution**: uses `userFunctions` map instead of mangled name construction, fixing collisions with builtin instruction names.
- **LSP signature formatting**: empty parameter lists no longer produce spurious ` -- `.
- **Benchmarks modernized**: named parameters and operator syntax replacing stack manipulation.
- **CI**: `quadfmt --check` added to Arch, Alpine, and Debian builds.
- `AlwaysInlinerPass` now runs at all optimization levels (including `-O0`).
- `GlobalDCEPass` skipped in freestanding mode to preserve `pub` functions callable from assembly.
- Target triple and data layout set early in code generation for correct struct layouts on cross-compilation targets.
- Stack struct fields `capacity` and `size` changed from `size_t` to `uint64_t` for consistent layout across 32-bit and 64-bit targets.
- Freestanding mode: `pub` functions get external linkage so they survive DCE and can be called from assembly.
- `make format` now formats both C++/H files (via `clang-format`) and `.qd` stdlib/example files (via `quadfmt -w`).

### Fixed

- `<` operator code generation edge case.
- `cast<i64>` on pointer values no longer crashes with "Cannot cast type to integer".
- Compile-time stack path for `st8`/`ld8` family: fixed a latent bug where these builtins would read from the wrong stack in native functions.
- Formatter output validation distinguishes source parse errors from formatter bugs.
- i386 data layout patched with `i64:32:64` to match the System V ABI alignment.

### Removed

- `lineWidth` formatting option from `quadfmt`.
- `pkg/deploy-aur.sh` deploy script (replaced by CI-based AUR publishing).

## [0.2.1] - 2026-03-19

Initial tagged release.

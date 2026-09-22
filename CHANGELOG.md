# Changelog

All notable changes to the Quadrate programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Exponent notation for floats**: `1e3`, `1.5e-3` and `6.02214076e23`; a point or an exponent makes a literal an `f64`, the point needs a digit on each side (`5.` and `.5` report the float that was meant), and a literal too large for an f64 is an error.
- **`clone`**: a shallow copy of a struct — same type, fields copied, what a field points to shared — rejected where the struct type is not known.
- **Nested array types**: `[][]i64` is a type, `[n][]i64` builds one filled with distinct empty arrays, and qualified element types like `[]mod::Point` have a spelling. Every type position now reads every type form through one `parseTypeFrom`; nesting past 64 deep is rejected.
- **`[n]T` array literals, replacing the `make` family**: `[10]i64` is ten zeros, `[2]Point` two distinct instances rather than two nulls, `[]i64` an empty array. A type name hard against the `]` makes the brackets a size; `[10]` is still one element.
- **Array-typed struct field defaults**: `struct Bag { xs:[]i64 = []i64 }` and `Bag {}` work, so an array-holding struct can be default-constructed.
- **`fuzz_formatter` and `fuzz_lsp_text`**: the two fuzz targets `tests/fuzz/meson.build` had always named but which were never written; `tests/run_all.sh --suite fuzz` now runs all three.
- **`error` — errors as values (R24)**: `error::last` packs the `err` channel into an `Error` struct, `wrap` composes context, `root` walks the cause chain. The channel is unchanged, so `error::last` has to run first in the failure arm.
- **`docscheck` checks 1,010 of the documented examples, up from 219.**
- **Specification §11.2.2, "Ownership"**: who retains and who releases, which §11 never said.
- **`memory` suite case `holders`**: an array of strings, an array of structs, a closure's captured block, a deferred block, a generic struct field and a plain string field, each built and dropped 20,000 times.
- **`mem::set_any`, `mem::get_any`, `mem::clear_any` and `mem::AnySize`**: a slot that carries a value's type with it — eight bytes of payload and eight of tag.
- **`json::parse` — a real JSON parser.**
- **`tests/run_memory_test.sh`** (suite `memory`, test `memory_test`): three programs built and dropped at two iteration counts a hundredfold apart, failing if peak RSS grows by more than 4 MB between them.
- **`stack fn` — a function whose arguments stay on the stack.**
- **`while` is back, and the condition is written once.**
- **`strings::byte_len`**, the size of a string's UTF-8 encoding.
- **`math::inf`, `math::nan`, `math::is_nan`, `math::is_inf`, `math::is_finite`.**
- **`print` renders arrays.**
- **Exhaustiveness warning for a `switch` over an enum.**
- **`sort::by`, `sort::is_sorted_by` and `sort::lower_bound_by`**: sorting, checking and binary search ordered by a caller-supplied comparator, using C's `qsort` convention.
- **Function-pointer tests** for dispatch through a struct field and through a declared parameter, plus rejections for a signature mismatch and an unconsumed result.
- **Generics tests** for `[]T`, `fn(T -- T)` and a returned `Box<T>`, through plain and module-qualified calls, plus rejections for a binding conflict and a parameter mismatch.
- **`switch` arm stack-effect rule** (spec §6.5.1).
- **`qd_stack_truncate(ctx, target)`** in `libqdrt`, and a matching freestanding stub.
- Regression tests pinning the checks that a call in the body used to switch off — arity, `if`-arm balance and `defer` effect — and the stack rules for the arms of a fallible call.
- **fish completions** (`completions/quad.fish`), covering all ten tools with per-option descriptions, subcommand dispatch for `quad` and `quadpm`, and passthrough.
- **zsh completions** (`completions/_quad`), covering all ten tools with per-option descriptions, `.qd`-filtered file arguments, subcommand dispatch and passthrough.
- **`quaddoc` reports files it could not parse**, as `file:line:column: warning: ...`, and carries on documenting what did parse.
- **`qdcli::Help`** (`lib/cli/src/help.cc`): a shared renderer for the tools' `--help` pages.
- **`tests/run_cli_surface_test.sh`** (suite `tools`, test `cli_surface_test`): 277 checks pinning what all ten tools share — `--help` shape, `--version` format, how a bad option is reported, and that the bash completions list every documented option.
- **`completions/quad.bash` rewritten** against the tools' actual flags, and covered by `cli_surface_test`.
- **Long forms for `quadc`'s short options**: `--output`, `--debug`, `--stack-size`, `--include` and `--module`, and `--output` for `quaddoc`'s `-o`.
- **`--no-color` on `quad` and `quadmcp`**, the last two tools that rejected it.
- **`BaseOptions::optionError`**: lets a tool's option handler say what was actually wrong with a flag instead of falling back to "unknown option".
- **`SemanticValidator::finalStackTypes()`, `finalStackStructTypes()` and `finalStackFunction()`**: what the most recently type-checked function body leaves on the stack.
- **`quadrepl` `:doc <name>`**: shows a name's stack effect, for builtins, keywords, standard library functions and functions defined earlier in the session.
- **`quadrepl` command aliases**: `:quit` and `:exit`, and `:save`/`:load` alongside `.save`/`.load`.

### Changed

- **u8t tokenizer updated to 1.4.2**: one number scanner upstream instead of two near-identical copies. The token stream is unchanged over the test corpus and 200k random numeric inputs.
- **`sort` takes arrays, and only arrays.** All sixteen entry points take a `[]T`; the numeric ones took a raw `mem::alloc` buffer and a `count` beside it, so `[5 2 8 1 9]` could not be sorted by anything.
- **The `strings` FFI drops the count beside the array.** `sort`, `sort_desc`, `join` and `column` take a `[]str` and read its length; `split`, `split_n`, `lines` and `words` return the array alone. The two spellings for one job are gone — `strs count strings::sort` is `strs strings::sort` — and `words` over a string with no words returns an empty array rather than null.
- **`ct`'s containers write through the receiver instead of returning a new struct.** `Vec::push`/`pop`/`set`/`reset`, `Deque`'s four ends, `Queue::enqueue`/`dequeue`, `Map::insert`/`remove` and `Set::add`/`remove` return the container they were called on, and `release` clears the fields so a second one is a no-op.
- **`stdlib/regex` appends states instead of preallocating 256 null slots**, which cost 256 slots per compiled regex and left the unfilled ones as nulls.
- **`stdlib/regex` represents its NFA as typed structs rather than raw byte offsets**: `State`, `Nfa` and `Parser` records replace forty lines of hand-computed offset constants and their accessors.
- **`and`, `or` and `not` are logical; the bitwise operations moved to `bits` (R13).** `2 1 and` is `1` now, not `0`; the bitwise forms are `bits::and`, `bits::or`, `bits::xor` and `bits::not`, each an `inline stack fn` over one instruction. `lnot` is gone. This does not add short-circuiting, and R13 is closed anyway: the corpus has no site that needs it.
- **Two dialects, spelled explicitly.** Binding is the unmarked default and `stack fn` marks the function whose inputs stay on the stack; the choice per function is a readability judgement. 48 of 452 functions that take parameters are `stack fn`.
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
- **The element type of an array literal is inferred from all of its elements**, not just from element zero.
- **Specification §11.2.1, "Reference Cycles"**: reference counting alone must not be expected to reclaim a cycle, and this implementation provides neither a cycle detector nor weak references.
- **`hof` is generic.**
- **Arrays decide their element type on first use.**
- **The function-level output check compares pointer types structurally**, with the function's own type parameters as wildcards.
- **A function pointer can be called through a struct field or a declared parameter.**
- **`call` no longer switches off the function-level checks.**
- **A function pointer is checked against the field it initialises.**
- **`buildFnTypeString` renders declared type names**, so a function returning a struct is `fn( -- Point)` rather than `fn( -- ptr)`.
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
- **Generic type parameters are skipped in method-call argument checks.**
- **Diagnostics** for the `if` rule report the arms' resulting depths (`then: 2 value(s), else: 1 value(s)`) and carry a hint after a fallible call.
- **`quaddoc` reads declarations from the compiler's AST.**
- **Parameter tables describe the parameters the function actually has.**
- **Every tool's `--help` follows one layout**: `<tool> - <summary>`, a description paragraph, `Usage:`, an `Options:` block opening with `-h`/`-v`/`--no-color`, tool-specific sections, then `Examples:`.
- **Compiler diagnostics drop the `quadc: ` prefix** when they carry a source location.
- **`quadpm`'s diagnostics match the other tools.**
- **`quadmcp --version`** reports `quadmcp <version> (<commit>, built <date>)` like its siblings.
- **`quadlint` reports a bad option value once.**
- **`quadc` with no arguments** reports `quadc: no input files` on stderr and exits 1.
- **`quadrepl` keeps values on the stack.**
- **`quadrepl` tab completion reads the standard library from disk.**
- **`quadrepl` compiles and runs each line once.**
- **`quadrepl` accepts `struct`, `enum`, `const`, `type` and `var` declarations.**

### Fixed

- **A `type` alias no longer launders a sized integer type past the check that rejects it.** `type Byte = u8` then `fn f(b:Byte …)` compiled clean; the rejection ran on the written name, before alias resolution. It now reads through the chain at all five positions that reject one — parameter, return value, module global, cast target and anonymous function — and names both spellings: `'Byte' is an alias for 'u8', a memory-layout type`.
- **`ct::Vec` shared its buffer between stale copies, so a `release` freed memory another live `Vec` still pointed at.** `push` allocated a new `Vec<T>` on every call and returned it, sharing `data` with the value it was called on: the old value stayed valid with its old `len` and its pre-realloc pointer, giving a use-after-free and a double-free reachable without touching `mem`, and an allocation per element. It writes through the receiver now, as `>>field` does, and `pop`, `set` and `reset` — and the same shape in `Deque`, `Queue`, `Map` and `Set` — go with it.
- **`enum E { A = 1_000 }` compiled clean, with the wrong value**: the scanner handed the parser `1` and then an identifier `_000`, read as the next variant. Digit separators are now rejected by name wherever a numeric literal is scanned.
- **The interpreter tier refuses what it does not interpret, by name.** An anonymous function reported a parse error about a method declaration, and a method was accepted with its receiver dropped; both are refused now, before anything is recorded.
- **A `[]T` no longer satisfies a declared `ptr`.** An array could be handed to any of the 54 stdlib functions taking a raw `arr:ptr count:i64` buffer, which then wrote through the `qd_array_t` header. The rule applies to parameters, outputs and struct fields alike; `flag::parse`, `fuzzy::best` and the structs chapter's `Stack` are corrected to `[]T`.
- **An FFI `import` block can spell every type a signature can.** Its parameter list read one identifier after the `:`, so `[]str`, `fn(...)`, `*T` and `mod::T` came apart silently. Fifteen stdlib declarations that said `ptr` over a real array are corrected.
- **A field is checked the same way wherever it is written.** The five field-write sites had five answers; three did no pointer-type check at all. They now share one `fieldValueRejected`.
- **`<<field` reads the field the value's own type names.** With the type unknown the backend took the first struct in the module declaring that name and used its layout, silently returning the wrong value. The validator now records the resolved type on the node; an ambiguous fallback search is reported instead of guessed, and reporting it fails the compile.
- **An unterminated `<` in a type is a parse error.** `Box<` parsed, so the formatter wrote back output that no longer parsed.
- **A struct holding an array field releases it.** The generated destructor stopped at strings and nested structs, leaking 192 bytes an iteration for `struct B { xs:[]i64 }`. The unused second copy of the field walk, `generateStructCleanup`, is deleted, and the memory suite reads `VmHWM` instead of `getrusage(RUSAGE_CHILDREN)`, which hid the leak under 13 MB of floor.
- **An enum value written in binary, and one written with a leading zero.** `parseEnumDeclaration` used `strtoll` at base 0, so `010` was octal and `0b101` depended on the libc. All six such call sites, and `qd_eval`'s tokeniser, now go through one `readIntegerLiteral`.
- **A negative hex or binary literal lexes and compiles.** `-0x10` scanned as `-0` and an identifier `x10`; fixed upstream in libu8t 1.4.1, and the two workarounds it forced in `parseEnumDeclaration` are gone. The compiler's own two literal parsers tested the first character for `'0'`, so a sign hid the radix prefix; both now step over the sign and keep the bound exact at `-0x8000000000000000`.
- **An unterminated `/*` or `"` is a parse error.** Both were accepted in silence, and the formatter re-emitted a `*/` the author never wrote or grew a module name by a newline every pass. The string check now covers every place a string token is consumed.
- **A declared parameter struct type is no longer overwritten by inference.** Inference attributes an unattributable field access to the first parameter, so `fn connect(re:Nfa …)` was recorded as taking a `State` and every correct call was rejected. Inference is now the fallback for a parameter that declared no struct type.
- **The specification describes what `>>field` actually does (R21).** It read as a functional update; a struct is a reference and `>>field` writes through it, so every other name bound to it sees the change.
- **A function-pointer type keeps its `(` glued to the `fn`**, which the anonymous-function normaliser was separating, so `g:fn (i64 -- i64)` did not parse.
- **A quote's escaping is judged from the run of backslashes, and only inside a string.** Testing the single character before the quote read `"a\\"` as unterminated and made the formatter hand files back untouched.
- **A nested block comment is read to its own end.** `parseComment` dropped the last character before EOF and the formatter's brace scanners did not nest at all.
- **The formatter no longer drops what shares a line with a block's braces**, which lost the body of `fn one() { 1 print nl`.
- **The formatter takes a block's `{` from the parser instead of guessing**, rather than using the first `{` on the line.
- **A struct or `import` declaration copies out its own text, and only that.** Both copied whole lines, so a declaration sharing a line with what follows carried it along and emitted it twice.
- **The formatter hands back the source when a block's braces do not balance**, instead of scanning to EOF or emitting only a declaration's first line.
- **A `use` path is quoted whenever the parser would not read it back bare.**
- **A struct construction is only expanded when its fields survive being separated**, and a line left alone keeps its indentation. The old expansion never reached a fixed point and rewrote string literals in a live `switch` arm.
- **The struct-construction expansion stops at a comment**, rather than matching a `}` that is commented out.
- **Only a construction the line itself opens and closes is expanded**, and what follows it on the line has to balance too.
- **The code after a multi-line string or comment ends is accounted for**; the re-indenter stopped at the closing `"` or `*/` and missed a brace opened on the rest of that line.
- **A block comment's continuation lines are indented from the line that opened it**, instead of gaining a tab every pass.
- **An anonymous function followed by a bare `->` keeps no trailing space.**
- **A second anonymous function on a line is normalised on the same pass.**
- **The formatter's multiline-string tracker toggles instead of clearing**, so a line holding two quotes no longer makes the formatter diverge.
- **A token the parser cannot use is reported rather than swallowed.** Twelve such places left the file a brace short of what the parser had read and still parsed clean — a bare `$`, stray braces in parameter lists, type arguments and array literals, a lone `-`, `::` or `&` with no name after it, a field default with no value, `defer` with no block, and a struct construction reaching EOF. The four copies of the type-argument reader are now one, `parseTypeArgumentList`.
- **An enum value whose literal was cut short is rejected**; a negative hex or binary enum value is recorded in decimal, since it cannot be written back with the minus attached.
- **Parse errors come back in source order**, including the two only known once the whole file has been read.
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
- **An anonymous function can appear wherever a value can** — a struct-literal field, an array-literal element, a nested literal of either.
- **`!` and `?` now fire for `mem`, `net` and `http::run`, which they did not at all.**
- **`strings::substring` returns the error it declares instead of aborting.**
- **`json` no longer aborts or hangs on a malformed document.**
- **`drop` releases what it discards.**
- **An assertion that fails inside a helper now fails its test.**
- **Character indexing is no longer O(n) per access.**
- **Calling a module's imported function by its bare name is now a compile error instead of silently wrong code**, including inside the module that declares the import.
- **Nested array literals parse.**
- **`--dump-ast` names eleven node types it was printing as `Unknown`**: `ArrayLiteral`, `ArrayIndex`, `FieldSet`, `LoopStatement`, `ImportStatement`, `GlobalVarDeclaration`, `FunctionPointerReference`, `Comment`, `TestDeclaration`, `AnonymousFunction` and `TypeAliasDeclaration`.
- **`quaddoc` signatures with a function-pointer parameter are no longer truncated**— the fault was in `docs/gen_docs.sh`, not the renderer.
- **`gen_docs.sh` preserves `/// doccheck:` directives**, emitted as an `<!-- doccheck: … -->` comment before the example fence.
- **An array that never received an element leaked its buffer.**
- **`sort::ints`, `sort::ints_desc`, `sort::floats` and `sort::floats_desc` returned unsorted data.**
- **`flag::int` and `flag::float` corrupted the caller's stack when the flag was missing.**
- **`regex::get_cclass` pushed the integer `0` where the signature promised a `ptr`** on the not-found path; it now pushes `null`.
- **`examples/errors/errors.qd`** dropped a non-existent error value in both failure arms and pushed a dummy result before `panic`.
- **Documentation examples that left values on the stack or read the wrong one**, in `reference/generics.md`, `learn/8-examples/user-input.md` and the specification's §10.6 worked example.
- Nine language tests encoded the pre-spec convention of pushing an error value before `panic`, or declared no outputs for a function that returns one, or relied on the checks being off.
- **`quaddoc` and `quadmcp` had no shell completion in practice.**
- **The `sys` module was missing from the documentation entirely**— all 17 functions and the module page.
- **The `ct` container library was missing its functions**— `vec`, `deque`, `hashmap`, `queue`, `pair` and `set` published a struct and nothing else, and `ct` itself showed 4 of its 22 functions.
- **Signatures with a function-pointer parameter were truncated mid-type.**
- **Receiver methods were rendered as syntax that does not exist**: `fn f:Flag destroy()` rather than `fn (f:Flag) destroy()`.
- **The cross-reference graph counted words in comments, builtins and field accesses as calls.**
- **`ImportedFunction::line` and `::column` were wrong in any file containing non-ASCII before the declaration.**
- **`tests/quadpm/test_quadpm.sh` blocked for a minute and then failed tests 36 and 37** on any machine with `commit.gpgsign` set globally.
- **`quadmcp` ignored an unrecognised option and started serving anyway**, exiting 0, so a mistyped flag looked like a hang.
- **`quadpm` printed its entire help page to stdout on an unknown command**, burying the one line that said what was wrong.
- **`echo 'code' | quadc` could not reach the stdin path.**
- **`quaddoc -o`, `--title` and `--css` with no value** were reported as unknown options rather than as options missing an argument.
- **`use` of any standard library module failed from a build tree.**
- **`quadrepl` rewrote the word `print` inside string literals.**
- **`quadrepl` prompt escape sequences** are wrapped in `RL_PROMPT_START_IGNORE`/`RL_PROMPT_END_IGNORE`, so readline no longer misplaces the cursor on a wrapped line.
- **`quadrepl` `reset`** left the runtime stack in place while clearing the history behind it.
- **`quadrepl` history file** grew without bound; it is now capped at 5000 entries.
- Stack floats print as `2` and `1.41421` rather than `2.000000`; long strings are elided in the prompt but shown whole by `stack`.
- `qd_clone_context` leaked `error_context` when the `program_name` copy failed.
- **The specification is stamped 0.5.0**, the release it describes, with a Document History entry for it.
- **Appendix A no longer spells constructs the compiler rejects**: `make<T>`, the `error { … }` literal and `>>field!` are gone from the grammar, `while_stmt` and `return` are in it, `scoped_id` reaches `mod::Enum::Variant`, and a numeric literal's sign is part of the literal.
- **§2.4 no longer claims whitespace is insignificant.** Adjacency after `]` and the sign on a numeric literal both read it, and both are normative; the section says so and points at the two rules.
- **§13.6 marks `time::format!` and `parse!`**, the module's two fallible functions, which it omitted while listing the total ones.
- **§14.3 lists `qd_land`, `qd_lor` and `qd_lnot`**, the runtime entry points the logical `and`/`or`/`not` lower to, which it left out while classifying all six of the bitwise ones together; Appendix B.3 no longer files three logical operators under "Bitwise Operators".
- **`while` is in the keyword list, and §6.4 "Conditional Loops" is a sibling of `loop` and `for`** rather than a subsection of one of them.
- **`sizeof<T>` is documented with the stack effect it has** (`( -- n)`), not as though a type were pushed.
- **`nth` is documented once**, under array operations rather than also under stack rearrangement.
- **§3.5 drops `UNKNOWN`, `TAINTED` and `TYPEVAR`**, compiler internals a program cannot write.
- **§6.1.1 and §11.2.2 drop their changelog prose.** A specification states the rule, not its history.
- **§7.5 says an unused named parameter is a warning**, which is what it is.

### Removed

- **The five array-creation builtins**: `make`, `makei`, `makef`, `makes`, `makep`. Array creation is a literal now — see `[n]T` under Added. All five report the rewrite when used.
- **`xor` and `lnot` as builtins.** `xor` is `bits::xor`; `lnot` is `not`. Both report the rewrite when used.
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

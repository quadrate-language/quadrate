# Changelog

All notable changes to the Quadrate programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Nested array types.** `[][]i64` is a type now, and `[n][]i64` builds one. An array of
  arrays could always be *built* -- `[[1 2] [3 4]]` has worked since the nested-literal fix --
  but the type had no spelling, so it could not cross a signature, sit in a struct field or be
  given a name, and an array of arrays existed only untyped, through `[n]ptr` and a `cast<ptr>`
  before every access.

  ```qd
  struct Grid { rows:[][]i64 }
  type Table = [][]str

  fn sum(g:[][]i64 -- total:i64) { ... }

  [2][]i64              // two distinct empty []i64
  [[1 2] [3 4]]         // an expression of type [][]i64
  ```

  `[n][]T` fills with `n` distinct empty arrays rather than `n` nulls, for the reason `[n]T`
  over a struct fills with instances: the zero of an array is an empty one, the way the zero of
  `str` is a real empty string. A null slot does not report itself, it surfaces later as
  `len: null array` from the first thing that reads it.

  The cause was that `[]T` was written out by hand at each of the six places a type can appear
  -- two in an anonymous function's parameter list, two in a named function's, one in a struct
  field, one in a type alias -- and none of the six recursed. They are replaced by one
  `parseTypeFrom` in `ast_parse.h` that does, and that is also where the `fn(...)`, `*T`,
  `mod::T` and `T<U>` forms are now read, each of which had been written out two or three times
  with slightly different reach. A qualified element type falls out of the same change:
  `[]mod::Point` had no spelling either, because the copies read one identifier and stopped.
  Every position now *reads* every form, which is not the same as supporting it -- a struct
  field declared `Box<i64>` is still rejected, but as one "Unknown type" where the reach used
  to stop short and spill the rest of the declaration into the top level as a cascade.

  Two adjacent array literals, `[1 2][3 4]`, now say so rather than reporting a size in an
  array type: a `[` hard against a `]` is an element type by the same adjacency rule that makes
  `[10]i64` a size, so the two literals want a space between them.

  Nesting past 64 deep is rejected rather than read. The wrappers are counted in a loop, not a
  recursion, so the parser itself does not care -- but the readers downstream of it do peel one
  `[]` per stack frame, and a type nobody means to write should not be the thing that finds out.
- **`[n]T` array literals, replacing the `make` family.** Array *types* were already written
  `[]T` everywhere a signature mentions them, while creation was written `make<T>`, which names
  the element type instead — two spellings of the same array depending on which side of the `--`
  it sat on. Creation is now a literal, and the element type is spelled the way a signature
  spells it.

  Whether the brackets hold **elements** or a **size** is decided by what follows the `]`: a type
  name immediately after it, with no whitespace, makes the brackets a size.

  ```qd
  [1 2 3]      // elements
  [10]i64      // ten 0
  []i64        // the size left out, so none of them: an empty []i64
  [10]         // still one element, the number 10
  [src len]i64 // the size is any expression leaving one i64
  ```

  For the scalars the zero values are the ones the runtime already produced, so `[10]i64` lowers
  to the call `10 make<i64>` lowered to and nothing changed underneath: `0` for the integer types,
  `0.0` for `f64`/`f32`, `""` for `str` (a real empty string, not null), null for `ptr`. For a type
  parameter of the enclosing generic the element type is unknowable at run time, so the array
  adopts one on first append, exactly as before.

  For a **struct**, `[2]Point` now holds two distinct *instances* rather than two nulls. It is
  `n` × `Point {}`, allowed exactly where `Point {}` is — every field needs a default — which puts
  the question of whether a type has a sensible zero on the struct's author rather than on the
  array, and settles the recursive case for free, since a struct-valued default does not parse and
  so `[n]Node` on a self-referential type is rejected rather than recursing. `[]Point` with the
  size omitted constructs nothing and needs no defaults, which is the form to reach for when a
  type has no sensible zero.

  This is what closes the hole the old spelling left: `3 make<Point>` gave null slots, and reading
  a field out of an unfilled one segfaulted, reported as `Segmentation fault (stack overflow from
  unbounded recursion?)`.

  `[n]T` is an expression, not a type. Arrays are dynamic and `[]T` is the only array type there
  is, so `[10]i64` and `[]i64` both produce a value of type `[]i64` and a size in type position
  now reports that rather than `Expected ']'`.

  The builtin was barely load-bearing to begin with: one `make<T>` in all of stdlib, none in the
  examples, and the stdlib's own array builders (`hof::map`, `hof::filter`) never used it — they
  build with `[]` and `append`. The interpreter tier never had it at all; its builtin table
  carried `makei`/`makef`/`makes`/`makep` and no generic `make`, so `make<i64>` could not run
  there. The literal works in both tiers.
- **Array-typed struct field defaults.** `struct Bag { xs:[]i64 = []i64 }` and `Bag {}` now work;
  the field-default parser took scalar literals only, so a struct with an array field could never
  be default-constructed — and therefore could never be an element type of `[n]T`.
- **`fuzz_formatter` and `fuzz_lsp_text`**, the two fuzz targets `tests/fuzz/meson.build`
  had always named but which were never written, so `meson compile -C build/fuzz` could
  not build the directory. `fuzz_formatter` checks the formatter's two contracts on every
  input the parser accepts — the output parses, and formatting it again changes nothing —
  and found every formatter bug listed under Fixed below. `tests/run_all.sh --suite fuzz`
  now runs all three targets rather than only `fuzz_parser`.
- **`error` — errors as values (R24).** `error::last` packs what the `err` builtin returns
  into an `Error` struct, `wrap` composes context and keeps the code, and `root` walks the
  cause chain, so a failure can be stored, returned, collected into an array or given context
  by the frame that has it — none of which the two loose values `err` hands back allowed. It
  is forty lines of Quadrate and needed no compiler change.
  The alternative was to push the `Error` on the failure path of every fallible call, which is
  what "a value that cannot be put on the stack cannot be composed" literally asks for, and
  most of the machinery for it exists: the validator already models the failure arm as a
  different stack shape, and what to push on failure is decided in one place. It was measured
  against the corpus first, and the measurement reversed the conclusion: of **394 failure arms
  after bare fallible calls, 330 ignore the error and 64 read it** (249 switch arms, 145
  if/else arms, plus 6 sites with no failure arm at all). Pushing taxes the 330 to improve the
  64, which is the wrong way round for a language that charges for what you do not use. So the
  channel stays where it is and the module reads it; `Error` is already the type a future
  stack-passing design would push, if the evidence ever turns round.
  What follows from keeping the channel: `err` empties on read and the next fallible call
  clears it, so `error::last` has to run first in the failure arm. The module says so.
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

- **`stdlib/regex` appends states instead of preallocating 256 null slots.** `regex_new` built its
  `states` with `MAX_STATES make<State>` and `add_state` wrote into it with `set` at an index that
  was always `nstates` — an append loop wearing a preallocation. It cost 256 slots for every
  compiled regex and left the unfilled ones as nulls a field access would have walked into. It is
  `[]State` and `append` now.
- **`stdlib/regex` represents its NFA as typed structs rather than raw byte offsets.**
  The eight state kinds were stored in hand-computed slots — `ST_TYPE = 0`, `ST_CHAR = 8`,
  … `STATE_SIZE = 40` — reached through `state_get`/`state_set` with an integer offset,
  so nothing was checked and the layout comment had already drifted from the constants
  (it claimed 48 bytes). It is now a `State` struct in a `[]State`, with `Nfa` and
  `Parser` records for the other two hand-rolled blocks. Forty lines of offset constants
  and the accessor plumbing are gone, the field names are checked against `State`
  specifically, and `release` loses two manual frees because the array and the struct are
  refcounted.
  Done as the measurement the sum-types item asks for: `regex` is the third candidate for
  a variant type, and this is how far the existing type system carries it. Reasonably far
  — what a sum type would still add is that only the fields belonging to the current kind
  are reachable, which the struct cannot express and a comment now records instead.
- **`and`, `or` and `not` are logical; the bitwise operations moved to `bits` (R13).** They
  were bitwise and used throughout as logical, which is only correct while every operand is
  already 0 or 1: `2 1 and` was `0` and `2 1 or` was `3`, neither of them a truth value. The
  plain names now go to the common case — `2 1 and` is `1` — and the bitwise forms are
  `bits::and`, `bits::or`, `bits::xor` and `bits::not`, where the prefix says which meaning
  is meant. `lnot` is gone; `not` is the logical negation it used to spell. `shl` and `shr`
  have no logical counterpart, so nothing collides and they stay builtins.
  The awkward name lands where it costs least. Bitwise use is concentrated — 301 uses
  across 32 files, but 218 of them in ten: `bytes`, the three `crypto` hashes, `uuid`,
  `hex`, `bits` itself and the kernel example, all code that is *about* bits, where
  `bits::and` reads as accurate rather than verbose and where the line often ended in
  `bits::mask` already. Logical use is 233 across 44 files, thinly spread, and keeps the
  short names and no import.
  `bits::and` and friends are `pub inline stack fn` wrapping one instruction each, the same
  way `sys` wraps the privileged instructions, so a module-qualified name costs nothing at
  runtime. The interpreter tier has no module system, so it registers the qualified
  spellings as plain names — a device with no package resolution still needs bit
  manipulation.
  There is no ambiguity between a builtin and a module function of the same name: Quadrate
  has no unqualified import, so `bits::and` is parsed as a scoped identifier and its member
  name never reaches the builtin table. `bytes::xor` and `bits::xor` coexist in the corpus
  today.
  **This does not add short-circuiting, and R13 is closed anyway.** `and` and `or` are
  still ordinary stack words and both operands are still evaluated, so a guard of the form
  `i len < xs i nth ... and` still reads `xs[i]`. Measured before closing: the corpus has
  **no site using that shape**, and **no site working around its absence** — a bound or
  null check written as a nested `if` whose body immediately indexes turns up twice, and
  neither is a guard that short-circuiting would collapse. The C idiom
  `if (p != null && p->x > 0)` does not arise here, because values are bound with `->` and
  branched on with `switch`, which separates the test from the use. So the item is closed
  for lack of a call site rather than because anything answers it. If one ever appears, the
  design was settled on 2026-09-18 and stands: short-circuiting is deferred evaluation, the
  concatenative answer to deferred evaluation is a quotation, and the spelling is the only
  open part — `[ … ] [ … ] and` over quotations, or combinators in `hof` beside
  `when`/`unless`. Adding `land`/`lor` as a third pair of boolean primitives would spend
  the budget on a special case of a mechanism the language already has. The hazard itself
  is documented where a reader meets it, in `reference.md` and specification §12.3.
- **Two dialects, spelled explicitly (decided 2026-09-19).** The goal was the concatenative
  property — juxtaposition is composition, so any contiguous run of words can be lifted into
  a named word and replaced by its name without changing meaning. What was decided instead
  is narrower: **the difference must not be silent.** Binding is the unmarked default,
  `stack fn` marks the function whose inputs stay on the stack, and the choice per function
  is a readability judgement rather than a property of a colon. The language carries both,
  and nothing forces the corpus to move.
  Which code is *better* stack-direct was answered by porting `stdlib/hof` both ways (R47):
  **a body whose parameters are used once, in the order the caller pushed them, is
  ceremony** — `apply` became the single word `call` — while a body that uses a value twice
  or takes two functions becomes a puzzle. `bi` stack-direct is `pick rot call rot rot
  call`, which is correct, passes its tests, and tells a reader nothing. 48 of 452 functions
  that take parameters are `stack fn`.
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

- **`<<field` reads the field the value's own type names.** When the type was not known at the
  access, the backend searched every struct in the module, took the first one declaring a field
  of that name and computed the offset from *its* layout. Two places left the type unknown, and
  both were silent -- the wrong `f64` came back, not a crash -- and which struct won was decided
  by map order, so adding a struct could change an unrelated function's answer.

  ```qd
  struct Alpha { pad:f64  x:f64 }
  struct Beta  { x:f64    y:f64 }

  fn (b:Beta) get_x( -- r:f64)   { b <<x }          // read Alpha's x: Beta's y came back
  fn first_x(bs:[]Beta -- r:f64) { bs 0 nth <<x }   // the same
  ```

  `nth` never learned the element type of the array it read: by the time it runs, the array is
  two pushes back and which name put it there is not recoverable. The validator does know --
  it tracks element types as it walks the stack -- so it records the resolved one on the node,
  the way `call` already carries its callee's fallibility, and `<<field`, `-> name` and method
  dispatch all read it from there. Chains work for the same reason: `rows 0 nth 1 nth <<x` over
  a `[][]Beta`.

  A method's receiver was qualified with the enclosing module's name against a literal `"main"`,
  but the program's own module is named after whatever was handed to `generate()`, which for a
  file compiled directly is its path. So the receiver's type was recorded as `<path>::Beta`,
  which resolves to nothing, and every method body fell through to the search. The test is
  against `mainModuleName` now.

  The search itself no longer guesses. It was allowed to when a type *was* known but had failed
  to resolve, on the grounds that the fallback is a reasonable best effort; a type that resolved
  to nothing is no more information than no type at all, and with more than one candidate it now
  reports the ambiguity instead of picking one. Reporting it fails the compile, too: that path
  and the unknown-field one both gave up without emitting the read and then let the build finish,
  so the binary was short a field access and the exit status said everything was fine.
- **An unterminated `<` in a type is a parse error.** The type argument reader ended its run
  at end of file in silence, so `Box<` was a type the parser accepted and nothing rejected it
  until the validator. The formatter only promises anything about source that parses, so it
  wrote that back out and its own output no longer parsed -- the invariant `fuzz_formatter`
  exists to check, which is what found it, on source the struct-field type parser only started
  reaching once it shared the one type reader.
- **A struct holding an array field releases it.** The generated destructor released the
  string fields and the nested structs and stopped there, so the `qd_array_t` and its
  element buffer outlived every struct that held one: `struct B { xs:[]i64 }` built in a
  loop leaked 192 bytes an iteration. Nothing lost them -- the pointer registry still held
  both at exit -- so valgrind called them reachable and every suite ran clean. An array
  field owns a reference the same way a str field does, since construction moves the one
  the stack held into the slot and `>>field` already hands back the one it replaced, and
  the struct's last reference now hands back what the slot still holds.
  Two things had hidden it. `generateStructCleanup`, a second copy of the destructor's
  field walk, was never called from anywhere and is deleted rather than kept in step. And
  the memory suite measured peak RSS with `getrusage(RUSAGE_CHILDREN)` from a python
  parent, where a forked child is charged for every page the interpreter holds -- some
  13 MB of floor, under which no leak was visible at all. It reads the kernel's own
  `VmHWM` now, and the case added for this leak fails by 7 MB without the fix.
- **An enum value written in binary, and one written with a leading zero.** `Bits = -0b101`
  was 0 and `Lead = 010` was 8, while the same literals in an expression were -5 and 10:
  `parseEnumDeclaration` read the value with `strtoll` at base 0, which is C's spelling of
  an integer rather than Quadrate's. Base 0 takes a leading zero for octal, which the
  language has no notion of, and whether it takes a `0b` prefix at all is up to the C
  library -- glibc 2.38 and newer do, older ones and musl do not -- so the binary case was
  a value that changed with the machine the compiler was built on rather than with the
  program.
  The two parsers that already read the spelling correctly -- `integerLiteralProblem` in
  the validator and `safeParseInt64` in the backend -- were copies of each other, and the
  interpreter tier had four more `strtoll` base 0 calls behind literals, constants, array
  elements and `switch` case labels. All of them, and `qd_eval`'s tokeniser in the
  embedding API, now go through one `readIntegerLiteral` in `quadrate/qc/numeric_literal.h`.
  An enum value that will not read is reported where it is written instead of silently
  becoming 0.
- **A negative hex or binary literal lexes and compiles.** `-0x10` came back from the
  scanner as the integer `-0` followed by the identifier `x10`, and `-0b101` as `-0` and
  `b101`: u8t's negative-number branch duplicated only the decimal path, so the `0x`/`0b`
  prefix handling in the positive branch above it never ran. In an expression the stray
  identifier was undefined and `quadc` said so, misleadingly (`Undefined identifier
  'x10'`); in an enum value it was taken as the next variant, and the formatter wrote the
  enum back split in two. Fixed upstream in libu8t 1.4.1, which factors the prefix
  handling into one `scanner_scan_radix_literal` that both branches call;
  `subprojects/u8t.wrap` is bumped to it and the two workarounds it forced — the
  malformed-literal guard in `parseEnumDeclaration` and the decimal rewriting of enum
  values, which discarded the author's spelling — are gone.
  Both of the compiler's own literal parsers made the same assumption and are fixed with
  it: `integerLiteralProblem` in the validator and `safeParseInt64` in the backend each
  tested the *first* character for `'0'` to find a radix prefix, so a sign in front hid
  it. Each now steps over the sign first and hands the sign and digits to `from_chars`
  together, which is what keeps the bound exact — the magnitude of the most negative i64
  does not fit a positive one, so `-0x8000000000000000` is in range while
  `-0x8000000000000001` is not, and both are now reported as such rather than as an
  invalid format. Found 2026-09-20 by `fuzz_formatter`.
- **An unterminated `/*` or `"` is a parse error.** u8t ends a string token at end of
  file exactly as it does at a closing quote, and the comment reader stopped at end of
  file the same way, so both were accepted in silence: the swallowed text vanished and
  what was left read as a clean parse of a shorter program. `fn main() { /* a }` parsed,
  and the formatter re-emitted the comment with a `*/` the author never wrote;
  `use "abc` took the rest of the file as a module name, and re-formatting grew that
  name by a newline every pass, so the formatter never reached a fixed point. The string
  check existed but only in the expression parser; it now covers every place a string
  token is consumed — `use`, `import`, `test` names, constant and global initialisers,
  struct field defaults, array elements and `switch` case values.
- **`make<T>` works as a struct-construction field value.** The type parameter was parsed
  only in expression position, so `P { xs = 4 make<Cell> }` built the instruction without
  it and the backend lowered it to a `qd_make` that does not exist — a link failure rather
  than a compile error. The parsing is now one shared `parseInstructionTypeParam`, used by
  the field-value parser and by the two expression sites that had a copy each.
- **A declared parameter struct type is no longer overwritten by inference.** Signature
  collection records the declared type and then infers one from the fields the body
  touches, and the inferred one won. The inference reads every field mentioned in the
  body, and when it cannot tell where an access came from it attributes it to the first
  parameter — so `fn connect(re:Nfa …)` whose body does `re idx state_at <<kind` was
  recorded as taking a `State`, the struct that has `kind`, and every call was then
  rejected for passing exactly the type the signature asks for. Inference is now the
  fallback for a parameter that declared no struct type, which is what it was for.
  This was masking a real error in `cmd/quadmcp/server.qd`, which declared
  `reject_unknown_options(f:Flag …)` for a `flag::Flag`; the overwrite happened to paper
  over the missing qualification. Now fixed at the declaration.
- **The specification describes what `>>field` actually does (R21).** It said the operator
  "pushes modified struct back" and "returns updated struct", which reads as a functional
  update — hand it a struct, get a new one back with the field changed. A struct is a
  reference and `>>field` writes through it: the value pushed back is the one passed in,
  already modified, so every other name bound to that struct sees the change, and `drop`
  discards the reference rather than an update. The idiom the spec showed, `p 42 >>x -> p`,
  rebinds `p` to what it already was; it is replaced by an example of the aliasing, which
  is the thing worth knowing. **Decided 2026-09-20: mutable references are the design, and
  the wording was the bug** — the item had previously inverted to "the spec is right, the
  implementation is the odd one out", which is now settled the other way.
- **A function-pointer type keeps its `(` glued to the `fn`.** The anonymous-function
  normaliser puts a space after `fn`, which is right for a value and wrong for a type:
  the parser only reads `fn` as a type when the `(` follows it directly, so
  `g:fn (i64 -- i64)` does not parse. It now leaves `fn` alone in type position, which a
  `:` before it identifies, and still normalises the value form on the same line.
- **A quote's escaping is judged from the run of backslashes, and only inside a string.**
  The parser's unterminated-string check and every string scan in the formatter tested
  the single character before the quote, which reads the closing quote of `"a\\"` as
  escaped — that backslash is itself escaped — and reads the opening quote of `t{\"}` as
  escaped, though outside a string nothing is. The parser took `use "\"` for a
  terminated literal; the formatter, still inside a string it thought was open, could
  not find the block's closing brace and handed the file back untouched, or counted a
  brace inside a string it thought had ended.
- **A nested block comment is read to its own end.** `parseComment` tracked nesting but
  dropped the last character before end of file, and the formatter's own brace scanners
  did not nest at all, so in `fn t(){/*/**/}*/}` the inner `*/` looked like the outer
  comment's close and the `}` after it looked like the body's.
- **The formatter no longer drops what shares a line with a block's braces.** It emitted
  the lines strictly between the `{` line and the `}` line, so `fn one() { 1 print nl`
  lost `1 print nl` and `	2 print nl }` lost `2 print nl`. On
  `fn main()\n {0 5 1 for it {` that was the whole body.
- **The formatter takes a block's `{` from the parser instead of guessing.** It used the
  first `{` on the block's line, which in `fn i({){}` is the one inside the parameter
  list; the parser records the block's own brace, so the position is known.
- **A struct or `import` declaration copies out its own text, and only that.** Both are
  re-emitted from the source rather than rebuilt, and both copied whole lines from the
  declaration's first line to its last: in `struct P{e:i=c}fn i(){}` the struct's copy
  carried the function after it, which was then emitted again as itself, and in
  `struct P{}struct P{e:i=c}` the second declaration copied out the first one's text.
  The copy now stops at the declaration's own `}`, and a declaration that does not own
  the first brace on its line is handed back rather than copied from the wrong start.
- **The formatter hands back the source when a block's braces do not balance.** Three
  places fell back to a guess instead — scanning to the end of the file for a function
  body, and emitting only the first line of a struct or `import` declaration — so a
  brace-unbalanced file came back with a brace the author never wrote, or with every
  field under a `struct P {` dropped.
- **A `use` path is quoted whenever the parser would not read it back bare.** The test
  was for spaces and slashes, so a path the heuristic had not met lost its quotes and
  no longer resolved.
- **A struct construction is only expanded when its fields survive being separated.**
  The expansion gives each field a line of its own, so a name that is not an identifier
  or a value whose braces do not balance leaves the lines after it at a brace depth the
  next pass reads differently — `V{r{=0 y=}}` splits into `r{ = 0` and `y = }` — and an
  empty name or value came back as `\t = `, whose trailing space the next pass trims.
  Either way the formatter never reached a fixed point. Such a line is now left alone,
  and it keeps its indentation: the bail-out paths returned the line unchanged and the
  caller emitted what came back verbatim, at column 0. This was live in the corpus: a
  `switch` arm reading `mem::ErrInvalidArg { err -> code -> _  "negative code=" print
  code print nl }` was expanded as a struct construction, and the rewrite put spaces
  inside the string literal, turning it into `"negative code = "`.
- **The struct-construction expansion stops at a comment.** A comment ends the code on
  the line, and the braces inside it are text: in `V{x=//}` the `}` is commented out
  and the construction carries on to the next line, but the expansion matched it and
  emitted a brace the author never wrote.
- **Only a construction the line itself opens and closes is expanded.** One nested
  inside another brace group has that group's braces around it, and giving its fields
  lines of their own splits those across lines: `Ok { P { x = 1 y = 2 } -> p }` came
  back as `Ok { P {` / the fields / `} -> p }`, which the next pass indents differently
  — so `quadfmt -c` reported the file as unformatted however many times it was
  formatted. That one is plain Quadrate, not a fuzzer shape. What follows the
  construction on the line has to balance too: `P{x=0}}` put `} }` on one line, where
  the re-indenter reads two leading closes but the expansion meant one, so the line
  dedented further on every pass.
- **The code after a multi-line string or comment ends is accounted for.** The
  re-indenter stopped at the closing `"` or `*/` and moved to the next line, so a brace,
  a string or a comment opened on the rest of that line was invisible to it: everything
  under `b */ true if {`, or under a `true if {` on the line a multi-line string ends,
  was indented as if the `if` had never opened.
- **A block comment's continuation lines are indented from the line that opened it.**
  They used the brace depth *after* that line, so when the opening line also opened a
  brace — `r{/*` — the continuation sat one level deeper than the base its relative
  indent is measured against, and gained a tab on every pass.
- **An anonymous function followed by a bare `->` keeps no trailing space.** The
  normaliser wrote `" -> " + name` with nothing for the name, and the next pass trimmed
  the space back off.
- **A second anonymous function on a line is normalised on the same pass.** After
  rewriting one, the scan resumed at the rebuilt line's length *plus* the offset it had
  started from, which lands past the end of the line, so anything after the first
  `fn(...){...}` waited for the next pass — and `quadfmt -c` run straight after
  `quadfmt -w` reported the file as unformatted.
- **The formatter's multiline-string tracker toggles instead of clearing.** A
  continuation line holding two quotes closes one string and opens the next; clearing
  on each left the tracker reading the lines after it as code, and since that depended
  on where the lines had been split, the formatter never converged.
- **A token the parser cannot use is reported rather than swallowed.** Several places
  scanned a token, found nothing to do with it and moved on, which left the file one
  brace short of what the parser had read — and every one of them still parsed clean:
  a bare `$`, which only ever introduces an interpolated string (`fn s(){$//f{`); a
  `{`, `}` or `/` in a parameter list (`fn i({){}`, `fn a(){fn(//){`); a `{` or `}` in
  type arguments (`fn i(){fn(i<}>--){}}`) or in an array literal; a parameter type that
  is neither an identifier nor `[]T` (`fn a(){fn(n:}--){}}`); an array type missing its
  `]` or its element type (`fn n(){fn([<vt>){}}`); a `-` that is not part of
  `--` (`fn m(){fn(-}){}}`); `::` with no name after it (`fn m(){i::}}`, `fn s(){a{F::}}}`);
  `&` with no function name after it (`fn t(){p{&}}}`); a struct field default with no
  value after the `=` (`struct c{e:i=}`); `defer` with no block; and a struct
  construction that reaches end of file (`var g = r{`), whose recorded source text the
  formatter then re-emitted with a newline the next pass read back as part of it. The
  four copies of the type-argument reader are now one, `parseTypeArgumentList`. Two of
  these fire on `tests/formatter/04_for_loop.qd`, whose body held a bare `$` and which
  therefore never compiled; it is now the `it` the loop binds.
- **An enum value whose literal was cut short is rejected.** libu8t 1.4.0 lexes `-0x10`
  as the integer `-0` followed by the identifier `x10` (see TODO, Upstream). Everywhere
  else that identifier is undefined and the compiler says so; in an enum value it became
  the next variant, and the formatter then wrote `Mask = -0x10` back as `Mask = -0` and a
  variant `x10`. The separated form `= - 0x10` does read as minus sixteen, but cannot be
  written back with the minus attached, so a negative hex or binary enum value is now
  recorded in decimal; a negative decimal keeps the spelling the author used.
- **Parse errors come back in source order.** The two that are only known once the
  whole file has been read — the unterminated comment and the unterminated string —
  were appended after errors from earlier lines.
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

- **The five array-creation builtins**: `make`, `makei`, `makef`, `makes`, `makep`. Array creation
  is a literal now — see `[n]T` under Added. All five report the rewrite when used, the way the
  removed stack shufflers do.

  `make` with no type parameter was the one that most needed to reach the user: it passed the
  validator, which defaulted its tracked type to `[]i64`, but codegen only handled `make` *with* a
  type parameter, so `3 make -> a` compiled clean and died at link with `undefined reference to
  'qd_make'`, with `quadlint` saying nothing. It is a compile error now.
- **`xor` and `lnot` as builtins.** `xor` is `bits::xor`; `lnot` is `not`. Both report the
  rewrite when used, the way the removed stack shufflers do.
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

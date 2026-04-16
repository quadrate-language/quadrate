# Changelog

All notable changes to the Quadrate programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

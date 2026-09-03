# Quadrate

A stack-based language that compiles to native code via LLVM.

> For users: see **https://quad.r8.rs** for documentation, tutorials, and the online playground.

## Building

Requires: Meson, Ninja, C++20 compiler (Clang recommended), LLVM 14+

```bash
make debug          # Debug build
make release        # Optimized build
make tests          # Run test suite (1838 tests)
make format         # Format C/C++ source
```

### Platform dependencies

**Arch Linux**: `pacman -S meson clang ninja llvm readline`

**Debian/Ubuntu**: `apt install clang llvm-dev meson ninja-build libreadline-dev libssl-dev pkg-config`

**Alpine**: `apk add bash clang clang-dev compiler-rt llvm-dev meson musl-dev readline-dev`

### Build outputs

```
dist/bin/           Binaries (quad, quadc, quadfmt, quadlint, quadlsp, quadpm, quadrepl, quaduses, quaddoc, quadmcp)
dist/lib/           Shared libraries (libqdrt.so, libqd.so)
dist/lib/quadrate/  Static libraries for embedding
dist/include/       C/C++ headers for embedding
dist/share/         Standard library modules, completions, API docs
```

## Project structure

```
cmd/                Command-line tools (quad, quadc, quadfmt, quadlint, quadlsp, quadpm, quadrepl, quaduses, quaddoc, quadmcp)
lib/qc/             Compiler frontend (parser, semantic validator)
lib/llvmgen/        LLVM code generator
lib/rt/             Runtime library (stack, strings, context)
lib/qd/             Build/execution driver
lib/*/              Standard library modules (36+ modules)
tests/              Test suite
examples/           Example programs
docs/               Documentation site (mkdocs)
tools/              Playground, scripts
```

## Testing

```bash
make tests                          # Full suite
make tests TEST=regex               # Single test
make tests SUITE=stdlib             # Single suite
make valgrind                       # Memory leak checks
make fuzz                           # Fuzz testing (requires clang)
```

## Releasing

```bash
make tag BUMP=patch                 # Bump version, commit, tag (0.2.0 -> 0.2.1)
git push && git push origin 0.2.1  # CI builds tarballs on tagged commits
```

See [CONTRIBUTING.md](./CONTRIBUTING.md) for the full release workflow.

## Links

- **Documentation**: https://quad.r8.rs
- **Playground**: https://quad.r8.rs/play/
- **Issues**: https://github.com/quadrate-language/quadrate/issues
- **Pull requests**: https://github.com/quadrate-language/quadrate/pulls

## License

Compiler: GNU General Public License v3.0 — see [LICENSE](./LICENSE)

Standard library: Apache License 2.0

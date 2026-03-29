# Quadrate

A stack-based language that compiles to native code via LLVM.

> **Status:** 2.0.0-alpha — usable but APIs may change.

**Documentation**: https://quad.r8.rs | **Playground**: https://quad.r8.rs/play/

## What is Quadrate?

Quadrate is a [concatenative](https://concatenative.org/wiki/view/Concatenative%20language) programming language in the tradition of Forth and Factor. Instead of passing arguments to functions, values are pushed onto a stack and functions consume/produce stack values:

```quadrate
5 3 +      // Push 5, push 3, add them -> stack contains 8
2 *        // Push 2, multiply -> stack contains 16
```

Functions declare their stack effects with a signature: `(inputs -- outputs)`:

```quadrate
fn square(x:i64 -- result:i64) {
    x x *   // Named params are auto-bound to locals
}

fn main() {
    4 square print nl   // Prints: 16
}
```

## Why Quadrate?

- **Native performance** — compiles to machine code via LLVM, no runtime overhead
- **Stack effect checking** — the compiler verifies every function's stack signature at compile time
- **Batteries included** — 36+ stdlib modules: networking, crypto, JSON, threading, HTTP, regex
- **Embeddable** — link Quadrate into C/C++ programs as a scripting engine
- **Tooling** — formatter, linter, LSP, package manager, REPL, and documentation generator

## Example

```quadrate
use strings

fn greet(name:str -- ) {
    $"Hello, {name}!" print nl
}

fn main() {
    "World" greet
}
```

```bash
quad run hello.qd
```

## Install

Requires: Meson, C++20 compiler (Clang recommended), LLVM 14+

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make release && sudo make install
```

See the [getting started guide](https://quad.r8.rs/getting-started/) for platform-specific instructions.

## Editor Support

- **VS Code**: [quadrate-vscode](https://git.sr.ht/~klahr/quadrate-vscode) — syntax highlighting, LSP integration
- **Neovim**: [quadrate.nvim](https://git.sr.ht/~klahr/quadrate.nvim) — TreeSitter highlighting, LSP
- **Any editor with LSP**: run `quad lsp` for diagnostics, completion, go-to-definition, rename

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for build instructions and development workflow.

- **Issues**: https://todo.sr.ht/~klahr/quadrate
- **Patches**: ~klahr/quadrate@lists.sr.ht

## License

Compiler: GNU General Public License v3.0 — see [LICENSE](./LICENSE)
Standard library: Apache License 2.0

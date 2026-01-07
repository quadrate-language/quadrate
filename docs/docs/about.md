# About Quadrate

Quadrate is a stack-based programming language designed for explicit data flow and compile-time verification.

## Design philosophy

Quadrate makes data flow explicit through stack-based evaluation. Every function declares its stack effect - what values it consumes and produces. This enables the compiler to validate all operations before execution.

```qd
fn double(x:i64 -- result:i64) {
	2 *
}
```

This function takes one integer and produces one integer. The signature documents the stack effect, and the compiler enforces it.

## Stack language heritage

Stack-based programming has a long history:

- **Forth** (1970) - The original, still used in embedded systems
- **PostScript** (1982) - Powers PDF rendering
- **Factor** (2003) - Modern stack language with advanced features

Quadrate builds on these foundations while adding static types, LLVM compilation, and a complete development toolchain.

## Design principles

**Explicit over implicit.** Operations and their effects are visible in the code.

**Compile-time over runtime.** Errors are caught during compilation, not execution.

**Simple over clever.** Straightforward solutions are preferred.

**Complete over minimal.** The toolchain includes formatter, linter, LSP, and package manager.

## Source code

Quadrate is open source and developed in the open:

**Repository:** [git.sr.ht/~klahr/quadrate](https://git.sr.ht/~klahr/quadrate)

Contributions, bug reports, and feedback are welcome. The project uses SourceHut for hosting and mailing lists for discussion.

## Built with

Quadrate wouldn't exist without these projects:

- **LLVM** - The compiler backend that generates native code
- **Meson** - The build system
- **Tree-sitter** - Powers the syntax highlighting in editors

## License

Quadrate is free software released under the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html).

You're free to use, modify, and distribute Quadrate. If you distribute modified versions, share your improvements with the community.

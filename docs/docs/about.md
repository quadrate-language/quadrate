# About Quadrate

Quadrate is a stack-based programming language for people who want to understand exactly what their code is doing.

## The Philosophy

Most programming languages hide complexity behind abstractions. Variables, objects, and implicit conversions make code easier to write but harder to reason about. Where did that value come from? What happens when this function is called?

Quadrate takes a different approach: **make the data flow visible**.

Every function declares its stack effect - what it takes and what it leaves behind. When you read Quadrate code, you can trace values through the program like following a wire through a circuit.

```qd
fn double(x:i64 -- result:i64) {
	2 *
}
```

This function takes one integer and leaves one integer. No hidden inputs, no side effects, no surprises.

## The Heritage

Stack-based programming has a rich history:

- **Forth** (1970) - The original, still used in embedded systems
- **PostScript** (1982) - Powers every PDF you've ever read
- **Factor** (2003) - Modern stack language with advanced features

Quadrate builds on these foundations while adding modern conveniences: static types, LLVM compilation, and a complete development toolchain.

## Design Principles

**Explicit over implicit.** If something happens, you should see it in the code.

**Compile-time over runtime.** Catch errors before the program runs, not after.

**Simple over clever.** A straightforward solution beats an elegant one.

**Complete over minimal.** Ship the formatter, linter and LSP.

## Source Code

Quadrate is open source and developed in the open:

**Repository:** [git.sr.ht/~klahr/quadrate](https://git.sr.ht/~klahr/quadrate)

Contributions, bug reports, and feedback are welcome. The project uses SourceHut for hosting and mailing lists for discussion.

## Built With

Quadrate wouldn't exist without these projects:

- **LLVM** - The compiler backend that generates native code
- **Meson** - The build system
- **Tree-sitter** - Powers the syntax highlighting in editors

## License

Quadrate is free software released under the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html).

You're free to use, modify, and distribute Quadrate. If you distribute modified versions, share your improvements with the community.

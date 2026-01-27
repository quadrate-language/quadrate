# Quadrate

A stack-based language that compiles to native code via LLVM.

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
    dup *   // Duplicate top of stack, multiply
}

fn main() {
    4 square print nl   // Prints: 16
}
```

Quadrate compiles to native code, supports modules, structs, and can be embedded in C/C++ applications.

## Example

```quadrate
fn main() {
    "Hello, World!" print nl
}
```

```bash
quadc -r hello.qd
```

## Install

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make release && sudo make install
```

Requires: Meson, C++20 compiler, LLVM 14+

## Contributing

- **Issues**: https://todo.sr.ht/~klahr/quadrate
- **Patches**: ~klahr/quadrate@lists.sr.ht

## License

GNU General Public License v3.0 - see [LICENSE](./LICENSE)

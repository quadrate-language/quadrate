# Quadrate

A stack-based language that compiles to native code via LLVM.

**Documentation**: https://quad.r8.rs

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

# About Quadrate

Quadrate is a stack-based programming language designed for clarity and performance.

## Design Goals

- **Explicit data flow** - Stack effects make it clear what functions consume and produce
- **Compile-time safety** - Type checking and stack effect validation catch errors early
- **Native performance** - LLVM backend generates optimized machine code
- **Practical error handling** - Fallible functions require explicit error handling
- **Simple memory model** - Reference-counted heap allocation with automatic cleanup

## Why Stack-Based?

Stack-based languages have a long history, from Forth to PostScript to Factor. Quadrate brings this paradigm to modern systems programming:

1. **No hidden state** - Function signatures declare exactly what they consume and produce
2. **Composability** - Functions naturally chain together
3. **Low overhead** - Simple execution model with minimal runtime

## Source Code

Quadrate is open source and hosted on SourceHut:

- Repository: [git.sr.ht/~klahr/quadrate](https://git.sr.ht/~klahr/quadrate)

## License

Quadrate is released under the GPL-3.0 License

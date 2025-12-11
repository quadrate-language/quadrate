# quadc

Quadrate compiler.

## Usage

```bash
quadc [options] <file.qd>
```

## Options

- `-o <name>` - Output executable name
- `-r`, `--run` - Compile and run immediately
- `-g` - Include debug information
- `--test` - Compile and run tests
- `--save-temps` - Keep intermediate files
- `--dump-tokens` - Print lexer tokens
- `--dump-ast` - Print AST structure
- `--verbose` - Show compilation commands
- `--no-colors` - Disable colored output

## Examples

```bash
quadc hello.qd -r           # Compile and run
quadc hello.qd -o hello     # Compile to executable
quadc --test tests.qd       # Run tests
```

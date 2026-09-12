# quadrepl

Interactive REPL for Quadrate.

## Usage

```bash
quadrepl
```

Values a line leaves behind stay on the stack, and the prompt shows it. Names
bound with `->` and declarations (`fn`, `struct`, `const`, `use`, ...) stay for
the rest of the session. Each line is compiled and run once, on its own, so an
effect in one line does not happen again on the next.

Piping works too: with stdin on a pipe the final stack is printed when input
ends.

```bash
echo "2 3 add" | quadrepl   # 5
```

## Commands

- `stack` - Show the stack
- `type` - Show the type of each stack value
- `clear` - Clear the stack
- `reset` - Clear the stack, locals, definitions and imports
- `:doc <name>` - Show a signature; with no exact match, search names
- `:save <file>` / `:load <file>` - Write or replay session history
- `help` - Show available commands
- `exit`, `:q`, Ctrl-D - Exit REPL

## Example

```
[]> 5 3
[5 3]> +
[8]> dup *
[64]> print
64
[]> :doc rot
  rot ( a b c -- b c a )
    Rotates the top three values, moving third to top.
```

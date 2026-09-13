# quadrepl

Quadrate REPL.

Interactive Read-Eval-Print Loop. Each line is compiled and run on its own, taking
the state the previous line left; values it leaves behind stay on the stack, and
the prompt shows them. Because a line runs once, an effect in one line does not
happen again on the next.

## Usage

```bash
quadrepl [options]
```

## Options

| Option | Description |
|--------|-------------|
| `-p, --print` | Print stack to stdout on exit (implied when stdin is a pipe) |

## Session commands

| Command | Description |
|---------|-------------|
| `stack` | Show the stack |
| `type` | Show the type of each stack value |
| `clear` | Clear the stack |
| `reset` | Clear the stack, locals, definitions and imports |
| `:doc <name>` | Show a signature; with no exact match, search names |
| `:save <file>` / `:load <file>` | Write or replay session history |
| `help` | Show the full list, including key bindings |
| `exit`, `:q`, Ctrl-D | Exit the REPL |

Names bound with `->` and declarations (`fn`, `struct`, `enum`, `const`, `type`,
`var`, `use`) stay for the rest of the session.

## Examples

```
[]> 5 3
[5 3]> +
[8]> dup *
[64]> print
64
```

```bash
echo '2 3 add' | quadrepl   # 5 — the final stack is printed at end of input
```

`quadrepl --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.

# wc

A clone of the classic Unix `wc` (word count) tool. Counts newlines, words,
and bytes in each named file (or stdin if no files are given).

## Run

```bash
quad run wc.qd FILE...
quad run wc.qd < input.txt
```

## Examples

```sh
$ quad run wc.qd README.md
       8      32     227 README.md

$ quad run wc.qd -l *.qd
     270 wc.qd

$ echo "hello world" | quad run wc.qd
       1       2      12
```

## Flags

| Flag           | Meaning                            |
| -------------- | ---------------------------------- |
| `-l, --lines`  | print only the newline counts      |
| `-w, --words`  | print only the word counts         |
| `-c, --bytes`  | print only the byte counts         |
| `-h, --help`   | show usage and exit                |
| `--version`    | print the version and exit         |
| `--`           | end of flags (next arg is a file)  |

With no count flag set, all three counts are printed (matching `wc`).

## Features demonstrated

- File I/O via `io::read_file` and stdin via `io::readline` loop
- Manual buffer growth with `mem::alloc` / `mem::realloc`
- Byte-level scanning with `mem::get_byte` / `mem::set_byte`
- Command-line flag parsing via `flag::parse` and inspection of `f.argv`
- Switch-based pattern matching on `Result` values
- Accumulating totals across multiple inputs

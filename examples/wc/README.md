# wc

A clone of the classic Unix `wc` (word count) tool. Counts newlines, words,
and bytes in each named file (or stdin if no files are given).

## Run

```bash
quad run wc.qd -- FILE...
quad run wc.qd < input.txt
```

## Examples

```sh
$ quad run wc.qd -- README.md
      52     293    1705 README.md

$ quad run wc.qd -- -l wc.qd
     128 wc.qd

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

With no count flag set, all three counts are printed (matching `wc`).
Any argument starting with `-` is taken as a flag, never a file.

## Features demonstrated

- Stack code with no named locals: every word is a line or two, and its
  inputs stay on the stack
- Counting by running `each` over a string with the counts on the stack
  under it: `0 1 rot fn { word } each drop`
- The combinators `tri` (three counts of one string) and `tri_star` (one
  column for each count, in order)
- Deciding once: each column is a word held in a global, `column` or `skip`,
  chosen from the flags at startup, so printing a row tests nothing
- Global variables for the settings and running totals
- File I/O via `io::read_file`, stdin read the same way from `/dev/stdin`
- Command-line flag parsing via `flag::parse` and `flag::either`
- Switch-based pattern matching on a fallible call's status

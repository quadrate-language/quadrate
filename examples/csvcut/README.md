# csvcut

A small `cut`-like tool for CSV. Reads a CSV file (or stdin), parses
each row with RFC-4180-ish quoting rules (commas separate fields,
double-quoted fields may contain commas and escaped quotes `""`),
and prints the selected columns.

## Run

```bash
quad run csvcut.qd COLUMNS [FILE]
```

## Examples

```sh
$ cat data.csv
id,name,city
1,Alice,Stockholm
2,Bob,"New York, NY"
3,"O""Brien",Dublin

$ quad run csvcut.qd 1,3 data.csv
id,city
1,Stockholm
2,New York, NY
3,Dublin

$ quad run csvcut.qd 1,1 data.csv     # single column (see note below)
id
1
2
3

$ cat data.csv | quad run csvcut.qd 2
name
Alice
Bob
O"Brien

$ quad run csvcut.qd -d ';' 1,3 semi.csv
```

## Args & flags

| Arg / flag      | Meaning                                                |
| --------------- | ------------------------------------------------------ |
| `COLUMNS`       | comma-separated 1-based column indices (required)      |
| `FILE`          | input CSV (default: stdin)                             |
| `-d CHAR`       | field delimiter (default `,`)                          |
| `-h, --help`    | show usage and exit                                    |
| `--version`     | print version and exit                                 |
| `--`            | end of flags (next arg is positional)                  |

### Single-column note

Pass `-f 1,1` (or any spec containing a non-digit) for a single column.
This works around a stdlib quirk where the runtime `read` instruction
type-infers numeric-looking args and pushes them as integers, which
then breaks any string-handling code (including `flag::parse`). Tracked
in TODO.md as a follow-up.

## Features demonstrated

- File I/O via `io::read_file` and stdin via `io::readline` loop
- CSV parsing with double-quoted field handling (incl. escaped `""`)
- Manual `argv` walking (avoids the `flag::parse` numeric-arg bug)
- `sb::StringBuilder` for efficient row assembly
- `mem::alloc` / `mem::set_i64` / `mem::get_i64` for the column-index array

# regex

Regular expression module for Quadrate.

## Installation

`regex` ships with the Quadrate toolchain, so there is nothing to install:

```quadrate
use regex
```

## Usage

```qd
use regex

fn main() {
    "h.*d" "hello world" regex::is_match print nl   // 1

    "[0-9]+" regex::compile -> re
    re "abc123def" regex::find -> end -> start
    start print " " print end print nl              // 3 6
    re "a1b22" "#" regex::replace print nl          // a#b#
    re regex::release
}
```

## Functions

- `is_match(pattern:str s:str -- result:i64)` - 1 if the pattern matches the whole of s
- `compile(pattern:str -- re:Regex)` - Compile a pattern once, for repeated use
- `(re:Regex) matches(s:str -- result:i64)` - 1 if the compiled pattern matches the whole of s
- `(re:Regex) find(s:str -- start:i64 end:i64)` - Byte range of the first match, end exclusive; -1 -1 if none
- `(re:Regex) find_all(s:str max:i64 -- results:ptr count:i64)` - Start/end pairs of every match; free `results` with `mem::free`
- `(re:Regex) replace(s:str repl:str -- result:str)` - Replace every match
- `(re:Regex) match_count(s:str -- count:i64)` - Number of non-overlapping matches
- `(re:Regex) release()` - Free the compiled pattern

## Supported Syntax

- `.` - Any character
- `*` - Zero or more
- `+` - One or more
- `?` - Zero or one
- `[abc]` - Character class
- `[a-z]` - Character range
- `[^abc]` - Negated character class
- `^` - Start of string
- `$` - End of string
- `\d` - Digit
- `\w` - Word character
- `\s` - Whitespace

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or pull request on [GitHub](https://github.com/quadrate-language/quadrate).

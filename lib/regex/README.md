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
    "hello world" "w.*d" regex::match if {
        "matched!" print nl
    }

    "abc123def" "[0-9]+" regex::find -> result found
    found if {
        result print nl  // prints "123"
    }
}
```

## Functions

- `match(text:str pattern:str -- matched:bool)` - Check if pattern matches anywhere in text
- `match_full(text:str pattern:str -- matched:bool)` - Check if pattern matches entire text
- `find(text:str pattern:str -- result:str found:bool)` - Find first match
- `replace(text:str pattern:str replacement:str -- result:str)` - Replace all matches

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

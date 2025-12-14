# Pygments Quadrate Lexer

Syntax highlighting for the [Quadrate](https://git.sr.ht/~klahr/quadrate) programming language.

## Installation

```bash
pip install .
```

Or for development:

```bash
pip install -e .
```

## Usage

### In Markdown (MkDocs)

````markdown
```qd
fn main() {
    "Hello, World!" print nl
}
```
````

### Command Line

```bash
pygmentize -l quadrate -f html example.qd
```

### Python

```python
from pygments import highlight
from pygments.formatters import HtmlFormatter
from quadrate_lexer import QuadrateLexer

code = 'fn main() { "Hello" print nl }'
print(highlight(code, QuadrateLexer(), HtmlFormatter()))
```

## Features

Highlights:

- Keywords: `fn`, `struct`, `if`, `else`, `for`, `loop`, etc.
- Types: `i64`, `f64`, `str`, `ptr`, `bool`
- Built-in operations: `dup`, `drop`, `swap`, `print`, etc.
- Strings with escape sequences
- Numbers (integer, float, hex, binary)
- Comments (`//` and `///`)
- Module syntax (`module::function`)
- Function pointers (`&funcname`)
- Field access (`@field`, `!field`)

## License

GPL-3.0

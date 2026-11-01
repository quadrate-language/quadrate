# Combinators

Demonstrates the builtin combinators, the searching words among them, with function values written in place as `fn { }`, and
`when`/`unless` from the `hof` module.

## Run

```bash
quad run hof.qd
```

## Features

- `bi` - Run two function values on the same value
- `keep` - Run a function value and keep the value
- `dip` - Run a function value under the top value
- `both` - Run one function value on two values
- `when/unless` - Run a function value on a condition
- `times` - Run a function value n times
- `each`, `map`, `filter`, `fold` - Over arrays
- `any`, `all`, `find`, `index` - Search an array or a string, stopping at the first element that decides

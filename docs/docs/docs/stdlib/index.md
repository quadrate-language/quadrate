# Standard Library

The Quadrate standard library provides modules for common programming tasks.

## Using Modules

Import a module with `use`:

```qd
use str
use math

fn main() {
	"hello" str::upper print nl  // HELLO
	16.0 math::sqrt print nl  // 4
}
```

## Fallible Functions

Functions marked with `!` can fail and require error handling:

```qd
use str

fn main() {
	"hello" 0 3 str::substring! print nl  // "hel"
}
```

## Available Modules

| Module | Description |
|--------|-------------|

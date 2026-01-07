# Hello world

Let's write your first Quadrate program.

## Your first program

Create a file called `hello.qd`:

```qd
fn main() {
	"Hello, World!" print nl
}
```

Run it:

```bash
quad run hello.qd
```

Output:

```
Hello, World!
```

## Breaking it down

### The function

```qd
fn main() {
```

- `fn` declares a function
- `main` is the entry point (every program needs one)
- `()` is the **stack signature** (more on this later)
- `{` starts the function body

### The string

```qd
"Hello, World!"
```

Writing a value pushes it onto the stack. This pushes the string `"Hello, World!"`.

### Printing

```qd
print
```

`print` takes the top value from the stack and prints it.

### Newline

```qd
nl
```

`nl` prints a newline character.

## Comments

Use `//` for single-line comments:

```qd
fn main() {
	// This is a comment
	"Hello!" print nl  // This prints a greeting
}
```

Use `/* */` for multi-line comments:

```qd
fn main() {
	/*
	This is a
	multi-line comment
	*/
	"Hello!" print nl
}
```

## Multiple prints

You can print multiple things:

```qd
fn main() {
	"Hello, " print
	"World!" print
	nl
}
```

Output:

```
Hello, World!
```

## Printing numbers

Numbers work the same way:

```qd
fn main() {
	42 print nl
	3.14 print nl
}
```

Output:

```
42
3.14
```

## What's next?

Now that you can print things, let's learn about [Values and Types](values-types.md).

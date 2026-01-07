# Tutorial: hello world

This tutorial walks through your first Quadrate program in detail.

## Creating the program

Create a file called `hello.qd`:

```qd
fn main() {
	"Hello, World!" print nl
}
```

## Running it

```bash
quad run hello.qd
```

Output:
```
Hello, World!
```

## Understanding the code

Let's break down each part:

### The function declaration

```qd
fn main() {
```

- `fn` - keyword to declare a function
- `main` - the function name (entry point for the program)
- `()` - empty parentheses mean no inputs and no outputs

Functions that take or return values use a **stack signature** with `--` to separate inputs from outputs. For example, `fn add(a:i64 b:i64 -- sum:i64)` takes two integers and returns one. We'll see this in the [Stack Tutorial](tutorial-stack.md).

### The string

```qd
"Hello, World!"
```

This **pushes** the string onto the stack. In Quadrate, writing a literal value puts it on the stack.

### The print

```qd
print
```

This **pops** the top value from the stack and prints it.

### The newline

```qd
nl
```

Prints a newline character.

## The stack

Quadrate is **stack-based**. When you write a value, it goes onto a stack. When you call an operation, it takes values from the stack and puts results back.

```qd
fn main() {
	2 3 +      // Push 2, push 3, add them → 5 on stack
	print nl   // Pop 5 and print it
}
```

This is called **concatenative** programming - operations chain together left-to-right, each one consuming and producing stack values.

The [Stack Tutorial](tutorial-stack.md) covers this in detail. For now, just know:
- Values get **pushed** onto the stack
- Operations **pop** their inputs and **push** their outputs

## Exploring further

### Multiple prints

```qd
fn main() {
	"Hello, " print
	"World!" print
	nl
}
```

### Using variables

```qd
fn main() {
	"World" -> name
	"Hello, " print name print "!" print nl
}
```

### Adding numbers

```qd
fn main() {
	"2 + 3 = " print
	2 3 + print nl
}
```

### A simple function

```qd
fn greet(name:str -- ) {
	-> name
	"Hello, " print name print "!" print nl
}

fn main() {
	"Alice" greet
	"Bob" greet
}
```

Output:
```
Hello, Alice!
Hello, Bob!
```

## Compilation options

### Compile only

```bash
quadc hello.qd -o hello
./hello
```

### Verbose output

```bash
quadc --verbose hello.qd
```

### View generated IR

```bash
quadc --dump-ir hello.qd
```

## Next steps

- **Next:** [Stack Tutorial](tutorial-stack.md) - Deep dive into stack-based programming
- [Standard Library](stdlib/index.md) - Available modules and functions

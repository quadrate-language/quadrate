# Tutorial: Hello World

This tutorial walks through your first Quadrate program in detail.

## Creating the Program

Create a file called `hello.qd`:

```qd
fn main( -- ) {
    "Hello, World!" print nl
}
```

## Running It

```bash
quad run hello.qd
```

Output:
```
Hello, World!
```

## Understanding the Code

Let's break down each part:

### The Function Declaration

```qd
fn main( -- ) {
```

- `fn` - keyword to declare a function
- `main` - the function name (entry point for the program)
- `( -- )` - the **stack signature**:
  - Left of `--`: inputs (parameters consumed from stack)
  - Right of `--`: outputs (values left on stack)
  - Empty on both sides means no inputs, no outputs

### The String

```qd
"Hello, World!"
```

This **pushes** the string onto the stack. In Quadrate, writing a literal value puts it on the stack.

### The Print

```qd
print
```

This **pops** the top value from the stack and prints it.

### The Newline

```qd
nl
```

Prints a newline character.

## Exploring Further

### Multiple Prints

```qd
fn main( -- ) {
    "Hello, " print
    "World!" print
    nl
}
```

### Using Variables

```qd
fn main( -- ) {
    "World" -> name
    "Hello, " print name print "!" print nl
}
```

### Adding Numbers

```qd
fn main( -- ) {
    "2 + 3 = " print
    2 3 + print nl
}
```

### A Simple Function

```qd
fn greet(name:str -- ) {
    -> name
    "Hello, " print name print "!" print nl
}

fn main( -- ) {
    "Alice" greet
    "Bob" greet
}
```

Output:
```
Hello, Alice!
Hello, Bob!
```

## Compilation Options

### Compile Only

```bash
quadc hello.qd -o hello
./hello
```

### Verbose Output

```bash
quadc --verbose hello.qd
```

### View Generated IR

```bash
quadc --dump-ir hello.qd
```

## Next Steps

- [Stack Tutorial](tutorial-stack.md) - Learn how the stack works
- [Structs Tutorial](tutorial-structs.md) - Work with structured data
- [Error Handling](tutorial-errors.md) - Handle errors properly

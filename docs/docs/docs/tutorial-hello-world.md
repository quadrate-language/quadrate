# Tutorial: Hello, world!

## Project setup
```bash
mkdir hello-world
cd hello-world
```

## Create a source file
Create a file named `main.qd` and add the following code:

```rust
fn main() {
    "Hello, world!" . nl
}
```

## Run the program
```bash
quadc -r main.qd
```

### Expected output
```
Hello, world!
```

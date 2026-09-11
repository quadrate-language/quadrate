# interp

Interpreted execution of Quadrate source.

## Overview

Parses with the front-end (`lib/qc`) and walks the AST, calling runtime
operations (`lib/rt`) directly. No LLVM, no code generation, no toolchain at run
time.

`lib/qd` compiles through LLVM, which is the right trade for code that runs many
times and the wrong one for code a person is typing. Use `lib/qd` for the first
and this for the second; both can share one context.

## Key functions

- `qd_interp_create()` - Create an interpreter with a context of its own
- `qd_interp_attach()` - Create an interpreter over an existing context
- `qd_interp_eval()` - Parse and execute source against the persistent stack
- `qd_interp_error()` - Message from the most recent failure
- `qd_interp_depth()` - Number of values on the stack
- `qd_interp_peek()` - Read a stack value without removing it
- `qd_interp_register()` - Make a C function callable by name from source
- `qd_interp_destroy()` - Destroy the interpreter

## Example

```c
qd_interp* interp = qd_interp_create(1024);

qd_interp_eval(interp, "2 3 +");

qd_interp_value value;
if (qd_interp_peek(interp, 0, &value)) {
    printf("%s\n", value.text);   // 5
}

qd_interp_destroy(interp);
```

The stack persists across calls, which is what a prompt needs:

```c
qd_interp_eval(interp, "2");
qd_interp_eval(interp, "3");
qd_interp_eval(interp, "+");      // 5
```

## Errors

Interpreted code is typed by people and is wrong all the time, so nothing here
ends the process. Arity is checked before an instruction runs, and everything
else — type mismatch, division by zero, an out-of-range `pick` — executes inside
a recovery point, so the runtime unwinds to the interpreter instead of calling
`_exit(1)`. See the `RecoverableErrors` group in `rt/runtime.h`.

```c
if (!qd_interp_eval(interp, "1 0 /")) {
    printf("%s\n", qd_interp_error(interp));   // div: Division by zero
}
```

## Native functions

`qd_interp_register()` makes a C function callable by name, which is how an
embedder exposes its own capabilities to code a user types.

```c
static int beep(qd_context* ctx, void* userdata) {
    (void)ctx; (void)userdata;
    sound_the_buzzer();
    return 0;               // non-zero raises an error
}

qd_interp_register(interp, "hw::beep", "( -- )", beep, NULL);
qd_interp_eval(interp, "hw::beep");
```

Names may be scoped (`hw::beep`) or plain (`beep`). The signature's inputs are
counted so the stack is checked before the call, the same guard builtins get;
pass `NULL` to skip it. Registering a name twice replaces the earlier entry.

`lib/qd` offers the same capability through `qd_register_function()`, but has to
generate C stubs and link them so `dlopen` can resolve the symbols, which is why
it needs a compiler and linker present at run time. Here it is a name lookup
while walking the tree — nothing is compiled. The function pointer type is
structurally identical to `qd_native_fn`, so one C function can be registered
with either API without a cast.

## Coverage

Builtin instructions, literals, and registered native functions. Control flow,
Quadrate-defined functions, variables and module imports are not interpreted yet; they are reported as an
error rather than failing silently.

## Constraints

`longjmp` does not run destructors, so nothing between the recovery point and a
runtime call may hold an object that needs one. The walk uses references and
trivially destructible locals only, and changes to it must keep to that.

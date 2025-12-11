# qd

High-level embedding API for Quadrate.

## Overview

Provides a simplified C API for embedding Quadrate scripts in applications. Handles module management, script compilation, and function execution.

## Key Functions

- `qd_create_context()` - Create execution context
- `qd_get_module()` - Get or create a module
- `qd_add_script()` - Add source code to module
- `qd_register_function()` - Register native C function
- `qd_build()` - Compile module
- `qd_execute()` - Run Quadrate code

## Example

```c
qd_context* ctx = qd_create_context(1024);
qd_module* mod = qd_get_module(ctx, "main");
qd_add_script(mod, "fn hello( -- ) { \"Hello\" print nl }");
qd_build(mod);
qd_execute(ctx, "main::hello");
qd_free_context(ctx);
```

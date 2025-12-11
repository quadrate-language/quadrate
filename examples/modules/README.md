# Modules

Demonstrates the module system.

## Run

```bash
quadc -r main.qd
```

## Structure

```
modules/
├── main.qd           # Main program
└── math_utils/
    └── module.qd     # Custom module
```

## Features

- `use` imports
- Module function calls (`module::function`)
- Local module discovery

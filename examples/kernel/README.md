# Kernel

Bare-metal i686 multiboot kernel. Writes "Hello, Quadrate kernel!" to VGA
text mode and halts. Pure Quadrate — no C shim, no FFI. The only
non-Quadrate code is `boot.S` (multiboot header + stack setup).

## Build

```bash
make              # kernel.elf — 34 KB multiboot ELF
make iso          # kernel.iso — GRUB-bootable (needs xorriso)
```

## Run

```bash
make run          # qemu -kernel kernel.elf
make run-iso      # qemu -cdrom kernel.iso
```

Needs `qemu-system-i386`.

## Features

- `quadc --freestanding` compilation (no libc, no auto-main)
- `st16` builtin for direct memory writes
- `mem::set_byte` from the freestanding-safe libmem (no heap)
- `cast<ptr>` for integer-to-pointer conversion
- Multiboot1 boot protocol
- Cross-compilation to i686 via `--target`

# Kernel

Bare-metal x86_64 multiboot kernel. Boots into long mode, sets up GDT/IDT,
remaps the PIC, and provides a VGA text-mode console with PIT timer and
PS/2 keyboard input. Non-Quadrate code: `boot.S` (multiboot header +
32→64-bit transition) and `interrupts.S` (ISR stubs/tables).

## Build

```bash
make              # kernel.elf — multiboot ELF
make iso          # kernel.iso — GRUB-bootable (needs xorriso + grub-mkrescue)
```

## Run

```bash
make run          # qemu-system-x86_64 -kernel kernel.elf
make run-iso      # qemu-system-x86_64 -cdrom kernel.iso
```

Needs `qemu-system-x86_64`.

## Features

- `quadc --freestanding` compilation (no libc, no auto-main)
- `sys::st8`/`sys::st16`/`sys::st32`/`sys::st64` for direct memory writes
- `sys::ld8`/`sys::ld16`/`sys::ld64` for direct memory reads
- `sys::port_out8`/`sys::port_in8` for I/O port access
- `sys::cli`/`sys::sti`/`sys::hlt` for interrupt control
- GDT and IDT setup from Quadrate (64-bit entries)
- PIC remapping and interrupt dispatch
- PIT timer tick counter
- PS/2 keyboard with scancode-to-ASCII translation
- VGA text-mode output with scrolling and cursor tracking
- Multiboot1 boot protocol with 32→64-bit long mode transition
- Cross-compilation to x86_64 via `--target`
- FFI: imports `interrupts.o` for low-level assembly helpers

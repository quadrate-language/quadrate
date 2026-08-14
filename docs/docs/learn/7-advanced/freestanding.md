# Freestanding mode

Freestanding mode compiles Quadrate with no libc and no hosted operating system, for
kernels, bootloaders, microcontrollers, and anything else that runs on bare metal.

```bash
quadc --freestanding kernel.qd -o kernel.o
```

The output is a **relocatable object file**, not an executable. You link it yourself,
against your own entry stub and linker script.

## What changes

| Hosted build | `--freestanding` |
|---|---|
| `quadc` emits `int main(int, char**)` and links an executable | `quadc` emits a `void _start(void)` shim and stops at `.o` |
| Runtime context is heap-allocated per run | A single statically allocated `qd_freestanding_ctx` |
| Whole stdlib available | `bits`, `limits`, `mem`, `sys`, plus your own `.qd` files |
| `print`/`panic`/`spawn` work | Rejected at compile time |
| A fatal error prints a stack trace and aborts | It calls `qd_freestanding_halt()` |

## The entry point

Declare `_start` (or `main`) as a `pub fn` taking and returning nothing:

<!-- doccheck: skip freestanding-only; compiles under --freestanding, not a hosted program -->
```quadrate
pub fn _start( -- ) {
	// kernel body
}
```

`quadc` emits a C-callable `void _start(void)` that hands your function the static
context and calls `qd_freestanding_halt()` when it returns. Your assembly entry stub
sets up a stack and jumps here.

Dead-code elimination is disabled in freestanding mode, because `pub` functions are
routinely called from assembly and nothing in the Quadrate side references them.

## What you can use

**Allowed modules** — `bits`, `limits`, `mem`, `sys`. Everything else either allocates or
calls into libc, so `use io` (and friends) is a compile error. Your own file imports
(`use "vga.qd"`) are always allowed: you control what is in them.

`mem` is split at build time. `libmem-freestanding.a` contains only the raw memory
operations — `set_byte`/`get_byte`, `set_i64`/`get_i64`, `set_f64`/`get_f64`,
`set_ptr`/`get_ptr`, the sized `set_i8`…`get_u32` accessors, `copy`, `zero`, `fill`,
`ptr_add`, `is_null`, `is_not_null`. The allocating half (`alloc`, `realloc`, `free`,
`to_string`, `from_string`) lives in `libmem.a` and is simply not linked, so reaching for
it is a link error rather than a silent heap dependency.

**Rejected builtins** — `print`, `prints`, `printv`, `printsv`, `nl`, `read`, `panic`,
`err`, `spawn`, `wait`, `detach`. Each gives
`builtin 'X' is not available in --freestanding mode`. Arithmetic, comparison, bitwise
ops, stack manipulation, locals, `if`/`loop`/`for`/`switch`, structs, arrays, function
pointers and FFI all work normally.

## The `sys` module

`sys` is the bare-metal escape hatch. Every function is `inline`, so the compiler lowers
it to instructions with no call overhead and no runtime dependency.

**Raw memory** — these lower directly to LLVM `store`/`load`. No runtime call, no libc.
This is how a kernel touches MMIO and hardware registers without an FFI shim.

<!-- doccheck: skip freestanding-only; needs a bare-metal target to link -->
```quadrate
use sys

const VGA_BASE = 0xB8000

fn vga_put(row:i64 col:i64 ch:i64 -- ) {
	VGA_BASE row 80 * col + 2 * + 0 ch sys::st8
	VGA_BASE row 80 * col + 2 * + 1 0x0F sys::st8
}
```

| Function | Effect |
|---|---|
| `sys::st8` / `st16` / `st32` / `st64` | `(addr:i64 offset:i64 value:i64 -- )` — store the low 8/16/32/64 bits |
| `sys::ld8` / `ld16` / `ld32` / `ld64` | `(addr:i64 offset:i64 -- value:i64)` — load, zero-extended |

**x86 I/O ports** — `sys::port_out8` / `port_out16` / `port_out32`
`(port:i64 value:i64 -- )` and `sys::port_in8` / `port_in16` / `port_in32`
`(port:i64 -- value:i64)`.

**CPU control** — `sys::cli`, `sys::sti`, `sys::hlt`. The idle loop is
`loop { sys::hlt }`.

Port I/O and the CPU-control functions require an x86 target.

## What does not work

**Division and modulo halt.** 64-bit `/` and `%` on a 32-bit target need the compiler
runtime (`__divdi3`, `__moddi3`), which freestanding mode does not link. The symbols
resolve so unrelated code still compiles, but `qd_div` and `qd_mod` call
`qd_freestanding_halt()` at runtime. If you need integer division, either link libgcc or
implement it yourself. Shifts and multiplication are fine.

**Strings are not meaningfully supported.** `qd_push_s_ref` is a stub — a string literal
will compile and do nothing useful. Work in bytes.

**There is no error reporting.** `panic` is rejected outright, and every internal fatal
path (stack underflow, stack overflow, division, a failed `castp`) halts silently instead
of printing. Getting a wrong answer and getting a halt look the same from outside, so
lean on the compile-time stack checker.

**Call-stack tracking is compiled but inert.** `qd_push_call`/`qd_pop_call` are no-ops so
that codegen emitting them still links.

## Overriding the halt hook

`qd_freestanding_halt` is weak. The default is `cli; hlt` in a loop on x86 and `wfi` on
ARM/AArch64. Define your own to report the failure before stopping:

```c
void qd_freestanding_halt(void) {
	vga_print("HALT");
	for (;;) {
		__asm__ volatile("cli; hlt");
	}
}
```

The value stack is statically allocated at `QD_FREESTANDING_STACK_CAP` elements (default
1024). Rebuild `freestanding.c` with `-DQD_FREESTANDING_STACK_CAP=...` if you need a
deeper one — there is no growth path and overflow halts.

## Linking it together

The recommended shape is three files beside your `.qd`: an assembly entry stub, a linker
script, and a Makefile that compiles the freestanding runtime alongside your object.

**`boot.S`** — multiboot header, a stack in `.bss`, and a jump to `_start`:

```asm
.section .multiboot
	.align 8
	/* multiboot header goes here */

.section .bss.stack, "aw", @nobits
	.align 16
stack_bottom:
	.skip 16384
stack_top:

.section .text
.global _kernel_entry
_kernel_entry:
	mov $stack_top, %esp
	call _start
1:	cli
	hlt
	jmp 1b
```

**`linker.ld`** — load at 1 MiB and keep the multiboot header inside the first 8 KiB, or
the bootloader will not find it:

```ld
ENTRY(_kernel_entry)

SECTIONS {
	. = 1M;
	.boot   ALIGN(4K) : { KEEP(*(.multiboot)) }
	.text   ALIGN(4K) : { *(.text) *(.text.*) }
	.rodata ALIGN(4K) : { *(.rodata) *(.rodata.*) }
	.data   ALIGN(4K) : { *(.data) *(.data.*) }
	.bss    ALIGN(4K) : { *(COMMON) *(.bss) *(.bss.*) *(.bss.stack) }
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
```

Put `.bss.stack` last so the downward-growing CPU stack cannot run into runtime data.

**Building** — compile the Quadrate object, the freestanding runtime, and the raw half of
`mem`, then link with `-nostdlib`:

```make
kernel.o: kernel.qd
	quadc --freestanding --target x86_64-unknown-none-elf -O2 kernel.qd -o kernel.o

freestanding.o: lib/rt/src/freestanding.c
	clang --target=x86_64-unknown-none-elf -ffreestanding -fno-builtin -nostdlib \
	      -fno-stack-protector -mno-red-zone -O2 -Ilib/rt/include -c $< -o $@

mem.o: lib/mem/src/mem.c
	clang --target=x86_64-unknown-none-elf -ffreestanding -fno-builtin -nostdlib \
	      -Ilib/rt/include -Ilib/mem/include -c $< -o $@

kernel.elf: boot.o kernel.o freestanding.o mem.o linker.ld
	ld -nostdlib -T linker.ld -o $@ boot.o kernel.o freestanding.o mem.o
```

`-mno-red-zone` matters on x86_64: interrupt handlers will clobber the red zone.

## A worked example

`examples/kernel/` is a complete bare-metal x86_64 multiboot kernel written in Quadrate.
It boots through a 32→64-bit transition, sets up a GDT and IDT, remaps the PIC, and runs
a VGA text console with a PIT timer and PS/2 keyboard input — with only `boot.S`
(multiboot header and long-mode transition) and `interrupts.S` (ISR stubs) outside
Quadrate.

```bash
cd examples/kernel
make          # kernel.elf
make iso      # GRUB-bootable image (needs xorriso + grub-mkrescue)
make run      # boot it under QEMU
```

Two details worth copying: the IDT and GDT are built with `sys::st64` writes from
Quadrate rather than from C, and the assembly side exports address-of helpers
(`gdt_addr`, `idt_addr`, `tick_addr`, …) through a normal `import "interrupts.o"` block,
so shared state lives in `.bss` where both languages can reach it.

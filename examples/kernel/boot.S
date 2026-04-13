// boot.S — multiboot1 header + entry stub for the Quadrate kernel.
//
// Multiboot1-compatible loaders (GRUB, qemu -kernel) look for a magic
// header in the first 8KB of the loaded image, then jump to our entry
// point in 32-bit protected mode with paging disabled and the stack in
// an undefined state. We set up our own stack and jump to Quadrate's
// _start.

.set MAGIC,    0x1BADB002
.set FLAGS,    0x00000003   // align modules + provide memory info
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

// 16 KiB of stack space, .bss-allocated. Aligned to 16 for SSE-safe
// callers (Quadrate codegen targets default System V ABI alignment).
.section .bss
.align 16
stack_bottom:
	.skip 16384
stack_top:

.section .text
.global _kernel_entry
.type _kernel_entry, @function
_kernel_entry:
	movl $stack_top, %esp
	// Quadrate's _start is `void _start(void)` — no args, no return value
	// expected (it ends with a halt call).
	call _start
	// If _start ever returns (it shouldn't), just hang.
1:
	cli
	hlt
	jmp 1b
.size _kernel_entry, . - _kernel_entry

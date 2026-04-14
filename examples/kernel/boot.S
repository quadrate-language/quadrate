// Multiboot1 header + entry stub.

.set MAGIC,    0x1BADB002
.set FLAGS,    0x00000003
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

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
	call _start
1:	cli
	hlt
	jmp 1b
.size _kernel_entry, . - _kernel_entry

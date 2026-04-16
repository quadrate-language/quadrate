// x86_64 interrupt infrastructure: GDT/IDT storage, ISR stubs, helpers.

// ---------------------------------------------------------------------------
// BSS
// ---------------------------------------------------------------------------
.section .bss
.align 16

// GDT: 6 entries x 8 bytes = 48 bytes
.global gdt_entries
gdt_entries:
	.skip 48

// GDT descriptor (10 bytes: 2-byte limit + 8-byte base)
.global gdt_descriptor
gdt_descriptor:
	.skip 10

// IDT: 256 entries x 16 bytes = 4096 bytes (x86_64 IDT entries are 16 bytes)
.global idt_entries
idt_entries:
	.skip 4096

// IDT descriptor (10 bytes: 2-byte limit + 8-byte base)
.global idt_descriptor
idt_descriptor:
	.skip 10

// Current interrupt number
.global interrupt_number
interrupt_number:
	.skip 8

// Timer tick counter
.global tick_count
tick_count:
	.skip 8

// Keyboard cursor position
.global cursor_col
cursor_col:
	.skip 8

.global cursor_row
cursor_row:
	.skip 8

// ---------------------------------------------------------------------------
// DATA: ISR address table (64-bit pointers)
// ---------------------------------------------------------------------------
.section .data
.align 8
.global isr_table
isr_table:
	.quad isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
	.quad isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
	.quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
	.quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
	.quad isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
	.quad isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47

// ---------------------------------------------------------------------------
// TEXT: Helper functions (bare C names — Quadrate import generates wrappers)
// ---------------------------------------------------------------------------
.section .text

// lgdt: void lgdt(int64_t descriptor_addr)
// SysV x86_64: first arg in %rdi
.global lgdt
.type lgdt, @function
lgdt:
	lgdt (%rdi)
	// Reload segment registers
	pushq $0x08
	leaq .Lgdt_flush(%rip), %rax
	pushq %rax
	lretq
.Lgdt_flush:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	ret
.size lgdt, . - lgdt

// lidt: void lidt(int64_t descriptor_addr)
.global lidt
.type lidt, @function
lidt:
	lidt (%rdi)
	ret
.size lidt, . - lidt

// Address getters
.macro ADDR_GETTER name, symbol
.global \name
.type \name, @function
\name:
	leaq \symbol(%rip), %rax
	ret
.size \name, . - \name
.endm

ADDR_GETTER gdt_addr,       gdt_entries
ADDR_GETTER gdt_desc_addr,  gdt_descriptor
ADDR_GETTER idt_addr,       idt_entries
ADDR_GETTER idt_desc_addr,  idt_descriptor
ADDR_GETTER isr_table_addr, isr_table
ADDR_GETTER int_num_addr,   interrupt_number
ADDR_GETTER tick_addr,      tick_count
ADDR_GETTER cursor_col_addr, cursor_col
ADDR_GETTER cursor_row_addr, cursor_row

// ---------------------------------------------------------------------------
// ISR stubs (x86_64)
// ---------------------------------------------------------------------------

.macro ISR_NOERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
	pushq $0
	pushq $\num
	jmp isr_common
.size isr\num, . - isr\num
.endm

.macro ISR_ERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
	pushq $\num
	jmp isr_common
.size isr\num, . - isr\num
.endm

// CPU exceptions
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

// Hardware IRQs
ISR_NOERRCODE 32
ISR_NOERRCODE 33
ISR_NOERRCODE 34
ISR_NOERRCODE 35
ISR_NOERRCODE 36
ISR_NOERRCODE 37
ISR_NOERRCODE 38
ISR_NOERRCODE 39
ISR_NOERRCODE 40
ISR_NOERRCODE 41
ISR_NOERRCODE 42
ISR_NOERRCODE 43
ISR_NOERRCODE 44
ISR_NOERRCODE 45
ISR_NOERRCODE 46
ISR_NOERRCODE 47

// ---------------------------------------------------------------------------
// Common ISR handler (x86_64)
// ---------------------------------------------------------------------------
.extern qd_freestanding_ctx
.extern usr_kernel.qd_handle_interrupt

isr_common:
	// Save all general-purpose registers
	pushq %rax
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %rbp
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	// Store interrupt number (at rsp + 15*8 = 120, then int_no, then err_code)
	movq 120(%rsp), %rax
	movq %rax, interrupt_number(%rip)

	// Call Quadrate handler: usr_kernel.qd_handle_interrupt(qd_freestanding_ctx*)
	// SysV x86_64: first arg in %rdi
	leaq qd_freestanding_ctx(%rip), %rdi
	call usr_kernel.qd_handle_interrupt

	// Restore registers
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rbp
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax

	// Pop int_no + error_code
	addq $16, %rsp
	iretq

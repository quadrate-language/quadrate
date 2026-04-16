// Interrupt infrastructure: GDT/IDT storage, ISR stubs, helper functions.
//
// ISR stubs save registers, store the interrupt number, call the Quadrate
// handler, restore registers, and iret. This is inherently assembly — the
// CPU pushes a specific interrupt frame that must be handled before any
// calling convention applies.

// ---------------------------------------------------------------------------
// BSS: GDT, IDT, and kernel state
// ---------------------------------------------------------------------------
.section .bss
.align 16

// GDT: 6 entries x 8 bytes = 48 bytes (null, kcode, kdata, + 3 spare)
.global gdt_entries
gdt_entries:
	.skip 48

// GDT descriptor (6 bytes: 2-byte limit + 4-byte base)
.global gdt_descriptor
gdt_descriptor:
	.skip 6

// IDT: 256 entries x 8 bytes = 2048 bytes
.global idt_entries
idt_entries:
	.skip 2048

// IDT descriptor (6 bytes: 2-byte limit + 4-byte base)
.global idt_descriptor
idt_descriptor:
	.skip 6

// Current interrupt number (written by ISR stub, read by Quadrate handler)
.global interrupt_number
interrupt_number:
	.skip 4

// Timer tick counter
.global tick_count
tick_count:
	.skip 4

// Keyboard cursor column position
.global cursor_col
cursor_col:
	.skip 4

// Keyboard cursor row position
.global cursor_row
cursor_row:
	.skip 4

// ---------------------------------------------------------------------------
// DATA: ISR address table (for IDT population from Quadrate)
// ---------------------------------------------------------------------------
.section .data
.align 4
.global isr_table
isr_table:
	.long isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
	.long isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
	.long isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
	.long isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
	.long isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
	.long isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47

// ---------------------------------------------------------------------------
// TEXT: Helper functions
// ---------------------------------------------------------------------------
// These use bare C names (lgdt, lidt, gdt_addr, etc.) matching what the
// Quadrate import mechanism expects. The import generates usr_hw_* wrappers
// that handle the runtime stack, then call these with direct C arguments.
.section .text

// --- lgdt: load GDT and reload segment registers ---
// C signature: void lgdt(int64_t descriptor_addr)
// On i686 cdecl, first i64 arg is at 4(%esp) (low 32 bits).
.global lgdt
.type lgdt, @function
lgdt:
	movl 4(%esp), %eax
	lgdt (%eax)
	ljmp $0x08, $.Lgdt_flush    // 0x08 = kernel code segment
.Lgdt_flush:
	movw $0x10, %ax              // 0x10 = kernel data segment
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	ret
.size lgdt, . - lgdt

// --- lidt: load IDT ---
// C signature: void lidt(int64_t descriptor_addr)
.global lidt
.type lidt, @function
lidt:
	movl 4(%esp), %eax
	lidt (%eax)
	ret
.size lidt, . - lidt

// ---------------------------------------------------------------------------
// Address getters: return BSS symbol addresses.
// C signature: int64_t name(void) — returns address in %eax (low 32 bits).
// ---------------------------------------------------------------------------
.macro ADDR_GETTER name, symbol
.global \name
.type \name, @function
\name:
	movl $\symbol, %eax
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
// ISR stubs: 48 entries (exceptions 0-31, IRQs 32-47)
// ---------------------------------------------------------------------------

// ISR without error code: push dummy 0, then interrupt number
.macro ISR_NOERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
	pushl $0            // dummy error code
	pushl $\num         // interrupt number
	jmp isr_common
.size isr\num, . - isr\num
.endm

// ISR with error code: CPU already pushed it, just push interrupt number
.macro ISR_ERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
	pushl $\num         // interrupt number (error code already on stack)
	jmp isr_common
.size isr\num, . - isr\num
.endm

// CPU exceptions (0-31)
ISR_NOERRCODE 0     // Division by zero
ISR_NOERRCODE 1     // Debug
ISR_NOERRCODE 2     // NMI
ISR_NOERRCODE 3     // Breakpoint
ISR_NOERRCODE 4     // Overflow
ISR_NOERRCODE 5     // Bound range exceeded
ISR_NOERRCODE 6     // Invalid opcode
ISR_NOERRCODE 7     // Device not available
ISR_ERRCODE   8     // Double fault
ISR_NOERRCODE 9     // Coprocessor segment overrun
ISR_ERRCODE   10    // Invalid TSS
ISR_ERRCODE   11    // Segment not present
ISR_ERRCODE   12    // Stack-segment fault
ISR_ERRCODE   13    // General protection fault
ISR_ERRCODE   14    // Page fault
ISR_NOERRCODE 15    // Reserved
ISR_NOERRCODE 16    // x87 FP exception
ISR_ERRCODE   17    // Alignment check
ISR_NOERRCODE 18    // Machine check
ISR_NOERRCODE 19    // SIMD FP exception
ISR_NOERRCODE 20    // Virtualization exception
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

// Hardware IRQs (remapped to vectors 32-47)
ISR_NOERRCODE 32    // IRQ0: PIT timer
ISR_NOERRCODE 33    // IRQ1: Keyboard
ISR_NOERRCODE 34    // IRQ2: Cascade
ISR_NOERRCODE 35    // IRQ3: COM2
ISR_NOERRCODE 36    // IRQ4: COM1
ISR_NOERRCODE 37    // IRQ5: LPT2
ISR_NOERRCODE 38    // IRQ6: Floppy
ISR_NOERRCODE 39    // IRQ7: LPT1 / spurious
ISR_NOERRCODE 40    // IRQ8: CMOS RTC
ISR_NOERRCODE 41    // IRQ9
ISR_NOERRCODE 42    // IRQ10
ISR_NOERRCODE 43    // IRQ11
ISR_NOERRCODE 44    // IRQ12: PS/2 mouse
ISR_NOERRCODE 45    // IRQ13: FPU
ISR_NOERRCODE 46    // IRQ14: Primary ATA
ISR_NOERRCODE 47    // IRQ15: Secondary ATA

// ---------------------------------------------------------------------------
// Common ISR handler: save registers, call Quadrate, restore, iret
// ---------------------------------------------------------------------------
.extern qd_freestanding_ctx
.extern usr_kernel.qd_handle_interrupt

isr_common:
	pusha                           // save edi, esi, ebp, esp, ebx, edx, ecx, eax

	// Store interrupt number to BSS (Quadrate reads it from there)
	movl 32(%esp), %eax             // int_no is at esp+32 (after pusha=8 regs * 4 bytes)
	movl %eax, (interrupt_number)

	// Call Quadrate handler: usr_kernel.qd_handle_interrupt(qd_freestanding_ctx*)
	pushl $qd_freestanding_ctx
	call usr_kernel.qd_handle_interrupt
	addl $4, %esp

	popa                            // restore all registers
	addl $8, %esp                   // pop int_no + error_code
	iret

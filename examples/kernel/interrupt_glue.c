/**
 * Bridge between the Quadrate import wrappers and the asm helpers.
 *
 * The import mechanism generates usr_hw_<name> wrappers that call bare
 * <name> functions. These bare functions must have the Quadrate calling
 * convention: int fn(qd_context* ctx), popping/pushing from the runtime stack.
 */

#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <stdint.h>

extern int qd_pop_i(qd_context* ctx, int64_t* value);
extern int qd_push_i(qd_context* ctx, int64_t value);

// Asm helpers (bare C-callable, defined in interrupts.S with asm_ prefix)
extern void asm_lgdt(void* descriptor_addr);
extern void asm_lidt(void* descriptor_addr);
extern int64_t asm_gdt_addr(void);
extern int64_t asm_gdt_desc_addr(void);
extern int64_t asm_idt_addr(void);
extern int64_t asm_idt_desc_addr(void);
extern int64_t asm_isr_table_addr(void);
extern int64_t asm_int_num_addr(void);
extern int64_t asm_tick_addr(void);
extern int64_t asm_cursor_col_addr(void);
extern int64_t asm_cursor_row_addr(void);

// The import generates usr_hw_lgdt which calls lgdt(ctx).
// We provide lgdt(ctx) that bridges to asm_lgdt.

int lgdt(qd_context* ctx) {
	int64_t addr;
	qd_pop_i(ctx, &addr);
	asm_lgdt((void*)(uintptr_t)addr);
	return 0;
}

int lidt(qd_context* ctx) {
	int64_t addr;
	qd_pop_i(ctx, &addr);
	asm_lidt((void*)(uintptr_t)addr);
	return 0;
}

#define ADDR_BRIDGE(name)                                \
	int name(qd_context* ctx) {                          \
		return qd_push_i(ctx, asm_##name());             \
	}

ADDR_BRIDGE(gdt_addr)
ADDR_BRIDGE(gdt_desc_addr)
ADDR_BRIDGE(idt_addr)
ADDR_BRIDGE(idt_desc_addr)
ADDR_BRIDGE(isr_table_addr)
ADDR_BRIDGE(int_num_addr)
ADDR_BRIDGE(tick_addr)
ADDR_BRIDGE(cursor_col_addr)
ADDR_BRIDGE(cursor_row_addr)

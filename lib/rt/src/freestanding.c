/**
 * @file freestanding.c
 * @brief Freestanding (no-libc) Quadrate runtime
 *
 * This is the slimmed runtime for `quadc --freestanding` builds — kernels,
 * bootloaders, microcontrollers, anything where libc is unavailable.
 *
 * Provides the minimum set of symbols a freestanding-compiled `.o` needs:
 *   - qd_freestanding_ctx     : a statically-allocated qd_context
 *   - qd_freestanding_halt    : weak halt hook (busy `cli; hlt` by default)
 *   - The value-stack push/pop functions (qd_push_*, qd_pop_*, etc.)
 *   - No-op stubs for call-stack tracking and error reporting
 *
 * What is NOT provided (and is intentionally absent):
 *   - malloc/free            — no heap.
 *   - printf/fprintf/etc.    — no I/O.
 *   - String / array / struct / closure machinery — anything that allocates.
 *
 * If the user's program references those, they'll get an undefined-symbol
 * link error — which is the right outcome: those features genuinely don't
 * work without a hosted environment, and the error tells them exactly
 * what they used.
 *
 * Build: linked together as libqdrt-freestanding.a. Compile flags should
 * include -ffreestanding -fno-builtin to avoid the toolchain trying to
 * substitute libc primitives.
 */

#include <quadrate/rt/context.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Static context with a fixed-size value stack.
// ---------------------------------------------------------------------------

#ifndef QD_FREESTANDING_STACK_CAP
#define QD_FREESTANDING_STACK_CAP 1024
#endif

// Storage for the static stack.
static qd_stack_element_t g_stack_storage[QD_FREESTANDING_STACK_CAP];
static qd_stack g_stack = {
		.data = g_stack_storage,
		.capacity = QD_FREESTANDING_STACK_CAP,
		.size = 0,
};

// The static context exposed to generated code as @qd_freestanding_ctx.
qd_context qd_freestanding_ctx = {
		.st = &g_stack,
		.error_code = 0,
		.error_msg = (char*)0,
		.argc = 0,
		.argv = (char**)0,
		.program_name = (char*)0,
		.call_stack_depth = 0,
		.error_context = (char*)0,
		.userdata = (void*)0,
};

// ---------------------------------------------------------------------------
// Halt hook. Weak so users can override with their own (e.g. a kernel panic
// that prints to VGA before halting).
// ---------------------------------------------------------------------------

__attribute__((weak)) void qd_freestanding_halt(void) {
	for (;;) {
#if defined(__x86_64__) || defined(__i386__)
		__asm__ volatile("cli; hlt");
#elif defined(__aarch64__)
		__asm__ volatile("msr daifset, #2; wfi");
#elif defined(__arm__)
		__asm__ volatile("cpsid i; wfi");
#else
		// Generic spin — at least don't busy-loop without yielding the CPU.
		__asm__ volatile("");
#endif
	}
}

// libc replacements that some compiler-emitted error paths reference.
// They all just halt — there's no recoverable "exit" in a kernel.
__attribute__((noreturn)) void _exit(int code) {
	(void)code;
	qd_freestanding_halt();
	__builtin_unreachable();
}

// `write` stub: silently discard. Generated code may call write() via
// fwrite/printf paths in some configurations; we don't have a stdout.
long write(int fd, const void* buf, unsigned long count) {
	(void)fd;
	(void)buf;
	return (long)count;
}

// ---------------------------------------------------------------------------
// Call-stack tracking — no-op stubs. The real runtime uses these for stack
// traces in panic messages, but a freestanding kernel has nowhere to print.
// ---------------------------------------------------------------------------

void qd_push_call(qd_context* ctx, const char* name, const char* file, size_t line) {
	(void)ctx;
	(void)name;
	(void)file;
	(void)line;
}

void qd_pop_call(qd_context* ctx) {
	(void)ctx;
}

void qd_print_stack_trace(qd_context* ctx) {
	(void)ctx;
}

void qd_print_error_msg(qd_context* ctx, const char* func_name) {
	(void)ctx;
	(void)func_name;
}

// ---------------------------------------------------------------------------
// Value-stack push/pop. Minimal implementations that operate directly on
// the stack array — no error reporting (just halt on overflow/underflow).
// ---------------------------------------------------------------------------

static int push_elem(qd_context* ctx, qd_stack_element_t elem) {
	qd_stack* st = ctx->st;
	if (st->size >= st->capacity) {
		qd_freestanding_halt();
	}
	st->data[st->size++] = elem;
	return 0;
}

int qd_push_i(qd_context* ctx, int64_t value) {
	qd_stack_element_t e;
	e.type = QD_STACK_TYPE_INT;
	e.value.i = value;
	return push_elem(ctx, e);
}

int qd_push_f(qd_context* ctx, double value) {
	qd_stack_element_t e;
	e.type = QD_STACK_TYPE_FLOAT;
	e.value.f = value;
	return push_elem(ctx, e);
}

int qd_push_p(qd_context* ctx, void* value) {
	qd_stack_element_t e;
	e.type = QD_STACK_TYPE_PTR;
	e.value.p = value;
	return push_elem(ctx, e);
}

int qd_pop_i(qd_context* ctx, int64_t* value) {
	qd_stack* st = ctx->st;
	if (st->size == 0) {
		qd_freestanding_halt();
	}
	qd_stack_element_t e = st->data[--st->size];
	if (value) {
		*value = e.value.i;
	}
	return 0;
}

int qd_pop_f(qd_context* ctx, double* value) {
	qd_stack* st = ctx->st;
	if (st->size == 0) {
		qd_freestanding_halt();
	}
	qd_stack_element_t e = st->data[--st->size];
	if (value) {
		*value = e.value.f;
	}
	return 0;
}

int qd_pop_p(qd_context* ctx, void** value) {
	qd_stack* st = ctx->st;
	if (st->size == 0) {
		qd_freestanding_halt();
	}
	qd_stack_element_t e = st->data[--st->size];
	if (value) {
		*value = e.value.p;
	}
	return 0;
}

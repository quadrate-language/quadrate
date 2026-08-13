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
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdbool.h>
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

// Tiny memcpy / memset implementations. Compilers (especially clang)
// lower variable-size `__builtin_memcpy` / `__builtin_memset` calls to
// the libc symbols, even with `-fno-builtin`. We provide them here so
// freestanding code links cleanly. Byte loops are fine — they're not
// hot paths in the kernel demo, and the compiler is free to vectorize.
void* memcpy(void* dst, const void* src, unsigned long n) {
	unsigned char* d = (unsigned char*)dst;
	const unsigned char* s = (const unsigned char*)src;
	for (unsigned long i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dst;
}

void* memset(void* dst, int c, unsigned long n) {
	unsigned char* d = (unsigned char*)dst;
	unsigned char b = (unsigned char)c;
	for (unsigned long i = 0; i < n; i++) {
		d[i] = b;
	}
	return dst;
}

// `free` stub: in freestanding mode there's no heap, so there's nothing
// to free. The compiler emits free() calls in defensive local-cleanup
// paths even when the user code never allocates. They're no-ops here.
void free(void* p) {
	(void)p;
}

// Refcount-decrement stubs: same rationale as free() above. The compiler
// emits these when locals go out of scope; in freestanding mode no
// strings or refcounted pointers are ever created, so no-ops are safe.
// Signatures match the real runtime declarations exactly.
void qd_string_release(qd_string_t* s) {
	(void)s;
}

void qd_ptr_release(void* p) {
	(void)p;
}

void* qd_ptr_retain(void* p) {
	return p;
}

// Closure machinery — not used in freestanding code paths but referenced
// by compiler-emitted defensive checks. Stub that always reports "not a
// closure" is correct: real closures need a heap, which we don't have.
int qd_closure_is_valid(const void* p) {
	(void)p;
	return 0;
}

void qd_closure_unregister(void* p) {
	(void)p;
}

// String-pointer push for compiler-emitted code paths that may try to
// push string literals. In freestanding mode we don't expect strings,
// but the symbol must resolve.
int qd_push_s_ref(qd_context* ctx, qd_string_t* str) {
	(void)ctx;
	(void)str;
	return 0;
}

// Generic stack-pop for code paths that don't care about type.
qd_stack_error qd_stack_pop(qd_stack* st, qd_stack_element_t* out) {
	if (st->size == 0) {
		qd_freestanding_halt();
	}
	if (out) {
		*out = st->data[st->size - 1];
	}
	st->size--;
	return QD_STACK_OK;
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

// Set error message on the context. The hosted runtime strdup()s; we
// just store the pointer, since freestanding callers always pass static
// string literals (and our `free` stub above is a no-op anyway, so even
// double-frees would be safe).
void qd_set_error_msg(qd_context* ctx, const char* msg) {
	ctx->error_msg = (char*)msg;
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

// ---------------------------------------------------------------------------
// Arithmetic / comparison / bitwise stack ops. The full runtime has
// type-aware versions; freestanding code is integer-only by convention,
// so we operate directly on i64 values with no type checking.
// ---------------------------------------------------------------------------

static int64_t pop_i_inline(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size == 0) {
		qd_freestanding_halt();
	}
	return st->data[--st->size].value.i;
}

static int push_i_inline(qd_context* ctx, int64_t v) {
	qd_stack_element_t e;
	e.type = QD_STACK_TYPE_INT;
	e.value.i = v;
	return push_elem(ctx, e);
}

#define BINOP_I(name, expr)                                                                                            \
	int name(qd_context* ctx) {                                                                                        \
		int64_t b = pop_i_inline(ctx);                                                                                 \
		int64_t a = pop_i_inline(ctx);                                                                                 \
		return push_i_inline(ctx, (expr));                                                                             \
	}

BINOP_I(qd_add, a + b)
BINOP_I(qd_sub, a - b)
BINOP_I(qd_mul, a * b)
BINOP_I(qd_and, a & b)
BINOP_I(qd_or, a | b)
BINOP_I(qd_xor, a ^ b)
BINOP_I(qd_shl, a << b)
BINOP_I(qd_shr, (int64_t)((uint64_t)a >> b))
BINOP_I(qd_eq, a == b ? 1 : 0)
BINOP_I(qd_neq, a != b ? 1 : 0)
BINOP_I(qd_lt, a < b ? 1 : 0)
BINOP_I(qd_lte, a <= b ? 1 : 0)
BINOP_I(qd_gt, a > b ? 1 : 0)
BINOP_I(qd_gte, a >= b ? 1 : 0)

int qd_not(qd_context* ctx) {
	int64_t a = pop_i_inline(ctx);
	return push_i_inline(ctx, ~a);
}

// Logical negation, as distinct from the bitwise `not` above.
int qd_lnot(qd_context* ctx) {
	int64_t a = pop_i_inline(ctx);
	return push_i_inline(ctx, a == 0 ? 1 : 0);
}

int qd_neg(qd_context* ctx) {
	int64_t a = pop_i_inline(ctx);
	return push_i_inline(ctx, -a);
}

// Stack underflow / type check used in defensive codegen paths. The
// full runtime would print a stack trace; we just halt if undersized.
void qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name) {
	(void)types;
	(void)func_name;
	if (ctx->st->size < count) {
		qd_freestanding_halt();
	}
}

// `cast<ptr>` codegen lowers to qd_castp: pop value (any type), push as ptr.
// In freestanding we treat the value as i64-sized regardless of the source
// type tag, since all our types are 8-byte-equivalent.
int qd_castp(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size == 0) {
		qd_freestanding_halt();
	}
	qd_stack_element_t* top = &st->data[st->size - 1];
	top->value.p = (void*)(unsigned long)top->value.i;
	top->type = QD_STACK_TYPE_PTR;
	return 0;
}

// Division and modulo: 64-bit / on 32-bit targets needs the compiler
// runtime (libgcc's __divdi3/__moddi3). We don't link those in
// --freestanding mode. Provide trap stubs — if the kernel needs
// division it must either link libgcc or implement int division
// itself in asm. The symbols still resolve so unrelated code compiles.
int qd_div(qd_context* ctx) {
	(void)ctx;
	qd_freestanding_halt();
	return 0;
}

int qd_mod(qd_context* ctx) {
	(void)ctx;
	qd_freestanding_halt();
	return 0;
}

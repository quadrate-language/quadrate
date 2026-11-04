// Need POSIX for strdup() - not part of C standard
#define _POSIX_C_SOURCE 200809L

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "runtime_internal.h"
#include <inttypes.h>
#include <math.h>
#include <quadrate/rt/array.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/qd_struct.h>
#include <quadrate/rt/runtime.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void dump_stack(qd_context* ctx);

// Closure registry for safe closure detection

// A target with no signals (a microcontroller under FreeRTOS, say) has no
// sigaltstack either, and its own fault handler reports a crash.
#ifdef SA_ONSTACK
static void qdrt_crash_handler(int sig) {
	const char* msg = sig == SIGSEGV ? "Fatal error: Segmentation fault (stack overflow from unbounded recursion?)\n"
									 : "Fatal error: Bus error\n";
	(void)!write(2, msg, strlen(msg));
	const char* want_abort = getenv("QUADRATE_ABORT_ON_FATAL");
	if (want_abort != NULL && want_abort[0] != '\0' && strcmp(want_abort, "0") != 0) {
		signal(sig, SIG_DFL);
		raise(sig);
	}
	_exit(1);
}

void qd_install_crash_handler(void) {
	static uint8_t alt_stack[64 * 1024];
	stack_t ss;
	ss.ss_sp = alt_stack;
	ss.ss_size = sizeof(alt_stack);
	ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) != 0) {
		return;
	}
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = qdrt_crash_handler;
	sa.sa_flags = SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
}
#else
void qd_install_crash_handler(void) {
}
#endif

_Noreturn void qdrt_fatal_exit(void) {
	fflush(NULL);
	const char* want_abort = getenv("QUADRATE_ABORT_ON_FATAL");
	if (want_abort != NULL && want_abort[0] != '\0' && strcmp(want_abort, "0") != 0) {
		abort();
	}
	_exit(1);
}

jmp_buf* qd_recovery_buf(qd_context* ctx) {
	return (ctx != NULL) ? &ctx->recovery_buf : NULL;
}

void qd_recovery_arm(qd_context* ctx) {
	if (ctx != NULL) {
		ctx->recovery_armed = true;
	}
}

void qd_recovery_disarm(qd_context* ctx) {
	if (ctx != NULL) {
		ctx->recovery_armed = false;
	}
}

bool qd_recovery_armed(const qd_context* ctx) {
	return ctx != NULL && ctx->recovery_armed;
}

_Noreturn void qdrt_fatal_raise(qd_context* ctx, const char* op, const char* fmt, ...) {
	char detail[384];
	va_list args;
	va_start(args, fmt);
	vsnprintf(detail, sizeof(detail), fmt, args);
	va_end(args);

	if (ctx != NULL && ctx->recovery_armed) {
		char message[448];
		snprintf(message, sizeof(message), "%s: %s", op, detail);
		qd_set_error_msg(ctx, message);
		ctx->recovery_armed = false;
		longjmp(ctx->recovery_buf, 1);
	}

	// What the program printed came before the failure, so it goes out first
	fflush(stdout);
	fprintf(stderr, "Fatal error in %s: %s\n", op, detail);
	qdrt_dump_stack(ctx);
	qd_print_stack_trace(ctx);
	qdrt_fatal_exit();
}

void qd_fatal_recover(qd_context* ctx, const char* op, const char* detail) {
	if (ctx == NULL || !ctx->recovery_armed) {
		return;
	}
	if (detail == NULL) {
		detail = (ctx->error_msg != NULL && ctx->error_msg[0] != '\0') ? ctx->error_msg : "failed";
	}
	char message[448];
	if (op != NULL) {
		snprintf(message, sizeof(message), "%s: %s", op, detail);
	} else {
		snprintf(message, sizeof(message), "%s", detail);
	}
	qd_set_error_msg(ctx, message);
	ctx->recovery_armed = false;
	longjmp(ctx->recovery_buf, 1);
}

_Noreturn void qd_fatal_raise(qd_context* ctx, const char* op, const char* fmt, ...) {
	char detail[384];
	va_list args;
	va_start(args, fmt);
	vsnprintf(detail, sizeof(detail), fmt, args);
	va_end(args);

	qdrt_fatal_raise(ctx, op, "%s", detail);
}

const char* qd_type_name(qd_stack_type type) {
	switch (type) {
	case QD_STACK_TYPE_INT:
		return "i64";
	case QD_STACK_TYPE_FLOAT:
		return "f64";
	case QD_STACK_TYPE_D128:
		return "d128";
	case QD_STACK_TYPE_STR:
		return "str";
	case QD_STACK_TYPE_PTR:
		return "ptr";
	}
	return "unknown";
}

static qd_stack_element_t arg_pop(qd_context* ctx, const char* op) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		qd_fatal_raise(ctx, op, "Stack underflow");
	}
	return elem;
}

_Noreturn static void arg_type_error(qd_context* ctx, const char* op, const char* expected, qd_stack_element_t elem) {
	const char* actual = qd_type_name(elem.type);
	qdrt_release_element(&elem);
	qd_fatal_raise(ctx, op, "Expected %s, got %s", expected, actual);
}

int64_t qd_arg_i(qd_context* ctx, const char* op) {
	const qd_stack_element_t elem = arg_pop(ctx, op);
	if (elem.type != QD_STACK_TYPE_INT) {
		arg_type_error(ctx, op, "i64", elem);
	}
	return elem.value.i;
}

double qd_arg_f(qd_context* ctx, const char* op) {
	const qd_stack_element_t elem = arg_pop(ctx, op);
	if (elem.type == QD_STACK_TYPE_INT) {
		return (double)elem.value.i;
	}
	if (elem.type != QD_STACK_TYPE_FLOAT) {
		arg_type_error(ctx, op, "f64", elem);
	}
	return elem.value.f;
}

qd_d128_t qd_arg_d(qd_context* ctx, const char* op) {
	const qd_stack_element_t elem = arg_pop(ctx, op);
	if (elem.type == QD_STACK_TYPE_INT) {
		return qd_d128_from_i64(elem.value.i);
	}
	if (elem.type != QD_STACK_TYPE_D128) {
		arg_type_error(ctx, op, "d128", elem);
	}
	return elem.value.d;
}

void* qd_arg_p(qd_context* ctx, const char* op) {
	const qd_stack_element_t elem = arg_pop(ctx, op);
	if (elem.type != QD_STACK_TYPE_PTR) {
		arg_type_error(ctx, op, "ptr", elem);
	}
	return elem.value.p;
}

qd_string_t* qd_arg_s(qd_context* ctx, const char* op) {
	const qd_stack_element_t elem = arg_pop(ctx, op);
	if (elem.type != QD_STACK_TYPE_STR) {
		arg_type_error(ctx, op, "str", elem);
	}
	return elem.value.s;
}

// The type a qd_args letter names, and whether a value of type `actual` passes as one
static const char* args_letter_type(char letter, qd_stack_type actual, bool* accepted) {
	switch (letter) {
	case 'i':
		*accepted = actual == QD_STACK_TYPE_INT;
		return "i64";
	case 'f':
		*accepted = actual == QD_STACK_TYPE_FLOAT || actual == QD_STACK_TYPE_INT;
		return "f64";
	case 'd':
		*accepted = actual == QD_STACK_TYPE_D128 || actual == QD_STACK_TYPE_INT;
		return "d128";
	case 's':
		*accepted = actual == QD_STACK_TYPE_STR;
		return "str";
	case 'p':
		*accepted = actual == QD_STACK_TYPE_PTR;
		return "ptr";
	case 'v':
		*accepted = true;
		return "any";
	}
	*accepted = false;
	return "?";
}

void qd_args(qd_context* ctx, const char* op, const char* sig, ...) {
	qd_stack* st = ctx->st;
	const size_t count = strlen(sig);
	if (st->size < count) {
		qd_fatal_raise(ctx, op, "Stack underflow");
	}
	const size_t base = st->size - count;
	for (size_t i = 0; i < count; i++) {
		bool accepted = false;
		const char* expected = args_letter_type(sig[i], st->data[base + i].type, &accepted);
		if (!accepted) {
			const char* actual = qd_type_name(st->data[base + i].type);
			if (count == 1) {
				qd_fatal_raise(ctx, op, "Expected %s, got %s", expected, actual);
			}
			qd_fatal_raise(ctx, op, "Expected %s for argument %zu, got %s", expected, i + 1, actual);
		}
	}

	va_list outs;
	va_start(outs, sig);
	for (size_t i = 0; i < count; i++) {
		const qd_stack_element_t* elem = &st->data[base + i];
		switch (sig[i]) {
		case 'i':
			*va_arg(outs, int64_t*) = elem->value.i;
			break;
		case 'f':
			*va_arg(outs, double*) = elem->type == QD_STACK_TYPE_INT ? (double)elem->value.i : elem->value.f;
			break;
		case 'd':
			*va_arg(outs, qd_d128_t*) =
					elem->type == QD_STACK_TYPE_INT ? qd_d128_from_i64(elem->value.i) : elem->value.d;
			break;
		case 's':
			*va_arg(outs, qd_string_t**) = elem->value.s;
			break;
		case 'p':
			*va_arg(outs, void**) = elem->value.p;
			break;
		case 'v':
			*va_arg(outs, qd_stack_element_t*) = *elem;
			break;
		}
	}
	va_end(outs);
	// The references move to the caller with the values, so the slots are dropped, not released
	st->size = base;
}

int qd_fail(qd_context* ctx, int code, const char* fmt, ...) {
	char msg[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);
	ctx->error_code = code;
	qd_set_error_msg(ctx, msg);
	qd_push_i(ctx, code);
	return code;
}

// A function value is a struct: { function, environment, capture count }, with the struct
// header in front of it, so it is counted, copied and released as any struct is. A named
// function's value (`&name`) is a static one the compiler emits, with no environment and a
// count that never reaches zero. A closure's function takes the environment as a second
// argument; a named function takes none.
typedef struct {
	void* fn;
	void** env;
	int64_t captures;
} qd_closure_t;

// The last reference to a closure gives back its captures: each is a block { refcount, slot }
// with the count in the eight bytes before the slot, shared by every closure that captured it,
// and the last one gives back what the slot holds as well
static void closure_destroy(void* fields) {
	qd_closure_t* closure = (qd_closure_t*)fields;
	for (int64_t i = 0; i < closure->captures; i++) {
		char* block = (char*)closure->env[i] - sizeof(int64_t);
		int64_t refcount;
		memcpy(&refcount, block, sizeof(refcount));
		refcount--;
		memcpy(block, &refcount, sizeof(refcount));
		if (refcount == 0) {
			// The captured slot owns what it holds, a string or a struct as much as a number
			qdrt_release_element((qd_stack_element_t*)(void*)closure->env[i]);
			free(block);
		}
	}
	free((void*)closure->env);
}

void* qd_closure_new(void* fn, void* env, int64_t captures) {
	qd_closure_t* closure = (qd_closure_t*)qd_struct_alloc(sizeof(qd_closure_t), closure_destroy);
	if (closure == NULL) {
		fprintf(stderr, "Fatal error: out of memory making a closure\n");
		abort();
	}
	closure->fn = fn;
	closure->env = (void**)env;
	closure->captures = captures;
	return closure;
}

int qd_call_value(qd_context* ctx, void* value) {
	if (value == NULL) {
		QDRT_FATAL(ctx, "call", "NULL function value");
	}
	const qd_closure_t* closure = (const qd_closure_t*)value;
	if (closure->env != NULL) {
		int (*fn)(qd_context*, void*);
		memcpy(&fn, &closure->fn, sizeof(fn));
		return fn(ctx, (void*)closure->env);
	}
	int (*fn)(qd_context*);
	memcpy(&fn, &closure->fn, sizeof(fn));
	return fn(ctx);
}

// A push that does not fit is a stack overflow, the same error compiled code raises for its
// inline pushes, so no caller has to check, and none can drop a value by not checking.
static int push_result(qd_context* ctx, qd_stack_error err) {
	if (err == QD_STACK_ERR_OVERFLOW) {
		QDRT_FATAL(ctx, "push", "Stack overflow (use -s to increase stack size)");
	}
	if (err == QD_STACK_ERR_ALLOC) {
		QDRT_FATAL(ctx, "push", "Out of memory");
	}
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "push", "Null string");
	}
	return 0;
}

int qd_push_i(qd_context* ctx, int64_t value) {
	return push_result(ctx, qd_stack_push_int(ctx->st, value));
}

int qd_push_f(qd_context* ctx, double value) {
	return push_result(ctx, qd_stack_push_float(ctx->st, value));
}

int qd_push_s(qd_context* ctx, const char* value) {
	return push_result(ctx, qd_stack_push_str(ctx->st, value));
}

int qd_push_s_ref(qd_context* ctx, qd_string_t* value) {
	if (value == NULL) {
		return push_result(ctx, QD_STACK_ERR_NULL_POINTER);
	}
	if (qd_stack_size(ctx->st) >= ctx->st->capacity) {
		return push_result(ctx, QD_STACK_ERR_OVERFLOW);
	}

	qd_string_retain(value);
	ctx->st->data[ctx->st->size].value.s = value;
	ctx->st->data[ctx->st->size].type = QD_STACK_TYPE_STR;
	ctx->st->data[ctx->st->size].is_error_tainted = false;
	ctx->st->size++;

	return 0;
}

int qd_push_raw(qd_context* ctx, void* value) {
	return push_result(ctx, qd_stack_push_raw(ctx->st, value));
}

int qd_push_struct(qd_context* ctx, void* value) {
	return push_result(ctx, qd_stack_push_kind(ctx->st, value, QD_PTR_STRUCT));
}

int qd_push_array(qd_context* ctx, void* value) {
	return push_result(ctx, qd_stack_push_kind(ctx->st, value, QD_PTR_ARRAY));
}

int qd_push_ptr_kind(qd_context* ctx, void* value, uint8_t kind) {
	return push_result(ctx, qd_stack_push_kind(ctx->st, value, kind));
}

int qd_push_slot(qd_context* ctx, const qd_stack_element_t* slot) {
	return push_result(ctx, qdrt_push_element(ctx->st, slot));
}

void qd_retain_slot(const qd_stack_element_t* slot) {
	qdrt_retain_element(slot);
}

void qd_release_slot(const qd_stack_element_t* slot) {
	qd_stack_element_t copy = *slot;
	qdrt_release_element(&copy);
}

int qd_push_d(qd_context* ctx, qd_d128_t value) {
	return push_result(ctx, qd_stack_push_d128(ctx->st, value));
}

int qd_push_d_bits(qd_context* ctx, uint64_t low, uint64_t high) {
	const qd_d128_t value = {{low, high}};
	return qd_push_d(ctx, value);
}

int qd_d128_matches_bits(const qd_stack_element_t* subject, uint64_t low, uint64_t high) {
	if (subject == NULL || subject->type != QD_STACK_TYPE_D128) {
		return 0;
	}
	const qd_d128_t label = {{low, high}};
	return qd_d128_compare(subject->value.d, label) == 0;
}

int qd_pop_i(qd_context* ctx, int64_t* value) {
	if (!ctx || !value) {
		return QD_ERR_NULL_POINTER;
	}
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return QD_ERR_GENERIC;
	}
	if (elem.type != QD_STACK_TYPE_INT) {
		return QD_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.i;
	return QD_OK;
}

int qd_pop_f(qd_context* ctx, double* value) {
	if (!ctx || !value) {
		return QD_ERR_NULL_POINTER;
	}
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return QD_ERR_GENERIC;
	}
	if (elem.type != QD_STACK_TYPE_FLOAT) {
		return QD_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.f;
	return QD_OK;
}

int qd_pop_s(qd_context* ctx, char* buf, size_t buf_size) {
	if (!ctx || !buf || buf_size == 0) {
		return QD_ERR_NULL_POINTER;
	}
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return QD_ERR_GENERIC;
	}
	if (elem.type != QD_STACK_TYPE_STR) {
		return QD_ERR_TYPE_MISMATCH;
	}
	const char* str = qd_string_data(elem.value.s);
	size_t len = qd_string_length(elem.value.s);
	int result = QD_OK;
	if (len >= buf_size) {
		len = buf_size - 1;
		result = QD_ERR_GENERIC;
	}
	memcpy(buf, str, len);
	buf[len] = '\0';
	qd_string_release(elem.value.s);
	return result;
}

int qd_pop_d(qd_context* ctx, qd_d128_t* value) {
	if (!ctx || !value) {
		return QD_ERR_NULL_POINTER;
	}
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return QD_ERR_GENERIC;
	}
	if (elem.type != QD_STACK_TYPE_D128) {
		qdrt_release_element(&elem);
		return QD_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.d;
	return QD_OK;
}

int qd_pop_p(qd_context* ctx, void** value) {
	if (!ctx || !value) {
		return QD_ERR_NULL_POINTER;
	}
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return QD_ERR_GENERIC;
	}
	if (elem.type != QD_STACK_TYPE_PTR) {
		return QD_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.p;
	return QD_OK;
}

// Helper function to check if string contains whitespace
int qd_peek(qd_context* ctx) {
	QDRT_CHECK_STACK(ctx, "peek", 1);

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_peek(ctx->st, &val);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "peek", "Failed to peek value");
	}
	switch (val.type) {
	case QD_STACK_TYPE_INT:
		printf("%" PRId64 "\n", val.value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		// Use %g to match qd_print's float formatting (consistent output).
		printf("%g\n", val.value.f);
		break;
	case QD_STACK_TYPE_STR:
		printf("%s\n", qd_string_data(val.value.s));
		break;
	case QD_STACK_TYPE_D128: {
		char text[QD_D128_STRING_MAX];
		qd_d128_format(val.value.d, text, sizeof text);
		printf("%s\n", text);
		break;
	}
	default:
		return (int){-3};
	}
	return (int){0};
}

// Helper function to check if a stack type is numeric
static inline bool is_numeric_type(qd_stack_type type) {
	return type == QD_STACK_TYPE_INT || type == QD_STACK_TYPE_FLOAT;
}

// Helper function to validate binary numeric operation setup
// Returns true if validation passed, false if error occurred (and already aborted)
static bool validate_binary_numeric_op(qd_context* ctx, const char* op_name) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		QDRT_FATAL_UNDERFLOW(ctx, op_name, 2, stack_size);
	}

	// Check both are numeric types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		QDRT_FATAL(ctx, op_name, "Failed to access stack elements");
	}

	if (!is_numeric_type(check_a.type) || !is_numeric_type(check_b.type)) {
		QDRT_FATAL(ctx, op_name, "Type error (expected numeric types)");
	}

	return true;
}

// Helper function to pop two values from stack for binary operations
static int pop_two_values(qd_context* ctx, qd_stack_element_t* a, qd_stack_element_t* b) {
	// Every caller validates the arity first, so a failure here means the stack changed
	// underneath us -- an invariant violation, not a recoverable condition.
	qd_stack_error err = qd_stack_pop(ctx->st, b);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "binary op", "Failed to pop right operand");
	}
	err = qd_stack_pop(ctx->st, a);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "binary op", "Failed to pop left operand");
	}
	return (int){0};
}

// Helper function to release string references if needed
static void release_if_string(qd_stack_element_t* elem) {
	if (elem->type == QD_STACK_TYPE_STR) {
		qd_string_release(elem->value.s);
	}
}

// Helper function to push a stack element, taking a reference to what it holds
static qd_stack_error push_element(qd_stack* stack, const qd_stack_element_t* elem) {
	qd_stack_error err;
	switch (elem->type) {
	case QD_STACK_TYPE_INT:
		err = qd_stack_push_int(stack, elem->value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		err = qd_stack_push_float(stack, elem->value.f);
		break;
	case QD_STACK_TYPE_STR:
		// Retain the string reference (increment refcount) and push
		qd_string_retain(elem->value.s);
		if (qd_stack_size(stack) >= stack->capacity) {
			qd_string_release(elem->value.s); // Cleanup on overflow
			return QD_STACK_ERR_OVERFLOW;
		}
		stack->data[stack->size].value.s = elem->value.s;
		stack->data[stack->size].type = QD_STACK_TYPE_STR;
		stack->data[stack->size].is_error_tainted = elem->is_error_tainted;
		stack->size++;
		return QD_STACK_OK;
	case QD_STACK_TYPE_PTR:
		// Take a reference of our own, as the string case above does. A slot owns what it
		// holds, so a value that ends up in two slots -- `dup`, `over`, `pick` -- must be
		// counted twice; otherwise releasing one slot frees a value the other still holds.
		// Raw memory has no count, and the slot says so; a struct or an array counts itself.
		if (elem->ptr_kind == QD_PTR_RAW) {
			err = qd_stack_push_raw(stack, elem->value.p);
			break;
		}
		if (elem->ptr_kind == QD_PTR_STRUCT || elem->ptr_kind == QD_PTR_ARRAY) {
			err = qd_stack_push_kind(stack, elem->value.p, elem->ptr_kind);
			if (err == QD_STACK_OK) {
				if (elem->ptr_kind == QD_PTR_STRUCT) {
					qd_struct_retain(elem->value.p);
				} else {
					qd_array_retain(elem->value.p);
				}
			}
			break;
		}
		err = qd_stack_push_raw(stack, elem->value.p); // no kind: raw memory
		break;
	case QD_STACK_TYPE_D128:
		err = qd_stack_push_d128(stack, elem->value.d);
		break;
	default:
		return QD_STACK_ERR_TYPE_MISMATCH;
	}
	if (err == QD_STACK_OK) {
		// Copy error taint flag for non-string types
		stack->data[stack->size - 1].is_error_tainted = elem->is_error_tainted;
	}
	return err;
}

// sqrt - square root

// cb - cube (x^3)

// cbrt - cube root

// ceil - ceiling (round up)

// floor - floor (round down)

// ln - natural logarithm (base e)

// log10 - base 10 logarithm

// call - invoke the function value on top of the stack. The slot's reference is held until
// the call returns: the value may be the only thing keeping its own environment alive.
int qd_call(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "call", "Stack underflow");
	}
	if (val.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "call", "Expected a function value, got %d", val.type);
	}
	const int result = qd_call_value(ctx, val.value.p);
	qdrt_release_element(&val);
	return result;
}

// dec - decrement (subtract 1, preserves type)
// inc - increment (add 1, preserves type)
// casti - cast top stack element to integer
int qd_casti(qd_context* ctx) {
	// Pop one value, convert to integer, push result
	QDRT_CHECK_STACK(ctx, "casti", 1);

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "casti", "Failed to pop value");
	}

	int64_t result;
	if (elem.type == QD_STACK_TYPE_INT) {
		result = elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		result = (int64_t)elem.value.f;
	} else if (elem.type == QD_STACK_TYPE_STR) {
		result = atoll(qd_string_data(elem.value.s));
		release_if_string(&elem); // Release the string after conversion
	} else if (elem.type == QD_STACK_TYPE_PTR) {
		result = (int64_t)(uintptr_t)elem.value.p;
	} else if (elem.type == QD_STACK_TYPE_D128) {
		result = qd_d128_to_i64(elem.value.d);
	} else {
		QDRT_FATAL(ctx, "casti", "Cannot cast type to integer");
	}

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "casti", "Stack overflow pushing int result");
	}

	return (int){0};
}

// castf - cast top stack element to float
int qd_castf(qd_context* ctx) {
	// Pop one value, convert to float, push result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "castf", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castf", "Failed to pop value");
	}

	double result;
	if (elem.type == QD_STACK_TYPE_INT) {
		result = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		result = elem.value.f;
	} else if (elem.type == QD_STACK_TYPE_STR) {
		result = atof(qd_string_data(elem.value.s));
		release_if_string(&elem); // Release the string after conversion
	} else if (elem.type == QD_STACK_TYPE_D128) {
		result = qd_d128_to_f64(elem.value.d);
	} else {
		QDRT_FATAL(ctx, "castf", "Cannot cast type to float");
	}

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castf", "Stack overflow pushing float result");
	}

	return (int){0};
}

// casts - cast top stack element to string
int qd_casts(qd_context* ctx) {
	// Pop one value, convert to string, push result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "casts", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "casts", "Failed to pop value");
	}

	char buffer[64];
	if (elem.type == QD_STACK_TYPE_INT) {
		snprintf(buffer, sizeof(buffer), "%" PRId64, elem.value.i);
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		snprintf(buffer, sizeof(buffer), "%g", elem.value.f);
	} else if (elem.type == QD_STACK_TYPE_STR) {
		// Already a string - just push it back (retain it)
		err = push_element(ctx->st, &elem);
		if (err != QD_STACK_OK) {
			QDRT_FATAL(ctx, "casts", "Stack overflow pushing string result");
		}
		// Release our reference from the pop
		release_if_string(&elem);
		return (int){0};
	} else if (elem.type == QD_STACK_TYPE_PTR) {
		// A raw pointer carries no string contents we can safely dereference -
		// blindly treating it as a qd_string_t* and retaining it would corrupt
		// arbitrary memory. Render its address instead (matching casti, which
		// yields the address as an integer). NULL becomes the empty string.
		if (elem.value.p == NULL) {
			buffer[0] = '\0';
		} else {
			snprintf(buffer, sizeof(buffer), "%" PRId64, (int64_t)(intptr_t)elem.value.p);
		}
	} else if (elem.type == QD_STACK_TYPE_D128) {
		qd_d128_format(elem.value.d, buffer, sizeof buffer);
	} else {
		QDRT_FATAL(ctx, "casts", "Cannot cast type to string");
	}

	err = qd_stack_push_str(ctx->st, buffer);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "casts", "Stack overflow pushing string result");
	}

	return (int){0};
}

// castp - cast to pointer
int qd_castp(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "castp", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castp", "Failed to pop value");
	}

	void* ptr_value = NULL;
	if (elem.type == QD_STACK_TYPE_PTR) {
		// The address, as raw memory. A struct or an array gives up the reference its slot held:
		// a raw pointer holds none, and carrying it over leaked the object.
		ptr_value = elem.value.p;
		qdrt_release_element(&elem);
	} else if (elem.type == QD_STACK_TYPE_INT) {
		// Cast integer to pointer
		ptr_value = (void*)(intptr_t)elem.value.i;
	} else {
		QDRT_FATAL(ctx, "castp", "Cannot cast type %d to pointer", elem.type);
	}

	err = qd_stack_push_raw(ctx->st, ptr_value);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castp", "Stack overflow pushing pointer result");
	}

	return (int){0};
}

// pow - exponentiation (base^exponent)

// round - round to nearest integer

// clear - empty the entire stack
// depth - push the current stack depth onto the stack
// fac - factorial (n!)

// inv - inverse/reciprocal (1/x, returns float)

// eq - equal (==) comparison: ( a b -- result )
// Pops two values, pushes 1 if equal, 0 otherwise
// neq - not equal (!=) comparison: ( a b -- result )
// Pops two values, pushes 1 if not equal, 0 otherwise
// lt - less than (<) comparison: ( a b -- result )
// Pops two values, pushes 1 if a < b, 0 otherwise
// gt - greater than (>) comparison: ( a b -- result )
// Pops two values, pushes 1 if a > b, 0 otherwise
// lte - less than or equal (<=) comparison: ( a b -- result )
// Pops two values, pushes 1 if a <= b, 0 otherwise
// gte - greater than or equal (>=) comparison: ( a b -- result )
// Pops two values, pushes 1 if a >= b, 0 otherwise
// within - check if value is within range [min, max]: ( value min max -- result )
// Pops three values, pushes 1 if min <= value <= max, 0 otherwise
// Dump current stack contents for debugging
static void dump_stack(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	fprintf(stderr, "\nStack dump (%zu elements):\n", stack_size);

	if (stack_size == 0) {
		fprintf(stderr, "  (empty)");
		return;
	}

	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t elem;
		qd_stack_error err = qd_stack_element(ctx->st, i, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "  [%zu]: <error reading element>\n", i);
			continue;
		}

		fprintf(stderr, "  [%zu]: ", i);
		switch (elem.type) {
		case QD_STACK_TYPE_INT:
			fprintf(stderr, "int = %" PRId64 "\n", elem.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			fprintf(stderr, "float = %f\n", elem.value.f);
			break;
		case QD_STACK_TYPE_STR:
			fprintf(stderr, "str = \"%s\"\n", qd_string_data(elem.value.s));
			break;
		case QD_STACK_TYPE_PTR:
			fprintf(stderr, "ptr = %p\n", elem.value.p);
			break;
		case QD_STACK_TYPE_D128: {
			char text[QD_D128_STRING_MAX];
			qd_d128_format(elem.value.d, text, sizeof text);
			fprintf(stderr, "d128 = %s\n", text);
			break;
		}
		default:
			fprintf(stderr, "<unknown type>");
			break;
		}
	}
}

void qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name) {
	qd_stack* st = ctx->st;

	// Fast path: direct struct access for common cases
	size_t stack_size = st->size;
	if (stack_size < count) {
		QDRT_FATAL(ctx, func_name, "Stack underflow (required %zu elements, have %zu)", count, stack_size);
	}

	// Fast path: single integer parameter (very common case like fib(n:i64))
	if (count == 1 && types[0] == QD_STACK_TYPE_INT) {
		if (st->data[stack_size - 1].type == QD_STACK_TYPE_INT) {
			return; // Fast success
		}
		// Fall through to error reporting
		const char* actual_type_name = qdrt_type_name(st->data[stack_size - 1].type);
		QDRT_FATAL(ctx, func_name, "Type mismatch for parameter 1 (expected int, got %s)", actual_type_name);
	}

	// Check types match (from bottom to top of required elements)
	for (size_t i = 0; i < count; i++) {
		// Skip type check for untyped parameters (marked with QD_STACK_TYPE_PTR)
		if (types[i] == QD_STACK_TYPE_PTR) {
			continue;
		}

		// Direct struct access instead of function call
		size_t stack_index = stack_size - count + i;
		qd_stack_element_t* elem = &st->data[stack_index];

		if (elem->type != types[i]) {
			const char* expected_type_name = qdrt_type_name(types[i]);
			const char* actual_type_name = qdrt_type_name(elem->type);

			QDRT_FATAL(ctx, func_name, "Type mismatch for parameter %zu (expected %s, got %s)", i + 1,
					expected_type_name, actual_type_name);
		}
	}
}

// drop - remove top element from stack: ( a -- )
// free - deallocate memory pointed to by pointer on stack: ( ptr -- )
int qd_free(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "free", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "free", "Failed to pop pointer");
	}

	// Verify it's a pointer type
	if (val.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "free", "Expected pointer type, got type %d", val.type);
	}

	// As the slot's kind says: a struct or an array gives up the stack's reference, raw memory
	// is freed outright (free(NULL) is safe)
	if (val.ptr_kind == QD_PTR_STRUCT || val.ptr_kind == QD_PTR_ARRAY) {
		qdrt_release_element(&val);
	} else {
		free(val.value.p);
	}

	return (int){0};
}

// rot - rotate top 3 elements: ( a b c -- b c a )
// pick - copy the third element to the top: ( x y z -- x y z x )
// roll - rotate n elements, moving nth to top: ( ... n -- ... )
// Not a word any more: it takes its depth at run time, which defeats static stack
// tracking. Kept as an internal op -- code generation emits it to bring a method's
// receiver to the top.
// mod - modulo operation: ( a b -- a%b )
// and - bitwise AND: ( a b -- a&b )
// or - bitwise OR: ( a b -- a|b )
// xor - bitwise XOR: ( a b -- a^b )
// not - bitwise NOT: ( a -- ~a )
// shl - shift left: ( a n -- a<<n )
// shr - shift right (logical): ( a n -- a>>n )
// neg - negate top element: ( a -- -a )
// min - minimum of top 2 elements: ( a b -- min(a,b) )

// max - maximum of top 2 elements: ( a b -- max(a,b) )

int qd_err(qd_context* ctx) {
	// Retrieve error information from the last failed fallible function call
	// Stack effect: ( -- msg code )
	// Push error message (empty string if no error)
	const char* msg = ctx->error_msg ? ctx->error_msg : "";
	qd_stack_error err = qd_stack_push_str(ctx->st, msg);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "err", "Failed to push error message");
	}

	// Push error code
	err = qd_stack_push_int(ctx->st, ctx->error_code);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "err", "Failed to push error code");
	}

	// Clear error state after reading
	ctx->error_code = 0;
	ctx->has_error = 0;
	if (ctx->error_msg) {
		free(ctx->error_msg);
		ctx->error_msg = NULL;
	}

	return (int){0};
}

int qd_panic(qd_context* ctx) {
	// Pop error code and message from stack and set error state
	// Stack notation: ( msg code -- ) means msg is pushed first, code is on top
	// Stack after: []
	qd_stack_element_t error_msg_elem, error_code_elem;

	// Pop error code (integer) - it's on top
	qd_stack_error err = qd_stack_pop(ctx->st, &error_code_elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "panic", "Stack underflow when popping error code");
	}

	if (error_code_elem.type != QD_STACK_TYPE_INT) {
		QDRT_FATAL(ctx, "panic", "Expected integer error code, got type %d", error_code_elem.type);
	}

	// Pop error message (string)
	err = qd_stack_pop(ctx->st, &error_msg_elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "panic", "Stack underflow when popping error message");
	}

	if (error_msg_elem.type != QD_STACK_TYPE_STR) {
		// QDRT_FATAL terminates, so nothing after it can run -- the dump, backtrace,
		// string release and abort() that used to follow were unreachable.
		QDRT_FATAL(ctx, "panic", "Expected string error message, got type %d", error_msg_elem.type);
	}

	// A caller reads the code as the call's status, where 0 is `Ok`: a failure carrying it
	// would read as a success. A literal 0 is rejected at compile time; this is the code that
	// was computed.
	if (error_code_elem.value.i == 0) {
		release_if_string(&error_msg_elem);
		QDRT_FATAL(ctx, "panic", "Error code 0 is Ok; a failure needs a non-zero code (Err is 1)");
	}

	// Set error code and message
	ctx->error_code = error_code_elem.value.i;
	ctx->has_error = 1;

	// Free old error message if it exists
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}

	// Duplicate the error message string (context owns a copy)
	ctx->error_msg = strdup(qd_string_data(error_msg_elem.value.s));
	// strdup failure results in NULL error_msg, which is acceptable
	// (error_code still indicates the error condition)
	// Release the qd_string_t
	release_if_string(&error_msg_elem);

	return (int){0};
}

int64_t qd_error_code(const qd_context* ctx) {
	if (!ctx) {
		return 0;
	}
	return ctx->error_code;
}

const char* qd_error_message(const qd_context* ctx) {
	if (!ctx) {
		return NULL;
	}
	return ctx->error_msg;
}

void qd_clear_error(qd_context* ctx) {
	if (!ctx) {
		return;
	}
	ctx->error_code = 0;
	ctx->has_error = 0;
	if (ctx->error_msg) {
		free(ctx->error_msg);
		ctx->error_msg = NULL;
	}
}

size_t qd_context_stack_size(const qd_context* ctx) {
	if (!ctx || !ctx->st) {
		return 0;
	}
	return qd_stack_size(ctx->st);
}

// Context management functions
void qd_assertion_failed(qd_context* ctx) {
	if (ctx != NULL) {
		ctx->assertion_failures++;
	}
}

int64_t qd_assertion_failures(const qd_context* ctx) {
	return (ctx != NULL) ? ctx->assertion_failures : 0;
}

void qd_assertion_reset(qd_context* ctx) {
	if (ctx != NULL) {
		ctx->assertion_failures = 0;
	}
}

qd_context* qd_create_context(size_t stack_size) {
	qd_context* ctx = (qd_context*)malloc(sizeof(qd_context));
	if (ctx) {
		qd_stack_error err = qd_stack_init(&ctx->st, stack_size);
		if (err != QD_STACK_OK) {
			free(ctx);
			return NULL;
		}
		ctx->recovery_armed = false;
		ctx->natives = NULL;
		ctx->error_code = 0;
		ctx->has_error = 0;
		ctx->error_msg = NULL;
		ctx->userdata = NULL;
		ctx->argc = 0;
		ctx->argv = NULL;
		ctx->program_name = NULL;
		ctx->call_stack_depth = 0;
		ctx->assertion_failures = 0;
	}
	return ctx;
}

void qd_free_context(qd_context* ctx) {
	if (ctx == NULL) {
		return;
	}
	qd_native_clear(ctx);
	qd_stack_destroy(ctx->st);
	if (ctx->program_name) {
		free(ctx->program_name);
	}
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}
	free(ctx);
}

qd_context* qd_clone_context(const qd_context* src) {
	if (src == NULL) {
		return NULL;
	}

	/* Allocate new context */
	qd_context* ctx = (qd_context*)malloc(sizeof(qd_context));
	if (ctx == NULL) {
		return NULL;
	}

	/* Clone the stack */
	qd_stack_error err = qd_stack_clone(&ctx->st, src->st);
	if (err != QD_STACK_OK) {
		free(ctx);
		return NULL;
	}

	/* Copy error state */
	ctx->recovery_armed = false;
	ctx->natives = NULL;
	ctx->error_code = src->error_code;
	ctx->has_error = src->has_error;
	if (src->error_msg != NULL) {
		ctx->error_msg = strdup(src->error_msg);
		if (ctx->error_msg == NULL) {
			qd_stack_destroy(ctx->st);
			free(ctx);
			return NULL;
		}
	} else {
		ctx->error_msg = NULL;
	}
	/* Share userdata pointer (shallow copy) */
	ctx->userdata = src->userdata;

	/* Share command-line arguments (not copied) */
	ctx->argc = src->argc;
	ctx->argv = src->argv;

	/* Copy program name if present */
	if (src->program_name != NULL) {
		ctx->program_name = strdup(src->program_name);
		if (ctx->program_name == NULL) {
			if (ctx->error_msg) {
				free(ctx->error_msg);
			}
			qd_stack_destroy(ctx->st);
			free(ctx);
			return NULL;
		}
	} else {
		ctx->program_name = NULL;
	}

	/* Copy call stack */
	ctx->call_stack_depth = src->call_stack_depth;
	for (size_t i = 0; i < src->call_stack_depth; i++) {
		ctx->call_stack[i] = src->call_stack[i];
		ctx->call_stack_files[i] = src->call_stack_files[i];
		ctx->call_stack_lines[i] = src->call_stack_lines[i];
	}

	return ctx;
}

// Call stack management for debugging/error reporting
void qd_push_call(qd_context* ctx, const char* func_name, const char* file, size_t line) {
	if (ctx->call_stack_depth < QD_MAX_CALL_STACK_DEPTH) {
		ctx->call_stack[ctx->call_stack_depth] = func_name;
		ctx->call_stack_files[ctx->call_stack_depth] = file;
		ctx->call_stack_lines[ctx->call_stack_depth] = line;
		ctx->call_stack_depth++;
	}
}

void qd_pop_call(qd_context* ctx) {
	if (ctx->call_stack_depth > 0) {
		ctx->call_stack_depth--;
	}
}

// ---------------------------------------------------------------------------
// Coverage tracking (test-mode only; instrumentation is opt-in via -coverage).
// We use plain global state because coverage runs in a single test-runner
// process and gets reported at exit.
// ---------------------------------------------------------------------------
// Overridable, as the tables are static: an embedder with no use for coverage
// should not pay 144 KB of RAM for them.
#ifndef QD_MAX_COVERED_FUNCS
#define QD_MAX_COVERED_FUNCS 16384
#endif
static const char* g_coverage_names[QD_MAX_COVERED_FUNCS];
static unsigned char g_coverage_called[QD_MAX_COVERED_FUNCS];
static int g_coverage_count = 0;

int qd_coverage_register(const char* name) {
	if (g_coverage_count >= QD_MAX_COVERED_FUNCS) {
		return -1;
	}
	int idx = g_coverage_count++;
	g_coverage_names[idx] = name;
	g_coverage_called[idx] = 0;
	return idx;
}

void qd_coverage_mark(int idx) {
	if (idx >= 0 && idx < g_coverage_count) {
		g_coverage_called[idx] = 1;
	}
}

void qd_coverage_report(int use_color) {
	// Flush stdout so the test summary (printed via printf to stdout) appears
	// before the coverage report (printed via fprintf to stderr).
	fflush(stdout);

	int called = 0;
	for (int i = 0; i < g_coverage_count; i++) {
		if (g_coverage_called[i]) {
			called++;
		}
	}
	int pct = g_coverage_count > 0 ? (called * 100 / g_coverage_count) : 0;
	const char* hdr_color = use_color ? "\x1b[1;36m" : "";
	const char* ok_color = use_color ? "\x1b[32m" : "";
	const char* miss_color = use_color ? "\x1b[33m" : "";
	const char* end_color = use_color ? "\x1b[0m" : "";
	fprintf(stderr, "\n%sFunction coverage:%s %s%d/%d called (%d%%)%s\n", hdr_color, end_color,
			(called == g_coverage_count) ? ok_color : miss_color, called, g_coverage_count, pct, end_color);
	if (called < g_coverage_count) {
		fprintf(stderr, "%sUncalled functions:%s\n", miss_color, end_color);
		for (int i = 0; i < g_coverage_count; i++) {
			if (!g_coverage_called[i]) {
				fprintf(stderr, "  %s\n", g_coverage_names[i]);
			}
		}
	}
}

void qd_print_stack_trace(qd_context* ctx) {
	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL && isatty(STDERR_FILENO);
	const char* color_start = use_color ? "\x1b[1;31m" : "";
	const char* color_end = use_color ? "\x1b[0m" : "";

	if (ctx->call_stack_depth == 0) {
		fprintf(stderr, "\n%sStack trace:%s (empty)\n", color_start, color_end);
		return;
	}
	fprintf(stderr, "\n%sStack trace:%s\n", color_start, color_end);
	for (size_t i = ctx->call_stack_depth; i > 0; i--) {
		size_t idx = i - 1;
		size_t frame_num = ctx->call_stack_depth - i;
		const char* func_name = ctx->call_stack[idx];
		const char* file = ctx->call_stack_files[idx];
		size_t line = ctx->call_stack_lines[idx];
		if (file && file[0] != '\0') {
			fprintf(stderr, "\t%zu: %s(%s:%zu)\n", frame_num, func_name, file, line);
		} else {
			fprintf(stderr, "\t%zu: %s\n", frame_num, func_name);
		}
	}
}

void qd_print_error_msg(qd_context* ctx, const char* func_name) {
	// What the program printed came before the failure, so it goes out first
	fflush(stdout);

	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL && isatty(STDERR_FILENO);
	const char* color_red = use_color ? "\x1b[1;31m" : "";
	const char* color_dim = use_color ? "\x1b[2m" : "";
	const char* color_reset = use_color ? "\x1b[0m" : "";

	fprintf(stderr, "%sFatal error:%s ", color_red, color_reset);

	// Build error chain from call stack (bottom to top, i.e., main -> ... -> current)
	if (ctx->call_stack_depth > 0) {
		for (size_t i = 0; i < ctx->call_stack_depth; i++) {
			const char* name = ctx->call_stack[i];
			// Extract just the function name (after :: if present)
			const char* short_name = strrchr(name, ':');
			if (short_name && short_name > name && *(short_name - 1) == ':') {
				short_name++; // Skip past ::
			} else {
				short_name = name;
			}
			fprintf(stderr, "%s%s%s -> ", color_dim, short_name, color_reset);
		}
	}

	// Print the failing function name
	fprintf(stderr, "%s", func_name);

	if (ctx->error_msg && ctx->error_msg[0] != '\0') {
		fprintf(stderr, " failed: %s\n", ctx->error_msg);
	} else {
		fprintf(stderr, " failed\n");
	}
}

void qd_debug_print_stack(qd_context* ctx) {
	if (!ctx || !ctx->st) {
		fprintf(stderr, "Error: Invalid context");
		return;
	}

	size_t stack_size = qd_stack_size(ctx->st);

	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL && isatty(STDERR_FILENO);
	const char* color_blue = use_color ? "\x1b[1;34m" : "";
	const char* color_green = use_color ? "\x1b[0;32m" : "";
	const char* color_yellow = use_color ? "\x1b[0;33m" : "";
	const char* color_cyan = use_color ? "\x1b[0;36m" : "";
	const char* color_end = use_color ? "\x1b[0m" : "";

	fprintf(stderr, "\n%s=== Data Stack ===%s\n", color_blue, color_end);
	fprintf(stderr, "Size: %zu element%s\n", stack_size, stack_size == 1 ? "" : "s");

	if (stack_size == 0) {
		fprintf(stderr, "(empty)");
		return;
	}

	fprintf(stderr, "\nTop of stack (most recent):");
	fprintf(stderr, "  %sIdx%s  %sType%s     %sValue%s\n", color_blue, color_end, color_green, color_end, color_yellow,
			color_end);
	fprintf(stderr, "  ----------------------------------------");

	// Print from top (most recent) to bottom
	for (size_t i = stack_size; i > 0; i--) {
		qd_stack_element_t elem;
		qd_stack_error err = qd_stack_element(ctx->st, i - 1, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "  [%zu]: <error reading element>\n", i - 1);
			continue;
		}

		fprintf(stderr, "  %s[%2zu]%s ", color_blue, i - 1, color_end);

		switch (elem.type) {
		case QD_STACK_TYPE_INT:
			fprintf(stderr, "%sint    %s %s%" PRId64 "%s\n", color_green, color_end, color_yellow, elem.value.i,
					color_end);
			break;
		case QD_STACK_TYPE_FLOAT:
			fprintf(stderr, "%sfloat  %s %s%g%s\n", color_green, color_end, color_yellow, elem.value.f, color_end);
			break;
		case QD_STACK_TYPE_STR:
			if (elem.value.s) {
				// Truncate long strings
				if (strlen(qd_string_data(elem.value.s)) > 40) {
					fprintf(stderr, "%sstring %s %s\"%.37s...\"%s\n", color_green, color_end, color_cyan,
							qd_string_data(elem.value.s), color_end);
				} else {
					fprintf(stderr, "%sstring %s %s\"%s\"%s\n", color_green, color_end, color_cyan,
							qd_string_data(elem.value.s), color_end);
				}
			} else {
				fprintf(stderr, "%sstring %s %s<null>%s\n", color_green, color_end, color_cyan, color_end);
			}
			break;
		case QD_STACK_TYPE_PTR:
			fprintf(stderr, "%sptr    %s %s%p%s\n", color_green, color_end, color_yellow, elem.value.p, color_end);
			break;
		case QD_STACK_TYPE_D128: {
			char text[QD_D128_STRING_MAX];
			qd_d128_format(elem.value.d, text, sizeof text);
			fprintf(stderr, "%sd128   %s %s%s%s\n", color_green, color_end, color_yellow, text, color_end);
			break;
		}
		default:
			fprintf(stderr, "<unknown type %d>\n", elem.type);
			break;
		}
	}
	fprintf(stderr, "Bottom of stack (oldest)\n");
}

// Exported helper functions for split runtime files (runtime_stack.c, etc.)

void qdrt_dump_stack(qd_context* ctx) {
	dump_stack(ctx);
}

bool qdrt_validate_binary_numeric_op(qd_context* ctx, const char* op_name) {
	return validate_binary_numeric_op(ctx, op_name);
}

int qdrt_pop_two_values(qd_context* ctx, qd_stack_element_t* a, qd_stack_element_t* b) {
	return pop_two_values(ctx, a, b);
}

qd_stack_error qdrt_push_element(qd_stack* stack, const qd_stack_element_t* elem) {
	return push_element(stack, elem);
}

const char* qdrt_type_name(qd_stack_type type) {
	switch (type) {
	case QD_STACK_TYPE_INT:
		return "int";
	case QD_STACK_TYPE_FLOAT:
		return "float";
	case QD_STACK_TYPE_STR:
		return "str";
	case QD_STACK_TYPE_PTR:
		return "ptr";
	case QD_STACK_TYPE_D128:
		return "d128";
	}
	return "unknown";
}

// Public helper functions

void qd_set_error_msg(qd_context* ctx, const char* msg) {
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}
	ctx->error_msg = strdup(msg);
	// If strdup fails, error_msg will be NULL, which is acceptable
	// (error_code still indicates the error condition)
}

// Version Information

const char* qd_version(void) {
#ifdef QD_VERSION
	return QD_VERSION;
#else
	return "unknown";
#endif
}

int qd_version_api(void) {
#ifdef QD_VERSION_API
	return QD_VERSION_API;
#else
	return 0;
#endif
}

void qd_set_userdata(qd_context* ctx, void* userdata) {
	if (ctx) {
		ctx->userdata = userdata;
	}
}

void* qd_get_userdata(qd_context* ctx) {
	if (!ctx) {
		return NULL;
	}
	return ctx->userdata;
}

// User-facing functions (usr_ prefix for import mechanism)
int usr_rt_version(qd_context* ctx) {
	return qd_push_s(ctx, qd_version());
}

int usr_rt_version_api(qd_context* ctx) {
	return qd_push_i(ctx, qd_version_api());
}

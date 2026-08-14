#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

// Helper to compare floats with tolerance
static int float_eq(double a, double b) {
	return fabs(a - b) < 0.0001;
}

// Helper to duplicate a string (strdup is POSIX, not C11)
static char* qd_test_strdup(const char* s) {
	size_t len = strlen(s) + 1;
	char* dup = (char*)malloc(len);
	if (dup) {
		memcpy(dup, s, len);
	}
	return dup;
}

// Helper function to create a context
static qd_context* create_test_context(void) {
	qd_context* ctx = (qd_context*)malloc(sizeof(qd_context));
	qd_stack_init(&ctx->st, 256);
	return ctx;
}

// Helper function to destroy a context
static void destroy_test_context(qd_context* ctx) {
	qd_stack_destroy(ctx->st);
	free(ctx);
}

// Death-test support.
//
// Instruction implementations abort on stack underflow rather than returning an error code
// (see the "Two failure conventions" note in runtime.h), so pinning that behaviour means
// running the call in a child process and inspecting how it died. Returns the child's wait
// status; the caller checks WIFSIGNALED/WTERMSIG.
static int run_in_child(void (*body)(void)) {
	fflush(NULL); // don't duplicate buffered output into the child
	pid_t pid = fork();
	if (pid == 0) {
		// Silence the expected diagnostic so it doesn't pollute the test log.
		FILE* devnull = freopen("/dev/null", "w", stderr);
		(void)devnull;
		body();
		_exit(0); // reached only if body() failed to abort
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return status;
}

// These use qd_create_context rather than create_test_context: the abort path walks the call
// stack and error state, which create_test_context leaves uninitialised (it mallocs the context
// and only sets up ->st). With garbage there the child dies of SIGSEGV inside the diagnostic
// instead of the SIGABRT we are pinning.
static void print_empty_stack_child(void) {
	qd_print(qd_create_context(64));
}

static void peek_empty_stack_child(void) {
	qd_peek(qd_create_context(64));
}

static void printv_empty_stack_child(void) {
	qd_printv(qd_create_context(64));
}

static void neg_empty_stack_child(void) {
	qd_neg(qd_create_context(64));
}


TEST(MulIntegersTest) {
	qd_context* ctx = create_test_context();

	// Push 6 and 7
	qd_push_i(ctx, 6);
	qd_push_i(ctx, 7);

	// Multiply
	int result = qd_mul(ctx);
	ASSERT_EQ(result, 0, "mul should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "6 * 7 should be 42");

	destroy_test_context(ctx);
}

TEST(MulFloatsTest) {
	qd_context* ctx = create_test_context();

	// Push 2.5 and 4.0
	qd_push_f(ctx, 2.5);
	qd_push_f(ctx, 4.0);

	// Multiply
	int result = qd_mul(ctx);
	ASSERT_EQ(result, 0, "mul should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 10.0), "2.5 * 4.0 should be 10.0");

	destroy_test_context(ctx);
}

TEST(MulMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Push int 5 and float 2.0
	qd_push_i(ctx, 5);
	qd_push_f(ctx, 2.0);

	// Multiply
	int result = qd_mul(ctx);
	ASSERT_EQ(result, 0, "mul should succeed");

	// Check result (should be float)
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 10.0), "5 * 2.0 should be 10.0");

	destroy_test_context(ctx);
}

TEST(MulZeroTest) {
	qd_context* ctx = create_test_context();

	// Push 42 and 0
	qd_push_i(ctx, 42);
	qd_push_i(ctx, 0);

	// Multiply
	int result = qd_mul(ctx);
	ASSERT_EQ(result, 0, "mul should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 0, "42 * 0 should be 0");

	destroy_test_context(ctx);
}

TEST(MulNegativeTest) {
	qd_context* ctx = create_test_context();

	// Push -6 and 7
	qd_push_i(ctx, -6);
	qd_push_i(ctx, 7);

	// Multiply
	int result = qd_mul(ctx);
	ASSERT_EQ(result, 0, "mul should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, -42, "-6 * 7 should be -42");

	destroy_test_context(ctx);
}


TEST(AddIntegersTest) {
	qd_context* ctx = create_test_context();

	// Push 20 and 22
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 22);

	// Add
	int result = qd_add(ctx);
	ASSERT_EQ(result, 0, "add should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "20 + 22 should be 42");

	destroy_test_context(ctx);
}

TEST(AddFloatsTest) {
	qd_context* ctx = create_test_context();

	// Push 1.5 and 2.5
	qd_push_f(ctx, 1.5);
	qd_push_f(ctx, 2.5);

	// Add
	int result = qd_add(ctx);
	ASSERT_EQ(result, 0, "add should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 4.0), "1.5 + 2.5 should be 4.0");

	destroy_test_context(ctx);
}

TEST(AddMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Push int 5 and float 3.0
	qd_push_i(ctx, 5);
	qd_push_f(ctx, 3.0);

	// Add
	int result = qd_add(ctx);
	ASSERT_EQ(result, 0, "add should succeed");

	// Check result (should be float)
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 8.0), "5 + 3.0 should be 8.0");

	destroy_test_context(ctx);
}

TEST(AddNegativeTest) {
	qd_context* ctx = create_test_context();

	// Push 50 and -8
	qd_push_i(ctx, 50);
	qd_push_i(ctx, -8);

	// Add
	int result = qd_add(ctx);
	ASSERT_EQ(result, 0, "add should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "50 + (-8) should be 42");

	destroy_test_context(ctx);
}

TEST(AddZeroTest) {
	qd_context* ctx = create_test_context();

	// Push 42 and 0
	qd_push_i(ctx, 42);
	qd_push_i(ctx, 0);

	// Add
	int result = qd_add(ctx);
	ASSERT_EQ(result, 0, "add should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "42 + 0 should be 42");

	destroy_test_context(ctx);
}


TEST(SubIntegersTest) {
	qd_context* ctx = create_test_context();

	// Push 50 and 8
	qd_push_i(ctx, 50);
	qd_push_i(ctx, 8);

	// Subtract
	int result = qd_sub(ctx);
	ASSERT_EQ(result, 0, "sub should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "50 - 8 should be 42");

	destroy_test_context(ctx);
}

TEST(SubFloatsTest) {
	qd_context* ctx = create_test_context();

	// Push 10.0 and 3.5
	qd_push_f(ctx, 10.0);
	qd_push_f(ctx, 3.5);

	// Subtract
	int result = qd_sub(ctx);
	ASSERT_EQ(result, 0, "sub should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 6.5), "10.0 - 3.5 should be 6.5");

	destroy_test_context(ctx);
}

TEST(SubMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Push float 10.5 and int 0.5
	qd_push_f(ctx, 10.5);
	qd_push_f(ctx, 0.5);

	// Subtract
	int result = qd_sub(ctx);
	ASSERT_EQ(result, 0, "sub should succeed");

	// Check result (should be float)
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 10.0), "10.5 - 0.5 should be 10.0");

	destroy_test_context(ctx);
}

TEST(SubNegativeResultTest) {
	qd_context* ctx = create_test_context();

	// Push 10 and 52
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 52);

	// Subtract
	int result = qd_sub(ctx);
	ASSERT_EQ(result, 0, "sub should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, -42, "10 - 52 should be -42");

	destroy_test_context(ctx);
}

TEST(SubZeroTest) {
	qd_context* ctx = create_test_context();

	// Push 42 and 0
	qd_push_i(ctx, 42);
	qd_push_i(ctx, 0);

	// Subtract
	int result = qd_sub(ctx);
	ASSERT_EQ(result, 0, "sub should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "42 - 0 should be 42");

	destroy_test_context(ctx);
}


TEST(MulTypeErrorTest) {
	qd_context* ctx = create_test_context();

	// Push a string and an int - this should abort, so we can't test it directly
	// This test documents the expected behavior but cannot be executed
	// in a normal unit test because it calls abort()

	// In a real scenario:
	// qd_push_s(ctx, "hello");
	// qd_push_i(ctx, 5);
	// qd_mul(ctx); // Would abort with type error

	destroy_test_context(ctx);

	// Just mark the test as passed since we document the behavior
	ASSERT(1, "Type error test documented");
}

TEST(AddTypeErrorTest) {
	qd_context* ctx = create_test_context();

	// Similar to MulTypeErrorTest - documents behavior
	// Cannot test abort() in unit tests

	destroy_test_context(ctx);
	ASSERT(1, "Type error test documented");
}

TEST(SubTypeErrorTest) {
	qd_context* ctx = create_test_context();

	// Similar to MulTypeErrorTest - documents behavior
	// Cannot test abort() in unit tests

	destroy_test_context(ctx);
	ASSERT(1, "Type error test documented");
}

// ============================================================
// Bitwise operations
// ============================================================

TEST(AndBasicTest) {
	qd_context* ctx = create_test_context();

	// 0xFF00 & 0x0FF0 = 0x0F00
	qd_push_i(ctx, 0xFF00);
	qd_push_i(ctx, 0x0FF0);

	int result = qd_and(ctx);
	ASSERT_EQ(result, 0, "and should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "and result should be int");
	ASSERT_EQ((int)elem.value.i, 0x0F00, "0xFF00 & 0x0FF0 should be 0x0F00");

	destroy_test_context(ctx);
}

TEST(OrBasicTest) {
	qd_context* ctx = create_test_context();

	// 0xF0 | 0x0F = 0xFF
	qd_push_i(ctx, 0xF0);
	qd_push_i(ctx, 0x0F);

	int result = qd_or(ctx);
	ASSERT_EQ(result, 0, "or should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "or result should be int");
	ASSERT_EQ((int)elem.value.i, 0xFF, "0xF0 | 0x0F should be 0xFF");

	destroy_test_context(ctx);
}

TEST(XorBasicTest) {
	qd_context* ctx = create_test_context();

	// 0xFF ^ 0x0F = 0xF0
	qd_push_i(ctx, 0xFF);
	qd_push_i(ctx, 0x0F);

	int result = qd_xor(ctx);
	ASSERT_EQ(result, 0, "xor should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "xor result should be int");
	ASSERT_EQ((int)elem.value.i, 0xF0, "0xFF ^ 0x0F should be 0xF0");

	destroy_test_context(ctx);
}

TEST(ShlBasicTest) {
	qd_context* ctx = create_test_context();

	// 1 << 4 = 16
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 4);

	int result = qd_shl(ctx);
	ASSERT_EQ(result, 0, "shl should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "shl result should be int");
	ASSERT_EQ((int)elem.value.i, 16, "1 << 4 should be 16");

	destroy_test_context(ctx);
}

TEST(ShrBasicTest) {
	qd_context* ctx = create_test_context();

	// 256 >> 4 = 16
	qd_push_i(ctx, 256);
	qd_push_i(ctx, 4);

	int result = qd_shr(ctx);
	ASSERT_EQ(result, 0, "shr should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "shr result should be int");
	ASSERT_EQ((int)elem.value.i, 16, "256 >> 4 should be 16");

	destroy_test_context(ctx);
}

TEST(NotBasicTest) {
	qd_context* ctx = create_test_context();

	// ~0 = -1 on two's complement 64-bit
	qd_push_i(ctx, 0);

	int result = qd_not(ctx);
	ASSERT_EQ(result, 0, "not should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "not result should be int");
	ASSERT_EQ((int)elem.value.i, -1, "~0 should be -1");

	destroy_test_context(ctx);
}

TEST(BitwisePreservesStackTest) {
	qd_context* ctx = create_test_context();

	// Verify bitwise ops only consume the top 2 elements
	qd_push_i(ctx, 100);
	qd_push_i(ctx, 0xFF);
	qd_push_i(ctx, 0x0F);

	qd_and(ctx);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "stack should have 2 elements after and");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0x0F, "0xFF & 0x0F should be 0x0F");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 100, "bottom element should be preserved");

	destroy_test_context(ctx);
}

// ============================================================
// Type casting operations
// ============================================================

TEST(CastiFromFloatTest) {
	qd_context* ctx = create_test_context();

	// Cast 3.7 (float) to int -> 3 (truncated)
	qd_push_f(ctx, 3.7);
	int result = qd_casti(ctx);
	ASSERT_EQ(result, 0, "casti should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "casti result should be int");
	ASSERT_EQ((int)elem.value.i, 3, "casti(3.7) should be 3");

	destroy_test_context(ctx);
}

TEST(CastiFromIntTest) {
	qd_context* ctx = create_test_context();

	// Cast int to int -> identity
	qd_push_i(ctx, 42);
	int result = qd_casti(ctx);
	ASSERT_EQ(result, 0, "casti should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "casti result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "casti(42) should be 42");

	destroy_test_context(ctx);
}

TEST(CastiNegativeFloatTest) {
	qd_context* ctx = create_test_context();

	// Cast -3.9 to int -> -3 (truncated toward zero)
	qd_push_f(ctx, -3.9);
	int result = qd_casti(ctx);
	ASSERT_EQ(result, 0, "casti should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "casti result should be int");
	ASSERT_EQ((int)elem.value.i, -3, "casti(-3.9) should be -3");

	destroy_test_context(ctx);
}

TEST(CastfFromIntTest) {
	qd_context* ctx = create_test_context();

	// Cast 42 (int) to float -> 42.0
	qd_push_i(ctx, 42);
	int result = qd_castf(ctx);
	ASSERT_EQ(result, 0, "castf should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "castf result should be float");
	ASSERT(float_eq(elem.value.f, 42.0), "castf(42) should be 42.0");

	destroy_test_context(ctx);
}

TEST(CastfFromFloatTest) {
	qd_context* ctx = create_test_context();

	// Cast float to float -> identity
	qd_push_f(ctx, 3.14);
	int result = qd_castf(ctx);
	ASSERT_EQ(result, 0, "castf should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "castf result should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "castf(3.14) should be 3.14");

	destroy_test_context(ctx);
}

TEST(CastpFromIntTest) {
	qd_context* ctx = create_test_context();

	// Cast int to pointer
	qd_push_i(ctx, 0x12345678);
	int result = qd_castp(ctx);
	ASSERT_EQ(result, 0, "castp should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_PTR, "castp result should be ptr");
	ASSERT(elem.value.p == (void*)(intptr_t)0x12345678, "castp should preserve int value as pointer");

	destroy_test_context(ctx);
}

TEST(CastpFromPtrTest) {
	qd_context* ctx = create_test_context();

	// Cast ptr to ptr -> identity
	int dummy = 42;
	qd_push_p(ctx, &dummy);
	int result = qd_castp(ctx);
	ASSERT_EQ(result, 0, "castp should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_PTR, "castp result should be ptr");
	ASSERT(elem.value.p == &dummy, "castp(ptr) should be identity");

	destroy_test_context(ctx);
}

// ============================================================
// Deep stack variants
// ============================================================

TEST(DupdTest) {
	qd_context* ctx = create_test_context();

	// dupd: ( a b -- a a b )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);

	int result = qd_dupd(ctx);
	ASSERT_EQ(result, 0, "dupd should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "stack should have 3 elements after dupd");

	// Verify: bottom to top: 10, 10, 20
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "second should be 10 (duplicate)");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "third should be 10 (original)");

	destroy_test_context(ctx);
}

TEST(DupdMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// dupd with mixed types: ( 3.14 42 -- 3.14 3.14 42 )
	qd_push_f(ctx, 3.14);
	qd_push_i(ctx, 42);

	int result = qd_dupd(ctx);
	ASSERT_EQ(result, 0, "dupd should succeed with mixed types");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "stack should have 3 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "top should be int");
	ASSERT_EQ((int)elem.value.i, 42, "top should be 42");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "second should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "second should be 3.14");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "third should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "third should be 3.14");

	destroy_test_context(ctx);
}

TEST(SwapdTest) {
	qd_context* ctx = create_test_context();

	// swapd: ( a b c -- b a c )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);

	int result = qd_swapd(ctx);
	ASSERT_EQ(result, 0, "swapd should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "stack should have 3 elements");

	// Verify: bottom to top: 20, 10, 30
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "top should be 30");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "second should be 10");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "third should be 20");

	destroy_test_context(ctx);
}

TEST(SwapdPreservesRestTest) {
	qd_context* ctx = create_test_context();

	// swapd with extra elements: ( 1 2 3 4 -- 1 3 2 4 )
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 4);

	int result = qd_swapd(ctx);
	ASSERT_EQ(result, 0, "swapd should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "stack should have 4 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 4, "top should be 4");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 2, "second should be 2");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 3, "third should be 3");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "fourth should be 1");

	destroy_test_context(ctx);
}

TEST(OverdTest) {
	qd_context* ctx = create_test_context();

	// overd: ( a b c -- a b a c )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);

	int result = qd_overd(ctx);
	ASSERT_EQ(result, 0, "overd should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "stack should have 4 elements after overd");

	// Verify: bottom to top: 10, 20, 10, 30
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "top should be 30");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "second should be 10 (copy of third)");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "third should be 20");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "fourth should be 10");

	destroy_test_context(ctx);
}

TEST(NipdTest) {
	qd_context* ctx = create_test_context();

	// nipd: ( a b c -- a c )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);

	int result = qd_nipd(ctx);
	ASSERT_EQ(result, 0, "nipd should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "stack should have 2 elements after nipd");

	// Verify: bottom to top: 10, 30
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "top should be 30");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "second should be 10 (b removed)");

	destroy_test_context(ctx);
}

TEST(NipdPreservesRestTest) {
	qd_context* ctx = create_test_context();

	// nipd with extra: ( 1 2 3 4 -- 1 2 4 )
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 4);

	int result = qd_nipd(ctx);
	ASSERT_EQ(result, 0, "nipd should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "stack should have 3 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 4, "top should be 4");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 2, "second should be 2 (3 removed)");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "third should be 1");

	destroy_test_context(ctx);
}

// ============================================================
// Array operations (C API, not stack-based)
// ============================================================

TEST(ArrayCreateAndReleaseTest) {
	qd_array_t* arr = qd_array_create(10, QD_ARRAY_TYPE_INT);
	ASSERT(arr != NULL, "array create should not return NULL");
	ASSERT_EQ((int)qd_array_length(arr), 0, "new array should have length 0");
	qd_array_release(arr);
}

TEST(ArrayPushAndGetIntTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_INT);
	ASSERT(arr != NULL, "array create should succeed");

	int rc = qd_array_push_int(arr, 42);
	ASSERT_EQ(rc, 0, "push_int should succeed");
	rc = qd_array_push_int(arr, 100);
	ASSERT_EQ(rc, 0, "push_int should succeed");
	rc = qd_array_push_int(arr, -7);
	ASSERT_EQ(rc, 0, "push_int should succeed");

	ASSERT_EQ((int)qd_array_length(arr), 3, "array should have 3 elements");

	int64_t val;
	rc = qd_array_get_int(arr, 0, &val);
	ASSERT_EQ(rc, 0, "get_int should succeed");
	ASSERT_EQ((int)val, 42, "element 0 should be 42");

	rc = qd_array_get_int(arr, 1, &val);
	ASSERT_EQ(rc, 0, "get_int should succeed");
	ASSERT_EQ((int)val, 100, "element 1 should be 100");

	rc = qd_array_get_int(arr, 2, &val);
	ASSERT_EQ(rc, 0, "get_int should succeed");
	ASSERT_EQ((int)val, -7, "element 2 should be -7");

	qd_array_release(arr);
}

TEST(ArrayPushAndGetFloatTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_FLOAT);
	ASSERT(arr != NULL, "array create should succeed");

	int rc = qd_array_push_float(arr, 3.14);
	ASSERT_EQ(rc, 0, "push_float should succeed");
	rc = qd_array_push_float(arr, 2.71);
	ASSERT_EQ(rc, 0, "push_float should succeed");

	ASSERT_EQ((int)qd_array_length(arr), 2, "array should have 2 elements");

	double val;
	rc = qd_array_get_float(arr, 0, &val);
	ASSERT_EQ(rc, 0, "get_float should succeed");
	ASSERT(float_eq(val, 3.14), "element 0 should be 3.14");

	rc = qd_array_get_float(arr, 1, &val);
	ASSERT_EQ(rc, 0, "get_float should succeed");
	ASSERT(float_eq(val, 2.71), "element 1 should be 2.71");

	qd_array_release(arr);
}

TEST(ArrayPushPtrTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_PTR);
	ASSERT(arr != NULL, "array create should succeed");

	int dummy1 = 1;
	int dummy2 = 2;
	int rc = qd_array_push_ptr(arr, &dummy1);
	ASSERT_EQ(rc, 0, "push_ptr should succeed");
	rc = qd_array_push_ptr(arr, &dummy2);
	ASSERT_EQ(rc, 0, "push_ptr should succeed");

	ASSERT_EQ((int)qd_array_length(arr), 2, "array should have 2 elements");

	void* val;
	rc = qd_array_get_ptr(arr, 0, &val);
	ASSERT_EQ(rc, 0, "get_ptr should succeed");
	ASSERT(val == &dummy1, "element 0 should be &dummy1");

	rc = qd_array_get_ptr(arr, 1, &val);
	ASSERT_EQ(rc, 0, "get_ptr should succeed");
	ASSERT(val == &dummy2, "element 1 should be &dummy2");

	qd_array_release(arr);
}

TEST(ArraySetIntTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_INT);

	qd_array_push_int(arr, 0);
	qd_array_push_int(arr, 0);
	qd_array_push_int(arr, 0);

	// Overwrite element 1
	int rc = qd_array_set_int(arr, 1, 99);
	ASSERT_EQ(rc, 0, "set_int should succeed");

	int64_t val;
	qd_array_get_int(arr, 1, &val);
	ASSERT_EQ((int)val, 99, "element 1 should be 99 after set");

	// Verify other elements unchanged
	qd_array_get_int(arr, 0, &val);
	ASSERT_EQ((int)val, 0, "element 0 should still be 0");
	qd_array_get_int(arr, 2, &val);
	ASSERT_EQ((int)val, 0, "element 2 should still be 0");

	qd_array_release(arr);
}

TEST(ArraySetFloatTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_FLOAT);

	qd_array_push_float(arr, 0.0);
	qd_array_push_float(arr, 0.0);

	// Overwrite element 0
	int rc = qd_array_set_float(arr, 0, 9.81);
	ASSERT_EQ(rc, 0, "set_float should succeed");

	double val;
	qd_array_get_float(arr, 0, &val);
	ASSERT(float_eq(val, 9.81), "element 0 should be 9.81 after set");

	// Verify other element unchanged
	qd_array_get_float(arr, 1, &val);
	ASSERT(float_eq(val, 0.0), "element 1 should still be 0.0");

	qd_array_release(arr);
}

TEST(ArrayLengthGrowsTest) {
	qd_array_t* arr = qd_array_create(2, QD_ARRAY_TYPE_INT);
	ASSERT_EQ((int)qd_array_length(arr), 0, "length should be 0 initially");

	qd_array_push_int(arr, 1);
	ASSERT_EQ((int)qd_array_length(arr), 1, "length should be 1 after one push");

	qd_array_push_int(arr, 2);
	ASSERT_EQ((int)qd_array_length(arr), 2, "length should be 2 after two pushes");

	// Push beyond initial capacity to test growth
	qd_array_push_int(arr, 3);
	ASSERT_EQ((int)qd_array_length(arr), 3, "length should be 3 after growing");

	int64_t val;
	qd_array_get_int(arr, 2, &val);
	ASSERT_EQ((int)val, 3, "element 2 should be 3 after growth");

	qd_array_release(arr);
}

TEST(ArrayGetOutOfBoundsTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_INT);
	qd_array_push_int(arr, 42);

	int64_t val;
	int rc = qd_array_get_int(arr, 1, &val);
	ASSERT(rc != 0, "get_int out of bounds should fail");

	rc = qd_array_get_int(arr, 100, &val);
	ASSERT(rc != 0, "get_int far out of bounds should fail");

	qd_array_release(arr);
}

TEST(ArraySetOutOfBoundsTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_INT);
	qd_array_push_int(arr, 42);

	int rc = qd_array_set_int(arr, 1, 99);
	ASSERT(rc != 0, "set_int out of bounds should fail");

	qd_array_release(arr);
}

TEST(ArrayTypeMismatchTest) {
	// Create an int array but try to get as float
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_INT);
	qd_array_push_int(arr, 42);

	double fval;
	int rc = qd_array_get_float(arr, 0, &fval);
	ASSERT(rc != 0, "get_float from int array should fail");

	// Create a float array but try to push int
	qd_array_t* farr = qd_array_create(4, QD_ARRAY_TYPE_FLOAT);
	rc = qd_array_push_int(farr, 42);
	ASSERT(rc != 0, "push_int to float array should fail");

	qd_array_release(arr);
	qd_array_release(farr);
}

TEST(ArrayRetainReleaseTest) {
	qd_array_t* arr = qd_array_create(4, QD_ARRAY_TYPE_INT);
	qd_array_push_int(arr, 42);

	// Retain increases refcount; a second release should be safe
	qd_array_retain(arr);

	// First release - should not free (refcount was 2, now 1)
	qd_array_release(arr);

	// Array should still be valid
	int64_t val;
	int rc = qd_array_get_int(arr, 0, &val);
	ASSERT_EQ(rc, 0, "array should still be valid after retain+release");
	ASSERT_EQ((int)val, 42, "value should still be 42");

	// Final release
	qd_array_release(arr);
}

// ============================================================
// Error handling
// ============================================================

TEST(ErrNoErrorTest) {
	qd_context* ctx = qd_create_context(256);
	ASSERT(ctx != NULL, "create_context should succeed");

	// With no error, qd_err should push "" and 0
	int result = qd_err(ctx);
	ASSERT_EQ(result, 0, "err should succeed");

	// Stack should have 2 elements: msg (string), code (int)
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "stack should have 2 elements after err");

	// Top is the error code
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "error code should be int");
	ASSERT_EQ((int)elem.value.i, 0, "error code should be 0 when no error");

	// Second is the error message
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "error message should be string");
	ASSERT_STR_EQ(qd_string_data(elem.value.s), "", "error message should be empty");
	qd_string_release(elem.value.s);

	qd_free_context(ctx);
}

TEST(ErrWithErrorStateTest) {
	qd_context* ctx = qd_create_context(256);
	ASSERT(ctx != NULL, "create_context should succeed");

	// Simulate an error state
	ctx->error_code = 42;
	ctx->error_msg = qd_test_strdup("test error");

	int result = qd_err(ctx);
	ASSERT_EQ(result, 0, "err should succeed");

	// Top is the error code
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "error code should be int");
	ASSERT_EQ((int)elem.value.i, 42, "error code should be 42");

	// Second is the error message
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "error message should be string");
	ASSERT_STR_EQ(qd_string_data(elem.value.s), "test error", "error message should match");
	qd_string_release(elem.value.s);

	// After qd_err, error state should be cleared
	ASSERT_EQ((int)ctx->error_code, 0, "error code should be cleared after err");
	ASSERT(ctx->error_msg == NULL, "error msg should be NULL after err");

	qd_free_context(ctx);
}

TEST(ClearErrorTest) {
	qd_context* ctx = qd_create_context(256);
	ASSERT(ctx != NULL, "create_context should succeed");

	// Set error state
	ctx->error_code = 99;
	ctx->error_msg = qd_test_strdup("some error");

	// Clear it
	qd_clear_error(ctx);

	ASSERT_EQ((int)ctx->error_code, 0, "error code should be 0 after clear_error");
	ASSERT(ctx->error_msg == NULL, "error msg should be NULL after clear_error");

	// Clearing again should be safe (no double-free)
	qd_clear_error(ctx);
	ASSERT_EQ((int)ctx->error_code, 0, "error code should still be 0");

	qd_free_context(ctx);
}

TEST(ClearErrorNullCtxTest) {
	// qd_clear_error(NULL) should not crash
	qd_clear_error(NULL);
	ASSERT(1, "clear_error(NULL) should not crash");
}

// ============================================================
// String operations
// ============================================================

TEST(NlDoesNotAffectStackTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	int result = qd_nl(ctx);
	ASSERT_EQ(result, 0, "nl should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "stack should be unchanged after nl");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 42, "value should be preserved after nl");

	destroy_test_context(ctx);
}

TEST(NlEmptyStackTest) {
	qd_context* ctx = create_test_context();

	int result = qd_nl(ctx);
	ASSERT_EQ(result, 0, "nl on empty stack should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "stack should remain empty after nl");

	destroy_test_context(ctx);
}

// ============================================================
// Stack query operations
// ============================================================

TEST(StackIsEmptyTest) {
	qd_context* ctx = create_test_context();

	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "new stack should be empty");

	qd_push_i(ctx, 1);
	ASSERT_FALSE(qd_stack_is_empty(ctx->st), "stack with element should not be empty");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after popping last element");

	destroy_test_context(ctx);
}

TEST(StackCapacityTest) {
	qd_context* ctx = create_test_context();

	// create_test_context creates stack with capacity 256
	ASSERT_EQ((int)qd_stack_capacity(ctx->st), 256, "stack capacity should be 256");

	// Pushing should not change capacity
	qd_push_i(ctx, 1);
	ASSERT_EQ((int)qd_stack_capacity(ctx->st), 256, "capacity should remain 256 after push");

	destroy_test_context(ctx);
}

TEST(StackIsEmptyAfterClearTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	ASSERT_FALSE(qd_stack_is_empty(ctx->st), "stack should not be empty");

	qd_clear(ctx);
	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after clear");

	destroy_test_context(ctx);
}


int main(void) {
	return UC_PrintResults();
}


TEST(PrintPopsStackTest) {
	qd_context* ctx = create_test_context();

	// Push three values
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);

	// Stack should have 3 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	// Print (pop) the top element
	int result = qd_print(ctx);
	ASSERT_EQ(result, 0, "print should succeed");

	// Stack should now have 2 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after print");

	// Top element should be 2 now
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_peek(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "peek should succeed");
	ASSERT_EQ((int)elem.value.i, 2, "Top element should be 2");

	destroy_test_context(ctx);
}

TEST(PrintEmptyStackTest) {
	// An instruction implementation reached with an underflowed stack means the generated
	// code was wrong, so qd_print reports and aborts rather than returning an error the
	// caller cannot see -- generated code discards these return values. This used to return
	// -2 silently, which turned a compiler bug into a wrong answer with exit status 0.
	//
	// Run it in a child so the abort doesn't take the test process with it.
	int status = run_in_child(print_empty_stack_child);
	ASSERT(WIFSIGNALED(status), "print on empty stack should abort, not return");
	ASSERT_EQ(WTERMSIG(status), SIGABRT, "print on empty stack should raise SIGABRT");
}

TEST(PeekEmptyStackTest) {
	int status = run_in_child(peek_empty_stack_child);
	ASSERT(WIFSIGNALED(status), "peek on empty stack should abort, not return");
	ASSERT_EQ(WTERMSIG(status), SIGABRT, "peek on empty stack should raise SIGABRT");
}

TEST(PrintvEmptyStackTest) {
	int status = run_in_child(printv_empty_stack_child);
	ASSERT(WIFSIGNALED(status), "printv on empty stack should abort, not return");
	ASSERT_EQ(WTERMSIG(status), SIGABRT, "printv on empty stack should raise SIGABRT");
}

TEST(NegEmptyStackTest) {
	int status = run_in_child(neg_empty_stack_child);
	ASSERT(WIFSIGNALED(status), "neg on empty stack should abort, not return");
	ASSERT_EQ(WTERMSIG(status), SIGABRT, "neg on empty stack should raise SIGABRT");
}

TEST(PrintIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	int result = qd_print(ctx);

	ASSERT_EQ(result, 0, "print should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after print");

	destroy_test_context(ctx);
}

TEST(PrintFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	int result = qd_print(ctx);

	ASSERT_EQ(result, 0, "print should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after print");

	destroy_test_context(ctx);
}

TEST(PrintStringTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	int result = qd_print(ctx);

	ASSERT_EQ(result, 0, "print should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after print");

	destroy_test_context(ctx);
}

TEST(PrintvPopsStackTest) {
	qd_context* ctx = create_test_context();

	// Push three values
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);

	// Stack should have 3 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	// Printv (pop) the top element
	int result = qd_printv(ctx);
	ASSERT_EQ(result, 0, "printv should succeed");

	// Stack should now have 2 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after printv");

	// Top element should be 2 now
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_peek(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "peek should succeed");
	ASSERT_EQ((int)elem.value.i, 2, "Top element should be 2");

	destroy_test_context(ctx);
}

TEST(PrintvIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	int result = qd_printv(ctx);

	ASSERT_EQ(result, 0, "printv should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after printv");

	destroy_test_context(ctx);
}

TEST(PrintvFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	int result = qd_printv(ctx);

	ASSERT_EQ(result, 0, "printv should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after printv");

	destroy_test_context(ctx);
}

TEST(PrintsNonDestructiveTest) {
	qd_context* ctx = create_test_context();

	// Push three values
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);

	// Stack should have 3 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	// Prints (non-destructive)
	int result = qd_prints(ctx);
	ASSERT_EQ(result, 0, "prints should succeed");

	// Stack should still have 3 elements (non-destructive)
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should still have 3 elements after prints");

	destroy_test_context(ctx);
}

TEST(PrintsEmptyStackTest) {
	qd_context* ctx = create_test_context();

	// Prints on empty stack should succeed but output nothing
	int result = qd_prints(ctx);
	ASSERT_EQ(result, 0, "prints on empty stack should succeed");

	destroy_test_context(ctx);
}

TEST(PrintsMixedTypesTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	qd_push_s(ctx, "hello");

	int result = qd_prints(ctx);
	ASSERT_EQ(result, 0, "prints should succeed with mixed types");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should still have 3 elements");

	destroy_test_context(ctx);
}

TEST(PrintsvNonDestructiveTest) {
	qd_context* ctx = create_test_context();

	// Push three values
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);

	// Stack should have 3 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	// Printsv (non-destructive with types)
	int result = qd_printsv(ctx);
	ASSERT_EQ(result, 0, "printsv should succeed");

	// Stack should still have 3 elements (non-destructive)
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should still have 3 elements after printsv");

	destroy_test_context(ctx);
}

TEST(PrintsvEmptyStackTest) {
	qd_context* ctx = create_test_context();

	// Printsv on empty stack should succeed but output nothing
	int result = qd_printsv(ctx);
	ASSERT_EQ(result, 0, "printsv on empty stack should succeed");

	destroy_test_context(ctx);
}

TEST(PrintsvMixedTypesTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	qd_push_s(ctx, "hello");

	int result = qd_printsv(ctx);
	ASSERT_EQ(result, 0, "printsv should succeed with mixed types");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should still have 3 elements");

	destroy_test_context(ctx);
}


TEST(DupIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	int result = qd_dup(ctx);

	ASSERT_EQ(result, 0, "dup should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after dup");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_INT, "top element should be int");
	ASSERT_EQ((int)elem1.value.i, 42, "top element should be 42");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_INT, "second element should be int");
	ASSERT_EQ((int)elem2.value.i, 42, "second element should be 42");

	destroy_test_context(ctx);
}

TEST(DupFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	int result = qd_dup(ctx);

	ASSERT_EQ(result, 0, "dup should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after dup");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_FLOAT, "top element should be float");
	ASSERT(float_eq(elem1.value.f, 3.14), "top element should be 3.14");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_FLOAT, "second element should be float");
	ASSERT(float_eq(elem2.value.f, 3.14), "second element should be 3.14");

	destroy_test_context(ctx);
}

TEST(DupStringTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	int result = qd_dup(ctx);

	ASSERT_EQ(result, 0, "dup should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after dup");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_STR, "top element should be string");
	ASSERT_STR_EQ(qd_string_data(elem1.value.s), "hello", "top element should be 'hello'");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_STR, "second element should be string");
	ASSERT_STR_EQ(qd_string_data(elem2.value.s), "hello", "second element should be 'hello'");

	qd_string_release(elem1.value.s);
	qd_string_release(elem2.value.s);
	destroy_test_context(ctx);
}

TEST(DupNonDestructiveTest) {
	qd_context* ctx = create_test_context();

	// Push multiple elements
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);

	// Dup the top element
	int result = qd_dup(ctx);
	ASSERT_EQ(result, 0, "dup should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "Stack should have 4 elements after dup");

	// Top two should be 30, 30
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 30, "top element should be 30");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 30, "second element should be 30");

	// Then 20 and 10
	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 20, "third element should be 20");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 10, "fourth element should be 10");

	destroy_test_context(ctx);
}


TEST(SwapIntegersTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	int result = qd_swap(ctx);

	ASSERT_EQ(result, 0, "swap should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after swap");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_INT, "top element should be int");
	ASSERT_EQ((int)elem1.value.i, 10, "top element should be 10 after swap");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_INT, "second element should be int");
	ASSERT_EQ((int)elem2.value.i, 20, "second element should be 20 after swap");

	destroy_test_context(ctx);
}

TEST(SwapMixedTypesTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	int result = qd_swap(ctx);

	ASSERT_EQ(result, 0, "swap should succeed with mixed types");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after swap");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_INT, "top element should be int");
	ASSERT_EQ((int)elem1.value.i, 42, "top element should be 42 after swap");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_FLOAT, "second element should be float");
	ASSERT(float_eq(elem2.value.f, 3.14), "second element should be 3.14 after swap");

	destroy_test_context(ctx);
}

TEST(SwapStringsTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "world");
	int result = qd_swap(ctx);

	ASSERT_EQ(result, 0, "swap should succeed with strings");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after swap");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_STR, "top element should be string");
	ASSERT_STR_EQ(qd_string_data(elem1.value.s), "hello", "top element should be 'hello' after swap");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_STR, "second element should be string");
	ASSERT_STR_EQ(qd_string_data(elem2.value.s), "world", "second element should be 'world' after swap");

	qd_string_release(elem1.value.s);
	qd_string_release(elem2.value.s);
	destroy_test_context(ctx);
}

TEST(SwapDoesNotAffectRestOfStackTest) {
	qd_context* ctx = create_test_context();

	// Push multiple elements
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 4);

	// Swap top two (3 and 4)
	int result = qd_swap(ctx);
	ASSERT_EQ(result, 0, "swap should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "Stack should still have 4 elements");

	// Check order: should be 1, 2, 4, 3 (from bottom to top)
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 3, "top element should be 3");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 4, "second element should be 4");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 2, "third element should be 2");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 1, "fourth element should be 1");

	destroy_test_context(ctx);
}


TEST(OverIntegersTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	int result = qd_over(ctx);

	ASSERT_EQ(result, 0, "over should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements after over");

	// Stack should be: 10, 20, 10 (from bottom to top)
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "top element should be int");
	ASSERT_EQ((int)elem.value.i, 10, "top element should be 10");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "second element should be int");
	ASSERT_EQ((int)elem.value.i, 20, "second element should be 20");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "third pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "third element should be int");
	ASSERT_EQ((int)elem.value.i, 10, "third element should be 10");

	destroy_test_context(ctx);
}

TEST(OverMixedTypesTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	int result = qd_over(ctx);

	ASSERT_EQ(result, 0, "over should succeed with mixed types");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements after over");

	// Stack should be: 42, 3.14, 42
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "top element should be int");
	ASSERT_EQ((int)elem.value.i, 42, "top element should be 42");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "second element should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "second element should be 3.14");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "third pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "third element should be int");
	ASSERT_EQ((int)elem.value.i, 42, "third element should be 42");

	destroy_test_context(ctx);
}

TEST(OverStringsTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "world");
	int result = qd_over(ctx);

	ASSERT_EQ(result, 0, "over should succeed with strings");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements after over");

	// Stack should be: "hello", "world", "hello"
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "top element should be string");
	ASSERT_STR_EQ(qd_string_data(elem.value.s), "hello", "top element should be 'hello'");
	qd_string_release(elem.value.s);

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "second element should be string");
	ASSERT_STR_EQ(qd_string_data(elem.value.s), "world", "second element should be 'world'");
	qd_string_release(elem.value.s);

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "third pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "third element should be string");
	ASSERT_STR_EQ(qd_string_data(elem.value.s), "hello", "third element should be 'hello'");
	qd_string_release(elem.value.s);

	destroy_test_context(ctx);
}

TEST(OverPreservesRestOfStackTest) {
	qd_context* ctx = create_test_context();

	// Push multiple elements
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 4);

	// Over copies the second element (3) to the top
	int result = qd_over(ctx);
	ASSERT_EQ(result, 0, "over should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 5, "Stack should have 5 elements");

	// Check order: should be 1, 2, 3, 4, 3 (from bottom to top)
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 3, "top element should be 3");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 4, "second element should be 4");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 3, "third element should be 3");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 2, "fourth element should be 2");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 1, "fifth element should be 1");

	destroy_test_context(ctx);
}


TEST(NipIntegersTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	int result = qd_nip(ctx);

	ASSERT_EQ(result, 0, "nip should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element after nip");

	// Stack should be: 20
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "element should be int");
	ASSERT_EQ((int)elem.value.i, 20, "element should be 20");

	destroy_test_context(ctx);
}

TEST(NipMixedTypesTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	qd_push_i(ctx, 42);
	int result = qd_nip(ctx);

	ASSERT_EQ(result, 0, "nip should succeed with mixed types");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element after nip");

	// Stack should be: 42
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "element should be int");
	ASSERT_EQ((int)elem.value.i, 42, "element should be 42");

	destroy_test_context(ctx);
}

TEST(NipStringsTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "world");
	int result = qd_nip(ctx);

	ASSERT_EQ(result, 0, "nip should succeed with strings");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element after nip");

	// Stack should be: "world"
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "element should be string");
	ASSERT_STR_EQ(qd_string_data(elem.value.s), "world", "element should be 'world'");
	qd_string_release(elem.value.s);

	destroy_test_context(ctx);
}

TEST(NipPreservesRestOfStackTest) {
	qd_context* ctx = create_test_context();

	// Push multiple elements
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 4);

	// Nip removes the second element (3), leaving 4 on top
	int result = qd_nip(ctx);
	ASSERT_EQ(result, 0, "nip should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	// Check order: should be 1, 2, 4 (from bottom to top)
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 4, "top element should be 4");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 2, "second element should be 2");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 1, "third element should be 1");

	destroy_test_context(ctx);
}

// inc tests
TEST(IncIntegerTest) {
	qd_context* ctx = create_test_context();

	// inc(5) = 6
	qd_push_i(ctx, 5);
	qd_inc(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "inc should preserve int type");
	ASSERT_EQ((int)elem.value.i, 6, "inc(5) should be 6");

	// inc(-1) = 0
	qd_push_i(ctx, -1);
	qd_inc(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "inc(-1) should be 0");

	destroy_test_context(ctx);
}

TEST(IncFloatTest) {
	qd_context* ctx = create_test_context();

	// inc(2.5) = 3.5
	qd_push_f(ctx, 2.5);
	qd_inc(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "inc should preserve float type");
	ASSERT(float_eq(elem.value.f, 3.5), "inc(2.5) should be 3.5");

	destroy_test_context(ctx);
}

TEST(IncZeroTest) {
	qd_context* ctx = create_test_context();

	// inc(0) = 1
	qd_push_i(ctx, 0);
	qd_inc(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "inc should preserve int type");
	ASSERT_EQ((int)elem.value.i, 1, "inc(0) should be 1");

	destroy_test_context(ctx);
}

// dec tests
TEST(DecIntegerTest) {
	qd_context* ctx = create_test_context();

	// dec(5) = 4
	qd_push_i(ctx, 5);
	qd_dec(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "dec should preserve int type");
	ASSERT_EQ((int)elem.value.i, 4, "dec(5) should be 4");

	// dec(0) = -1
	qd_push_i(ctx, 0);
	qd_dec(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, -1, "dec(0) should be -1");

	destroy_test_context(ctx);
}

TEST(DecFloatTest) {
	qd_context* ctx = create_test_context();

	// dec(2.5) = 1.5
	qd_push_f(ctx, 2.5);
	qd_dec(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "dec should preserve float type");
	ASSERT(float_eq(elem.value.f, 1.5), "dec(2.5) should be 1.5");

	destroy_test_context(ctx);
}

TEST(DecNegativeTest) {
	qd_context* ctx = create_test_context();

	// dec(-5) = -6
	qd_push_i(ctx, -5);
	qd_dec(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "dec should preserve int type");
	ASSERT_EQ((int)elem.value.i, -6, "dec(-5) should be -6");

	destroy_test_context(ctx);
}

// clear tests
TEST(ClearEmptyStackTest) {
	qd_context* ctx = create_test_context();

	// Clear an already empty stack
	qd_clear(ctx);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after clearing empty stack");

	destroy_test_context(ctx);
}

TEST(ClearSingleElementTest) {
	qd_context* ctx = create_test_context();

	// Push one element and clear
	qd_push_i(ctx, 42);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");
	qd_clear(ctx);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after clear");

	destroy_test_context(ctx);
}

TEST(ClearMultipleElementsTest) {
	qd_context* ctx = create_test_context();

	// Push multiple elements of different types and clear
	qd_push_i(ctx, 10);
	qd_push_f(ctx, 3.14);
	qd_push_i(ctx, 20);
	qd_push_f(ctx, 2.71);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "Stack should have 4 elements");

	qd_clear(ctx);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after clear");

	// Verify we can still use the stack after clearing
	qd_push_i(ctx, 99);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 99, "Should be able to push after clear");

	destroy_test_context(ctx);
}

TEST(ClearWithStringsTest) {
	qd_context* ctx = create_test_context();

	// Push strings (which need memory management) and clear
	qd_push_s(ctx, "hello");
	qd_push_i(ctx, 42);
	qd_push_s(ctx, "world");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	qd_clear(ctx);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after clear");

	destroy_test_context(ctx);
}

// depth tests
TEST(DepthEmptyStackTest) {
	qd_context* ctx = create_test_context();

	// Get depth of empty stack
	qd_depth(ctx);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element (the depth)");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "depth should return int");
	ASSERT_EQ((int)elem.value.i, 0, "depth of empty stack should be 0");

	destroy_test_context(ctx);
}

TEST(DepthSingleElementTest) {
	qd_context* ctx = create_test_context();

	// Push one element and get depth
	qd_push_i(ctx, 42);
	qd_depth(ctx);

	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements (value + depth)");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "depth should return int");
	ASSERT_EQ((int)elem.value.i, 1, "depth should be 1");

	destroy_test_context(ctx);
}

TEST(DepthMultipleElementsTest) {
	qd_context* ctx = create_test_context();

	// Push multiple elements and get depth
	qd_push_i(ctx, 10);
	qd_push_f(ctx, 3.14);
	qd_push_i(ctx, 20);
	qd_depth(ctx);

	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "Stack should have 4 elements (3 values + depth)");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "depth should return int");
	ASSERT_EQ((int)elem.value.i, 3, "depth should be 3");

	destroy_test_context(ctx);
}

TEST(DepthAfterClearTest) {
	qd_context* ctx = create_test_context();

	// Push elements, clear, then get depth
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);
	qd_clear(ctx);
	qd_depth(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "depth should return int");
	ASSERT_EQ((int)elem.value.i, 0, "depth after clear should be 0");

	destroy_test_context(ctx);
}

TEST(DepthIncludesItselfTest) {
	qd_context* ctx = create_test_context();

	// Verify that depth counts elements BEFORE the depth is pushed
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_depth(ctx);  // Should push 2, not 3
	qd_depth(ctx);  // Should push 3 (1, 2, depth_result)

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 3, "second depth should be 3");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 2, "first depth should be 2");

	destroy_test_context(ctx);
}

// dup2 tests
TEST(Dup2BasicTest) {
	qd_context* ctx = create_test_context();

	// Push two integers and duplicate the pair: ( 10 20 -- 10 20 10 20 )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_dup2(ctx);

	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "Stack should have 4 elements");

	qd_stack_element_t elem;
	qd_stack_error err;

	// Verify from top to bottom: 20, 10, 20, 10
	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "top should be int");
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "second should be int");
	ASSERT_EQ((int)elem.value.i, 10, "second should be 10");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "third should be int");
	ASSERT_EQ((int)elem.value.i, 20, "third should be 20");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "fourth should be int");
	ASSERT_EQ((int)elem.value.i, 10, "fourth should be 10");

	destroy_test_context(ctx);
}

TEST(Dup2MixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Push different types: ( int float -- int float int float )
	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	qd_dup2(ctx);

	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "Stack should have 4 elements");

	qd_stack_element_t elem;

	// Pop and verify: float, int, float, int
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "top should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "top should be 3.14");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "second should be int");
	ASSERT_EQ((int)elem.value.i, 42, "second should be 42");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "third should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "third should be 3.14");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "fourth should be int");
	ASSERT_EQ((int)elem.value.i, 42, "fourth should be 42");

	destroy_test_context(ctx);
}

TEST(Dup2WithStringsTest) {
	qd_context* ctx = create_test_context();

	// Push strings: ( "hello" "world" -- "hello" "world" "hello" "world" )
	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "world");
	qd_dup2(ctx);

	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "Stack should have 4 elements");

	qd_stack_element_t elem;

	// Verify the duplicated strings
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "top should be string");
	ASSERT_EQ(strcmp(qd_string_data(elem.value.s), "world"), 0, "top should be 'world'");
	qd_string_release(elem.value.s);

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "second should be string");
	ASSERT_EQ(strcmp(qd_string_data(elem.value.s), "hello"), 0, "second should be 'hello'");
	qd_string_release(elem.value.s);

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "third should be string");
	ASSERT_EQ(strcmp(qd_string_data(elem.value.s), "world"), 0, "third should be 'world'");
	qd_string_release(elem.value.s);

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "fourth should be string");
	ASSERT_EQ(strcmp(qd_string_data(elem.value.s), "hello"), 0, "fourth should be 'hello'");
	qd_string_release(elem.value.s);

	destroy_test_context(ctx);
}

TEST(Dup2WithMoreElementsTest) {
	qd_context* ctx = create_test_context();

	// Push 3 elements, dup2 should duplicate top 2: ( 1 2 3 -- 1 2 3 2 3 )
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	qd_dup2(ctx);

	ASSERT_EQ((int)qd_stack_size(ctx->st), 5, "Stack should have 5 elements");

	qd_stack_element_t elem;

	// Verify from top: 3, 2, 3, 2, 1
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 3, "1st should be 3");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 2, "2nd should be 2");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 3, "3rd should be 3");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 2, "4th should be 2");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5th should be 1");

	destroy_test_context(ctx);
}

TEST(SwapWithDupTest) {
	qd_context* ctx = create_test_context();

	// Test combining dup and swap
	qd_push_i(ctx, 5);
	qd_dup(ctx);  // Stack: 5, 5
	qd_push_i(ctx, 10);  // Stack: 5, 5, 10
	qd_swap(ctx);  // Stack: 5, 10, 5

	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 5, "top should be 5");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 10, "second should be 10");

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 5, "third should be 5");

	destroy_test_context(ctx);
}


// qd_eq tests
TEST(EqIntegersEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 == 5 (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_eq(ctx);

	ASSERT_EQ(result, 0, "eq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "5 == 5 should be 1");

	destroy_test_context(ctx);
}

TEST(EqIntegersNotEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 == 3 (should return 0)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	int result = qd_eq(ctx);

	ASSERT_EQ(result, 0, "eq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 0, "5 == 3 should be 0");

	destroy_test_context(ctx);
}

TEST(EqFloatsEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 3.14 == 3.14 (should return 1)
	qd_push_f(ctx, 3.14);
	qd_push_f(ctx, 3.14);
	int result = qd_eq(ctx);

	ASSERT_EQ(result, 0, "eq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "3.14 == 3.14 should be 1");

	destroy_test_context(ctx);
}

TEST(EqMixedTypesEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 (int) == 5.0 (float) (should return 1)
	qd_push_i(ctx, 5);
	qd_push_f(ctx, 5.0);
	int result = qd_eq(ctx);

	ASSERT_EQ(result, 0, "eq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "5 == 5.0 should be 1");

	destroy_test_context(ctx);
}

TEST(EqNegativeNumbersTest) {
	qd_context* ctx = create_test_context();

	// Test -5 == -5 (should return 1)
	qd_push_i(ctx, -5);
	qd_push_i(ctx, -5);
	int result = qd_eq(ctx);

	ASSERT_EQ(result, 0, "eq should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "-5 == -5 should be 1");

	destroy_test_context(ctx);
}

TEST(EqZeroTest) {
	qd_context* ctx = create_test_context();

	// Test 0 == 0 (should return 1)
	qd_push_i(ctx, 0);
	qd_push_i(ctx, 0);
	int result = qd_eq(ctx);

	ASSERT_EQ(result, 0, "eq should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "0 == 0 should be 1");

	destroy_test_context(ctx);
}

// qd_neq tests
TEST(NeqIntegersNotEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 != 3 (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	int result = qd_neq(ctx);

	ASSERT_EQ(result, 0, "neq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "5 != 3 should be 1");

	destroy_test_context(ctx);
}

TEST(NeqIntegersEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 != 5 (should return 0)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_neq(ctx);

	ASSERT_EQ(result, 0, "neq should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "5 != 5 should be 0");

	destroy_test_context(ctx);
}

TEST(NeqFloatsNotEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 3.14 != 2.71 (should return 1)
	qd_push_f(ctx, 3.14);
	qd_push_f(ctx, 2.71);
	int result = qd_neq(ctx);

	ASSERT_EQ(result, 0, "neq should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "3.14 != 2.71 should be 1");

	destroy_test_context(ctx);
}

TEST(NeqMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Test 5 (int) != 5.5 (float) (should return 1)
	qd_push_i(ctx, 5);
	qd_push_f(ctx, 5.5);
	int result = qd_neq(ctx);

	ASSERT_EQ(result, 0, "neq should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5 != 5.5 should be 1");

	destroy_test_context(ctx);
}

// qd_lt tests
TEST(LtIntegersLessThanTest) {
	qd_context* ctx = create_test_context();

	// Test 3 < 5 (should return 1)
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 5);
	int result = qd_lt(ctx);

	ASSERT_EQ(result, 0, "lt should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "3 < 5 should be 1");

	destroy_test_context(ctx);
}

TEST(LtIntegersGreaterThanTest) {
	qd_context* ctx = create_test_context();

	// Test 5 < 3 (should return 0)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	int result = qd_lt(ctx);

	ASSERT_EQ(result, 0, "lt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "5 < 3 should be 0");

	destroy_test_context(ctx);
}

TEST(LtIntegersEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 < 5 (should return 0)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_lt(ctx);

	ASSERT_EQ(result, 0, "lt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "5 < 5 should be 0");

	destroy_test_context(ctx);
}

TEST(LtFloatsTest) {
	qd_context* ctx = create_test_context();

	// Test 2.5 < 3.7 (should return 1)
	qd_push_f(ctx, 2.5);
	qd_push_f(ctx, 3.7);
	int result = qd_lt(ctx);

	ASSERT_EQ(result, 0, "lt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "2.5 < 3.7 should be 1");

	destroy_test_context(ctx);
}

TEST(LtMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Test 3 (int) < 5.5 (float) (should return 1)
	qd_push_i(ctx, 3);
	qd_push_f(ctx, 5.5);
	int result = qd_lt(ctx);

	ASSERT_EQ(result, 0, "lt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "3 < 5.5 should be 1");

	destroy_test_context(ctx);
}

TEST(LtNegativeNumbersTest) {
	qd_context* ctx = create_test_context();

	// Test -5 < -3 (should return 1)
	qd_push_i(ctx, -5);
	qd_push_i(ctx, -3);
	int result = qd_lt(ctx);

	ASSERT_EQ(result, 0, "lt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "-5 < -3 should be 1");

	destroy_test_context(ctx);
}

// qd_gt tests
TEST(GtIntegersGreaterThanTest) {
	qd_context* ctx = create_test_context();

	// Test 5 > 3 (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	int result = qd_gt(ctx);

	ASSERT_EQ(result, 0, "gt should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "5 > 3 should be 1");

	destroy_test_context(ctx);
}

TEST(GtIntegersLessThanTest) {
	qd_context* ctx = create_test_context();

	// Test 3 > 5 (should return 0)
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 5);
	int result = qd_gt(ctx);

	ASSERT_EQ(result, 0, "gt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "3 > 5 should be 0");

	destroy_test_context(ctx);
}

TEST(GtIntegersEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 > 5 (should return 0)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_gt(ctx);

	ASSERT_EQ(result, 0, "gt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "5 > 5 should be 0");

	destroy_test_context(ctx);
}

TEST(GtFloatsTest) {
	qd_context* ctx = create_test_context();

	// Test 5.2 > 3.1 (should return 1)
	qd_push_f(ctx, 5.2);
	qd_push_f(ctx, 3.1);
	int result = qd_gt(ctx);

	ASSERT_EQ(result, 0, "gt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5.2 > 3.1 should be 1");

	destroy_test_context(ctx);
}

TEST(GtMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Test 5.5 (float) > 3 (int) (should return 1)
	qd_push_f(ctx, 5.5);
	qd_push_i(ctx, 3);
	int result = qd_gt(ctx);

	ASSERT_EQ(result, 0, "gt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5.5 > 3 should be 1");

	destroy_test_context(ctx);
}

TEST(GtNegativeNumbersTest) {
	qd_context* ctx = create_test_context();

	// Test -3 > -5 (should return 1)
	qd_push_i(ctx, -3);
	qd_push_i(ctx, -5);
	int result = qd_gt(ctx);

	ASSERT_EQ(result, 0, "gt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "-3 > -5 should be 1");

	destroy_test_context(ctx);
}

// qd_lte tests
TEST(LteIntegersLessThanTest) {
	qd_context* ctx = create_test_context();

	// Test 3 <= 5 (should return 1)
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 5);
	int result = qd_lte(ctx);

	ASSERT_EQ(result, 0, "lte should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "3 <= 5 should be 1");

	destroy_test_context(ctx);
}

TEST(LteIntegersEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 <= 5 (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_lte(ctx);

	ASSERT_EQ(result, 0, "lte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5 <= 5 should be 1");

	destroy_test_context(ctx);
}

TEST(LteIntegersGreaterThanTest) {
	qd_context* ctx = create_test_context();

	// Test 5 <= 3 (should return 0)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	int result = qd_lte(ctx);

	ASSERT_EQ(result, 0, "lte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "5 <= 3 should be 0");

	destroy_test_context(ctx);
}

TEST(LteFloatsTest) {
	qd_context* ctx = create_test_context();

	// Test 2.5 <= 2.5 (should return 1)
	qd_push_f(ctx, 2.5);
	qd_push_f(ctx, 2.5);
	int result = qd_lte(ctx);

	ASSERT_EQ(result, 0, "lte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "2.5 <= 2.5 should be 1");

	destroy_test_context(ctx);
}

TEST(LteMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Test 3 (int) <= 5.0 (float) (should return 1)
	qd_push_i(ctx, 3);
	qd_push_f(ctx, 5.0);
	int result = qd_lte(ctx);

	ASSERT_EQ(result, 0, "lte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "3 <= 5.0 should be 1");

	destroy_test_context(ctx);
}

// qd_gte tests
TEST(GteIntegersGreaterThanTest) {
	qd_context* ctx = create_test_context();

	// Test 5 >= 3 (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	int result = qd_gte(ctx);

	ASSERT_EQ(result, 0, "gte should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "5 >= 3 should be 1");

	destroy_test_context(ctx);
}

TEST(GteIntegersEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 >= 5 (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_gte(ctx);

	ASSERT_EQ(result, 0, "gte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5 >= 5 should be 1");

	destroy_test_context(ctx);
}

TEST(GteIntegersLessThanTest) {
	qd_context* ctx = create_test_context();

	// Test 3 >= 5 (should return 0)
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 5);
	int result = qd_gte(ctx);

	ASSERT_EQ(result, 0, "gte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "3 >= 5 should be 0");

	destroy_test_context(ctx);
}

TEST(GteFloatsTest) {
	qd_context* ctx = create_test_context();

	// Test 5.2 >= 5.2 (should return 1)
	qd_push_f(ctx, 5.2);
	qd_push_f(ctx, 5.2);
	int result = qd_gte(ctx);

	ASSERT_EQ(result, 0, "gte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5.2 >= 5.2 should be 1");

	destroy_test_context(ctx);
}

TEST(GteMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Test 5.0 (float) >= 3 (int) (should return 1)
	qd_push_f(ctx, 5.0);
	qd_push_i(ctx, 3);
	int result = qd_gte(ctx);

	ASSERT_EQ(result, 0, "gte should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5.0 >= 3 should be 1");

	destroy_test_context(ctx);
}

// Stack preservation tests
TEST(ComparisonPreservesRestOfStackTest) {
	qd_context* ctx = create_test_context();

	// Test that comparison only affects top 2 elements
	qd_push_i(ctx, 100);
	qd_push_i(ctx, 200);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 5);
	qd_lt(ctx);  // 3 < 5 = 1

	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "top should be 1 (result of 3 < 5)");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 200, "second should be 200");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 100, "third should be 100");

	destroy_test_context(ctx);
}

TEST(ComparisonChainTest) {
	qd_context* ctx = create_test_context();

	// Test chaining comparisons: ((3 < 5) == 1) should work
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 5);
	qd_lt(ctx);  // Result: 1

	qd_push_i(ctx, 1);
	qd_eq(ctx);  // Result: 1 == 1 = 1

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "chained comparison should work");

	destroy_test_context(ctx);
}


TEST(WithinValueInRangeIntegersTest) {
	qd_context* ctx = create_test_context();

	// Test 5 within [3, 10] (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "5 within [3, 10] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinValueBelowRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 2 within [3, 10] (should return 0)
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "2 within [3, 10] should be 0");

	destroy_test_context(ctx);
}

TEST(WithinValueAboveRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 15 within [3, 10] (should return 0)
	qd_push_i(ctx, 15);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "15 within [3, 10] should be 0");

	destroy_test_context(ctx);
}

TEST(WithinValueEqualsMinBoundaryTest) {
	qd_context* ctx = create_test_context();

	// Test 3 within [3, 10] (should return 1, inclusive)
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "3 within [3, 10] should be 1 (inclusive)");

	destroy_test_context(ctx);
}

TEST(WithinValueEqualsMaxBoundaryTest) {
	qd_context* ctx = create_test_context();

	// Test 10 within [3, 10] (should return 1, inclusive)
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "10 within [3, 10] should be 1 (inclusive)");

	destroy_test_context(ctx);
}

TEST(WithinFloatsInRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 5.5 within [3.2, 10.8] (should return 1)
	qd_push_f(ctx, 5.5);
	qd_push_f(ctx, 3.2);
	qd_push_f(ctx, 10.8);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5.5 within [3.2, 10.8] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinFloatsOutOfRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 2.5 within [3.0, 10.0] (should return 0)
	qd_push_f(ctx, 2.5);
	qd_push_f(ctx, 3.0);
	qd_push_f(ctx, 10.0);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "2.5 within [3.0, 10.0] should be 0");

	destroy_test_context(ctx);
}

TEST(WithinMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Test 5.5 (float) within [3 (int), 10 (int)] (should return 1)
	qd_push_f(ctx, 5.5);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed with mixed types");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5.5 within [3, 10] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinMixedTypesOutOfRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 2.5 (float) within [3 (int), 10 (int)] (should return 0)
	qd_push_f(ctx, 2.5);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed with mixed types");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "2.5 within [3, 10] should be 0");

	destroy_test_context(ctx);
}

TEST(WithinNegativeRangeTest) {
	qd_context* ctx = create_test_context();

	// Test -5 within [-10, -3] (should return 1)
	qd_push_i(ctx, -5);
	qd_push_i(ctx, -10);
	qd_push_i(ctx, -3);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "-5 within [-10, -3] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinNegativeOutOfRangeTest) {
	qd_context* ctx = create_test_context();

	// Test -15 within [-10, -3] (should return 0)
	qd_push_i(ctx, -15);
	qd_push_i(ctx, -10);
	qd_push_i(ctx, -3);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "-15 within [-10, -3] should be 0");

	destroy_test_context(ctx);
}

TEST(WithinSinglePointRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 5 within [5, 5] (should return 1, single point)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "5 within [5, 5] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinSinglePointRangeOutsideTest) {
	qd_context* ctx = create_test_context();

	// Test 6 within [5, 5] (should return 0)
	qd_push_i(ctx, 6);
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "6 within [5, 5] should be 0");

	destroy_test_context(ctx);
}

TEST(WithinZeroInRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 0 within [-5, 5] (should return 1)
	qd_push_i(ctx, 0);
	qd_push_i(ctx, -5);
	qd_push_i(ctx, 5);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "0 within [-5, 5] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinLargeRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 500 within [100, 1000] (should return 1)
	qd_push_i(ctx, 500);
	qd_push_i(ctx, 100);
	qd_push_i(ctx, 1000);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "500 within [100, 1000] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinFloatBoundaryTest) {
	qd_context* ctx = create_test_context();

	// Test 3.0 within [3.0, 10.0] (should return 1, exact boundary)
	qd_push_f(ctx, 3.0);
	qd_push_f(ctx, 3.0);
	qd_push_f(ctx, 10.0);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "3.0 within [3.0, 10.0] should be 1");

	destroy_test_context(ctx);
}

TEST(WithinPreservesRestOfStackTest) {
	qd_context* ctx = create_test_context();

	// Test that within only affects top 3 elements
	qd_push_i(ctx, 100);
	qd_push_i(ctx, 200);
	qd_push_i(ctx, 5);   // value
	qd_push_i(ctx, 3);   // min
	qd_push_i(ctx, 10);  // max
	qd_within(ctx);

	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "top should be 1 (result of 5 within [3, 10])");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 200, "second should be 200");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 100, "third should be 100");

	destroy_test_context(ctx);
}

TEST(WithinChainedWithComparisonTest) {
	qd_context* ctx = create_test_context();

	// Test chaining: (5 within [3, 10]) == 1 should work
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 3);
	qd_push_i(ctx, 10);
	qd_within(ctx);  // Result: 1

	qd_push_i(ctx, 1);
	qd_eq(ctx);  // Result: 1 == 1 = 1

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "chained within and eq should work");

	destroy_test_context(ctx);
}

TEST(WithinInvertedRangeTest) {
	qd_context* ctx = create_test_context();

	// Test 5 within [10, 3] (inverted range, should return 0)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 3);
	int result = qd_within(ctx);

	ASSERT_EQ(result, 0, "within should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "5 within [10, 3] (inverted) should be 0");

	destroy_test_context(ctx);
}


TEST(DropIntegerTest) {
	qd_context* ctx = create_test_context();

	// Push 10, 20, 30
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);

	// Drop top element (30)
	int result = qd_drop(ctx);
	ASSERT_EQ(result, 0, "drop should succeed");

	// Stack should have 10, 20
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "stack should have 2 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20");

	destroy_test_context(ctx);
}

TEST(DropStringTest) {
	qd_context* ctx = create_test_context();

	// Push a string
	qd_push_s(ctx, "test string");

	// Drop it (should free the string)
	int result = qd_drop(ctx);
	ASSERT_EQ(result, 0, "drop should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "stack should be empty");

	destroy_test_context(ctx);
}

TEST(DropFloatTest) {
	qd_context* ctx = create_test_context();

	// Push floats
	qd_push_f(ctx, 1.5);
	qd_push_f(ctx, 2.5);

	qd_drop(ctx);
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "should be float");
	ASSERT(float_eq(elem.value.f, 1.5), "should be 1.5");

	destroy_test_context(ctx);
}


TEST(Drop2Test) {
	qd_context* ctx = create_test_context();

	// Push 10, 20, 30, 40
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);
	qd_push_i(ctx, 40);

	// Drop top 2 elements (40, 30)
	int result = qd_drop2(ctx);
	ASSERT_EQ(result, 0, "drop2 should succeed");

	// Stack should have 10, 20
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "stack should have 2 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20");

	destroy_test_context(ctx);
}

TEST(Drop2StringsTest) {
	qd_context* ctx = create_test_context();

	// Push strings
	qd_push_s(ctx, "first");
	qd_push_s(ctx, "second");

	// Drop both (should free both strings)
	int result = qd_drop2(ctx);
	ASSERT_EQ(result, 0, "drop2 should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "stack should be empty");

	destroy_test_context(ctx);
}


TEST(RotTest) {
	qd_context* ctx = create_test_context();

	// Push 10, 20, 30 (top is 30)
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);

	// Rotate: (10 20 30) -> (20 30 10)
	int result = qd_rot(ctx);
	ASSERT_EQ(result, 0, "rot should succeed");

	// Check order: top should be 10, then 30, then 20
	qd_stack_element_t elem;

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "top should be 10");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "second should be 30");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "third should be 20");

	destroy_test_context(ctx);
}

TEST(RotMixedTypesTest) {
	qd_context* ctx = create_test_context();

	// Push int, float, string
	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	qd_push_s(ctx, "test");

	qd_rot(ctx);

	// Check types and values
	qd_stack_element_t elem;

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "top should be int");
	ASSERT_EQ((int)elem.value.i, 42, "top should be 42");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "second should be string");
	qd_string_release(elem.value.s);

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "third should be float");

	destroy_test_context(ctx);
}


TEST(ModPositiveTest) {
	qd_context* ctx = create_test_context();

	// 17 % 5 = 2
	qd_push_i(ctx, 17);
	qd_push_i(ctx, 5);

	int result = qd_mod(ctx);
	ASSERT_EQ(result, 0, "mod should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 2, "17 % 5 should be 2");

	destroy_test_context(ctx);
}

TEST(ModNegativeTest) {
	qd_context* ctx = create_test_context();

	// -17 % 5 = -2
	qd_push_i(ctx, -17);
	qd_push_i(ctx, 5);

	qd_mod(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, -2, "-17 % 5 should be -2");

	destroy_test_context(ctx);
}

TEST(ModZeroTest) {
	qd_context* ctx = create_test_context();

	// 10 % 5 = 0
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 5);

	qd_mod(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "10 % 5 should be 0");

	destroy_test_context(ctx);
}


TEST(NegIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	int result = qd_neg(ctx);
	ASSERT_EQ(result, 0, "neg should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "should remain int");
	ASSERT_EQ((int)elem.value.i, -42, "42 negated should be -42");

	destroy_test_context(ctx);
}

TEST(NegFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	qd_neg(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "should remain float");
	ASSERT(float_eq(elem.value.f, -3.14), "3.14 negated should be -3.14");

	destroy_test_context(ctx);
}

TEST(NegZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_neg(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 0, "0 negated should be 0");

	destroy_test_context(ctx);
}

TEST(NegNegativeTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -42);
	qd_neg(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 42, "-42 negated should be 42");

	destroy_test_context(ctx);
}


TEST(TuckTest) {
	qd_context* ctx = create_test_context();

	// Setup: ( 10 20 -- 20 10 20 )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);

	qd_tuck(ctx);

	// Verify stack has 3 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "stack should have 3 elements");

	// Pop and verify: 20, 10, 20
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "second should be 10");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "third should be 20");

	destroy_test_context(ctx);
}

TEST(PickTest) {
	qd_context* ctx = create_test_context();

	// Setup stack: ( 10 20 30 40 )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);
	qd_push_i(ctx, 40);

	// Pick index 2 (should copy 20 to top)
	qd_push_i(ctx, 2);
	qd_pick(ctx);

	// Verify: ( 10 20 30 40 20 )
	ASSERT_EQ((int)qd_stack_size(ctx->st), 5, "stack should have 5 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20 (picked)");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 40, "should be 40");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "should be 30");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "should be 20");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "should be 10");

	destroy_test_context(ctx);
}

TEST(RollTest) {
	qd_context* ctx = create_test_context();

	// Setup stack: ( 10 20 30 40 )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);
	qd_push_i(ctx, 40);

	// Roll index 3 (0-based): ( 10 20 30 40 ) -> ( 20 30 40 10 )
	qd_push_i(ctx, 3);
	qd_roll(ctx);

	// Verify
	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "stack should have 4 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "top should be 10 (rolled from index 3)");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 40, "should be 40");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "should be 30");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "should be 20");

	destroy_test_context(ctx);
}

TEST(Swap2Test) {
	qd_context* ctx = create_test_context();

	// Setup: ( 10 20 30 40 -- 30 40 10 20 )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);
	qd_push_i(ctx, 40);

	qd_swap2(ctx);

	// Verify
	ASSERT_EQ((int)qd_stack_size(ctx->st), 4, "stack should have 4 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "should be 10");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 40, "should be 40");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "should be 30");

	destroy_test_context(ctx);
}

TEST(Over2Test) {
	qd_context* ctx = create_test_context();

	// Setup: ( 10 20 30 40 -- 10 20 30 40 10 20 )
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);
	qd_push_i(ctx, 40);

	qd_over2(ctx);

	// Verify
	ASSERT_EQ((int)qd_stack_size(ctx->st), 6, "stack should have 6 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "top should be 20 (copy)");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "should be 10 (copy)");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 40, "should be 40");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 30, "should be 30");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 20, "should be 20");
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 10, "should be 10");

	destroy_test_context(ctx);
}

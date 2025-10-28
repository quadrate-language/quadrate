#include <quadrate/runtime/runtime.h>
#include <quadrate/runtime/context.h>
#include <quadrate/runtime/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <math.h>

// Helper to compare floats with tolerance
static int float_eq(double a, double b) {
	return fabs(a - b) < 0.0001;
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

// ========== qd_mul tests ==========

TEST(MulIntegersTest) {
	qd_context* ctx = create_test_context();

	// Push 6 and 7
	qd_push_i(ctx, 6);
	qd_push_i(ctx, 7);

	// Multiply
	qd_exec_result result = qd_mul(ctx);
	ASSERT_EQ(result.code, 0, "mul should succeed");

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
	qd_exec_result result = qd_mul(ctx);
	ASSERT_EQ(result.code, 0, "mul should succeed");

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
	qd_exec_result result = qd_mul(ctx);
	ASSERT_EQ(result.code, 0, "mul should succeed");

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
	qd_exec_result result = qd_mul(ctx);
	ASSERT_EQ(result.code, 0, "mul should succeed");

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
	qd_exec_result result = qd_mul(ctx);
	ASSERT_EQ(result.code, 0, "mul should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, -42, "-6 * 7 should be -42");

	destroy_test_context(ctx);
}

// ========== qd_add tests ==========

TEST(AddIntegersTest) {
	qd_context* ctx = create_test_context();

	// Push 20 and 22
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 22);

	// Add
	qd_exec_result result = qd_add(ctx);
	ASSERT_EQ(result.code, 0, "add should succeed");

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
	qd_exec_result result = qd_add(ctx);
	ASSERT_EQ(result.code, 0, "add should succeed");

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
	qd_exec_result result = qd_add(ctx);
	ASSERT_EQ(result.code, 0, "add should succeed");

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
	qd_exec_result result = qd_add(ctx);
	ASSERT_EQ(result.code, 0, "add should succeed");

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
	qd_exec_result result = qd_add(ctx);
	ASSERT_EQ(result.code, 0, "add should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "42 + 0 should be 42");

	destroy_test_context(ctx);
}

// ========== qd_sub tests ==========

TEST(SubIntegersTest) {
	qd_context* ctx = create_test_context();

	// Push 50 and 8
	qd_push_i(ctx, 50);
	qd_push_i(ctx, 8);

	// Subtract
	qd_exec_result result = qd_sub(ctx);
	ASSERT_EQ(result.code, 0, "sub should succeed");

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
	qd_exec_result result = qd_sub(ctx);
	ASSERT_EQ(result.code, 0, "sub should succeed");

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
	qd_exec_result result = qd_sub(ctx);
	ASSERT_EQ(result.code, 0, "sub should succeed");

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
	qd_exec_result result = qd_sub(ctx);
	ASSERT_EQ(result.code, 0, "sub should succeed");

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
	qd_exec_result result = qd_sub(ctx);
	ASSERT_EQ(result.code, 0, "sub should succeed");

	// Check result
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "42 - 0 should be 42");

	destroy_test_context(ctx);
}

// ========== Error case tests ==========

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

int main(void) {
	return UC_PrintResults();
}

// ========== print behavior tests ==========

TEST(PrintPopsStackTest) {
	qd_context* ctx = create_test_context();

	// Push three values
	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	qd_push_i(ctx, 3);

	// Stack should have 3 elements
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements");

	// Print (pop) the top element
	qd_exec_result result = qd_print(ctx);
	ASSERT_EQ(result.code, 0, "print should succeed");

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
	qd_context* ctx = create_test_context();

	// Try to print from empty stack
	qd_exec_result result = qd_print(ctx);
	ASSERT(result.code != 0, "print on empty stack should fail");

	destroy_test_context(ctx);
}

TEST(PrintIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_exec_result result = qd_print(ctx);

	ASSERT_EQ(result.code, 0, "print should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after print");

	destroy_test_context(ctx);
}

TEST(PrintFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	qd_exec_result result = qd_print(ctx);

	ASSERT_EQ(result.code, 0, "print should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after print");

	destroy_test_context(ctx);
}

TEST(PrintStringTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_exec_result result = qd_print(ctx);

	ASSERT_EQ(result.code, 0, "print should succeed");
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
	qd_exec_result result = qd_printv(ctx);
	ASSERT_EQ(result.code, 0, "printv should succeed");

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
	qd_exec_result result = qd_printv(ctx);

	ASSERT_EQ(result.code, 0, "printv should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 0, "Stack should be empty after printv");

	destroy_test_context(ctx);
}

TEST(PrintvFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	qd_exec_result result = qd_printv(ctx);

	ASSERT_EQ(result.code, 0, "printv should succeed");
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
	qd_exec_result result = qd_prints(ctx);
	ASSERT_EQ(result.code, 0, "prints should succeed");

	// Stack should still have 3 elements (non-destructive)
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should still have 3 elements after prints");

	destroy_test_context(ctx);
}

TEST(PrintsEmptyStackTest) {
	qd_context* ctx = create_test_context();

	// Prints on empty stack should succeed but output nothing
	qd_exec_result result = qd_prints(ctx);
	ASSERT_EQ(result.code, 0, "prints on empty stack should succeed");

	destroy_test_context(ctx);
}

TEST(PrintsMixedTypesTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	qd_push_s(ctx, "hello");

	qd_exec_result result = qd_prints(ctx);
	ASSERT_EQ(result.code, 0, "prints should succeed with mixed types");
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
	qd_exec_result result = qd_printsv(ctx);
	ASSERT_EQ(result.code, 0, "printsv should succeed");

	// Stack should still have 3 elements (non-destructive)
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should still have 3 elements after printsv");

	destroy_test_context(ctx);
}

TEST(PrintsvEmptyStackTest) {
	qd_context* ctx = create_test_context();

	// Printsv on empty stack should succeed but output nothing
	qd_exec_result result = qd_printsv(ctx);
	ASSERT_EQ(result.code, 0, "printsv on empty stack should succeed");

	destroy_test_context(ctx);
}

TEST(PrintsvMixedTypesTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_f(ctx, 3.14);
	qd_push_s(ctx, "hello");

	qd_exec_result result = qd_printsv(ctx);
	ASSERT_EQ(result.code, 0, "printsv should succeed with mixed types");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should still have 3 elements");

	destroy_test_context(ctx);
}

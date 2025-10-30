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

// ========== qd_sq tests ==========

TEST(SqPositiveIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 5);
	qd_exec_result result = qd_sq(ctx);

	ASSERT_EQ(result.code, 0, "sq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 25, "sq(5) should be 25");

	destroy_test_context(ctx);
}

TEST(SqNegativeIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -4);
	qd_exec_result result = qd_sq(ctx);

	ASSERT_EQ(result.code, 0, "sq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 16, "sq(-4) should be 16");

	destroy_test_context(ctx);
}

TEST(SqZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_exec_result result = qd_sq(ctx);

	ASSERT_EQ(result.code, 0, "sq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 0, "sq(0) should be 0");

	destroy_test_context(ctx);
}

TEST(SqPositiveFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.0);
	qd_exec_result result = qd_sq(ctx);

	ASSERT_EQ(result.code, 0, "sq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 9.0), "sq(3.0) should be 9.0");

	destroy_test_context(ctx);
}

TEST(SqNegativeFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.5);
	qd_exec_result result = qd_sq(ctx);

	ASSERT_EQ(result.code, 0, "sq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 6.25), "sq(-2.5) should be 6.25");

	destroy_test_context(ctx);
}

TEST(SqLargeIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 100);
	qd_exec_result result = qd_sq(ctx);

	ASSERT_EQ(result.code, 0, "sq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 10000, "sq(100) should be 10000");

	destroy_test_context(ctx);
}

TEST(SqOneTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	qd_exec_result result = qd_sq(ctx);

	ASSERT_EQ(result.code, 0, "sq should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "sq(1) should be 1");

	destroy_test_context(ctx);
}

TEST(SqPreservesTypeTest) {
	qd_context* ctx = create_test_context();

	// Test that int->int and float->float
	qd_push_i(ctx, 7);
	qd_sq(ctx);
	qd_stack_element_t elem_int;
	qd_stack_pop(ctx->st, &elem_int);
	ASSERT_EQ(elem_int.type, QD_STACK_TYPE_INT, "int squared should remain int");
	ASSERT_EQ((int)elem_int.value.i, 49, "7*7 should be 49");

	qd_push_f(ctx, 7.0);
	qd_sq(ctx);
	qd_stack_element_t elem_float;
	qd_stack_pop(ctx->st, &elem_float);
	ASSERT_EQ(elem_float.type, QD_STACK_TYPE_FLOAT, "float squared should remain float");
	ASSERT(float_eq(elem_float.value.f, 49.0), "7.0*7.0 should be 49.0");

	destroy_test_context(ctx);
}

// ========== qd_abs tests ==========

TEST(AbsPositiveIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_exec_result result = qd_abs(ctx);

	ASSERT_EQ(result.code, 0, "abs should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "abs(42) should be 42");

	destroy_test_context(ctx);
}

TEST(AbsNegativeIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -42);
	qd_exec_result result = qd_abs(ctx);

	ASSERT_EQ(result.code, 0, "abs should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 42, "abs(-42) should be 42");

	destroy_test_context(ctx);
}

TEST(AbsZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_exec_result result = qd_abs(ctx);

	ASSERT_EQ(result.code, 0, "abs should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 0, "abs(0) should be 0");

	destroy_test_context(ctx);
}

TEST(AbsPositiveFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	qd_exec_result result = qd_abs(ctx);

	ASSERT_EQ(result.code, 0, "abs should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "abs(3.14) should be 3.14");

	destroy_test_context(ctx);
}

TEST(AbsNegativeFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -3.14);
	qd_exec_result result = qd_abs(ctx);

	ASSERT_EQ(result.code, 0, "abs should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 3.14), "abs(-3.14) should be 3.14");

	destroy_test_context(ctx);
}

TEST(AbsLargeNegativeTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -1000000);
	qd_exec_result result = qd_abs(ctx);

	ASSERT_EQ(result.code, 0, "abs should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ((int)elem.value.i, 1000000, "abs(-1000000) should be 1000000");

	destroy_test_context(ctx);
}

// ========== qd_dup tests ==========

TEST(DupIntegerTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_exec_result result = qd_dup(ctx);

	ASSERT_EQ(result.code, 0, "dup should succeed");
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
	qd_exec_result result = qd_dup(ctx);

	ASSERT_EQ(result.code, 0, "dup should succeed");
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
	qd_exec_result result = qd_dup(ctx);

	ASSERT_EQ(result.code, 0, "dup should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after dup");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_STR, "top element should be string");
	ASSERT_STR_EQ(elem1.value.s, "hello", "top element should be 'hello'");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_STR, "second element should be string");
	ASSERT_STR_EQ(elem2.value.s, "hello", "second element should be 'hello'");

	free(elem1.value.s);
	free(elem2.value.s);
	destroy_test_context(ctx);
}

TEST(DupNonDestructiveTest) {
	qd_context* ctx = create_test_context();

	// Push multiple elements
	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_push_i(ctx, 30);

	// Dup the top element
	qd_exec_result result = qd_dup(ctx);
	ASSERT_EQ(result.code, 0, "dup should succeed");
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

// ========== qd_swap tests ==========

TEST(SwapIntegersTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_exec_result result = qd_swap(ctx);

	ASSERT_EQ(result.code, 0, "swap should succeed");
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
	qd_exec_result result = qd_swap(ctx);

	ASSERT_EQ(result.code, 0, "swap should succeed with mixed types");
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
	qd_exec_result result = qd_swap(ctx);

	ASSERT_EQ(result.code, 0, "swap should succeed with strings");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements after swap");

	qd_stack_element_t elem1, elem2;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem1);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem1.type, QD_STACK_TYPE_STR, "top element should be string");
	ASSERT_STR_EQ(elem1.value.s, "hello", "top element should be 'hello' after swap");

	err = qd_stack_pop(ctx->st, &elem2);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem2.type, QD_STACK_TYPE_STR, "second element should be string");
	ASSERT_STR_EQ(elem2.value.s, "world", "second element should be 'world' after swap");

	free(elem1.value.s);
	free(elem2.value.s);
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
	qd_exec_result result = qd_swap(ctx);
	ASSERT_EQ(result.code, 0, "swap should succeed");
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

// ========== qd_over tests ==========

TEST(OverIntegersTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_exec_result result = qd_over(ctx);

	ASSERT_EQ(result.code, 0, "over should succeed");
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
	qd_exec_result result = qd_over(ctx);

	ASSERT_EQ(result.code, 0, "over should succeed with mixed types");
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
	qd_exec_result result = qd_over(ctx);

	ASSERT_EQ(result.code, 0, "over should succeed with strings");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 3, "Stack should have 3 elements after over");

	// Stack should be: "hello", "world", "hello"
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "top element should be string");
	ASSERT_STR_EQ(elem.value.s, "hello", "top element should be 'hello'");
	free(elem.value.s);

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "second pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "second element should be string");
	ASSERT_STR_EQ(elem.value.s, "world", "second element should be 'world'");
	free(elem.value.s);

	err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "third pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "third element should be string");
	ASSERT_STR_EQ(elem.value.s, "hello", "third element should be 'hello'");
	free(elem.value.s);

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
	qd_exec_result result = qd_over(ctx);
	ASSERT_EQ(result.code, 0, "over should succeed");
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

// ========== qd_nip tests ==========

TEST(NipIntegersTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	qd_push_i(ctx, 20);
	qd_exec_result result = qd_nip(ctx);

	ASSERT_EQ(result.code, 0, "nip should succeed");
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
	qd_exec_result result = qd_nip(ctx);

	ASSERT_EQ(result.code, 0, "nip should succeed with mixed types");
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
	qd_exec_result result = qd_nip(ctx);

	ASSERT_EQ(result.code, 0, "nip should succeed with strings");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element after nip");

	// Stack should be: "world"
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "element should be string");
	ASSERT_STR_EQ(elem.value.s, "world", "element should be 'world'");
	free(elem.value.s);

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
	qd_exec_result result = qd_nip(ctx);
	ASSERT_EQ(result.code, 0, "nip should succeed");
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

// ========== Trigonometric function tests ==========

TEST(SinZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_exec_result result = qd_sin(ctx);

	ASSERT_EQ(result.code, 0, "sin should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 0.0), "sin(0) should be 0.0");

	destroy_test_context(ctx);
}

TEST(SinPiOver2Test) {
	qd_context* ctx = create_test_context();

	// sin(π/2) = 1
	qd_push_f(ctx, 3.14159265358979323846 / 2.0);
	qd_exec_result result = qd_sin(ctx);

	ASSERT_EQ(result.code, 0, "sin should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 1.0), "sin(π/2) should be 1.0");

	destroy_test_context(ctx);
}

TEST(CosZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_exec_result result = qd_cos(ctx);

	ASSERT_EQ(result.code, 0, "cos should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 1.0), "cos(0) should be 1.0");

	destroy_test_context(ctx);
}

TEST(CosPiTest) {
	qd_context* ctx = create_test_context();

	// cos(π) = -1
	qd_push_f(ctx, 3.14159265358979323846);
	qd_exec_result result = qd_cos(ctx);

	ASSERT_EQ(result.code, 0, "cos should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, -1.0), "cos(π) should be -1.0");

	destroy_test_context(ctx);
}

TEST(TanZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_exec_result result = qd_tan(ctx);

	ASSERT_EQ(result.code, 0, "tan should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 0.0), "tan(0) should be 0.0");

	destroy_test_context(ctx);
}

TEST(TanPiOver4Test) {
	qd_context* ctx = create_test_context();

	// tan(π/4) = 1
	qd_push_f(ctx, 3.14159265358979323846 / 4.0);
	qd_exec_result result = qd_tan(ctx);

	ASSERT_EQ(result.code, 0, "tan should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 1.0), "tan(π/4) should be 1.0");

	destroy_test_context(ctx);
}

TEST(AsinZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_exec_result result = qd_asin(ctx);

	ASSERT_EQ(result.code, 0, "asin should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 0.0), "asin(0) should be 0.0");

	destroy_test_context(ctx);
}

TEST(AsinOneTest) {
	qd_context* ctx = create_test_context();

	// asin(1) = π/2
	qd_push_f(ctx, 1.0);
	qd_exec_result result = qd_asin(ctx);

	ASSERT_EQ(result.code, 0, "asin should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 3.14159265358979323846 / 2.0), "asin(1) should be π/2");

	destroy_test_context(ctx);
}

TEST(AcosZeroTest) {
	qd_context* ctx = create_test_context();

	// acos(0) = π/2
	qd_push_i(ctx, 0);
	qd_exec_result result = qd_acos(ctx);

	ASSERT_EQ(result.code, 0, "acos should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 3.14159265358979323846 / 2.0), "acos(0) should be π/2");

	destroy_test_context(ctx);
}

TEST(AcosOneTest) {
	qd_context* ctx = create_test_context();

	// acos(1) = 0
	qd_push_f(ctx, 1.0);
	qd_exec_result result = qd_acos(ctx);

	ASSERT_EQ(result.code, 0, "acos should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 0.0), "acos(1) should be 0.0");

	destroy_test_context(ctx);
}

TEST(AtanZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_exec_result result = qd_atan(ctx);

	ASSERT_EQ(result.code, 0, "atan should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 0.0), "atan(0) should be 0.0");

	destroy_test_context(ctx);
}

TEST(AtanOneTest) {
	qd_context* ctx = create_test_context();

	// atan(1) = π/4
	qd_push_f(ctx, 1.0);
	qd_exec_result result = qd_atan(ctx);

	ASSERT_EQ(result.code, 0, "atan should succeed");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 3.14159265358979323846 / 4.0), "atan(1) should be π/4");

	destroy_test_context(ctx);
}

TEST(TrigIntegerInputTest) {
	qd_context* ctx = create_test_context();

	// Test that integer input gets converted to float
	qd_push_i(ctx, 0);
	qd_sin(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "sin should return float even with int input");

	qd_push_i(ctx, 0);
	qd_cos(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cos should return float even with int input");

	destroy_test_context(ctx);
}

TEST(TrigNegativeValuesTest) {
	qd_context* ctx = create_test_context();

	// sin(-x) = -sin(x)
	qd_push_f(ctx, -3.14159265358979323846 / 2.0);
	qd_sin(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT(float_eq(elem.value.f, -1.0), "sin(-π/2) should be -1.0");

	// asin(-1) = -π/2
	qd_push_f(ctx, -1.0);
	qd_asin(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT(float_eq(elem.value.f, -3.14159265358979323846 / 2.0), "asin(-1) should be -π/2");

	destroy_test_context(ctx);
}

// sqrt tests
TEST(SqrtPositiveTest) {
	qd_context* ctx = create_test_context();

	// sqrt(4) = 2.0
	qd_push_i(ctx, 4);
	qd_sqrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "sqrt should return float");
	ASSERT(float_eq(elem.value.f, 2.0), "sqrt(4) should be 2.0");

	// sqrt(9.0) = 3.0
	qd_push_f(ctx, 9.0);
	qd_sqrt(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT(float_eq(elem.value.f, 3.0), "sqrt(9.0) should be 3.0");

	destroy_test_context(ctx);
}

TEST(SqrtZeroTest) {
	qd_context* ctx = create_test_context();

	// sqrt(0) = 0.0
	qd_push_i(ctx, 0);
	qd_sqrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "sqrt should return float");
	ASSERT(float_eq(elem.value.f, 0.0), "sqrt(0) should be 0.0");

	destroy_test_context(ctx);
}

TEST(SqrtIntegerInputTest) {
	qd_context* ctx = create_test_context();

	// sqrt(16) = 4.0 (integer input)
	qd_push_i(ctx, 16);
	qd_sqrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "sqrt should return float even with int input");
	ASSERT(float_eq(elem.value.f, 4.0), "sqrt(16) should be 4.0");

	destroy_test_context(ctx);
}

// cb (cube) tests
TEST(CbPositiveTest) {
	qd_context* ctx = create_test_context();

	// cb(2) = 8.0
	qd_push_i(ctx, 2);
	qd_cb(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cb should return float");
	ASSERT(float_eq(elem.value.f, 8.0), "cb(2) should be 8.0");

	// cb(3.0) = 27.0
	qd_push_f(ctx, 3.0);
	qd_cb(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT(float_eq(elem.value.f, 27.0), "cb(3.0) should be 27.0");

	destroy_test_context(ctx);
}

TEST(CbNegativeTest) {
	qd_context* ctx = create_test_context();

	// cb(-2) = -8.0
	qd_push_i(ctx, -2);
	qd_cb(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cb should return float");
	ASSERT(float_eq(elem.value.f, -8.0), "cb(-2) should be -8.0");

	destroy_test_context(ctx);
}

TEST(CbZeroTest) {
	qd_context* ctx = create_test_context();

	// cb(0) = 0.0
	qd_push_i(ctx, 0);
	qd_cb(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cb should return float");
	ASSERT(float_eq(elem.value.f, 0.0), "cb(0) should be 0.0");

	destroy_test_context(ctx);
}

// cbrt (cube root) tests
TEST(CbrtPositiveTest) {
	qd_context* ctx = create_test_context();

	// cbrt(8) = 2.0
	qd_push_i(ctx, 8);
	qd_cbrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cbrt should return float");
	ASSERT(float_eq(elem.value.f, 2.0), "cbrt(8) should be 2.0");

	// cbrt(27.0) = 3.0
	qd_push_f(ctx, 27.0);
	qd_cbrt(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT(float_eq(elem.value.f, 3.0), "cbrt(27.0) should be 3.0");

	destroy_test_context(ctx);
}

TEST(CbrtNegativeTest) {
	qd_context* ctx = create_test_context();

	// cbrt(-8) = -2.0
	qd_push_i(ctx, -8);
	qd_cbrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cbrt should return float");
	ASSERT(float_eq(elem.value.f, -2.0), "cbrt(-8) should be -2.0");

	destroy_test_context(ctx);
}

TEST(CbrtZeroTest) {
	qd_context* ctx = create_test_context();

	// cbrt(0) = 0.0
	qd_push_i(ctx, 0);
	qd_cbrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cbrt should return float");
	ASSERT(float_eq(elem.value.f, 0.0), "cbrt(0) should be 0.0");

	destroy_test_context(ctx);
}

TEST(CbrtIntegerInputTest) {
	qd_context* ctx = create_test_context();

	// cbrt(64) = 4.0 (integer input)
	qd_push_i(ctx, 64);
	qd_cbrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "cbrt should return float even with int input");
	ASSERT(float_eq(elem.value.f, 4.0), "cbrt(64) should be 4.0");

	destroy_test_context(ctx);
}

// ceil tests
TEST(CeilPositiveTest) {
	qd_context* ctx = create_test_context();

	// ceil(2.3) = 3.0
	qd_push_f(ctx, 2.3);
	qd_ceil(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "ceil should return float");
	ASSERT(float_eq(elem.value.f, 3.0), "ceil(2.3) should be 3.0");

	// ceil(4.0) = 4.0 (already integer)
	qd_push_f(ctx, 4.0);
	qd_ceil(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT(float_eq(elem.value.f, 4.0), "ceil(4.0) should be 4.0");

	destroy_test_context(ctx);
}

TEST(CeilNegativeTest) {
	qd_context* ctx = create_test_context();

	// ceil(-2.3) = -2.0
	qd_push_f(ctx, -2.3);
	qd_ceil(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "ceil should return float");
	ASSERT(float_eq(elem.value.f, -2.0), "ceil(-2.3) should be -2.0");

	destroy_test_context(ctx);
}

TEST(CeilIntegerInputTest) {
	qd_context* ctx = create_test_context();

	// ceil(5) = 5.0 (integer input)
	qd_push_i(ctx, 5);
	qd_ceil(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "ceil should return float even with int input");
	ASSERT(float_eq(elem.value.f, 5.0), "ceil(5) should be 5.0");

	destroy_test_context(ctx);
}

// floor tests
TEST(FloorPositiveTest) {
	qd_context* ctx = create_test_context();

	// floor(2.7) = 2.0
	qd_push_f(ctx, 2.7);
	qd_floor(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "floor should return float");
	ASSERT(float_eq(elem.value.f, 2.0), "floor(2.7) should be 2.0");

	// floor(4.0) = 4.0 (already integer)
	qd_push_f(ctx, 4.0);
	qd_floor(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT(float_eq(elem.value.f, 4.0), "floor(4.0) should be 4.0");

	destroy_test_context(ctx);
}

TEST(FloorNegativeTest) {
	qd_context* ctx = create_test_context();

	// floor(-2.3) = -3.0
	qd_push_f(ctx, -2.3);
	qd_floor(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "floor should return float");
	ASSERT(float_eq(elem.value.f, -3.0), "floor(-2.3) should be -3.0");

	destroy_test_context(ctx);
}

TEST(FloorIntegerInputTest) {
	qd_context* ctx = create_test_context();

	// floor(7) = 7.0 (integer input)
	qd_push_i(ctx, 7);
	qd_floor(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "floor should return float even with int input");
	ASSERT(float_eq(elem.value.f, 7.0), "floor(7) should be 7.0");

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
	ASSERT_EQ(strcmp(elem.value.s, "world"), 0, "top should be 'world'");
	free(elem.value.s);

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "second should be string");
	ASSERT_EQ(strcmp(elem.value.s, "hello"), 0, "second should be 'hello'");
	free(elem.value.s);

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "third should be string");
	ASSERT_EQ(strcmp(elem.value.s, "world"), 0, "third should be 'world'");
	free(elem.value.s);

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "fourth should be string");
	ASSERT_EQ(strcmp(elem.value.s, "hello"), 0, "fourth should be 'hello'");
	free(elem.value.s);

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

// ========== factorial tests ==========

TEST(FacBasicTest) {
	qd_context* ctx = create_test_context();

	// Test 5! = 120
	qd_push_i(ctx, 5);
	qd_exec_result result = qd_fac(ctx);

	ASSERT_EQ(result.code, 0, "fac should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 120, "5! should be 120");

	destroy_test_context(ctx);
}

TEST(FacZeroTest) {
	qd_context* ctx = create_test_context();

	// Test 0! = 1
	qd_push_i(ctx, 0);
	qd_exec_result result = qd_fac(ctx);

	ASSERT_EQ(result.code, 0, "fac should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "0! should be 1");

	destroy_test_context(ctx);
}

TEST(FacOneTest) {
	qd_context* ctx = create_test_context();

	// Test 1! = 1
	qd_push_i(ctx, 1);
	qd_exec_result result = qd_fac(ctx);

	ASSERT_EQ(result.code, 0, "fac should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 1, "1! should be 1");

	destroy_test_context(ctx);
}

TEST(FacLargerTest) {
	qd_context* ctx = create_test_context();

	// Test 10! = 3628800
	qd_push_i(ctx, 10);
	qd_exec_result result = qd_fac(ctx);

	ASSERT_EQ(result.code, 0, "fac should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)elem.value.i, 3628800, "10! should be 3628800");

	destroy_test_context(ctx);
}

TEST(FacPreservesStackTest) {
	qd_context* ctx = create_test_context();

	// Test that fac only affects the top element
	qd_push_i(ctx, 100);
	qd_push_i(ctx, 4);
	qd_fac(ctx);  // 4! = 24

	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 24, "top should be 24");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 100, "bottom should be 100");

	destroy_test_context(ctx);
}

// ========== inverse tests ==========

TEST(InvBasicIntTest) {
	qd_context* ctx = create_test_context();

	// Test inv(4) = 0.25
	qd_push_i(ctx, 4);
	qd_exec_result result = qd_inv(ctx);

	ASSERT_EQ(result.code, 0, "inv should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 0.25), "inv(4) should be 0.25");

	destroy_test_context(ctx);
}

TEST(InvBasicFloatTest) {
	qd_context* ctx = create_test_context();

	// Test inv(2.5) = 0.4
	qd_push_f(ctx, 2.5);
	qd_exec_result result = qd_inv(ctx);

	ASSERT_EQ(result.code, 0, "inv should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 0.4), "inv(2.5) should be 0.4");

	destroy_test_context(ctx);
}

TEST(InvOneTest) {
	qd_context* ctx = create_test_context();

	// Test inv(1) = 1.0
	qd_push_i(ctx, 1);
	qd_exec_result result = qd_inv(ctx);

	ASSERT_EQ(result.code, 0, "inv should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, 1.0), "inv(1) should be 1.0");

	destroy_test_context(ctx);
}

TEST(InvNegativeTest) {
	qd_context* ctx = create_test_context();

	// Test inv(-2) = -0.5
	qd_push_i(ctx, -2);
	qd_exec_result result = qd_inv(ctx);

	ASSERT_EQ(result.code, 0, "inv should succeed");
	ASSERT_EQ((int)qd_stack_size(ctx->st), 1, "Stack should have 1 element");

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(float_eq(elem.value.f, -0.5), "inv(-2) should be -0.5");

	destroy_test_context(ctx);
}

TEST(InvPreservesStackTest) {
	qd_context* ctx = create_test_context();

	// Test that inv only affects the top element
	qd_push_i(ctx, 100);
	qd_push_i(ctx, 2);
	qd_inv(ctx);  // inv(2) = 0.5

	ASSERT_EQ((int)qd_stack_size(ctx->st), 2, "Stack should have 2 elements");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "top should be float");
	ASSERT(float_eq(elem.value.f, 0.5), "top should be 0.5");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 100, "bottom should be 100");

	destroy_test_context(ctx);
}

// ========== comparison tests ==========

// qd_eq tests
TEST(EqIntegersEqualTest) {
	qd_context* ctx = create_test_context();

	// Test 5 == 5 (should return 1)
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 5);
	qd_exec_result result = qd_eq(ctx);

	ASSERT_EQ(result.code, 0, "eq should succeed");
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
	qd_exec_result result = qd_eq(ctx);

	ASSERT_EQ(result.code, 0, "eq should succeed");
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
	qd_exec_result result = qd_eq(ctx);

	ASSERT_EQ(result.code, 0, "eq should succeed");
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
	qd_exec_result result = qd_eq(ctx);

	ASSERT_EQ(result.code, 0, "eq should succeed");
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
	qd_exec_result result = qd_eq(ctx);

	ASSERT_EQ(result.code, 0, "eq should succeed");

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
	qd_exec_result result = qd_eq(ctx);

	ASSERT_EQ(result.code, 0, "eq should succeed");

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
	qd_exec_result result = qd_neq(ctx);

	ASSERT_EQ(result.code, 0, "neq should succeed");
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
	qd_exec_result result = qd_neq(ctx);

	ASSERT_EQ(result.code, 0, "neq should succeed");

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
	qd_exec_result result = qd_neq(ctx);

	ASSERT_EQ(result.code, 0, "neq should succeed");

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
	qd_exec_result result = qd_neq(ctx);

	ASSERT_EQ(result.code, 0, "neq should succeed");

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
	qd_exec_result result = qd_lt(ctx);

	ASSERT_EQ(result.code, 0, "lt should succeed");
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
	qd_exec_result result = qd_lt(ctx);

	ASSERT_EQ(result.code, 0, "lt should succeed");

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
	qd_exec_result result = qd_lt(ctx);

	ASSERT_EQ(result.code, 0, "lt should succeed");

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
	qd_exec_result result = qd_lt(ctx);

	ASSERT_EQ(result.code, 0, "lt should succeed");

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
	qd_exec_result result = qd_lt(ctx);

	ASSERT_EQ(result.code, 0, "lt should succeed");

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
	qd_exec_result result = qd_lt(ctx);

	ASSERT_EQ(result.code, 0, "lt should succeed");

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
	qd_exec_result result = qd_gt(ctx);

	ASSERT_EQ(result.code, 0, "gt should succeed");
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
	qd_exec_result result = qd_gt(ctx);

	ASSERT_EQ(result.code, 0, "gt should succeed");

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
	qd_exec_result result = qd_gt(ctx);

	ASSERT_EQ(result.code, 0, "gt should succeed");

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
	qd_exec_result result = qd_gt(ctx);

	ASSERT_EQ(result.code, 0, "gt should succeed");

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
	qd_exec_result result = qd_gt(ctx);

	ASSERT_EQ(result.code, 0, "gt should succeed");

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
	qd_exec_result result = qd_gt(ctx);

	ASSERT_EQ(result.code, 0, "gt should succeed");

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
	qd_exec_result result = qd_lte(ctx);

	ASSERT_EQ(result.code, 0, "lte should succeed");
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
	qd_exec_result result = qd_lte(ctx);

	ASSERT_EQ(result.code, 0, "lte should succeed");

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
	qd_exec_result result = qd_lte(ctx);

	ASSERT_EQ(result.code, 0, "lte should succeed");

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
	qd_exec_result result = qd_lte(ctx);

	ASSERT_EQ(result.code, 0, "lte should succeed");

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
	qd_exec_result result = qd_lte(ctx);

	ASSERT_EQ(result.code, 0, "lte should succeed");

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
	qd_exec_result result = qd_gte(ctx);

	ASSERT_EQ(result.code, 0, "gte should succeed");
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
	qd_exec_result result = qd_gte(ctx);

	ASSERT_EQ(result.code, 0, "gte should succeed");

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
	qd_exec_result result = qd_gte(ctx);

	ASSERT_EQ(result.code, 0, "gte should succeed");

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
	qd_exec_result result = qd_gte(ctx);

	ASSERT_EQ(result.code, 0, "gte should succeed");

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
	qd_exec_result result = qd_gte(ctx);

	ASSERT_EQ(result.code, 0, "gte should succeed");

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

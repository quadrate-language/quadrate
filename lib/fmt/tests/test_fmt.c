/**
 * @file test_fmt.c
 * @brief Unit tests for the qdfmt format library
 */

#include <quadrate/fmt/fmt.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/qd_string.h>
#include <unit-check/uc.h>
#include <string.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}


TEST(SprintfStringTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "World");           // arg1 (below format)
	qd_push_s(ctx, "Hello %s!");       // format (on top)
	usr_fmt_sprintf(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintf %s result should be string");
	ASSERT_STR_EQ("Hello World!", qd_string_data(elem.value.s), "sprintf %s");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintfIntTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);               // arg1 (below format)
	qd_push_s(ctx, "The answer is %d"); // format (on top)
	usr_fmt_sprintf(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintf %d result should be string");
	ASSERT_STR_EQ("The answer is 42", qd_string_data(elem.value.s), "sprintf %d");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintfFloatTest) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);             // arg1 (below format)
	qd_push_s(ctx, "Pi is %.2f");     // format (on top)
	usr_fmt_sprintf(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintf %f result should be string");
	ASSERT(strstr(qd_string_data(elem.value.s), "3.14") != NULL, "sprintf %f should contain 3.14");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintfMultipleArgsTest) {
	qd_context* ctx = create_test_context();

	// Stack order: args below, format on top
	// For "Hello %s, you are %d years old!"
	// First arg is %s -> "Alice", second arg is %d -> 30
	qd_push_s(ctx, "Alice");          // arg1 (deepest)
	qd_push_i(ctx, 30);               // arg2
	qd_push_s(ctx, "Hello %s, you are %d years old!"); // format (on top)
	usr_fmt_sprintf(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintf multi result should be string");
	ASSERT_STR_EQ("Hello Alice, you are 30 years old!", qd_string_data(elem.value.s), "sprintf multiple args");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintfEscapedPercentTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "100%%");          // format (on top, no args needed)
	usr_fmt_sprintf(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintf %% result should be string");
	ASSERT_STR_EQ("100%", qd_string_data(elem.value.s), "sprintf %%");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintfNoArgsTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "plain text");     // format with no specifiers
	usr_fmt_sprintf(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintf no args result should be string");
	ASSERT_STR_EQ("plain text", qd_string_data(elem.value.s), "sprintf no args");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintlnTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	usr_fmt_sprintln(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintln result should be string");
	ASSERT_STR_EQ("hello\n", qd_string_data(elem.value.s), "sprintln appends newline");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintlnEmptyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_fmt_sprintln(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintln empty result should be string");
	ASSERT_STR_EQ("\n", qd_string_data(elem.value.s), "sprintln empty string produces newline");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintfHexTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 255);
	qd_push_s(ctx, "0x%x");
	usr_fmt_sprintf(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "sprintf %x result should be string");
	ASSERT_STR_EQ("0xff", qd_string_data(elem.value.s), "sprintf %x");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(SprintfStackCleanTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "world");
	qd_push_s(ctx, "Hello %s!");
	usr_fmt_sprintf(ctx);

	// After sprintf: stack should have exactly 1 element (the result)
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "sprintf should leave exactly 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qd_string_release(elem.value.s);

	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after pop");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}

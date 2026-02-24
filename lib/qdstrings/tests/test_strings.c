/**
 * @file test_strings.c
 * @brief Unit tests for the qdstrings string library
 */

#define _POSIX_C_SOURCE 200809L

#include <qdstrings/strings.h>
#include <qdrt/runtime.h>
#include <qdrt/context.h>
#include <qdrt/stack.h>
#include <qdrt/qd_string.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}


TEST(StrLenTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	usr_strings_len(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "result should be int");
	ASSERT_EQ(5, (int)elem.value.i, "length should be 5");

	destroy_test_context(ctx);
}

TEST(StrLenEmptyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_strings_len(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "empty string length should be 0");

	destroy_test_context(ctx);
}


TEST(StrConcatTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, " world");
	usr_strings_concat(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "result should be string");
	ASSERT_STR_EQ("hello world", qd_string_data(elem.value.s), "concat result");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrConcatEmptyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "");
	usr_strings_concat(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello", qd_string_data(elem.value.s), "concat with empty");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(StrContainsTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "world");
	usr_strings_contains(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "should contain substring");

	destroy_test_context(ctx);
}

TEST(StrContainsNotFoundTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "xyz");
	usr_strings_contains(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "should not contain substring");

	destroy_test_context(ctx);
}


TEST(StrStartsWithTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "hello");
	usr_strings_starts_with(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "should start with prefix");

	destroy_test_context(ctx);
}

TEST(StrEndsWithTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "world");
	usr_strings_ends_with(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "should end with suffix");

	destroy_test_context(ctx);
}


TEST(StrUpperTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "Hello World");
	usr_strings_upper(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("HELLO WORLD", qd_string_data(elem.value.s), "uppercase result");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrLowerTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "Hello World");
	usr_strings_lower(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello world", qd_string_data(elem.value.s), "lowercase result");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(StrTrimTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "  hello world  ");
	usr_strings_trim(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello world", qd_string_data(elem.value.s), "trimmed result");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(StrCompareEqualTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "hello");
	usr_strings_compare(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "equal strings should return 0");

	destroy_test_context(ctx);
}

TEST(StrCompareLessTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "abc");
	qd_push_s(ctx, "abd");
	usr_strings_compare(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(-1, (int)elem.value.i, "less than should return -1");

	destroy_test_context(ctx);
}

TEST(StrCompareGreaterTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "abd");
	qd_push_s(ctx, "abc");
	usr_strings_compare(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "greater than should return 1");

	destroy_test_context(ctx);
}


TEST(StrSubstringTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_i(ctx, 6);  // start
	qd_push_i(ctx, 5);  // length
	usr_strings_substring(ctx);

	// Pop status code first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(STRINGS_ERR_OK, (int)status_elem.value.i, "substring should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("world", qd_string_data(elem.value.s), "substring result");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrCharAtTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_i(ctx, 1);
	usr_strings_char_at(ctx);

	// Pop status code first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(STRINGS_ERR_OK, (int)status_elem.value.i, "char_at should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ('e', (int)elem.value.i, "char at index 1 should be 'e'");

	destroy_test_context(ctx);
}

TEST(StrIndexOfTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "world");
	usr_strings_index_of(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(6, (int)elem.value.i, "index of 'world' should be 6");

	destroy_test_context(ctx);
}

TEST(StrIndexOfNotFoundTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "xyz");
	usr_strings_index_of(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(-1, (int)elem.value.i, "not found should return -1");

	destroy_test_context(ctx);
}

TEST(StrFromCharTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 'A');
	usr_strings_from_char(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("A", qd_string_data(elem.value.s), "should create 'A' string");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrReplaceTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "world");
	qd_push_s(ctx, "universe");
	usr_strings_replace(ctx);

	// Pop status code first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(STRINGS_ERR_OK, (int)status_elem.value.i, "replace should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello universe", qd_string_data(elem.value.s), "replace result");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrSplitTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "a,b,c");
	qd_push_s(ctx, ",");
	usr_strings_split(ctx);

	// Pop status code first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(STRINGS_ERR_OK, (int)status_elem.value.i, "split should succeed");

	qd_stack_element_t count_elem;
	qd_stack_pop(ctx->st, &count_elem);
	ASSERT_EQ(3, (int)count_elem.value.i, "should have 3 parts");

	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, ptr_elem.type, "result should be ptr");

	// Clean up split results
	qd_string_t** parts = (qd_string_t**)ptr_elem.value.p;
	for (int i = 0; i < 3; i++) {
		qd_string_release(parts[i]);
	}
	free(parts);

	destroy_test_context(ctx);
}

TEST(StrJoinTest) {
	qd_context* ctx = create_test_context();

	// Create array of C strings (join expects char**)
	char** parts = malloc(3 * sizeof(char*));
	parts[0] = strdup("a");
	parts[1] = strdup("b");
	parts[2] = strdup("c");

	qd_push_p(ctx, parts);
	qd_push_i(ctx, 3);
	qd_push_s(ctx, ",");
	usr_strings_join(ctx);

	// Pop status code first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(STRINGS_ERR_OK, (int)status_elem.value.i, "join should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("a,b,c", qd_string_data(elem.value.s), "join result");

	qd_string_release(elem.value.s);
	for (int i = 0; i < 3; i++) {
		free(parts[i]);
	}
	free(parts);

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}

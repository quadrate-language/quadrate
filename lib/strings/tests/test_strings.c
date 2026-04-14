/**
 * @file test_strings.c
 * @brief Unit tests for the qdstrings string library
 */

#define _POSIX_C_SOURCE 200809L

#include <quadrate/strings/strings.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/qd_string.h>
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

/* Forward declarations for functions not yet in the public header */
int usr_strings_is_empty(qd_context* ctx);
int usr_strings_is_blank(qd_context* ctx);
int usr_strings_equals_ignore_case(qd_context* ctx);
int usr_strings_pad_left(qd_context* ctx);
int usr_strings_pad_right(qd_context* ctx);
int usr_strings_center(qd_context* ctx);
int usr_strings_capitalize(qd_context* ctx);
int usr_strings_title(qd_context* ctx);
int usr_strings_trim_prefix(qd_context* ctx);
int usr_strings_trim_suffix(qd_context* ctx);
int usr_strings_replace_first(qd_context* ctx);
int usr_strings_insert(qd_context* ctx);
int usr_strings_remove_range(qd_context* ctx);
int usr_strings_is_numeric(qd_context* ctx);
int usr_strings_is_alpha(qd_context* ctx);
int usr_strings_is_alphanumeric(qd_context* ctx);
int usr_strings_is_ascii(qd_context* ctx);
int usr_strings_is_lowercase(qd_context* ctx);
int usr_strings_is_uppercase(qd_context* ctx);
int usr_strings_char_count(qd_context* ctx);
int usr_strings_slice(qd_context* ctx);


/* ---------- repeat ---------- */

TEST(StrRepeatTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "ab");
	qd_push_i(ctx, 3);
	usr_strings_repeat(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("ababab", qd_string_data(elem.value.s), "repeat 'ab' 3 times");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrRepeatZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "x");
	qd_push_i(ctx, 0);
	usr_strings_repeat(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("", qd_string_data(elem.value.s), "repeat 'x' 0 times");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- reverse ---------- */

TEST(StrReverseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	usr_strings_reverse(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("olleh", qd_string_data(elem.value.s), "reverse 'hello'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrReverseEmptyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_strings_reverse(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("", qd_string_data(elem.value.s), "reverse empty string");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrReverseSingleCharTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "a");
	usr_strings_reverse(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("a", qd_string_data(elem.value.s), "reverse single char");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- trim_left ---------- */

TEST(StrTrimLeftTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "  hello  ");
	usr_strings_trim_left(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello  ", qd_string_data(elem.value.s), "trim_left '  hello  '");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- trim_right ---------- */

TEST(StrTrimRightTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "  hello  ");
	usr_strings_trim_right(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("  hello", qd_string_data(elem.value.s), "trim_right '  hello  '");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- count ---------- */

TEST(StrCountTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello hello hello");
	qd_push_s(ctx, "hello");
	usr_strings_count(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(3, (int)elem.value.i, "count 'hello' in 'hello hello hello'");

	destroy_test_context(ctx);
}

TEST(StrCountNonOverlappingTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "aaa");
	qd_push_s(ctx, "aa");
	usr_strings_count(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "non-overlapping count 'aa' in 'aaa'");

	destroy_test_context(ctx);
}


/* ---------- last_index_of ---------- */

TEST(StrLastIndexOfTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world hello");
	qd_push_s(ctx, "hello");
	usr_strings_last_index_of(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(12, (int)elem.value.i, "last_index_of 'hello'");

	destroy_test_context(ctx);
}

TEST(StrLastIndexOfNotFoundTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "xyz");
	usr_strings_last_index_of(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(-1, (int)elem.value.i, "last_index_of not found");

	destroy_test_context(ctx);
}


/* ---------- index_of_from ---------- */

TEST(StrIndexOfFromTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello hello");
	qd_push_s(ctx, "hello");
	qd_push_i(ctx, 1);
	usr_strings_index_of_from(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(6, (int)elem.value.i, "index_of_from 'hello' starting at 1");

	destroy_test_context(ctx);
}


/* ---------- is_empty ---------- */

TEST(StrIsEmptyTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_strings_is_empty(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_empty on empty string");

	destroy_test_context(ctx);
}

TEST(StrIsEmptyFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "a");
	usr_strings_is_empty(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_empty on non-empty string");

	destroy_test_context(ctx);
}


/* ---------- is_blank ---------- */

TEST(StrIsBlankWhitespaceTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "  ");
	usr_strings_is_blank(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_blank on whitespace");

	destroy_test_context(ctx);
}

TEST(StrIsBlankEmptyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_strings_is_blank(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_blank on empty string");

	destroy_test_context(ctx);
}

TEST(StrIsBlankFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "a");
	usr_strings_is_blank(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_blank on non-blank string");

	destroy_test_context(ctx);
}


/* ---------- equals_ignore_case ---------- */

TEST(StrEqualsIgnoreCaseTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "Hello");
	qd_push_s(ctx, "hello");
	usr_strings_equals_ignore_case(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "equals_ignore_case 'Hello' vs 'hello'");

	destroy_test_context(ctx);
}

TEST(StrEqualsIgnoreCaseFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "abc");
	qd_push_s(ctx, "def");
	usr_strings_equals_ignore_case(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "equals_ignore_case 'abc' vs 'def'");

	destroy_test_context(ctx);
}


/* ---------- pad_left ---------- */

TEST(StrPadLeftTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hi");
	qd_push_i(ctx, 5);
	qd_push_s(ctx, "0");
	usr_strings_pad_left(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("000hi", qd_string_data(elem.value.s), "pad_left 'hi' to 5 with '0'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- pad_right ---------- */

TEST(StrPadRightTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hi");
	qd_push_i(ctx, 5);
	qd_push_s(ctx, ".");
	usr_strings_pad_right(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hi...", qd_string_data(elem.value.s), "pad_right 'hi' to 5 with '.'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- center ---------- */

TEST(StrCenterTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hi");
	qd_push_i(ctx, 6);
	qd_push_s(ctx, "-");
	usr_strings_center(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("--hi--", qd_string_data(elem.value.s), "center 'hi' to 6 with '-'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- capitalize ---------- */

TEST(StrCapitalizeTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	usr_strings_capitalize(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("Hello world", qd_string_data(elem.value.s), "capitalize 'hello world'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- title ---------- */

TEST(StrTitleTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	usr_strings_title(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("Hello World", qd_string_data(elem.value.s), "title 'hello world'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- trim_prefix ---------- */

TEST(StrTrimPrefixTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "hello ");
	usr_strings_trim_prefix(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("world", qd_string_data(elem.value.s), "trim_prefix 'hello ' from 'hello world'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- trim_suffix ---------- */

TEST(StrTrimSuffixTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, " world");
	usr_strings_trim_suffix(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello", qd_string_data(elem.value.s), "trim_suffix ' world' from 'hello world'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- replace_first ---------- */

TEST(StrReplaceFirstTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "aaa");
	qd_push_s(ctx, "a");
	qd_push_s(ctx, "b");
	usr_strings_replace_first(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("baa", qd_string_data(elem.value.s), "replace_first 'a' with 'b' in 'aaa'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- insert ---------- */

TEST(StrInsertTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "helo");
	qd_push_i(ctx, 3);
	qd_push_s(ctx, "l");
	usr_strings_insert(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello", qd_string_data(elem.value.s), "insert 'l' at pos 3 in 'helo'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- remove_range ---------- */

TEST(StrRemoveRangeTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 6);
	usr_strings_remove_range(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello", qd_string_data(elem.value.s), "remove_range from 5 len 6");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ---------- is_numeric ---------- */

TEST(StrIsNumericTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "12345");
	usr_strings_is_numeric(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_numeric '12345'");

	destroy_test_context(ctx);
}

TEST(StrIsNumericFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "12a45");
	usr_strings_is_numeric(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_numeric '12a45'");

	destroy_test_context(ctx);
}

TEST(StrIsNumericEmptyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_strings_is_numeric(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_numeric empty string");

	destroy_test_context(ctx);
}


/* ---------- is_alpha ---------- */

TEST(StrIsAlphaTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	usr_strings_is_alpha(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_alpha 'hello'");

	destroy_test_context(ctx);
}

TEST(StrIsAlphaFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello1");
	usr_strings_is_alpha(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_alpha 'hello1'");

	destroy_test_context(ctx);
}


/* ---------- is_alphanumeric ---------- */

TEST(StrIsAlphanumericTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello123");
	usr_strings_is_alphanumeric(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_alphanumeric 'hello123'");

	destroy_test_context(ctx);
}

TEST(StrIsAlphanumericFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello!");
	usr_strings_is_alphanumeric(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_alphanumeric 'hello!'");

	destroy_test_context(ctx);
}


/* ---------- is_ascii ---------- */

TEST(StrIsAsciiTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	usr_strings_is_ascii(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_ascii 'hello'");

	destroy_test_context(ctx);
}


/* ---------- is_lowercase ---------- */

TEST(StrIsLowercaseTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	usr_strings_is_lowercase(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_lowercase 'hello'");

	destroy_test_context(ctx);
}

TEST(StrIsLowercaseFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "Hello");
	usr_strings_is_lowercase(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_lowercase 'Hello'");

	destroy_test_context(ctx);
}


/* ---------- is_uppercase ---------- */

TEST(StrIsUppercaseTrueTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "HELLO");
	usr_strings_is_uppercase(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "is_uppercase 'HELLO'");

	destroy_test_context(ctx);
}

TEST(StrIsUppercaseFalseTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "Hello");
	usr_strings_is_uppercase(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "is_uppercase 'Hello'");

	destroy_test_context(ctx);
}


/* ---------- char_count ---------- */

TEST(StrCharCountTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	usr_strings_char_count(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(5, (int)elem.value.i, "char_count 'hello'");

	destroy_test_context(ctx);
}


/* ---------- slice ---------- */

TEST(StrSliceTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_i(ctx, 0);
	qd_push_i(ctx, 5);
	usr_strings_slice(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("hello", qd_string_data(elem.value.s), "slice 0..5 of 'hello world'");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


int main(void) {
	return UC_PrintResults();
}

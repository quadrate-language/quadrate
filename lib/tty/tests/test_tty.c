/**
 * @file test_tty.c
 * @brief Unit tests for the qdtty terminal detection library
 */

#include <quadrate/tty/tty.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}


TEST(TtyIsStdoutTest) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stdout(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "is_stdout result should be int");
	ASSERT(elem.value.i == 0 || elem.value.i == 1, "is_stdout should be 0 or 1");

	destroy_test_context(ctx);
}

TEST(TtyIsStderrTest) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stderr(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "is_stderr result should be int");
	ASSERT(elem.value.i == 0 || elem.value.i == 1, "is_stderr should be 0 or 1");

	destroy_test_context(ctx);
}

TEST(TtyIsStdinTest) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stdin(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "is_stdin result should be int");
	ASSERT(elem.value.i == 0 || elem.value.i == 1, "is_stdin should be 0 or 1");

	destroy_test_context(ctx);
}

TEST(TtySizeTest) {
	qd_context* ctx = create_test_context();

	usr_tty_size(ctx);

	// size pushes rows first, then cols: ( -- rows:i cols:i )
	// Pop cols first (top of stack), then rows
	qd_stack_element_t cols_elem;
	qd_stack_pop(ctx->st, &cols_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, cols_elem.type, "cols result should be int");
	ASSERT(cols_elem.value.i >= 0, "cols should be non-negative");

	qd_stack_element_t rows_elem;
	qd_stack_pop(ctx->st, &rows_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, rows_elem.type, "rows result should be int");
	ASSERT(rows_elem.value.i >= 0, "rows should be non-negative");

	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after popping size results");

	destroy_test_context(ctx);
}

TEST(TtyWidthTest) {
	qd_context* ctx = create_test_context();

	usr_tty_width(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "width result should be int");
	ASSERT(elem.value.i >= 0, "width should be non-negative");

	destroy_test_context(ctx);
}

TEST(TtyHeightTest) {
	qd_context* ctx = create_test_context();

	usr_tty_height(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "height result should be int");
	ASSERT(elem.value.i >= 0, "height should be non-negative");

	destroy_test_context(ctx);
}

TEST(TtyIsStdoutStackCleanTest) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stdout(ctx);

	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "is_stdout should push exactly 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);

	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after pop");

	destroy_test_context(ctx);
}

TEST(TtySizeStackCleanTest) {
	qd_context* ctx = create_test_context();

	usr_tty_size(ctx);

	ASSERT_EQ(2, (int)qd_stack_size(ctx->st), "size should push exactly 2 elements");

	qd_stack_element_t elem1, elem2;
	qd_stack_pop(ctx->st, &elem1);
	qd_stack_pop(ctx->st, &elem2);

	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after popping both");

	destroy_test_context(ctx);
}

TEST(TtyIsStdoutTypeConsistency) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stdout(ctx);
	qd_stack_element_t e1;
	qd_stack_pop(ctx->st, &e1);
	ASSERT_EQ(QD_STACK_TYPE_INT, e1.type, "is_stdout type should be int (consistency check)");

	usr_tty_is_stderr(ctx);
	qd_stack_element_t e2;
	qd_stack_pop(ctx->st, &e2);
	ASSERT_EQ(QD_STACK_TYPE_INT, e2.type, "is_stderr type should be int (consistency check)");

	usr_tty_is_stdin(ctx);
	qd_stack_element_t e3;
	qd_stack_pop(ctx->st, &e3);
	ASSERT_EQ(QD_STACK_TYPE_INT, e3.type, "is_stdin type should be int (consistency check)");

	destroy_test_context(ctx);
}

TEST(TtySizeWidthHeightConsistency) {
	qd_context* ctx = create_test_context();

	usr_tty_size(ctx);
	qd_stack_element_t cols_elem, rows_elem;
	qd_stack_pop(ctx->st, &cols_elem);
	qd_stack_pop(ctx->st, &rows_elem);

	usr_tty_width(ctx);
	qd_stack_element_t width_elem;
	qd_stack_pop(ctx->st, &width_elem);

	usr_tty_height(ctx);
	qd_stack_element_t height_elem;
	qd_stack_pop(ctx->st, &height_elem);

	ASSERT_EQ((int)cols_elem.value.i, (int)width_elem.value.i, "width() should match cols from size()");
	ASSERT_EQ((int)rows_elem.value.i, (int)height_elem.value.i, "height() should match rows from size()");

	destroy_test_context(ctx);
}

TEST(TtyMultipleCallsConsistency) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stdout(ctx);
	qd_stack_element_t first;
	qd_stack_pop(ctx->st, &first);

	usr_tty_is_stdout(ctx);
	qd_stack_element_t second;
	qd_stack_pop(ctx->st, &second);

	ASSERT_EQ((int)first.value.i, (int)second.value.i, "repeated is_stdout calls should return same value");

	usr_tty_width(ctx);
	qd_stack_element_t w1;
	qd_stack_pop(ctx->st, &w1);

	usr_tty_width(ctx);
	qd_stack_element_t w2;
	qd_stack_pop(ctx->st, &w2);

	ASSERT_EQ((int)w1.value.i, (int)w2.value.i, "repeated width calls should return same value");

	usr_tty_height(ctx);
	qd_stack_element_t h1;
	qd_stack_pop(ctx->st, &h1);

	usr_tty_height(ctx);
	qd_stack_element_t h2;
	qd_stack_pop(ctx->st, &h2);

	ASSERT_EQ((int)h1.value.i, (int)h2.value.i, "repeated height calls should return same value");

	destroy_test_context(ctx);
}

TEST(TtyIsStderrStackCleanTest) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stderr(ctx);
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "is_stderr should push exactly 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after popping is_stderr result");

	destroy_test_context(ctx);
}

TEST(TtyIsStdinStackCleanTest) {
	qd_context* ctx = create_test_context();

	usr_tty_is_stdin(ctx);
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "is_stdin should push exactly 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after popping is_stdin result");

	destroy_test_context(ctx);
}

TEST(TtyWidthStackCleanTest) {
	qd_context* ctx = create_test_context();

	usr_tty_width(ctx);
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "width should push exactly 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after popping width result");

	destroy_test_context(ctx);
}

TEST(TtyHeightStackCleanTest) {
	qd_context* ctx = create_test_context();

	usr_tty_height(ctx);
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "height should push exactly 1 element");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_TRUE(qd_stack_is_empty(ctx->st), "stack should be empty after popping height result");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}

/**
 * @file test_mem.c
 * @brief Unit tests for the qdmem memory library
 */

#include <quadrate/mem/mem.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Helper function to create a context
/* Must go through qd_create_context: a hand-rolled context initialises only ->st,
 * so any test that trips a fatal runtime path walks an uninitialised call stack and
 * dies of SIGSEGV instead of the SIGABRT it is asserting. */
static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

// Helper function to destroy a context
static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}


TEST(AllocBasicTest) {
	qd_context* ctx = create_test_context();

	// Allocate 64 bytes
	qd_push_i(ctx, 64);
	int result = usr_mem_alloc(ctx);
	ASSERT_EQ(result, 0, "alloc should succeed");

	// Pop Ok status first (success pushes [result, Ok])
	qd_stack_element_t status_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop status should succeed");
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	// Check that a pointer was returned
	qd_stack_element_t ptr_elem;
	err = qd_stack_pop(ctx->st, &ptr_elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(ptr_elem.type, QD_STACK_TYPE_PTR, "result should be ptr");
	ASSERT(ptr_elem.value.p != NULL, "pointer should not be null");

	// Free the allocated memory
	free(ptr_elem.value.p);

	destroy_test_context(ctx);
}


TEST(SetGetByteTest) {
	qd_context* ctx = create_test_context();

	// Allocate buffer
	qd_push_i(ctx, 16);
	usr_mem_alloc(ctx);

	// Pop Ok status first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	void* buffer = ptr_elem.value.p;

	// Set byte at offset 0
	// Pops: offset, address, value -> Push: value, address, offset
	qd_push_i(ctx, 0xAB);  // value
	qd_push_p(ctx, buffer);// address
	qd_push_i(ctx, 0);     // offset
	int result = usr_mem_set_byte(ctx);
	ASSERT_EQ(result, 0, "set_byte should succeed");

	// Set byte at offset 5
	qd_push_i(ctx, 0xCD);  // value
	qd_push_p(ctx, buffer);// address
	qd_push_i(ctx, 5);     // offset
	result = usr_mem_set_byte(ctx);
	ASSERT_EQ(result, 0, "set_byte should succeed");

	// Get byte at offset 0
	// Pops: offset, address -> Push: address, offset
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 0);
	result = usr_mem_get_byte(ctx);
	ASSERT_EQ(result, 0, "get_byte should succeed");

	qd_stack_element_t val_elem;
	qd_stack_pop(ctx->st, &val_elem);
	ASSERT_EQ(val_elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ((int)val_elem.value.i, 0xAB, "byte at offset 0 should be 0xAB");

	// Get byte at offset 5
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 5);
	usr_mem_get_byte(ctx);
	qd_stack_pop(ctx->st, &val_elem);
	ASSERT_EQ((int)val_elem.value.i, 0xCD, "byte at offset 5 should be 0xCD");

	free(buffer);
	destroy_test_context(ctx);
}


TEST(SetGetI64Test) {
	qd_context* ctx = create_test_context();

	// Allocate buffer
	qd_push_i(ctx, 64);
	usr_mem_alloc(ctx);

	// Pop Ok status first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	void* buffer = ptr_elem.value.p;

	// Set i64 at offset 0
	// Pops: offset, address, value -> Push: value, address, offset
	qd_push_i(ctx, 0x123456789ABCDEF0LL);
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 0);
	int result = usr_mem_set_i64(ctx);
	ASSERT_EQ(result, 0, "set_i64 should succeed");

	// Get i64 at offset 0
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 0);
	result = usr_mem_get_i64(ctx);
	ASSERT_EQ(result, 0, "get_i64 should succeed");

	qd_stack_element_t val_elem;
	qd_stack_pop(ctx->st, &val_elem);
	ASSERT_EQ(val_elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT(val_elem.value.i == 0x123456789ABCDEF0LL, "value should match");

	free(buffer);
	destroy_test_context(ctx);
}


TEST(SetGetF64Test) {
	qd_context* ctx = create_test_context();

	// Allocate buffer
	qd_push_i(ctx, 64);
	usr_mem_alloc(ctx);

	// Pop Ok status first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	void* buffer = ptr_elem.value.p;

	// Set f64 at offset 0
	// Pops: offset, address, value -> Push: value, address, offset
	qd_push_f(ctx, 3.14159);
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 0);
	int result = usr_mem_set_f64(ctx);
	ASSERT_EQ(result, 0, "set_f64 should succeed");

	// Get f64 at offset 0
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 0);
	result = usr_mem_get_f64(ctx);
	ASSERT_EQ(result, 0, "get_f64 should succeed");

	qd_stack_element_t val_elem;
	qd_stack_pop(ctx->st, &val_elem);
	ASSERT_EQ(val_elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT(val_elem.value.f > 3.14 && val_elem.value.f < 3.15, "value should be ~3.14159");

	free(buffer);
	destroy_test_context(ctx);
}


TEST(SetGetPtrTest) {
	qd_context* ctx = create_test_context();

	// Allocate buffer
	qd_push_i(ctx, 64);
	usr_mem_alloc(ctx);

	// Pop Ok status first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	void* buffer = ptr_elem.value.p;

	// Create a test pointer value
	void* test_ptr = (void*)0x12345678;

	// Set ptr at offset 0
	// Pops: offset, address, value -> Push: value, address, offset
	qd_push_p(ctx, test_ptr);
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 0);
	int result = usr_mem_set_ptr(ctx);
	ASSERT_EQ(result, 0, "set_ptr should succeed");

	// Get ptr at offset 0
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 0);
	result = usr_mem_get_ptr(ctx);
	ASSERT_EQ(result, 0, "get_ptr should succeed");

	qd_stack_element_t val_elem;
	qd_stack_pop(ctx->st, &val_elem);
	ASSERT_EQ(val_elem.type, QD_STACK_TYPE_PTR, "result should be ptr");
	ASSERT(val_elem.value.p == test_ptr, "pointer value should match");

	free(buffer);
	destroy_test_context(ctx);
}


TEST(ZeroTest) {
	qd_context* ctx = create_test_context();

	// Allocate buffer and fill with non-zero values
	unsigned char* buffer = malloc(16);
	memset(buffer, 0xFF, 16);

	// Zero first 8 bytes
	// Pops: bytes, address -> Push: address, bytes
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 8);
	int result = usr_mem_zero(ctx);
	ASSERT_EQ(result, 0, "zero should succeed");

	// Verify first 8 bytes are zero
	for (int i = 0; i < 8; i++) {
		ASSERT_EQ((int)buffer[i], 0, "byte should be zero");
	}

	// Verify remaining bytes are still 0xFF
	for (int i = 8; i < 16; i++) {
		ASSERT_EQ((int)buffer[i], 0xFF, "byte should be 0xFF");
	}

	free(buffer);
	destroy_test_context(ctx);
}


TEST(FillTest) {
	qd_context* ctx = create_test_context();

	// Allocate buffer
	unsigned char* buffer = malloc(16);
	memset(buffer, 0, 16);

	// Fill with 0xAB
	// Pops: bytes, address, value -> Push: value, address, bytes
	qd_push_i(ctx, 0xAB);  // value
	qd_push_p(ctx, buffer);// address
	qd_push_i(ctx, 10);    // bytes
	int result = usr_mem_fill(ctx);
	ASSERT_EQ(result, 0, "fill should succeed");

	// Verify first 10 bytes are 0xAB
	for (int i = 0; i < 10; i++) {
		ASSERT_EQ((int)buffer[i], 0xAB, "byte should be 0xAB");
	}

	// Verify remaining bytes are still 0
	for (int i = 10; i < 16; i++) {
		ASSERT_EQ((int)buffer[i], 0, "byte should be 0");
	}

	free(buffer);
	destroy_test_context(ctx);
}


TEST(CopyTest) {
	qd_context* ctx = create_test_context();

	// Create source buffer with test data
	unsigned char* src = malloc(16);
	for (int i = 0; i < 16; i++) {
		src[i] = (unsigned char)i;
	}

	// Create destination buffer
	unsigned char* dst = malloc(16);
	memset(dst, 0, 16);

	// Copy 8 bytes from src to dst
	// Pops: bytes, src, dst -> Push: dst, src, bytes
	qd_push_p(ctx, dst);
	qd_push_p(ctx, src);
	qd_push_i(ctx, 8);
	int result = usr_mem_copy(ctx);
	ASSERT_EQ(result, 0, "copy should succeed");

	// Verify first 8 bytes were copied
	for (int i = 0; i < 8; i++) {
		ASSERT_EQ((int)dst[i], i, "byte should match source");
	}

	// Verify remaining bytes are still 0
	for (int i = 8; i < 16; i++) {
		ASSERT_EQ((int)dst[i], 0, "byte should be 0");
	}

	free(src);
	free(dst);
	destroy_test_context(ctx);
}


TEST(ToStringTest) {
	qd_context* ctx = create_test_context();

	// Create buffer with "Hello" string
	char* buffer = malloc(8);
	strcpy(buffer, "Hello");

	// Convert to string (5 bytes)
	// Pops: length, buffer -> Push: buffer, length
	qd_push_p(ctx, buffer);
	qd_push_i(ctx, 5);
	int result = usr_mem_to_string(ctx);
	ASSERT_EQ(result, 0, "to_string should succeed");

	// Get result
	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);
	ASSERT_EQ(str_elem.type, QD_STACK_TYPE_STR, "result should be string");
	ASSERT_STR_EQ(qd_string_data(str_elem.value.s), "Hello", "string should match");

	qd_string_release(str_elem.value.s);
	free(buffer);
	destroy_test_context(ctx);
}

TEST(FromStringTest) {
	qd_context* ctx = create_test_context();

	// Push string
	qd_push_s(ctx, "Test");
	int result = usr_mem_from_string(ctx);
	ASSERT_EQ(result, 0, "from_string should succeed");

	// Get length
	qd_stack_element_t len_elem;
	qd_stack_pop(ctx->st, &len_elem);
	ASSERT_EQ(len_elem.type, QD_STACK_TYPE_INT, "length should be int");
	ASSERT_EQ((int)len_elem.value.i, 4, "length should be 4");

	// Get pointer
	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	ASSERT_EQ(ptr_elem.type, QD_STACK_TYPE_PTR, "result should be ptr");

	// Verify content
	ASSERT(memcmp(ptr_elem.value.p, "Test", 4) == 0, "content should match");

	// Free the allocated buffer
	free(ptr_elem.value.p);

	destroy_test_context(ctx);
}

TEST(ReallocTest) {
	qd_context* ctx = create_test_context();

	// Allocate 16 bytes
	qd_push_i(ctx, 16);
	int result = usr_mem_alloc(ctx);
	ASSERT_EQ(result, 0, "alloc should succeed");

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "alloc status should be Ok");

	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	void* ptr = ptr_elem.value.p;
	ASSERT(ptr != NULL, "alloc pointer should not be null");

	// Write data to verify it survives realloc
	memset(ptr, 0xAB, 16);

	// Realloc to 64 bytes: push ptr then new_bytes
	qd_push_p(ctx, ptr);
	qd_push_i(ctx, 64);
	result = usr_mem_realloc(ctx);
	ASSERT_EQ(result, 0, "realloc should succeed");

	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "realloc status should be Ok");

	qd_stack_pop(ctx->st, &ptr_elem);
	ASSERT(ptr_elem.value.p != NULL, "realloc pointer should not be null");

	// Verify original data survived
	unsigned char* data = (unsigned char*)ptr_elem.value.p;
	ASSERT_EQ((int)data[0], 0xAB, "first byte should survive realloc");
	ASSERT_EQ((int)data[15], 0xAB, "last original byte should survive realloc");

	free(ptr_elem.value.p);
	destroy_test_context(ctx);
}


TEST(AllocAlignedTest) {
	qd_context* ctx = create_test_context();

	// alloc_aligned pops bytes then alignment
	// Push alignment first, then bytes (alignment popped second)
	qd_push_i(ctx, 16);  // alignment
	qd_push_i(ctx, 64);  // bytes
	int result = usr_mem_alloc_aligned(ctx);
	ASSERT_EQ(result, 0, "alloc_aligned should succeed");

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "alloc_aligned status should be Ok");

	qd_stack_element_t ptr_elem;
	qd_stack_pop(ctx->st, &ptr_elem);
	ASSERT_EQ(ptr_elem.type, QD_STACK_TYPE_PTR, "result should be ptr");
	ASSERT(ptr_elem.value.p != NULL, "aligned pointer should not be null");

	// Verify 16-byte alignment
	ASSERT(((uintptr_t)ptr_elem.value.p % 16) == 0, "pointer should be 16-byte aligned");

	free(ptr_elem.value.p);
	destroy_test_context(ctx);
}


// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}

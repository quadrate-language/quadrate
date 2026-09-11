/**
 * @file test_thread.c
 * @brief Unit tests for the qdthread threading primitives library
 */

#include <quadrate/thread/thread.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

// ============================================================================
// Mutex tests
// ============================================================================

TEST(MutexCreateAndFreeTest) {
	qd_context* ctx = create_test_context();

	// Create mutex: ( -- mutex:ptr result:i )
	usr_thread_raw_mutex_new(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "mutex new result should be int");
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "mutex new should succeed");

	qd_stack_element_t mutex_elem;
	qd_stack_pop(ctx->st, &mutex_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, mutex_elem.type, "mutex should be a pointer");
	ASSERT_TRUE(mutex_elem.value.p != NULL, "mutex pointer should not be null");

	// Free the mutex: (mutex:ptr -- )
	qd_push_p(ctx, mutex_elem.value.p);
	usr_thread_raw_mutex_free(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after mutex free");

	destroy_test_context(ctx);
}

TEST(MutexLockUnlockTest) {
	qd_context* ctx = create_test_context();

	// Create mutex
	usr_thread_raw_mutex_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "mutex new should succeed");

	qd_stack_element_t mutex_elem;
	qd_stack_pop(ctx->st, &mutex_elem);
	void* mutex_ptr = mutex_elem.value.p;

	// Lock: (mutex:ptr -- result:i)
	qd_push_p(ctx, mutex_ptr);
	int ret = usr_thread_raw_mutex_lock(ctx);
	ASSERT_EQ(0, ret, "mutex lock should return 0");

	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "mutex lock should push THREAD_OK");

	// Unlock: (mutex:ptr -- result:i)
	qd_push_p(ctx, mutex_ptr);
	ret = usr_thread_raw_mutex_unlock(ctx);
	ASSERT_EQ(0, ret, "mutex unlock should return 0");

	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "mutex unlock should push THREAD_OK");

	// Free
	qd_push_p(ctx, mutex_ptr);
	usr_thread_raw_mutex_free(ctx);

	destroy_test_context(ctx);
}

TEST(MutexTryLockTest) {
	qd_context* ctx = create_test_context();

	// Create mutex
	usr_thread_raw_mutex_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t mutex_elem;
	qd_stack_pop(ctx->st, &mutex_elem);
	void* mutex_ptr = mutex_elem.value.p;

	// try_lock on unlocked mutex should succeed
	qd_push_p(ctx, mutex_ptr);
	usr_thread_raw_mutex_try_lock(ctx);

	qd_stack_element_t success;
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(QD_STACK_TYPE_INT, success.type, "try_lock result should be int");
	ASSERT_EQ(1, (int)success.value.i, "try_lock on unlocked mutex should return 1");

	// try_lock again on already-locked mutex should fail
	qd_push_p(ctx, mutex_ptr);
	usr_thread_raw_mutex_try_lock(ctx);

	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(0, (int)success.value.i, "try_lock on locked mutex should return 0");

	// Unlock and free
	qd_push_p(ctx, mutex_ptr);
	usr_thread_raw_mutex_unlock(ctx);
	qd_stack_pop(ctx->st, &result); // discard THREAD_OK

	qd_push_p(ctx, mutex_ptr);
	usr_thread_raw_mutex_free(ctx);

	destroy_test_context(ctx);
}

TEST(MutexDoubleLockUnlockCycleTest) {
	qd_context* ctx = create_test_context();

	// Create mutex
	usr_thread_raw_mutex_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t mutex_elem;
	qd_stack_pop(ctx->st, &mutex_elem);
	void* mutex_ptr = mutex_elem.value.p;

	// Lock/unlock cycle twice
	for (int i = 0; i < 2; i++) {
		qd_push_p(ctx, mutex_ptr);
		usr_thread_raw_mutex_lock(ctx);
		qd_stack_pop(ctx->st, &result);
		ASSERT_EQ(THREAD_OK, (int)result.value.i, "lock should succeed in cycle");

		qd_push_p(ctx, mutex_ptr);
		usr_thread_raw_mutex_unlock(ctx);
		qd_stack_pop(ctx->st, &result);
		ASSERT_EQ(THREAD_OK, (int)result.value.i, "unlock should succeed in cycle");
	}

	// Free
	qd_push_p(ctx, mutex_ptr);
	usr_thread_raw_mutex_free(ctx);

	destroy_test_context(ctx);
}

// ============================================================================
// Channel tests (buffered to avoid blocking)
// ============================================================================

TEST(ChannelBufferedCreateAndFreeTest) {
	qd_context* ctx = create_test_context();

	// Create buffered channel: (capacity:i64 -- ch:ptr result:i)
	qd_push_i(ctx, 4);
	usr_thread_raw_chan_buffered(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "chan_buffered result should be int");
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "chan_buffered should succeed");

	qd_stack_element_t ch_elem;
	qd_stack_pop(ctx->st, &ch_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, ch_elem.type, "channel should be a pointer");
	ASSERT_TRUE(ch_elem.value.p != NULL, "channel pointer should not be null");

	// Free: (ch:ptr -- )
	qd_push_p(ctx, ch_elem.value.p);
	usr_thread_raw_chan_free(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after chan free");

	destroy_test_context(ctx);
}

TEST(ChannelBufferedSendRecvTest) {
	qd_context* ctx = create_test_context();

	// Create buffered channel with capacity 4
	qd_push_i(ctx, 4);
	usr_thread_raw_chan_buffered(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "chan_buffered should succeed");

	qd_stack_element_t ch_elem;
	qd_stack_pop(ctx->st, &ch_elem);
	void* ch_ptr = ch_elem.value.p;

	// Send a value: (val ch:ptr -- result:i)
	qd_push_i(ctx, 42);
	qd_push_p(ctx, ch_ptr);
	int ret = usr_thread_raw_chan_send(ctx);
	ASSERT_EQ(0, ret, "chan_send should return 0");

	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "chan_send should push THREAD_OK");

	// Check length: (ch:ptr -- len:i64)
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_len(ctx);

	qd_stack_element_t len;
	qd_stack_pop(ctx->st, &len);
	ASSERT_EQ(1, (int)len.value.i, "channel should have 1 element after send");

	// Receive: (ch:ptr -- val result:i)
	qd_push_p(ctx, ch_ptr);
	ret = usr_thread_raw_chan_recv(ctx);
	ASSERT_EQ(0, ret, "chan_recv should return 0");

	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "chan_recv should push THREAD_OK");

	qd_stack_element_t val;
	qd_stack_pop(ctx->st, &val);
	ASSERT_EQ(QD_STACK_TYPE_INT, val.type, "received value should be int");
	ASSERT_EQ(42, (int)val.value.i, "received value should be 42");

	// Free channel
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_free(ctx);

	destroy_test_context(ctx);
}

TEST(ChannelTrySendTryRecvTest) {
	qd_context* ctx = create_test_context();

	// Create buffered channel with capacity 2
	qd_push_i(ctx, 2);
	usr_thread_raw_chan_buffered(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t ch_elem;
	qd_stack_pop(ctx->st, &ch_elem);
	void* ch_ptr = ch_elem.value.p;

	// try_send two values (fills buffer)
	qd_push_i(ctx, 10);
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_try_send(ctx);
	qd_stack_element_t success;
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(1, (int)success.value.i, "first try_send should succeed");

	qd_push_i(ctx, 20);
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_try_send(ctx);
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(1, (int)success.value.i, "second try_send should succeed");

	// try_send when full should fail
	qd_push_i(ctx, 30);
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_try_send(ctx);
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(0, (int)success.value.i, "try_send on full channel should return 0");

	// try_recv first value
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_try_recv(ctx);
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(1, (int)success.value.i, "first try_recv should succeed");
	qd_stack_element_t val;
	qd_stack_pop(ctx->st, &val);
	ASSERT_EQ(10, (int)val.value.i, "first recv value should be 10");

	// try_recv second value
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_try_recv(ctx);
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(1, (int)success.value.i, "second try_recv should succeed");
	qd_stack_pop(ctx->st, &val);
	ASSERT_EQ(20, (int)val.value.i, "second recv value should be 20");

	// try_recv on empty channel should fail
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_try_recv(ctx);
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(0, (int)success.value.i, "try_recv on empty channel should return 0");
	qd_stack_pop(ctx->st, &val); // dummy value

	// Free
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_free(ctx);

	destroy_test_context(ctx);
}

TEST(ChannelCloseTest) {
	qd_context* ctx = create_test_context();

	// Create buffered channel
	qd_push_i(ctx, 4);
	usr_thread_raw_chan_buffered(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t ch_elem;
	qd_stack_pop(ctx->st, &ch_elem);
	void* ch_ptr = ch_elem.value.p;

	// Channel should not be closed initially
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_is_closed(ctx);
	qd_stack_element_t closed;
	qd_stack_pop(ctx->st, &closed);
	ASSERT_EQ(0, (int)closed.value.i, "channel should not be closed initially");

	// Close channel
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_close(ctx);

	// Channel should be closed now
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_is_closed(ctx);
	qd_stack_pop(ctx->st, &closed);
	ASSERT_EQ(1, (int)closed.value.i, "channel should be closed after close");

	// Send on closed channel should fail
	qd_push_i(ctx, 99);
	qd_push_p(ctx, ch_ptr);
	int ret = usr_thread_raw_chan_send(ctx);
	ASSERT_NE(0, ret, "send on closed channel should return error");

	// Clear error state
	qd_clear_error(ctx);

	// Free
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_free(ctx);

	destroy_test_context(ctx);
}

TEST(ChannelCapacityAndLenTest) {
	qd_context* ctx = create_test_context();

	// Create buffered channel with capacity 8
	qd_push_i(ctx, 8);
	usr_thread_raw_chan_buffered(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t ch_elem;
	qd_stack_pop(ctx->st, &ch_elem);
	void* ch_ptr = ch_elem.value.p;

	// Check capacity
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_cap(ctx);
	qd_stack_element_t cap;
	qd_stack_pop(ctx->st, &cap);
	ASSERT_EQ(8, (int)cap.value.i, "channel capacity should be 8");

	// Check initial length
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_len(ctx);
	qd_stack_element_t len;
	qd_stack_pop(ctx->st, &len);
	ASSERT_EQ(0, (int)len.value.i, "channel should be empty initially");

	// Send 3 items
	for (int i = 0; i < 3; i++) {
		qd_push_i(ctx, i);
		qd_push_p(ctx, ch_ptr);
		usr_thread_raw_chan_send(ctx);
		qd_stack_pop(ctx->st, &result); // discard THREAD_OK
	}

	// Check length is 3
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_len(ctx);
	qd_stack_pop(ctx->st, &len);
	ASSERT_EQ(3, (int)len.value.i, "channel should have 3 elements");

	// Free (remaining elements are cleaned up)
	qd_push_p(ctx, ch_ptr);
	usr_thread_raw_chan_free(ctx);

	destroy_test_context(ctx);
}

TEST(ChannelUnbufferedCreateTest) {
	qd_context* ctx = create_test_context();

	// Create unbuffered channel: ( -- ch:ptr result:i)
	usr_thread_raw_chan_new(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "chan_new should succeed");

	qd_stack_element_t ch_elem;
	qd_stack_pop(ctx->st, &ch_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, ch_elem.type, "channel should be a pointer");
	ASSERT_TRUE(ch_elem.value.p != NULL, "channel pointer should not be null");

	// Check capacity is 0 (unbuffered)
	qd_push_p(ctx, ch_elem.value.p);
	usr_thread_raw_chan_cap(ctx);
	qd_stack_element_t cap;
	qd_stack_pop(ctx->st, &cap);
	ASSERT_EQ(0, (int)cap.value.i, "unbuffered channel capacity should be 0");

	// Free
	qd_push_p(ctx, ch_elem.value.p);
	usr_thread_raw_chan_free(ctx);

	destroy_test_context(ctx);
}

// ============================================================================
// WaitGroup tests
// ============================================================================

TEST(WaitGroupCreateAndFreeTest) {
	qd_context* ctx = create_test_context();

	// Create waitgroup: ( -- wg:ptr result:i)
	usr_thread_raw_wg_new(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "wg_new should succeed");

	qd_stack_element_t wg_elem;
	qd_stack_pop(ctx->st, &wg_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, wg_elem.type, "waitgroup should be a pointer");
	ASSERT_TRUE(wg_elem.value.p != NULL, "waitgroup pointer should not be null");

	// Free
	qd_push_p(ctx, wg_elem.value.p);
	usr_thread_raw_wg_free(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after wg free");

	destroy_test_context(ctx);
}

TEST(WaitGroupAddDoneWaitTest) {
	qd_context* ctx = create_test_context();

	// Create waitgroup
	usr_thread_raw_wg_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t wg_elem;
	qd_stack_pop(ctx->st, &wg_elem);
	void* wg_ptr = wg_elem.value.p;

	// Add 1: (n:i64 wg:ptr -- )
	qd_push_i(ctx, 1);
	qd_push_p(ctx, wg_ptr);
	usr_thread_raw_wg_add(ctx);

	// Done: (wg:ptr -- )
	qd_push_p(ctx, wg_ptr);
	usr_thread_raw_wg_done(ctx);

	// Wait should return immediately since count == 0: (wg:ptr -- result:i)
	qd_push_p(ctx, wg_ptr);
	int ret = usr_thread_raw_wg_wait(ctx);
	ASSERT_EQ(0, ret, "wg_wait should return 0 when count is 0");

	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "wg_wait should push THREAD_OK");

	// Free
	qd_push_p(ctx, wg_ptr);
	usr_thread_raw_wg_free(ctx);

	destroy_test_context(ctx);
}

// ============================================================================
// Once tests
// ============================================================================

static int once_test_counter = 0;

static int once_increment_func(qd_context* ctx) {
	(void)ctx;
	once_test_counter++;
	return 0;
}

TEST(OnceCreateAndFreeTest) {
	qd_context* ctx = create_test_context();

	// Create once: ( -- once:ptr result:i)
	usr_thread_raw_once_new(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "once_new should succeed");

	qd_stack_element_t once_elem;
	qd_stack_pop(ctx->st, &once_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, once_elem.type, "once should be a pointer");

	// Check done status (should be false initially)
	qd_push_p(ctx, once_elem.value.p);
	usr_thread_raw_once_done(ctx);
	qd_stack_element_t done;
	qd_stack_pop(ctx->st, &done);
	ASSERT_EQ(0, (int)done.value.i, "once should not be done initially");

	// Free
	qd_push_p(ctx, once_elem.value.p);
	usr_thread_raw_once_free(ctx);

	destroy_test_context(ctx);
}

TEST(OnceDoRunsOnceTest) {
	qd_context* ctx = create_test_context();
	once_test_counter = 0;

	// Create once
	usr_thread_raw_once_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t once_elem;
	qd_stack_pop(ctx->st, &once_elem);
	void* once_ptr = once_elem.value.p;

	// Convert function pointer to void*
	void* func_ptr;
	int (*func)(qd_context*) = once_increment_func;
	memcpy(&func_ptr, &func, sizeof(func_ptr));

	// Call once_do three times - function should only execute once
	for (int i = 0; i < 3; i++) {
		qd_push_p(ctx, func_ptr);
		qd_push_p(ctx, once_ptr);
		usr_thread_raw_once_do(ctx);
	}

	ASSERT_EQ(1, once_test_counter, "once_do should have run the function exactly once");

	// Check done status
	qd_push_p(ctx, once_ptr);
	usr_thread_raw_once_done(ctx);
	qd_stack_element_t done;
	qd_stack_pop(ctx->st, &done);
	ASSERT_EQ(1, (int)done.value.i, "once should be done after do");

	// Free
	qd_push_p(ctx, once_ptr);
	usr_thread_raw_once_free(ctx);

	destroy_test_context(ctx);
}

// ============================================================================
// RwLock tests
// ============================================================================

TEST(RwLockCreateAndFreeTest) {
	qd_context* ctx = create_test_context();

	// Create rwlock: ( -- rwlock:ptr result:i)
	usr_thread_raw_rwlock_new(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "rwlock_new should succeed");

	qd_stack_element_t rw_elem;
	qd_stack_pop(ctx->st, &rw_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, rw_elem.type, "rwlock should be a pointer");
	ASSERT_TRUE(rw_elem.value.p != NULL, "rwlock pointer should not be null");

	// Free
	qd_push_p(ctx, rw_elem.value.p);
	usr_thread_raw_rwlock_free(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after rwlock free");

	destroy_test_context(ctx);
}

TEST(RwLockReadLockUnlockTest) {
	qd_context* ctx = create_test_context();

	// Create rwlock
	usr_thread_raw_rwlock_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t rw_elem;
	qd_stack_pop(ctx->st, &rw_elem);
	void* rw_ptr = rw_elem.value.p;

	// Read lock: (rwlock:ptr -- result:i)
	qd_push_p(ctx, rw_ptr);
	int ret = usr_thread_raw_rwlock_read_lock(ctx);
	ASSERT_EQ(0, ret, "read_lock should return 0");
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "read_lock should push THREAD_OK");

	// Read unlock: (rwlock:ptr -- result:i)
	qd_push_p(ctx, rw_ptr);
	ret = usr_thread_raw_rwlock_read_unlock(ctx);
	ASSERT_EQ(0, ret, "read_unlock should return 0");
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "read_unlock should push THREAD_OK");

	// Free
	qd_push_p(ctx, rw_ptr);
	usr_thread_raw_rwlock_free(ctx);

	destroy_test_context(ctx);
}

TEST(RwLockWriteLockUnlockTest) {
	qd_context* ctx = create_test_context();

	// Create rwlock
	usr_thread_raw_rwlock_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t rw_elem;
	qd_stack_pop(ctx->st, &rw_elem);
	void* rw_ptr = rw_elem.value.p;

	// Write lock: (rwlock:ptr -- result:i)
	qd_push_p(ctx, rw_ptr);
	int ret = usr_thread_raw_rwlock_write_lock(ctx);
	ASSERT_EQ(0, ret, "write_lock should return 0");
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "write_lock should push THREAD_OK");

	// Write unlock: (rwlock:ptr -- result:i)
	qd_push_p(ctx, rw_ptr);
	ret = usr_thread_raw_rwlock_write_unlock(ctx);
	ASSERT_EQ(0, ret, "write_unlock should return 0");
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "write_unlock should push THREAD_OK");

	// Free
	qd_push_p(ctx, rw_ptr);
	usr_thread_raw_rwlock_free(ctx);

	destroy_test_context(ctx);
}

TEST(RwLockTryReadTest) {
	qd_context* ctx = create_test_context();

	// Create rwlock
	usr_thread_raw_rwlock_new(ctx);
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	qd_stack_element_t rw_elem;
	qd_stack_pop(ctx->st, &rw_elem);
	void* rw_ptr = rw_elem.value.p;

	// try_read on unlocked rwlock should succeed
	qd_push_p(ctx, rw_ptr);
	usr_thread_raw_rwlock_try_read(ctx);
	qd_stack_element_t success;
	qd_stack_pop(ctx->st, &success);
	ASSERT_EQ(1, (int)success.value.i, "try_read on unlocked rwlock should succeed");

	// Unlock
	qd_push_p(ctx, rw_ptr);
	usr_thread_raw_rwlock_read_unlock(ctx);
	qd_stack_pop(ctx->st, &result);

	// Free
	qd_push_p(ctx, rw_ptr);
	usr_thread_raw_rwlock_free(ctx);

	destroy_test_context(ctx);
}

// ============================================================================
// Thread utility tests
// ============================================================================

TEST(ThreadSelfTest) {
	qd_context* ctx = create_test_context();

	// Get current thread ID: ( -- id:i64)
	usr_thread_raw_self(ctx);

	qd_stack_element_t id;
	qd_stack_pop(ctx->st, &id);
	ASSERT_EQ(QD_STACK_TYPE_INT, id.type, "thread self should return int");
	// Thread ID should be some non-zero value
	ASSERT_TRUE(id.value.i != 0, "thread id should be non-zero");

	destroy_test_context(ctx);
}

TEST(ThreadYieldTest) {
	qd_context* ctx = create_test_context();

	// Yield should not crash and should leave stack unchanged
	usr_thread_raw_yield(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after yield");

	destroy_test_context(ctx);
}

TEST(ThreadCpuCountTest) {
	qd_context* ctx = create_test_context();

	// Get CPU count: ( -- count:i64)
	usr_thread_raw_cpu_count(ctx);

	qd_stack_element_t count;
	qd_stack_pop(ctx->st, &count);
	ASSERT_EQ(QD_STACK_TYPE_INT, count.type, "cpu_count should return int");
	ASSERT_TRUE(count.value.i >= 1, "cpu count should be at least 1");

	destroy_test_context(ctx);
}

TEST(ThreadSleepShortTest) {
	qd_context* ctx = create_test_context();

	// Sleep for 1 ms (should not block for long)
	qd_push_i(ctx, 1);
	usr_thread_raw_sleep(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after sleep");

	destroy_test_context(ctx);
}

// ============================================================================
// Barrier tests (single-thread: threshold=1, should return immediately)
// ============================================================================

TEST(BarrierSingleThreadTest) {
	qd_context* ctx = create_test_context();

	// Create barrier with threshold 1: (n:i64 -- barrier:ptr result:i)
	qd_push_i(ctx, 1);
	usr_thread_raw_barrier_new(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(THREAD_OK, (int)result.value.i, "barrier_new should succeed");

	qd_stack_element_t barrier_elem;
	qd_stack_pop(ctx->st, &barrier_elem);
	void* barrier_ptr = barrier_elem.value.p;
	ASSERT_TRUE(barrier_ptr != NULL, "barrier pointer should not be null");

	// Wait on barrier with threshold 1 - should return immediately
	// and this thread should be the serial thread
	qd_push_p(ctx, barrier_ptr);
	usr_thread_raw_barrier_wait(ctx);

	qd_stack_element_t is_serial;
	qd_stack_pop(ctx->st, &is_serial);
	ASSERT_EQ(1, (int)is_serial.value.i, "single thread at barrier should be serial");

	// Free
	qd_push_p(ctx, barrier_ptr);
	usr_thread_raw_barrier_free(ctx);

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}

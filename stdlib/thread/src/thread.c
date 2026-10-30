#define _DEFAULT_SOURCE
// Thread module implementation using C11 threads
#include "quadrate/thread/thread.h"

#include <quadrate/rt/context.h>
#include <quadrate/rt/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for sysconf

typedef struct {
	void* func;
	// The spawning context's stack capacity, which is what `-s` sets. The thread used
	// to get a hardcoded 1,024 elements whatever the program asked for, so a thread
	// body that pushed more died advising a flag that could not reach it.
	size_t stack_size;
} qd_thread_spawn_data;

static int thread_entry(void* arg) {
	qd_thread_spawn_data* data = (qd_thread_spawn_data*)arg;

	qd_context* thread_ctx = qd_create_context(data->stack_size);
	if (!thread_ctx) {
		free(data);
		return 1;
	}

	// Cast and call the function pointer
	// Functions in Quadrate have signature: int (*)(qd_context*)
	typedef int (*qd_function_ptr)(qd_context*);
	qd_function_ptr func;
	memcpy(&func, &data->func, sizeof(func));

	if (func) {
		func(thread_ctx);
	}

	qd_free_context(thread_ctx);
	free(data);
	return 0;
}

// spawn(func:ptr -- thread:ptr)!
int usr_thread_raw_spawn(qd_context* ctx) {
	void* func;
	qd_args(ctx, "thread::spawn", "p", &func);

	qd_thread* thread = malloc(sizeof(qd_thread));
	if (!thread) {
		return qd_fail(ctx, THREAD_ERR_CREATE, "thread::spawn: failed to allocate thread");
	}

	qd_thread_spawn_data* data = malloc(sizeof(qd_thread_spawn_data));
	if (!data) {
		free(thread);
		return qd_fail(ctx, THREAD_ERR_CREATE, "thread::spawn: failed to allocate thread data");
	}

	data->func = func;
	data->stack_size = (ctx->st != NULL) ? (size_t)ctx->st->capacity : QD_DEFAULT_STACK_SIZE;

	int result = thrd_create(&thread->handle, thread_entry, data);
	if (result != thrd_success) {
		free(data);
		free(thread);
		return qd_fail(ctx, THREAD_ERR_CREATE, "thread::spawn: failed to create thread");
	}

	thread->started = true;
	thread->joined = false;
	thread->detached = false;

	qd_push_p(ctx, thread);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// join(thread:ptr -- )!
int usr_thread_raw_join(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "thread::join", "p", &handle);

	qd_thread* thread = (qd_thread*)handle;

	if (!thread || thread->joined || thread->detached) {
		return qd_fail(ctx, THREAD_ERR_JOIN, "thread::join: invalid thread or already joined/detached");
	}

	int result = thrd_join(thread->handle, NULL);
	if (result != thrd_success) {
		return qd_fail(ctx, THREAD_ERR_JOIN, "thread::join: failed to join thread");
	}

	thread->joined = true;
	free(thread);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// detach(thread:ptr -- )!
int usr_thread_raw_detach(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "thread::detach", "p", &handle);

	qd_thread* thread = (qd_thread*)handle;

	if (!thread || thread->joined || thread->detached) {
		return qd_fail(ctx, THREAD_ERR_DETACH, "thread::detach: invalid thread or already joined/detached");
	}

	int result = thrd_detach(thread->handle);
	if (result != thrd_success) {
		return qd_fail(ctx, THREAD_ERR_DETACH, "thread::detach: failed to detach thread");
	}

	thread->detached = true;
	free(thread);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// is_alive(thread:ptr -- alive:i64)
int usr_thread_raw_is_alive(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "thread::is_alive", "p", &handle);

	qd_thread* thread = (qd_thread*)handle;
	int alive = (thread && thread->started && !thread->joined && !thread->detached) ? 1 : 0;
	qd_push_i(ctx, alive);
	return (int){0};
}

// sleep(ms:i64 -- )
int usr_thread_raw_sleep(qd_context* ctx) {
	int64_t ms;
	qd_args(ctx, "thread::sleep", "i", &ms);

	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	thrd_sleep(&ts, NULL);

	return (int){0};
}

// new( -- mutex:ptr)!
int usr_thread_raw_mutex_new(qd_context* ctx) {
	qd_mutex* mutex = malloc(sizeof(qd_mutex));
	if (!mutex) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "mutex::new: failed to allocate mutex");
	}

	int result = mtx_init(&mutex->handle, mtx_plain);
	if (result != thrd_success) {
		free(mutex);
		return qd_fail(ctx, THREAD_ERR_MUTEX, "mutex::new: failed to initialize mutex");
	}

	mutex->initialized = true;
	qd_push_p(ctx, mutex);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// lock(mutex:ptr -- )!
int usr_thread_raw_mutex_lock(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "mutex::lock", "p", &handle);

	qd_mutex* mutex = (qd_mutex*)handle;

	if (!mutex || !mutex->initialized) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "mutex::lock: invalid mutex");
	}

	int result = mtx_lock(&mutex->handle);
	if (result != thrd_success) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "mutex::lock: failed to lock mutex");
	}

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// unlock(mutex:ptr -- )!
int usr_thread_raw_mutex_unlock(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "mutex::unlock", "p", &handle);

	qd_mutex* mutex = (qd_mutex*)handle;

	if (!mutex || !mutex->initialized) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "mutex::unlock: invalid mutex");
	}

	int result = mtx_unlock(&mutex->handle);
	if (result != thrd_success) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "mutex::unlock: failed to unlock mutex");
	}

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// try_lock(mutex:ptr -- success:i64)
int usr_thread_raw_mutex_try_lock(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "mutex::try_lock", "p", &handle);

	qd_mutex* mutex = (qd_mutex*)handle;

	if (!mutex || !mutex->initialized) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	int result = mtx_trylock(&mutex->handle);
	qd_push_i(ctx, (result == thrd_success) ? 1 : 0);
	return (int){0};
}

// free(mutex:ptr -- )
int usr_thread_raw_mutex_free(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "mutex::free", "p", &handle);

	qd_mutex* mutex = (qd_mutex*)handle;

	if (mutex && mutex->initialized) {
		mtx_destroy(&mutex->handle);
		mutex->initialized = false;
	}
	free(mutex);
	return (int){0};
}

// Helper: create channel with given capacity
static qd_channel* channel_create(size_t capacity) {
	qd_channel* ch = malloc(sizeof(qd_channel));
	if (!ch) {
		return NULL;
	}

	if (mtx_init(&ch->mutex, mtx_plain) != thrd_success) {
		free(ch);
		return NULL;
	}

	if (cnd_init(&ch->not_empty) != thrd_success) {
		mtx_destroy(&ch->mutex);
		free(ch);
		return NULL;
	}

	if (cnd_init(&ch->not_full) != thrd_success) {
		cnd_destroy(&ch->not_empty);
		mtx_destroy(&ch->mutex);
		free(ch);
		return NULL;
	}

	// For unbuffered channels, use capacity 1 internally
	size_t buf_size = (capacity == 0) ? 1 : capacity;
	ch->buffer = malloc(buf_size * sizeof(int64_t));
	if (!ch->buffer) {
		cnd_destroy(&ch->not_full);
		cnd_destroy(&ch->not_empty);
		mtx_destroy(&ch->mutex);
		free(ch);
		return NULL;
	}

	ch->capacity = capacity;
	ch->count = 0;
	ch->head = 0;
	ch->tail = 0;
	ch->sent = 0;
	ch->received = 0;
	ch->receivers_waiting = 0;
	ch->closed = false;

	return ch;
}

// new( -- ch:ptr)!
int usr_thread_raw_chan_new(qd_context* ctx) {
	qd_channel* ch = channel_create(0); // unbuffered
	if (!ch) {
		return qd_fail(ctx, THREAD_ERR_CHANNEL, "channel::new: failed to create channel");
	}

	qd_push_p(ctx, ch);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// buffered(capacity:i64 -- ch:ptr)!
int usr_thread_raw_chan_buffered(qd_context* ctx) {
	int64_t capacity;
	qd_args(ctx, "channel::buffered", "i", &capacity);

	if (capacity < 1) {
		capacity = 1;
	}

	qd_channel* ch = channel_create((size_t)capacity);
	if (!ch) {
		return qd_fail(ctx, THREAD_ERR_CHANNEL, "channel::buffered: failed to create channel");
	}

	qd_push_p(ctx, ch);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// send(val:i64 ch:ptr -- )!
int usr_thread_raw_chan_send(qd_context* ctx) {
	int64_t val;
	void* handle;
	qd_args(ctx, "channel::send", "ip", &val, &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		return qd_fail(ctx, THREAD_ERR_CHANNEL, "channel::send: invalid channel");
	}

	mtx_lock(&ch->mutex);

	if (ch->closed) {
		mtx_unlock(&ch->mutex);
		return qd_fail(ctx, THREAD_ERR_CLOSED, "channel::send: send on closed channel");
	}

	// Wait for room. An unbuffered channel holds the one value being handed over.
	size_t effective_cap = (ch->capacity == 0) ? 1 : ch->capacity;
	while (ch->count >= effective_cap && !ch->closed) {
		cnd_wait(&ch->not_full, &ch->mutex);
	}

	if (ch->closed) {
		mtx_unlock(&ch->mutex);
		return qd_fail(ctx, THREAD_ERR_CLOSED, "channel::send: send on closed channel");
	}

	ch->buffer[ch->tail] = val;
	ch->tail = (ch->tail + 1) % effective_cap;
	ch->count++;
	const size_t ticket = ++ch->sent;
	cnd_signal(&ch->not_empty);

	// Unbuffered: the send is the handover, so wait until a receiver has taken the value
	if (ch->capacity == 0) {
		while (ch->received < ticket && !ch->closed) {
			cnd_wait(&ch->not_full, &ch->mutex);
		}
		if (ch->received < ticket) {
			// Closed first. The value is still the one held, so take it back: a send that
			// fails must not deliver.
			ch->count = 0;
			ch->head = 0;
			ch->tail = 0;
			ch->sent--;
			mtx_unlock(&ch->mutex);
			return qd_fail(ctx, THREAD_ERR_CLOSED, "channel::send: channel closed before the value was received");
		}
	}
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// recv(ch:ptr -- val:i64)!
int usr_thread_raw_chan_recv(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "channel::recv", "p", &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		return qd_fail(ctx, THREAD_ERR_CHANNEL, "channel::recv: invalid channel");
	}

	mtx_lock(&ch->mutex);

	// Wait for a value
	ch->receivers_waiting++;
	while (ch->count == 0 && !ch->closed) {
		cnd_wait(&ch->not_empty, &ch->mutex);
	}
	ch->receivers_waiting--;

	// If closed and empty, return error
	if (ch->count == 0 && ch->closed) {
		mtx_unlock(&ch->mutex);
		return qd_fail(ctx, THREAD_ERR_CLOSED, "channel::recv: recv on closed empty channel");
	}

	// Get value from buffer
	size_t effective_cap = (ch->capacity == 0) ? 1 : ch->capacity;
	int64_t val = ch->buffer[ch->head];
	ch->head = (ch->head + 1) % effective_cap;
	ch->count--;
	ch->received++;

	cnd_broadcast(&ch->not_full);
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, val);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// try_send(val:i64 ch:ptr -- success:i64)
int usr_thread_raw_chan_try_send(qd_context* ctx) {
	int64_t val;
	void* handle;
	qd_args(ctx, "channel::try_send", "ip", &val, &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	mtx_lock(&ch->mutex);

	if (ch->closed) {
		mtx_unlock(&ch->mutex);
		qd_push_i(ctx, 0);
		return (int){0};
	}

	// Unbuffered, it would block unless a receiver is already waiting for it
	size_t effective_cap = (ch->capacity == 0) ? 1 : ch->capacity;
	if (ch->count >= effective_cap || (ch->capacity == 0 && ch->receivers_waiting == 0)) {
		mtx_unlock(&ch->mutex);
		qd_push_i(ctx, 0);
		return (int){0};
	}

	ch->buffer[ch->tail] = val;
	ch->tail = (ch->tail + 1) % effective_cap;
	ch->count++;
	ch->sent++;

	cnd_signal(&ch->not_empty);
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, 1);
	return (int){0};
}

// try_recv(ch:ptr -- val:i64 success:i64)
int usr_thread_raw_chan_try_recv(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "channel::try_recv", "p", &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		qd_push_i(ctx, 0);
		qd_push_i(ctx, 0);
		return (int){0};
	}

	mtx_lock(&ch->mutex);

	if (ch->count == 0) {
		mtx_unlock(&ch->mutex);
		qd_push_i(ctx, 0);
		qd_push_i(ctx, 0);
		return (int){0};
	}

	size_t effective_cap = (ch->capacity == 0) ? 1 : ch->capacity;
	int64_t val = ch->buffer[ch->head];
	ch->head = (ch->head + 1) % effective_cap;
	ch->count--;
	ch->received++;

	cnd_broadcast(&ch->not_full);
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, val);
	qd_push_i(ctx, 1);
	return (int){0};
}

// close(ch:ptr -- )
int usr_thread_raw_chan_close(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "channel::close", "p", &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		return (int){0};
	}

	mtx_lock(&ch->mutex);
	ch->closed = true;
	cnd_broadcast(&ch->not_empty);
	cnd_broadcast(&ch->not_full);
	mtx_unlock(&ch->mutex);

	return (int){0};
}

// is_closed(ch:ptr -- closed:i64)
int usr_thread_raw_chan_is_closed(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "channel::is_closed", "p", &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		qd_push_i(ctx, 1);
		return (int){0};
	}

	mtx_lock(&ch->mutex);
	int closed = ch->closed ? 1 : 0;
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, closed);
	return (int){0};
}

// len(ch:ptr -- len:i64)
int usr_thread_raw_chan_len(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "channel::len", "p", &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	mtx_lock(&ch->mutex);
	int64_t len = (int64_t)ch->count;
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, len);
	return (int){0};
}

// cap(ch:ptr -- cap:i64)
int usr_thread_raw_chan_cap(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "channel::cap", "p", &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_push_i(ctx, (int64_t)ch->capacity);
	return (int){0};
}

// free(ch:ptr -- )
int usr_thread_raw_chan_free(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "channel::free", "p", &handle);

	qd_channel* ch = (qd_channel*)handle;
	if (!ch) {
		return (int){0};
	}

	cnd_destroy(&ch->not_full);
	cnd_destroy(&ch->not_empty);
	mtx_destroy(&ch->mutex);
	free(ch->buffer);
	free(ch);

	return (int){0};
}

// new( -- wg:ptr)!
int usr_thread_raw_wg_new(qd_context* ctx) {
	qd_waitgroup* wg = malloc(sizeof(qd_waitgroup));
	if (!wg) {
		return qd_fail(ctx, THREAD_ERR_CREATE, "waitgroup::new: failed to allocate");
	}

	if (mtx_init(&wg->mutex, mtx_plain) != thrd_success) {
		free(wg);
		return qd_fail(ctx, THREAD_ERR_CREATE, "waitgroup::new: failed to init mutex");
	}

	if (cnd_init(&wg->done) != thrd_success) {
		mtx_destroy(&wg->mutex);
		free(wg);
		return qd_fail(ctx, THREAD_ERR_CREATE, "waitgroup::new: failed to init condvar");
	}

	wg->count = 0;
	qd_push_p(ctx, wg);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// add(n:i64 wg:ptr -- )
int usr_thread_raw_wg_add(qd_context* ctx) {
	int64_t n;
	void* handle;
	qd_args(ctx, "waitgroup::add", "ip", &n, &handle);

	qd_waitgroup* wg = (qd_waitgroup*)handle;
	if (!wg) {
		return (int){0};
	}

	mtx_lock(&wg->mutex);
	wg->count += (int)n;
	mtx_unlock(&wg->mutex);

	return (int){0};
}

// done(wg:ptr -- )
int usr_thread_raw_wg_done(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "waitgroup::done", "p", &handle);

	qd_waitgroup* wg = (qd_waitgroup*)handle;
	if (!wg) {
		return (int){0};
	}

	mtx_lock(&wg->mutex);
	wg->count--;
	if (wg->count <= 0) {
		cnd_broadcast(&wg->done);
	}
	mtx_unlock(&wg->mutex);

	return (int){0};
}

// wait(wg:ptr -- )!
int usr_thread_raw_wg_wait(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "waitgroup::wait", "p", &handle);

	qd_waitgroup* wg = (qd_waitgroup*)handle;
	if (!wg) {
		return qd_fail(ctx, THREAD_ERR_CREATE, "waitgroup::wait: invalid waitgroup");
	}

	mtx_lock(&wg->mutex);
	while (wg->count > 0) {
		cnd_wait(&wg->done, &wg->mutex);
	}
	mtx_unlock(&wg->mutex);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// free(wg:ptr -- )
int usr_thread_raw_wg_free(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "waitgroup::free", "p", &handle);

	qd_waitgroup* wg = (qd_waitgroup*)handle;
	if (!wg) {
		return (int){0};
	}

	cnd_destroy(&wg->done);
	mtx_destroy(&wg->mutex);
	free(wg);

	return (int){0};
}

// ============================================================================
// Once - run initialization code exactly once
// ============================================================================

// new( -- once:ptr)!
int usr_thread_raw_once_new(qd_context* ctx) {
	qd_once* once = malloc(sizeof(qd_once));
	if (!once) {
		return qd_fail(ctx, THREAD_ERR_CREATE, "once::new: failed to allocate");
	}

	if (mtx_init(&once->mutex, mtx_plain) != thrd_success) {
		free(once);
		return qd_fail(ctx, THREAD_ERR_CREATE, "once::new: failed to init mutex");
	}

	once->completed = false;

	qd_push_p(ctx, once);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// do(func:ptr once:ptr -- )
int usr_thread_raw_once_do(qd_context* ctx) {
	void* func_p;
	void* handle;
	qd_args(ctx, "once::do", "pp", &func_p, &handle);

	qd_once* once = (qd_once*)handle;
	if (!once) {
		return (int){0};
	}

	mtx_lock(&once->mutex);
	if (!once->completed) {
		once->completed = true;
		mtx_unlock(&once->mutex);

		// Call the function outside the lock (safe for reentrancy)
		typedef int (*qd_function_ptr)(qd_context*);
		qd_function_ptr func;
		memcpy(&func, &func_p, sizeof(func));
		if (func) {
			func(ctx);
		}
	} else {
		mtx_unlock(&once->mutex);
	}

	return (int){0};
}

// done(once:ptr -- done:i64)
int usr_thread_raw_once_done(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "once::done", "p", &handle);

	qd_once* once = (qd_once*)handle;
	if (!once) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_push_i(ctx, once->completed ? 1 : 0);
	return (int){0};
}

// free(once:ptr -- )
int usr_thread_raw_once_free(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "once::free", "p", &handle);

	qd_once* once = (qd_once*)handle;
	if (once) {
		mtx_destroy(&once->mutex);
	}
	free(once);

	return (int){0};
}

// ============================================================================
// Thread utilities
// ============================================================================

// self( -- id:i64)
int usr_thread_raw_self(qd_context* ctx) {
	thrd_t self = thrd_current();
	// Cast thread ID to int64 - this is implementation-defined but works for comparison
	qd_push_i(ctx, (int64_t)(uintptr_t)self);
	return (int){0};
}

// yield( -- )
int usr_thread_raw_yield(qd_context* ctx) {
	(void)ctx;
	thrd_yield();
	return (int){0};
}

// cpu_count( -- count:i64)
int usr_thread_raw_cpu_count(qd_context* ctx) {
	long count = sysconf(_SC_NPROCESSORS_ONLN);
	if (count < 1) {
		count = 1;
	}
	qd_push_i(ctx, (int64_t)count);
	return (int){0};
}

// ============================================================================
// Barrier - synchronize threads at a point
// ============================================================================

// new(n:i64 -- barrier:ptr)!
int usr_thread_raw_barrier_new(qd_context* ctx) {
	int64_t n;
	qd_args(ctx, "barrier::new", "i", &n);

	if (n < 1) {
		n = 1;
	}

	qd_barrier* barrier = malloc(sizeof(qd_barrier));
	if (!barrier) {
		return qd_fail(ctx, THREAD_ERR_CREATE, "barrier::new: failed to allocate");
	}

	if (mtx_init(&barrier->mutex, mtx_plain) != thrd_success) {
		free(barrier);
		return qd_fail(ctx, THREAD_ERR_CREATE, "barrier::new: failed to init mutex");
	}

	if (cnd_init(&barrier->cond) != thrd_success) {
		mtx_destroy(&barrier->mutex);
		free(barrier);
		return qd_fail(ctx, THREAD_ERR_CREATE, "barrier::new: failed to init condvar");
	}

	barrier->threshold = (int)n;
	barrier->count = 0;
	barrier->generation = 0;

	qd_push_p(ctx, barrier);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// wait(barrier:ptr -- is_serial:i64)
int usr_thread_raw_barrier_wait(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "barrier::wait", "p", &handle);

	qd_barrier* barrier = (qd_barrier*)handle;
	if (!barrier) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	mtx_lock(&barrier->mutex);

	int my_generation = barrier->generation;
	barrier->count++;

	if (barrier->count >= barrier->threshold) {
		// Last thread to arrive - wake everyone and return 1 (serial thread)
		barrier->generation++;
		barrier->count = 0;
		cnd_broadcast(&barrier->cond);
		mtx_unlock(&barrier->mutex);
		qd_push_i(ctx, 1); // This is the serial thread
		return (int){0};
	}

	// Wait for barrier to trip
	while (my_generation == barrier->generation) {
		cnd_wait(&barrier->cond, &barrier->mutex);
	}

	mtx_unlock(&barrier->mutex);
	qd_push_i(ctx, 0); // Not the serial thread
	return (int){0};
}

// free(barrier:ptr -- )
int usr_thread_raw_barrier_free(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "barrier::free", "p", &handle);

	qd_barrier* barrier = (qd_barrier*)handle;
	if (!barrier) {
		return (int){0};
	}

	cnd_destroy(&barrier->cond);
	mtx_destroy(&barrier->mutex);
	free(barrier);

	return (int){0};
}

// ============================================================================
// RwLock - read-write lock (multiple readers, single writer)
// ============================================================================

// new( -- rwlock:ptr)!
int usr_thread_raw_rwlock_new(qd_context* ctx) {
	qd_rwlock* rw = malloc(sizeof(qd_rwlock));
	if (!rw) {
		return qd_fail(ctx, THREAD_ERR_CREATE, "rwlock::new: failed to allocate");
	}

	if (mtx_init(&rw->mutex, mtx_plain) != thrd_success) {
		free(rw);
		return qd_fail(ctx, THREAD_ERR_CREATE, "rwlock::new: failed to init mutex");
	}

	if (cnd_init(&rw->read_cond) != thrd_success) {
		mtx_destroy(&rw->mutex);
		free(rw);
		return qd_fail(ctx, THREAD_ERR_CREATE, "rwlock::new: failed to init read condvar");
	}

	if (cnd_init(&rw->write_cond) != thrd_success) {
		cnd_destroy(&rw->read_cond);
		mtx_destroy(&rw->mutex);
		free(rw);
		return qd_fail(ctx, THREAD_ERR_CREATE, "rwlock::new: failed to init write condvar");
	}

	rw->readers = 0;
	rw->writers_waiting = 0;
	rw->writer_active = false;

	qd_push_p(ctx, rw);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// read_lock(rwlock:ptr -- )!
int usr_thread_raw_rwlock_read_lock(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "rwlock::read_lock", "p", &handle);

	qd_rwlock* rw = (qd_rwlock*)handle;
	if (!rw) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "rwlock::read_lock: invalid rwlock");
	}

	mtx_lock(&rw->mutex);

	// Wait while a writer is active or writers are waiting (prefer writers)
	while (rw->writer_active || rw->writers_waiting > 0) {
		cnd_wait(&rw->read_cond, &rw->mutex);
	}

	rw->readers++;
	mtx_unlock(&rw->mutex);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// read_unlock(rwlock:ptr -- )!
int usr_thread_raw_rwlock_read_unlock(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "rwlock::read_unlock", "p", &handle);

	qd_rwlock* rw = (qd_rwlock*)handle;
	if (!rw) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "rwlock::read_unlock: invalid rwlock");
	}

	mtx_lock(&rw->mutex);
	rw->readers--;

	// If no more readers, signal waiting writers
	if (rw->readers == 0 && rw->writers_waiting > 0) {
		cnd_signal(&rw->write_cond);
	}

	mtx_unlock(&rw->mutex);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// write_lock(rwlock:ptr -- )!
int usr_thread_raw_rwlock_write_lock(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "rwlock::write_lock", "p", &handle);

	qd_rwlock* rw = (qd_rwlock*)handle;
	if (!rw) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "rwlock::write_lock: invalid rwlock");
	}

	mtx_lock(&rw->mutex);
	rw->writers_waiting++;

	// Wait while readers are active or another writer is active
	while (rw->readers > 0 || rw->writer_active) {
		cnd_wait(&rw->write_cond, &rw->mutex);
	}

	rw->writers_waiting--;
	rw->writer_active = true;
	mtx_unlock(&rw->mutex);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// write_unlock(rwlock:ptr -- )!
int usr_thread_raw_rwlock_write_unlock(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "rwlock::write_unlock", "p", &handle);

	qd_rwlock* rw = (qd_rwlock*)handle;
	if (!rw) {
		return qd_fail(ctx, THREAD_ERR_MUTEX, "rwlock::write_unlock: invalid rwlock");
	}

	mtx_lock(&rw->mutex);
	rw->writer_active = false;

	if (rw->writers_waiting > 0) {
		// Prefer writers
		cnd_signal(&rw->write_cond);
	} else {
		// Wake all readers
		cnd_broadcast(&rw->read_cond);
	}

	mtx_unlock(&rw->mutex);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// try_read(rwlock:ptr -- success:i64)
int usr_thread_raw_rwlock_try_read(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "rwlock::try_read", "p", &handle);

	qd_rwlock* rw = (qd_rwlock*)handle;
	if (!rw) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	mtx_lock(&rw->mutex);

	if (rw->writer_active || rw->writers_waiting > 0) {
		mtx_unlock(&rw->mutex);
		qd_push_i(ctx, 0);
		return (int){0};
	}

	rw->readers++;
	mtx_unlock(&rw->mutex);
	qd_push_i(ctx, 1);
	return (int){0};
}

// try_write(rwlock:ptr -- success:i64)
int usr_thread_raw_rwlock_try_write(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "rwlock::try_write", "p", &handle);

	qd_rwlock* rw = (qd_rwlock*)handle;
	if (!rw) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	mtx_lock(&rw->mutex);

	if (rw->readers > 0 || rw->writer_active) {
		mtx_unlock(&rw->mutex);
		qd_push_i(ctx, 0);
		return (int){0};
	}

	rw->writer_active = true;
	mtx_unlock(&rw->mutex);
	qd_push_i(ctx, 1);
	return (int){0};
}

// free(rwlock:ptr -- )
int usr_thread_raw_rwlock_free(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "rwlock::free", "p", &handle);

	qd_rwlock* rw = (qd_rwlock*)handle;
	if (!rw) {
		return (int){0};
	}

	cnd_destroy(&rw->write_cond);
	cnd_destroy(&rw->read_cond);
	mtx_destroy(&rw->mutex);
	free(rw);

	return (int){0};
}

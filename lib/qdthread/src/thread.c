// Thread module implementation using C11 threads
#include "qdthread/thread.h"

#include <qdrt/context.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for sysconf

// Helper to push a stack element based on its type
static void push_element(qd_context* ctx, qd_stack_element_t elem) {
	switch (elem.type) {
	case QD_STACK_TYPE_INT:
		qd_push_i(ctx, elem.value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		qd_push_f(ctx, elem.value.f);
		break;
	case QD_STACK_TYPE_PTR:
		qd_push_p(ctx, elem.value.p);
		break;
	case QD_STACK_TYPE_STR:
		qd_push_s_ref(ctx, elem.value.s);
		break;
	default:
		qd_push_i(ctx, 0);
		break;
	}
}


typedef struct {
	qd_stack_element_t func;
} qd_thread_spawn_data;

static int thread_entry(void* arg) {
	qd_thread_spawn_data* data = (qd_thread_spawn_data*)arg;

	qd_context* thread_ctx = qd_create_context(1024);
	if (!thread_ctx) {
		free(data);
		return 1;
	}

	// Cast and call the function pointer
	// Functions in Quadrate have signature: int (*)(qd_context*)
	typedef int (*qd_function_ptr)(qd_context*);
	qd_function_ptr func;
	memcpy(&func, &data->func.value.p, sizeof(func));

	if (func) {
		func(thread_ctx);
	}

	qd_free_context(thread_ctx);
	free(data);
	return 0;
}

// spawn(fn -- thread:ptr)!
int usr_thread_raw_spawn(qd_context* ctx) {
	qd_stack_element_t func_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &func_elem);

	if (err != QD_STACK_OK) {
		qd_set_error_msg(ctx, "thread::spawn: expected function");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	qd_thread* thread = malloc(sizeof(qd_thread));
	if (!thread) {
		qd_set_error_msg(ctx, "thread::spawn: failed to allocate thread");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	qd_thread_spawn_data* data = malloc(sizeof(qd_thread_spawn_data));
	if (!data) {
		free(thread);
		qd_set_error_msg(ctx, "thread::spawn: failed to allocate thread data");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	data->func = func_elem;

	int result = thrd_create(&thread->handle, thread_entry, data);
	if (result != thrd_success) {
		free(data);
		free(thread);
		qd_set_error_msg(ctx, "thread::spawn: failed to create thread");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "thread::join: expected thread pointer");
		ctx->error_code = THREAD_ERR_JOIN;
		return (int){THREAD_ERR_JOIN};
	}

	qd_thread* thread = (qd_thread*)elem.value.p;

	if (!thread || thread->joined || thread->detached) {
		qd_set_error_msg(ctx, "thread::join: invalid thread or already joined/detached");
		ctx->error_code = THREAD_ERR_JOIN;
		return (int){THREAD_ERR_JOIN};
	}

	int result = thrd_join(thread->handle, NULL);
	if (result != thrd_success) {
		qd_set_error_msg(ctx, "thread::join: failed to join thread");
		ctx->error_code = THREAD_ERR_JOIN;
		return (int){THREAD_ERR_JOIN};
	}

	thread->joined = true;
	free(thread);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// detach(thread:ptr -- )!
int usr_thread_raw_detach(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "thread::detach: expected thread pointer");
		ctx->error_code = THREAD_ERR_DETACH;
		return (int){THREAD_ERR_DETACH};
	}

	qd_thread* thread = (qd_thread*)elem.value.p;

	if (!thread || thread->joined || thread->detached) {
		qd_set_error_msg(ctx, "thread::detach: invalid thread or already joined/detached");
		ctx->error_code = THREAD_ERR_DETACH;
		return (int){THREAD_ERR_DETACH};
	}

	int result = thrd_detach(thread->handle);
	if (result != thrd_success) {
		qd_set_error_msg(ctx, "thread::detach: failed to detach thread");
		ctx->error_code = THREAD_ERR_DETACH;
		return (int){THREAD_ERR_DETACH};
	}

	thread->detached = true;
	free(thread);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// is_alive(thread:ptr -- alive:i64)
int usr_thread_raw_is_alive(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_thread* thread = (qd_thread*)elem.value.p;
	int alive = (thread && thread->started && !thread->joined && !thread->detached) ? 1 : 0;
	qd_push_i(ctx, alive);
	return (int){0};
}

// sleep(ms:i64 -- )
int usr_thread_raw_sleep(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_INT) {
		return (int){0};
	}

	int64_t ms = elem.value.i;
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
		qd_set_error_msg(ctx, "mutex::new: failed to allocate mutex");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	int result = mtx_init(&mutex->handle, mtx_plain);
	if (result != thrd_success) {
		free(mutex);
		qd_set_error_msg(ctx, "mutex::new: failed to initialize mutex");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	mutex->initialized = true;
	qd_push_p(ctx, mutex);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// lock(mutex:ptr -- )!
int usr_thread_raw_mutex_lock(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "mutex::lock: expected mutex pointer");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_mutex* mutex = (qd_mutex*)elem.value.p;

	if (!mutex || !mutex->initialized) {
		qd_set_error_msg(ctx, "mutex::lock: invalid mutex");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	int result = mtx_lock(&mutex->handle);
	if (result != thrd_success) {
		qd_set_error_msg(ctx, "mutex::lock: failed to lock mutex");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// unlock(mutex:ptr -- )!
int usr_thread_raw_mutex_unlock(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "mutex::unlock: expected mutex pointer");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_mutex* mutex = (qd_mutex*)elem.value.p;

	if (!mutex || !mutex->initialized) {
		qd_set_error_msg(ctx, "mutex::unlock: invalid mutex");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	int result = mtx_unlock(&mutex->handle);
	if (result != thrd_success) {
		qd_set_error_msg(ctx, "mutex::unlock: failed to unlock mutex");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// try_lock(mutex:ptr -- success:i64)
int usr_thread_raw_mutex_try_lock(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_mutex* mutex = (qd_mutex*)elem.value.p;

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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_mutex* mutex = (qd_mutex*)elem.value.p;

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
	if (!ch) return NULL;

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
	ch->buffer = malloc(buf_size * sizeof(qd_stack_element_t));
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
	ch->closed = false;
	ch->refcount = 1;

	return ch;
}

// new( -- ch:ptr)!
int usr_thread_raw_chan_new(qd_context* ctx) {
	qd_channel* ch = channel_create(0);  // unbuffered
	if (!ch) {
		qd_set_error_msg(ctx, "channel::new: failed to create channel");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	qd_push_p(ctx, ch);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// buffered(capacity:i64 -- ch:ptr)!
int usr_thread_raw_chan_buffered(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_INT) {
		qd_set_error_msg(ctx, "channel::buffered: expected capacity");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	int64_t capacity = elem.value.i;
	if (capacity < 1) capacity = 1;

	qd_channel* ch = channel_create((size_t)capacity);
	if (!ch) {
		qd_set_error_msg(ctx, "channel::buffered: failed to create channel");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	qd_push_p(ctx, ch);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// send(val ch:ptr -- )!
int usr_thread_raw_chan_send(qd_context* ctx) {
	qd_stack_element_t ch_elem, val_elem;
	qd_stack_error err;

	err = qd_stack_pop(ctx->st, &ch_elem);
	if (err != QD_STACK_OK || ch_elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "channel::send: expected channel pointer");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	err = qd_stack_pop(ctx->st, &val_elem);
	if (err != QD_STACK_OK) {
		qd_set_error_msg(ctx, "channel::send: expected value");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	qd_channel* ch = (qd_channel*)ch_elem.value.p;
	if (!ch) {
		qd_set_error_msg(ctx, "channel::send: invalid channel");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	mtx_lock(&ch->mutex);

	if (ch->closed) {
		mtx_unlock(&ch->mutex);
		qd_set_error_msg(ctx, "channel::send: send on closed channel");
		ctx->error_code = THREAD_ERR_CLOSED;
		return (int){THREAD_ERR_CLOSED};
	}

	// For unbuffered: wait until someone is ready to receive
	// For buffered: wait until there's space
	size_t effective_cap = (ch->capacity == 0) ? 1 : ch->capacity;
	while (ch->count >= effective_cap && !ch->closed) {
		cnd_wait(&ch->not_full, &ch->mutex);
	}

	if (ch->closed) {
		mtx_unlock(&ch->mutex);
		qd_set_error_msg(ctx, "channel::send: send on closed channel");
		ctx->error_code = THREAD_ERR_CLOSED;
		return (int){THREAD_ERR_CLOSED};
	}

	// Copy value to buffer
	ch->buffer[ch->tail] = val_elem;
	ch->tail = (ch->tail + 1) % effective_cap;
	ch->count++;

	cnd_signal(&ch->not_empty);
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// recv(ch:ptr -- val)!
int usr_thread_raw_chan_recv(qd_context* ctx) {
	qd_stack_element_t ch_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &ch_elem);

	if (err != QD_STACK_OK || ch_elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "channel::recv: expected channel pointer");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	qd_channel* ch = (qd_channel*)ch_elem.value.p;
	if (!ch) {
		qd_set_error_msg(ctx, "channel::recv: invalid channel");
		ctx->error_code = THREAD_ERR_CHANNEL;
		return (int){THREAD_ERR_CHANNEL};
	}

	mtx_lock(&ch->mutex);

	// Wait for a value
	while (ch->count == 0 && !ch->closed) {
		cnd_wait(&ch->not_empty, &ch->mutex);
	}

	// If closed and empty, return error
	if (ch->count == 0 && ch->closed) {
		mtx_unlock(&ch->mutex);
		qd_set_error_msg(ctx, "channel::recv: recv on closed empty channel");
		ctx->error_code = THREAD_ERR_CLOSED;
		return (int){THREAD_ERR_CLOSED};
	}

	// Get value from buffer
	size_t effective_cap = (ch->capacity == 0) ? 1 : ch->capacity;
	qd_stack_element_t val = ch->buffer[ch->head];
	ch->head = (ch->head + 1) % effective_cap;
	ch->count--;

	cnd_signal(&ch->not_full);
	mtx_unlock(&ch->mutex);

	push_element(ctx, val);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// try_send(val ch:ptr -- success:i64)
int usr_thread_raw_chan_try_send(qd_context* ctx) {
	qd_stack_element_t ch_elem, val_elem;
	qd_stack_error err;

	err = qd_stack_pop(ctx->st, &ch_elem);
	if (err != QD_STACK_OK || ch_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	err = qd_stack_pop(ctx->st, &val_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_channel* ch = (qd_channel*)ch_elem.value.p;
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

	size_t effective_cap = (ch->capacity == 0) ? 1 : ch->capacity;
	if (ch->count >= effective_cap) {
		mtx_unlock(&ch->mutex);
		qd_push_i(ctx, 0);
		return (int){0};
	}

	ch->buffer[ch->tail] = val_elem;
	ch->tail = (ch->tail + 1) % effective_cap;
	ch->count++;

	cnd_signal(&ch->not_empty);
	mtx_unlock(&ch->mutex);

	qd_push_i(ctx, 1);
	return (int){0};
}

// try_recv(ch:ptr -- val success:i64)
int usr_thread_raw_chan_try_recv(qd_context* ctx) {
	qd_stack_element_t ch_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &ch_elem);

	if (err != QD_STACK_OK || ch_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);  // dummy value
		qd_push_i(ctx, 0);  // success = false
		return (int){0};
	}

	qd_channel* ch = (qd_channel*)ch_elem.value.p;
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
	qd_stack_element_t val = ch->buffer[ch->head];
	ch->head = (ch->head + 1) % effective_cap;
	ch->count--;

	cnd_signal(&ch->not_full);
	mtx_unlock(&ch->mutex);

	push_element(ctx, val);
	qd_push_i(ctx, 1);
	return (int){0};
}

// close(ch:ptr -- )
int usr_thread_raw_chan_close(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_channel* ch = (qd_channel*)elem.value.p;
	if (!ch) return (int){0};

	mtx_lock(&ch->mutex);
	ch->closed = true;
	cnd_broadcast(&ch->not_empty);
	cnd_broadcast(&ch->not_full);
	mtx_unlock(&ch->mutex);

	return (int){0};
}

// is_closed(ch:ptr -- closed:i64)
int usr_thread_raw_chan_is_closed(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 1);
		return (int){0};
	}

	qd_channel* ch = (qd_channel*)elem.value.p;
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_channel* ch = (qd_channel*)elem.value.p;
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_channel* ch = (qd_channel*)elem.value.p;
	if (!ch) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_push_i(ctx, (int64_t)ch->capacity);
	return (int){0};
}

// free(ch:ptr -- )
int usr_thread_raw_chan_free(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_channel* ch = (qd_channel*)elem.value.p;
	if (!ch) return (int){0};

	mtx_lock(&ch->mutex);
	ch->refcount--;
	if (ch->refcount > 0) {
		mtx_unlock(&ch->mutex);
		return (int){0};
	}
	mtx_unlock(&ch->mutex);

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
		qd_set_error_msg(ctx, "waitgroup::new: failed to allocate");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	if (mtx_init(&wg->mutex, mtx_plain) != thrd_success) {
		free(wg);
		qd_set_error_msg(ctx, "waitgroup::new: failed to init mutex");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	if (cnd_init(&wg->done) != thrd_success) {
		mtx_destroy(&wg->mutex);
		free(wg);
		qd_set_error_msg(ctx, "waitgroup::new: failed to init condvar");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	wg->count = 0;
	qd_push_p(ctx, wg);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// add(n:i64 wg:ptr -- )
int usr_thread_raw_wg_add(qd_context* ctx) {
	qd_stack_element_t wg_elem, n_elem;
	qd_stack_error err;

	err = qd_stack_pop(ctx->st, &wg_elem);
	if (err != QD_STACK_OK || wg_elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	err = qd_stack_pop(ctx->st, &n_elem);
	if (err != QD_STACK_OK || n_elem.type != QD_STACK_TYPE_INT) {
		return (int){0};
	}

	qd_waitgroup* wg = (qd_waitgroup*)wg_elem.value.p;
	if (!wg) return (int){0};

	mtx_lock(&wg->mutex);
	wg->count += (int)n_elem.value.i;
	mtx_unlock(&wg->mutex);

	return (int){0};
}

// done(wg:ptr -- )
int usr_thread_raw_wg_done(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_waitgroup* wg = (qd_waitgroup*)elem.value.p;
	if (!wg) return (int){0};

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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "waitgroup::wait: expected pointer");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	qd_waitgroup* wg = (qd_waitgroup*)elem.value.p;
	if (!wg) {
		qd_set_error_msg(ctx, "waitgroup::wait: invalid waitgroup");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_waitgroup* wg = (qd_waitgroup*)elem.value.p;
	if (!wg) return (int){0};

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
		qd_set_error_msg(ctx, "once::new: failed to allocate");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	// Initialize the once_flag to ONCE_FLAG_INIT
	once_flag init = ONCE_FLAG_INIT;
	once->flag = init;
	once->func = NULL;
	once->completed = false;

	qd_push_p(ctx, once);
	qd_push_i(ctx, THREAD_OK);
	return (int){0};
}

// Thread-local storage for the function to call during call_once
static _Thread_local qd_context* tls_ctx = NULL;
static _Thread_local qd_stack_element_t tls_func;

static void once_callback(void) {
	if (tls_ctx && tls_func.type == QD_STACK_TYPE_PTR) {
		typedef int (*qd_function_ptr)(qd_context*);
		qd_function_ptr func;
		memcpy(&func, &tls_func.value.p, sizeof(func));
		if (func) {
			func(tls_ctx);
		}
	}
}

// do(func:ptr once:ptr -- )
int usr_thread_raw_once_do(qd_context* ctx) {
	qd_stack_element_t once_elem, func_elem;
	qd_stack_error err;

	err = qd_stack_pop(ctx->st, &once_elem);
	if (err != QD_STACK_OK || once_elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	err = qd_stack_pop(ctx->st, &func_elem);
	if (err != QD_STACK_OK || func_elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_once* once = (qd_once*)once_elem.value.p;
	if (!once) return (int){0};

	// Set up thread-local storage for the callback
	tls_ctx = ctx;
	tls_func = func_elem;

	// call_once ensures the function runs exactly once
	call_once(&once->flag, once_callback);
	once->completed = true;

	return (int){0};
}

// done(once:ptr -- done:i64)
int usr_thread_raw_once_done(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_once* once = (qd_once*)elem.value.p;
	if (!once) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_push_i(ctx, once->completed ? 1 : 0);
	return (int){0};
}

// free(once:ptr -- )
int usr_thread_raw_once_free(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_once* once = (qd_once*)elem.value.p;
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
	if (count < 1) count = 1;
	qd_push_i(ctx, (int64_t)count);
	return (int){0};
}


// ============================================================================
// Barrier - synchronize threads at a point
// ============================================================================

// new(n:i64 -- barrier:ptr)!
int usr_thread_raw_barrier_new(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_INT) {
		qd_set_error_msg(ctx, "barrier::new: expected count");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	int64_t n = elem.value.i;
	if (n < 1) n = 1;

	qd_barrier* barrier = malloc(sizeof(qd_barrier));
	if (!barrier) {
		qd_set_error_msg(ctx, "barrier::new: failed to allocate");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	if (mtx_init(&barrier->mutex, mtx_plain) != thrd_success) {
		free(barrier);
		qd_set_error_msg(ctx, "barrier::new: failed to init mutex");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	if (cnd_init(&barrier->cond) != thrd_success) {
		mtx_destroy(&barrier->mutex);
		free(barrier);
		qd_set_error_msg(ctx, "barrier::new: failed to init condvar");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_barrier* barrier = (qd_barrier*)elem.value.p;
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
		qd_push_i(ctx, 1);  // This is the serial thread
		return (int){0};
	}

	// Wait for barrier to trip
	while (my_generation == barrier->generation) {
		cnd_wait(&barrier->cond, &barrier->mutex);
	}

	mtx_unlock(&barrier->mutex);
	qd_push_i(ctx, 0);  // Not the serial thread
	return (int){0};
}

// free(barrier:ptr -- )
int usr_thread_raw_barrier_free(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_barrier* barrier = (qd_barrier*)elem.value.p;
	if (!barrier) return (int){0};

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
		qd_set_error_msg(ctx, "rwlock::new: failed to allocate");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	if (mtx_init(&rw->mutex, mtx_plain) != thrd_success) {
		free(rw);
		qd_set_error_msg(ctx, "rwlock::new: failed to init mutex");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	if (cnd_init(&rw->read_cond) != thrd_success) {
		mtx_destroy(&rw->mutex);
		free(rw);
		qd_set_error_msg(ctx, "rwlock::new: failed to init read condvar");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
	}

	if (cnd_init(&rw->write_cond) != thrd_success) {
		cnd_destroy(&rw->read_cond);
		mtx_destroy(&rw->mutex);
		free(rw);
		qd_set_error_msg(ctx, "rwlock::new: failed to init write condvar");
		ctx->error_code = THREAD_ERR_CREATE;
		return (int){THREAD_ERR_CREATE};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "rwlock::read_lock: expected rwlock pointer");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_rwlock* rw = (qd_rwlock*)elem.value.p;
	if (!rw) {
		qd_set_error_msg(ctx, "rwlock::read_lock: invalid rwlock");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "rwlock::read_unlock: expected rwlock pointer");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_rwlock* rw = (qd_rwlock*)elem.value.p;
	if (!rw) {
		qd_set_error_msg(ctx, "rwlock::read_unlock: invalid rwlock");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "rwlock::write_lock: expected rwlock pointer");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_rwlock* rw = (qd_rwlock*)elem.value.p;
	if (!rw) {
		qd_set_error_msg(ctx, "rwlock::write_lock: invalid rwlock");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_set_error_msg(ctx, "rwlock::write_unlock: expected rwlock pointer");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
	}

	qd_rwlock* rw = (qd_rwlock*)elem.value.p;
	if (!rw) {
		qd_set_error_msg(ctx, "rwlock::write_unlock: invalid rwlock");
		ctx->error_code = THREAD_ERR_MUTEX;
		return (int){THREAD_ERR_MUTEX};
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_rwlock* rw = (qd_rwlock*)elem.value.p;
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (int){0};
	}

	qd_rwlock* rw = (qd_rwlock*)elem.value.p;
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
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);

	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		return (int){0};
	}

	qd_rwlock* rw = (qd_rwlock*)elem.value.p;
	if (!rw) return (int){0};

	cnd_destroy(&rw->write_cond);
	cnd_destroy(&rw->read_cond);
	mtx_destroy(&rw->mutex);
	free(rw);

	return (int){0};
}

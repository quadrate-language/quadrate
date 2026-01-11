#ifndef QDTHREAD_THREAD_H
#define QDTHREAD_THREAD_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <threads.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error codes
#define THREAD_OK 1
#define THREAD_ERR_CREATE 2
#define THREAD_ERR_JOIN 3
#define THREAD_ERR_DETACH 4
#define THREAD_ERR_MUTEX 5
#define THREAD_ERR_CHANNEL 6
#define THREAD_ERR_CLOSED 7
#define THREAD_ERR_TIMEOUT 8

typedef struct qd_thread {
	thrd_t handle;
	bool started;
	bool joined;
	bool detached;
} qd_thread;

// Thread functions exposed to Quadrate (raw_ prefix to avoid collision with Quadrate wrappers)
qd_exec_result usr_thread_raw_spawn(qd_context* ctx);	 // (fn -- thread:ptr)!
qd_exec_result usr_thread_raw_join(qd_context* ctx);	 // (thread:ptr -- )!
qd_exec_result usr_thread_raw_detach(qd_context* ctx);	 // (thread:ptr -- )!
qd_exec_result usr_thread_raw_is_alive(qd_context* ctx); // (thread:ptr -- alive:i64)
qd_exec_result usr_thread_raw_sleep(qd_context* ctx);	 // (ms:i64 -- )

typedef struct qd_mutex {
	mtx_t handle;
	bool initialized;
} qd_mutex;

// Mutex functions exposed to Quadrate (raw_ prefix to avoid collision with Quadrate wrappers)
qd_exec_result usr_thread_raw_mutex_new(qd_context* ctx);	   // ( -- mutex:ptr)!
qd_exec_result usr_thread_raw_mutex_lock(qd_context* ctx);	   // (mutex:ptr -- )!
qd_exec_result usr_thread_raw_mutex_unlock(qd_context* ctx);   // (mutex:ptr -- )!
qd_exec_result usr_thread_raw_mutex_try_lock(qd_context* ctx); // (mutex:ptr -- success:i64)
qd_exec_result usr_thread_raw_mutex_free(qd_context* ctx);	   // (mutex:ptr -- )

typedef struct qd_channel {
	mtx_t mutex;
	cnd_t not_empty;
	cnd_t not_full;
	qd_stack_element_t* buffer;
	size_t capacity; // 0 = unbuffered (synchronous)
	size_t count;
	size_t head;
	size_t tail;
	bool closed;
	int refcount;
} qd_channel;

// Channel functions exposed to Quadrate (raw_ prefix to avoid collision with Quadrate wrappers)
qd_exec_result usr_thread_raw_chan_new(qd_context* ctx);	   // ( -- ch:ptr)!
qd_exec_result usr_thread_raw_chan_buffered(qd_context* ctx);  // (capacity:i64 -- ch:ptr)!
qd_exec_result usr_thread_raw_chan_send(qd_context* ctx);	   // (val ch:ptr -- )!
qd_exec_result usr_thread_raw_chan_recv(qd_context* ctx);	   // (ch:ptr -- val)!
qd_exec_result usr_thread_raw_chan_try_send(qd_context* ctx);  // (val ch:ptr -- success:i64)
qd_exec_result usr_thread_raw_chan_try_recv(qd_context* ctx);  // (ch:ptr -- val success:i64)
qd_exec_result usr_thread_raw_chan_close(qd_context* ctx);	   // (ch:ptr -- )
qd_exec_result usr_thread_raw_chan_is_closed(qd_context* ctx); // (ch:ptr -- closed:i64)
qd_exec_result usr_thread_raw_chan_len(qd_context* ctx);	   // (ch:ptr -- len:i64)
qd_exec_result usr_thread_raw_chan_cap(qd_context* ctx);	   // (ch:ptr -- cap:i64)
qd_exec_result usr_thread_raw_chan_free(qd_context* ctx);	   // (ch:ptr -- )

typedef struct qd_waitgroup {
	mtx_t mutex;
	cnd_t done;
	int count;
} qd_waitgroup;

// WaitGroup functions exposed to Quadrate (raw_ prefix to avoid collision with Quadrate wrappers)
qd_exec_result usr_thread_raw_wg_new(qd_context* ctx);	// ( -- wg:ptr)!
qd_exec_result usr_thread_raw_wg_add(qd_context* ctx);	// (n:i64 wg:ptr -- )
qd_exec_result usr_thread_raw_wg_done(qd_context* ctx); // (wg:ptr -- )
qd_exec_result usr_thread_raw_wg_wait(qd_context* ctx); // (wg:ptr -- )!
qd_exec_result usr_thread_raw_wg_free(qd_context* ctx); // (wg:ptr -- )

#ifdef __cplusplus
}
#endif

#endif // QDTHREAD_THREAD_H

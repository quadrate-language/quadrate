// Quadrate wrapper functions for qdlog

#define _POSIX_C_SOURCE 200809L
#include "../include/quadrate/log/log.h"
#include <quadrate/rt/context.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/qd_string.h>
#include <stdlib.h>
#include <string.h>

// Helper to set error message
static void set_error_msg(qd_context* ctx, const char* msg) {
	if (ctx->error_msg) free(ctx->error_msg);
	ctx->error_msg = strdup(msg);
}

// Pop a pointer argument, validating both the pop and the element type so we
// never dereference an uninitialized or wrongly-typed stack slot.
static int pop_ptr_arg(qd_context* ctx, void** out) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK || elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "log: expected pointer argument");
		ctx->error_code = -1;
		return -1;
	}
	*out = elem.value.p;
	return 0;
}

// Pop an integer argument with the same validation.
static int pop_int_arg(qd_context* ctx, int64_t* out) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK || elem.type != QD_STACK_TYPE_INT) {
		set_error_msg(ctx, "log: expected integer argument");
		ctx->error_code = -1;
		return -1;
	}
	*out = elem.value.i;
	return 0;
}

// Pop a string argument. On success the caller owns a reference to *out and
// must release it. Returns -1 (and releases nothing) on bad input.
static int pop_str_arg(qd_context* ctx, qd_string_t** out) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "log: expected string argument");
		ctx->error_code = -1;
		return -1;
	}
	*out = elem.value.s;
	return 0;
}

// raw_new( -- logger:ptr)
int usr_log_raw_new(qd_context* ctx) {
	qdlog_logger_t* logger = qdlog_new();
	if (!logger) {
		set_error_msg(ctx, "log::new: failed to create logger");
		ctx->error_code = QDLOG_ERR_ALLOC;
		return (int){QDLOG_ERR_ALLOC};
	}
	qd_push_p(ctx, logger);
	return 0;
}

// raw_free(logger:ptr -- )
int usr_log_raw_free(qd_context* ctx) {
	void* logger;
	if (pop_ptr_arg(ctx, &logger) != 0) {
		return -1;
	}
	qdlog_free((qdlog_logger_t*)logger);
	return 0;
}

// raw_set_level(level:i64 logger:ptr -- )
int usr_log_raw_set_level(qd_context* ctx) {
	void* logger;
	int64_t level;
	if (pop_ptr_arg(ctx, &logger) != 0 || pop_int_arg(ctx, &level) != 0) {
		return -1;
	}
	qdlog_set_level((qdlog_logger_t*)logger, (qdlog_level_t)level);
	return 0;
}

// raw_get_level(logger:ptr -- level:i64)
int usr_log_raw_get_level(qd_context* ctx) {
	void* logger;
	if (pop_ptr_arg(ctx, &logger) != 0) {
		return -1;
	}
	int64_t level = (int64_t)qdlog_get_level((qdlog_logger_t*)logger);
	qd_push_i(ctx, level);
	return 0;
}

// raw_set_format(format:i64 logger:ptr -- )
int usr_log_raw_set_format(qd_context* ctx) {
	void* logger;
	int64_t format;
	if (pop_ptr_arg(ctx, &logger) != 0 || pop_int_arg(ctx, &format) != 0) {
		return -1;
	}
	qdlog_set_format((qdlog_logger_t*)logger, (qdlog_format_t)format);
	return 0;
}

// raw_enable_stdout(logger:ptr -- )
int usr_log_raw_enable_stdout(qd_context* ctx) {
	void* logger;
	if (pop_ptr_arg(ctx, &logger) != 0) {
		return -1;
	}
	qdlog_enable_stdout((qdlog_logger_t*)logger);
	return 0;
}

// raw_disable_stdout(logger:ptr -- )
int usr_log_raw_disable_stdout(qd_context* ctx) {
	void* logger;
	if (pop_ptr_arg(ctx, &logger) != 0) {
		return -1;
	}
	qdlog_disable_stdout((qdlog_logger_t*)logger);
	return 0;
}

// raw_add_file(path:str logger:ptr -- err:i64)
int usr_log_raw_add_file(qd_context* ctx) {
	void* logger;
	qd_string_t* path_str;
	if (pop_ptr_arg(ctx, &logger) != 0 || pop_str_arg(ctx, &path_str) != 0) {
		return -1;
	}
	int64_t err = qdlog_add_file((qdlog_logger_t*)logger, qd_string_data(path_str));
	qd_string_release(path_str);
	qd_push_i(ctx, err);
	return 0;
}

// raw_add_file_rotate(max_files:i64 max_size:i64 mode:i64 path:str logger:ptr -- err:i64)
int usr_log_raw_add_file_rotate(qd_context* ctx) {
	void* logger;
	qd_string_t* path_str;
	int64_t mode, max_size, max_files;
	if (pop_ptr_arg(ctx, &logger) != 0 || pop_str_arg(ctx, &path_str) != 0) {
		return -1;
	}
	if (pop_int_arg(ctx, &mode) != 0 || pop_int_arg(ctx, &max_size) != 0 || pop_int_arg(ctx, &max_files) != 0) {
		qd_string_release(path_str);
		return -1;
	}

	int64_t err = qdlog_add_file_rotate(
			(qdlog_logger_t*)logger, qd_string_data(path_str), (qdlog_rotate_t)mode, max_size, max_files);
	qd_string_release(path_str);
	qd_push_i(ctx, err);
	return 0;
}

// raw_log(msg:str level:i64 logger:ptr -- )
int usr_log_raw_log(qd_context* ctx) {
	void* logger;
	int64_t level;
	qd_string_t* msg_str;
	if (pop_ptr_arg(ctx, &logger) != 0 || pop_int_arg(ctx, &level) != 0) {
		return -1;
	}
	if (pop_str_arg(ctx, &msg_str) != 0) {
		return -1;
	}

	qdlog_log((qdlog_logger_t*)logger, (qdlog_level_t)level, qd_string_data(msg_str));
	qd_string_release(msg_str);
	return 0;
}

// raw_log_kv(pairs_count:i64 pairs:ptr msg:str level:i64 logger:ptr -- )
int usr_log_raw_log_kv(qd_context* ctx) {
	void* logger;
	int64_t level;
	qd_string_t* msg_str;
	void* pairs;
	int64_t pairs_count;
	if (pop_ptr_arg(ctx, &logger) != 0 || pop_int_arg(ctx, &level) != 0) {
		return -1;
	}
	if (pop_str_arg(ctx, &msg_str) != 0) {
		return -1;
	}
	if (pop_ptr_arg(ctx, &pairs) != 0 || pop_int_arg(ctx, &pairs_count) != 0) {
		qd_string_release(msg_str);
		return -1;
	}

	qdlog_log_kv((qdlog_logger_t*)logger, (qdlog_level_t)level, qd_string_data(msg_str), (const char**)pairs,
			pairs_count);
	qd_string_release(msg_str);
	return 0;
}

// raw_flush(logger:ptr -- )
int usr_log_raw_flush(qd_context* ctx) {
	void* logger;
	if (pop_ptr_arg(ctx, &logger) != 0) {
		return -1;
	}
	qdlog_flush((qdlog_logger_t*)logger);
	return 0;
}

// raw_check_rotate(logger:ptr -- )
int usr_log_raw_check_rotate(qd_context* ctx) {
	void* logger;
	if (pop_ptr_arg(ctx, &logger) != 0) {
		return -1;
	}
	qdlog_check_rotate((qdlog_logger_t*)logger);
	return 0;
}

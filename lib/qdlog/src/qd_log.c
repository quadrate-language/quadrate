// Quadrate wrapper functions for qdlog

#define _POSIX_C_SOURCE 200809L
#include "qdlog/log.h"
#include <qdrt/context.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <qdrt/qd_string.h>
#include <stdlib.h>
#include <string.h>

// Helper to set error message
static void set_error_msg(qd_context* ctx, const char* msg) {
	if (ctx->error_msg) free(ctx->error_msg);
	ctx->error_msg = strdup(msg);
}

// raw_new( -- logger:ptr)
qd_exec_result usr_log_raw_new(qd_context* ctx) {
	qdlog_logger_t* logger = qdlog_new();
	if (!logger) {
		set_error_msg(ctx, "log::new: failed to create logger");
		ctx->error_code = QDLOG_ERR_ALLOC;
		return (qd_exec_result){QDLOG_ERR_ALLOC};
	}
	qd_push_p(ctx, logger);
	return (qd_exec_result){0};
}

// raw_free(logger:ptr -- )
qd_exec_result usr_log_raw_free(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qdlog_free(logger);
	return (qd_exec_result){0};
}

// raw_set_level(level:i64 logger:ptr -- )
qd_exec_result usr_log_raw_set_level(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qd_stack_pop(ctx->st, &elem);
	int64_t level = elem.value.i;
	qdlog_set_level(logger, (qdlog_level_t)level);
	return (qd_exec_result){0};
}

// raw_get_level(logger:ptr -- level:i64)
qd_exec_result usr_log_raw_get_level(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	int64_t level = (int64_t)qdlog_get_level(logger);
	qd_push_i(ctx, level);
	return (qd_exec_result){0};
}

// raw_set_format(format:i64 logger:ptr -- )
qd_exec_result usr_log_raw_set_format(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qd_stack_pop(ctx->st, &elem);
	int64_t format = elem.value.i;
	qdlog_set_format(logger, (qdlog_format_t)format);
	return (qd_exec_result){0};
}

// raw_enable_stdout(logger:ptr -- )
qd_exec_result usr_log_raw_enable_stdout(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qdlog_enable_stdout(logger);
	return (qd_exec_result){0};
}

// raw_disable_stdout(logger:ptr -- )
qd_exec_result usr_log_raw_disable_stdout(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qdlog_disable_stdout(logger);
	return (qd_exec_result){0};
}

// raw_add_file(path:str logger:ptr -- err:i64)
qd_exec_result usr_log_raw_add_file(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qd_stack_element_t path_elem;
	qd_stack_pop(ctx->st, &path_elem);
	const char* path = qd_string_data(path_elem.value.s);
	int64_t err = qdlog_add_file(logger, path);
	qd_string_release(path_elem.value.s);
	qd_push_i(ctx, err);
	return (qd_exec_result){0};
}

// raw_add_file_rotate(max_files:i64 max_size:i64 mode:i64 path:str logger:ptr -- err:i64)
qd_exec_result usr_log_raw_add_file_rotate(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qd_stack_element_t path_elem;
	qd_stack_pop(ctx->st, &path_elem);
	const char* path = qd_string_data(path_elem.value.s);
	qd_stack_pop(ctx->st, &elem);
	int64_t mode = elem.value.i;
	qd_stack_pop(ctx->st, &elem);
	int64_t max_size = elem.value.i;
	qd_stack_pop(ctx->st, &elem);
	int64_t max_files = elem.value.i;

	int64_t err = qdlog_add_file_rotate(logger, path, (qdlog_rotate_t)mode, max_size, max_files);
	qd_string_release(path_elem.value.s);
	qd_push_i(ctx, err);
	return (qd_exec_result){0};
}

// raw_log(msg:str level:i64 logger:ptr -- )
qd_exec_result usr_log_raw_log(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qd_stack_pop(ctx->st, &elem);
	int64_t level = elem.value.i;
	qd_stack_element_t msg_elem;
	qd_stack_pop(ctx->st, &msg_elem);
	const char* msg = qd_string_data(msg_elem.value.s);

	qdlog_log(logger, (qdlog_level_t)level, msg);
	qd_string_release(msg_elem.value.s);
	return (qd_exec_result){0};
}

// raw_log_kv(pairs_count:i64 pairs:ptr msg:str level:i64 logger:ptr -- )
qd_exec_result usr_log_raw_log_kv(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qd_stack_pop(ctx->st, &elem);
	int64_t level = elem.value.i;
	qd_stack_element_t msg_elem;
	qd_stack_pop(ctx->st, &msg_elem);
	const char* msg = qd_string_data(msg_elem.value.s);
	qd_stack_pop(ctx->st, &elem);
	const char** pairs = (const char**)elem.value.p;
	qd_stack_pop(ctx->st, &elem);
	int64_t pairs_count = elem.value.i;

	qdlog_log_kv(logger, (qdlog_level_t)level, msg, pairs, pairs_count);
	qd_string_release(msg_elem.value.s);
	return (qd_exec_result){0};
}

// raw_flush(logger:ptr -- )
qd_exec_result usr_log_raw_flush(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qdlog_flush(logger);
	return (qd_exec_result){0};
}

// raw_check_rotate(logger:ptr -- )
qd_exec_result usr_log_raw_check_rotate(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	qdlog_logger_t* logger = (qdlog_logger_t*)elem.value.p;
	qdlog_check_rotate(logger);
	return (qd_exec_result){0};
}

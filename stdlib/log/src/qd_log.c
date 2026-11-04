// Quadrate wrapper functions for qdlog
#define _POSIX_C_SOURCE 200809L
#include "../include/quadrate/log/log.h"
#include <inttypes.h>
#include <quadrate/rt/runtime.h>

// The raw words take the logger on top, the other arguments under it, as the private
// import block in log.qd declares them.

// A level, format or rotation mode outside its constants raises: the level indexed a table
// of names, and `logger 9 "x" log::log` printed whatever lay past its end.
static int64_t check_range(qd_context* ctx, const char* op, const char* what, int64_t value, int64_t last) {
	if (value < 0 || value > last) {
		qd_fatal_raise(ctx, op, "%s %" PRId64 " is not one of the module's constants", what, value);
	}
	return value;
}

static qdlog_logger_t* logger_arg(qd_context* ctx, const char* op) {
	void* logger;
	qd_args(ctx, op, "p", &logger);
	return (qdlog_logger_t*)logger;
}

// raw_new( -- logger:ptr ), null when it could not be made, which log::new checks for
int usr_log_raw_new(qd_context* ctx) {
	return qd_push_p(ctx, qdlog_new());
}

// raw_free(logger:ptr -- )
int usr_log_raw_free(qd_context* ctx) {
	qdlog_free(logger_arg(ctx, "log::free"));
	return 0;
}

// raw_set_level(level:i64 logger:ptr -- )
int usr_log_raw_set_level(qd_context* ctx) {
	int64_t level;
	void* logger;
	qd_args(ctx, "log::set_level", "ip", &level, &logger);
	check_range(ctx, "log::set_level", "Level", level, QDLOG_LEVEL_OFF);
	qdlog_set_level((qdlog_logger_t*)logger, (qdlog_level_t)level);
	return 0;
}

// raw_get_level(logger:ptr -- level:i64)
int usr_log_raw_get_level(qd_context* ctx) {
	return qd_push_i(ctx, (int64_t)qdlog_get_level(logger_arg(ctx, "log::get_level")));
}

// raw_set_format(format:i64 logger:ptr -- )
int usr_log_raw_set_format(qd_context* ctx) {
	int64_t format;
	void* logger;
	qd_args(ctx, "log::set_format", "ip", &format, &logger);
	check_range(ctx, "log::set_format", "Format", format, QDLOG_FORMAT_JSON);
	qdlog_set_format((qdlog_logger_t*)logger, (qdlog_format_t)format);
	return 0;
}

// raw_enable_stdout(logger:ptr -- )
int usr_log_raw_enable_stdout(qd_context* ctx) {
	qdlog_enable_stdout(logger_arg(ctx, "log::enable_stdout"));
	return 0;
}

// raw_disable_stdout(logger:ptr -- )
int usr_log_raw_disable_stdout(qd_context* ctx) {
	qdlog_disable_stdout(logger_arg(ctx, "log::disable_stdout"));
	return 0;
}

// raw_add_file(path:str logger:ptr -- err:i64)
int usr_log_raw_add_file(qd_context* ctx) {
	qd_string_t* path;
	void* logger;
	qd_args(ctx, "log::add_file", "sp", &path, &logger);
	const int64_t err = qdlog_add_file((qdlog_logger_t*)logger, qd_string_data(path));
	qd_string_release(path);
	return qd_push_i(ctx, err);
}

// raw_add_file_rotate(max_files:i64 max_size:i64 mode:i64 path:str logger:ptr -- err:i64)
int usr_log_raw_add_file_rotate(qd_context* ctx) {
	int64_t max_files;
	int64_t max_size;
	int64_t mode;
	qd_string_t* path;
	void* logger;
	qd_args(ctx, "log::add_file_rotate", "iiisp", &max_files, &max_size, &mode, &path, &logger);
	if (mode < 0 || mode > QDLOG_ROTATE_HOURLY) {
		qd_string_release(path);
		check_range(ctx, "log::add_file_rotate", "Rotation mode", mode, QDLOG_ROTATE_HOURLY);
	}
	const int64_t err = qdlog_add_file_rotate(
			(qdlog_logger_t*)logger, qd_string_data(path), (qdlog_rotate_t)mode, max_size, max_files);
	qd_string_release(path);
	return qd_push_i(ctx, err);
}

// raw_log(msg:str level:i64 logger:ptr -- )
int usr_log_raw_log(qd_context* ctx) {
	qd_string_t* msg;
	int64_t level;
	void* logger;
	qd_args(ctx, "log::log", "sip", &msg, &level, &logger);
	if (level < 0 || level >= QDLOG_LEVEL_OFF) {
		qd_string_release(msg);
		check_range(ctx, "log::log", "Level", level, QDLOG_LEVEL_OFF - 1);
	}
	qdlog_log((qdlog_logger_t*)logger, (qdlog_level_t)level, qd_string_data(msg));
	qd_string_release(msg);
	return 0;
}

// raw_flush(logger:ptr -- )
int usr_log_raw_flush(qd_context* ctx) {
	qdlog_flush(logger_arg(ctx, "log::flush"));
	return 0;
}

// raw_check_rotate(logger:ptr -- )
int usr_log_raw_check_rotate(qd_context* ctx) {
	qdlog_check_rotate(logger_arg(ctx, "log::check_rotate"));
	return 0;
}

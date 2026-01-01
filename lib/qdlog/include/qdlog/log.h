// qdlog - Logging library for Quadrate
// Provides structured logging with levels, rotation, and multiple outputs.

#ifndef QDLOG_LOG_H
#define QDLOG_LOG_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
typedef enum {
	QDLOG_LEVEL_DEBUG = 0,
	QDLOG_LEVEL_INFO = 1,
	QDLOG_LEVEL_WARN = 2,
	QDLOG_LEVEL_ERROR = 3,
	QDLOG_LEVEL_OFF = 4
} qdlog_level_t;

// Log format
typedef enum {
	QDLOG_FORMAT_TEXT = 0, // Human-readable text
	QDLOG_FORMAT_JSON = 1  // JSON lines
} qdlog_format_t;

// Rotation mode
typedef enum {
	QDLOG_ROTATE_NONE = 0,	// No rotation
	QDLOG_ROTATE_SIZE = 1,	// Rotate by file size
	QDLOG_ROTATE_DAILY = 2, // Rotate daily
	QDLOG_ROTATE_HOURLY = 3 // Rotate hourly
} qdlog_rotate_t;

// Opaque logger handle
typedef struct qdlog_logger qdlog_logger_t;

// Error codes
#define QDLOG_OK 0
#define QDLOG_ERR_ALLOC 2
#define QDLOG_ERR_FILE 3
#define QDLOG_ERR_INVALID 4

// Create a new logger (logs to stdout by default)
qdlog_logger_t* qdlog_new(void);

// Free logger resources
void qdlog_free(qdlog_logger_t* logger);

// Set minimum log level
void qdlog_set_level(qdlog_logger_t* logger, qdlog_level_t level);

// Get current log level
qdlog_level_t qdlog_get_level(qdlog_logger_t* logger);

// Set output format (text or JSON)
void qdlog_set_format(qdlog_logger_t* logger, qdlog_format_t format);

// Add file output
// Returns 0 on success, error code on failure
int64_t qdlog_add_file(qdlog_logger_t* logger, const char* path);

// Add file output with rotation
// max_size: max file size in bytes before rotation (for ROTATE_SIZE)
// max_files: max number of rotated files to keep (0 = unlimited)
int64_t qdlog_add_file_rotate(
		qdlog_logger_t* logger, const char* path, qdlog_rotate_t mode, int64_t max_size, int64_t max_files);

// Disable stdout output
void qdlog_disable_stdout(qdlog_logger_t* logger);

// Enable stdout output (default)
void qdlog_enable_stdout(qdlog_logger_t* logger);

// Log a message at the specified level
// key-value pairs are passed as: key1, val1, key2, val2, ..., NULL
void qdlog_log(qdlog_logger_t* logger, qdlog_level_t level, const char* message);

// Log with key-value pairs (for structured logging)
// pairs is an array of strings: [key1, val1, key2, val2, ...]
// pairs_count is the number of key-value pairs (not total strings)
void qdlog_log_kv(
		qdlog_logger_t* logger, qdlog_level_t level, const char* message, const char** pairs, int64_t pairs_count);

// Convenience macros for common levels
#define qdlog_debug(logger, msg) qdlog_log(logger, QDLOG_LEVEL_DEBUG, msg)
#define qdlog_info(logger, msg) qdlog_log(logger, QDLOG_LEVEL_INFO, msg)
#define qdlog_warn(logger, msg) qdlog_log(logger, QDLOG_LEVEL_WARN, msg)
#define qdlog_error(logger, msg) qdlog_log(logger, QDLOG_LEVEL_ERROR, msg)

// Force flush all outputs
void qdlog_flush(qdlog_logger_t* logger);

// Check if rotation is needed and perform it
void qdlog_check_rotate(qdlog_logger_t* logger);

#ifdef __cplusplus
}
#endif

#endif // QDLOG_LOG_H

// qdlog - Logging library implementation

#define _POSIX_C_SOURCE 200809L
#include "qdlog/log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

// Maximum number of file outputs
#define MAX_FILE_OUTPUTS 8

// File output configuration
typedef struct {
	FILE* file;
	char* path;
	qdlog_rotate_t rotate_mode;
	int64_t max_size;
	int64_t max_files;
	int64_t current_size;
	int last_rotate_day;   // For daily rotation
	int last_rotate_hour;  // For hourly rotation
} qdlog_file_t;

// Logger structure
struct qdlog_logger {
	qdlog_level_t level;
	qdlog_format_t format;
	int stdout_enabled;
	qdlog_file_t files[MAX_FILE_OUTPUTS];
	int file_count;
};

// Level names
static const char* level_names[] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"OFF"
};

// Level names lowercase for JSON
static const char* level_names_lower[] = {
	"debug",
	"info",
	"warn",
	"error",
	"off"
};

qdlog_logger_t* qdlog_new(void) {
	qdlog_logger_t* logger = calloc(1, sizeof(qdlog_logger_t));
	if (!logger) return NULL;

	logger->level = QDLOG_LEVEL_INFO;
	logger->format = QDLOG_FORMAT_TEXT;
	logger->stdout_enabled = 1;
	logger->file_count = 0;

	return logger;
}

void qdlog_free(qdlog_logger_t* logger) {
	if (!logger) return;

	for (int i = 0; i < logger->file_count; i++) {
		if (logger->files[i].file) {
			fclose(logger->files[i].file);
		}
		free(logger->files[i].path);
	}

	free(logger);
}

void qdlog_set_level(qdlog_logger_t* logger, qdlog_level_t level) {
	if (logger) {
		logger->level = level;
	}
}

qdlog_level_t qdlog_get_level(qdlog_logger_t* logger) {
	return logger ? logger->level : QDLOG_LEVEL_INFO;
}

void qdlog_set_format(qdlog_logger_t* logger, qdlog_format_t format) {
	if (logger) {
		logger->format = format;
	}
}

void qdlog_disable_stdout(qdlog_logger_t* logger) {
	if (logger) {
		logger->stdout_enabled = 0;
	}
}

void qdlog_enable_stdout(qdlog_logger_t* logger) {
	if (logger) {
		logger->stdout_enabled = 1;
	}
}

int64_t qdlog_add_file(qdlog_logger_t* logger, const char* path) {
	return qdlog_add_file_rotate(logger, path, QDLOG_ROTATE_NONE, 0, 0);
}

// Get current file size
static int64_t get_file_size(const char* path) {
	struct stat st;
	if (stat(path, &st) == 0) {
		return st.st_size;
	}
	return 0;
}

// Generate rotated filename: path.1, path.2, etc.
static void rotate_files(const char* base_path, int64_t max_files) {
	char old_path[1024];
	char new_path[1024];

	// Delete oldest file if max_files is set
	if (max_files > 0) {
		snprintf(old_path, sizeof(old_path), "%s.%lld", base_path, (long long)max_files);
		unlink(old_path);
	}

	// Rotate existing files: .N -> .N+1
	for (int64_t i = max_files > 0 ? max_files - 1 : 99; i >= 1; i--) {
		snprintf(old_path, sizeof(old_path), "%s.%lld", base_path, (long long)i);
		snprintf(new_path, sizeof(new_path), "%s.%lld", base_path, (long long)(i + 1));
		rename(old_path, new_path);
	}

	// Rotate current file to .1
	snprintf(new_path, sizeof(new_path), "%s.1", base_path);
	rename(base_path, new_path);
}

// Generate timestamped filename for time-based rotation
static void get_time_rotated_path(char* buf, size_t bufsize, const char* base_path,
                                   qdlog_rotate_t mode, struct tm* tm) {
	if (mode == QDLOG_ROTATE_DAILY) {
		snprintf(buf, bufsize, "%s.%04d%02d%02d",
		         base_path, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
	} else if (mode == QDLOG_ROTATE_HOURLY) {
		snprintf(buf, bufsize, "%s.%04d%02d%02d%02d",
		         base_path, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour);
	} else {
		snprintf(buf, bufsize, "%s", base_path);
	}
}

int64_t qdlog_add_file_rotate(qdlog_logger_t* logger, const char* path,
                               qdlog_rotate_t mode, int64_t max_size, int64_t max_files) {
	if (!logger || !path) return QDLOG_ERR_INVALID;
	if (logger->file_count >= MAX_FILE_OUTPUTS) return QDLOG_ERR_INVALID;

	// Determine initial file path
	char actual_path[1024];
	time_t now = time(NULL);
	struct tm* tm = localtime(&now);

	if (mode == QDLOG_ROTATE_DAILY || mode == QDLOG_ROTATE_HOURLY) {
		get_time_rotated_path(actual_path, sizeof(actual_path), path, mode, tm);
	} else {
		snprintf(actual_path, sizeof(actual_path), "%s", path);
	}

	FILE* file = fopen(actual_path, "a");
	if (!file) return QDLOG_ERR_FILE;

	qdlog_file_t* f = &logger->files[logger->file_count];
	f->file = file;
	f->path = strdup(path);
	if (!f->path) {
		fclose(file);
		return QDLOG_ERR_ALLOC;
	}
	f->rotate_mode = mode;
	f->max_size = max_size;
	f->max_files = max_files;
	f->current_size = get_file_size(actual_path);
	f->last_rotate_day = tm->tm_yday;
	f->last_rotate_hour = tm->tm_hour;

	logger->file_count++;
	return QDLOG_OK;
}

// Check and perform rotation if needed
static void check_and_rotate(qdlog_file_t* f) {
	if (f->rotate_mode == QDLOG_ROTATE_NONE) return;

	time_t now = time(NULL);
	struct tm* tm = localtime(&now);
	int need_rotate = 0;

	if (f->rotate_mode == QDLOG_ROTATE_SIZE) {
		if (f->max_size > 0 && f->current_size >= f->max_size) {
			need_rotate = 1;
		}
	} else if (f->rotate_mode == QDLOG_ROTATE_DAILY) {
		if (tm->tm_yday != f->last_rotate_day) {
			need_rotate = 1;
		}
	} else if (f->rotate_mode == QDLOG_ROTATE_HOURLY) {
		if (tm->tm_hour != f->last_rotate_hour || tm->tm_yday != f->last_rotate_day) {
			need_rotate = 1;
		}
	}

	if (need_rotate) {
		fclose(f->file);

		if (f->rotate_mode == QDLOG_ROTATE_SIZE) {
			rotate_files(f->path, f->max_files);
			f->file = fopen(f->path, "a");
		} else {
			// Time-based rotation - open new file with timestamp
			char new_path[1024];
			get_time_rotated_path(new_path, sizeof(new_path), f->path, f->rotate_mode, tm);
			f->file = fopen(new_path, "a");
		}

		f->current_size = 0;
		f->last_rotate_day = tm->tm_yday;
		f->last_rotate_hour = tm->tm_hour;
	}
}

void qdlog_check_rotate(qdlog_logger_t* logger) {
	if (!logger) return;

	for (int i = 0; i < logger->file_count; i++) {
		check_and_rotate(&logger->files[i]);
	}
}

// Escape string for JSON
static void write_json_string(FILE* out, const char* str) {
	fputc('"', out);
	for (const char* p = str; *p; p++) {
		switch (*p) {
			case '"':  fputs("\\\"", out); break;
			case '\\': fputs("\\\\", out); break;
			case '\n': fputs("\\n", out); break;
			case '\r': fputs("\\r", out); break;
			case '\t': fputs("\\t", out); break;
			default:
				if ((unsigned char)*p < 32) {
					fprintf(out, "\\u%04x", (unsigned char)*p);
				} else {
					fputc(*p, out);
				}
		}
	}
	fputc('"', out);
}

// Write log entry to a file handle
static int write_log_entry(FILE* out, qdlog_format_t format, qdlog_level_t level,
                           const char* message, const char** pairs, int64_t pairs_count) {
	time_t now = time(NULL);
	struct tm* tm = localtime(&now);
	int written = 0;

	if (format == QDLOG_FORMAT_JSON) {
		// JSON format: {"time":"...","level":"...","msg":"...","key":"val",...}
		written += fprintf(out, "{\"time\":\"%04d-%02d-%02dT%02d:%02d:%02d\",",
		                   tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		                   tm->tm_hour, tm->tm_min, tm->tm_sec);
		written += fprintf(out, "\"level\":\"%s\",\"msg\":", level_names_lower[level]);
		write_json_string(out, message);

		// Add key-value pairs
		for (int64_t i = 0; i < pairs_count; i++) {
			const char* key = pairs[i * 2];
			const char* val = pairs[i * 2 + 1];
			if (key && val) {
				fputc(',', out);
				write_json_string(out, key);
				fputc(':', out);
				write_json_string(out, val);
			}
		}

		written += fprintf(out, "}\n");
	} else {
		// Text format: 2025-01-15T10:30:00 [INFO] message key=val key=val
		written += fprintf(out, "%04d-%02d-%02dT%02d:%02d:%02d [%-5s] %s",
		                   tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		                   tm->tm_hour, tm->tm_min, tm->tm_sec,
		                   level_names[level], message);

		// Add key-value pairs
		for (int64_t i = 0; i < pairs_count; i++) {
			const char* key = pairs[i * 2];
			const char* val = pairs[i * 2 + 1];
			if (key && val) {
				written += fprintf(out, " %s=%s", key, val);
			}
		}

		written += fprintf(out, "\n");
	}

	return written;
}

void qdlog_log(qdlog_logger_t* logger, qdlog_level_t level, const char* message) {
	qdlog_log_kv(logger, level, message, NULL, 0);
}

void qdlog_log_kv(qdlog_logger_t* logger, qdlog_level_t level, const char* message,
                  const char** pairs, int64_t pairs_count) {
	if (!logger || !message) return;
	if (level < logger->level) return;

	// Check rotation before writing
	qdlog_check_rotate(logger);

	// Write to stdout
	if (logger->stdout_enabled) {
		write_log_entry(stdout, logger->format, level, message, pairs, pairs_count);
	}

	// Write to files
	for (int i = 0; i < logger->file_count; i++) {
		if (logger->files[i].file) {
			int written = write_log_entry(logger->files[i].file, logger->format,
			                              level, message, pairs, pairs_count);
			logger->files[i].current_size += written;
			fflush(logger->files[i].file);
		}
	}
}

void qdlog_flush(qdlog_logger_t* logger) {
	if (!logger) return;

	if (logger->stdout_enabled) {
		fflush(stdout);
	}

	for (int i = 0; i < logger->file_count; i++) {
		if (logger->files[i].file) {
			fflush(logger->files[i].file);
		}
	}
}

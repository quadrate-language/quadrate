/**
 * @file tty.c
 * @brief Implementation of terminal detection and information
 */

#include <qdtty/tty.h>
#include <qdrt/runtime.h>
#include <stdlib.h>
#include <unistd.h>

#include "platform/tty_platform.h"

int usr_tty_is_stdout(qd_context* ctx) {
	int64_t result = isatty(STDOUT_FILENO) ? 1 : 0;
	qd_push_i(ctx, result);
	return 0;
}

int usr_tty_is_stderr(qd_context* ctx) {
	int64_t result = isatty(STDERR_FILENO) ? 1 : 0;
	qd_push_i(ctx, result);
	return 0;
}

int usr_tty_is_stdin(qd_context* ctx) {
	int64_t result = isatty(STDIN_FILENO) ? 1 : 0;
	qd_push_i(ctx, result);
	return 0;
}

static void get_terminal_size(int* rows, int* cols) {
	*rows = 0;
	*cols = 0;

	if (tty_platform_winsize(STDOUT_FILENO, rows, cols) == 0) return;
	if (tty_platform_winsize(STDERR_FILENO, rows, cols) == 0) return;
	if (tty_platform_winsize(STDIN_FILENO, rows, cols) == 0) return;

	// Fall back to environment variables
	const char* lines_env = getenv("LINES");
	const char* cols_env = getenv("COLUMNS");

	if (lines_env) {
		*rows = atoi(lines_env);
	}
	if (cols_env) {
		*cols = atoi(cols_env);
	}
}

int usr_tty_size(qd_context* ctx) {
	int rows, cols;
	get_terminal_size(&rows, &cols);
	qd_push_i(ctx, (int64_t)rows);
	qd_push_i(ctx, (int64_t)cols);
	return 0;
}

int usr_tty_width(qd_context* ctx) {
	int rows, cols;
	get_terminal_size(&rows, &cols);
	(void)rows;
	qd_push_i(ctx, (int64_t)cols);
	return 0;
}

int usr_tty_height(qd_context* ctx) {
	int rows, cols;
	get_terminal_size(&rows, &cols);
	(void)cols;
	qd_push_i(ctx, (int64_t)rows);
	return 0;
}

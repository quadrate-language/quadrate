#define _DEFAULT_SOURCE
/**
 * @file io.c
 * @brief Implementation of file I/O operations
 */

#include <errno.h>
#include <pthread.h>
#include <quadrate/io/io.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FILE_REGISTRY_SIZE 256

// File handles are exposed to Quadrate as opaque tokens rather than raw FILE*
// pointers. Each open file occupies a slot with a generation counter, and a
// token encodes (generation << 16 | slot index). Closing a file bumps the
// slot's generation so a stale token from a previously-closed file no longer
// resolves - even if the OS later reuses the same FILE* address. Keying on the
// FILE* address directly (as before) suffered that ABA problem.
typedef struct {
	FILE* fp;
	uint32_t gen;
} file_slot_t;

static file_slot_t file_slots[FILE_REGISTRY_SIZE];
static pthread_mutex_t file_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

#define FILE_TOKEN_INDEX_BITS 16
#define FILE_TOKEN_INDEX_MASK ((uintptr_t)0xFFFF)

static void* file_token_encode(uint32_t gen, size_t index) {
	uintptr_t token = ((uintptr_t)gen << FILE_TOKEN_INDEX_BITS) | ((uintptr_t)index & FILE_TOKEN_INDEX_MASK);
	return (void*)token;
}

// Register an open FILE* and return its opaque token, or NULL if the table is full.
static void* file_handle_register(FILE* fp) {
	if (!fp) {
		return NULL;
	}
	pthread_mutex_lock(&file_registry_mutex);
	for (size_t i = 0; i < FILE_REGISTRY_SIZE; i++) {
		if (file_slots[i].fp == NULL) {
			if (file_slots[i].gen == 0) {
				file_slots[i].gen = 1; // generations start at 1 so valid tokens are non-zero
			}
			file_slots[i].fp = fp;
			void* token = file_token_encode(file_slots[i].gen, i);
			pthread_mutex_unlock(&file_registry_mutex);
			return token;
		}
	}
	pthread_mutex_unlock(&file_registry_mutex);
	return NULL;
}

// Resolve a token to its FILE*, or NULL if the token is stale/invalid.
static FILE* file_handle_resolve(void* token) {
	uintptr_t t = (uintptr_t)token;
	size_t index = (size_t)(t & FILE_TOKEN_INDEX_MASK);
	uint32_t gen = (uint32_t)(t >> FILE_TOKEN_INDEX_BITS);
	if (index >= FILE_REGISTRY_SIZE) {
		return NULL;
	}
	pthread_mutex_lock(&file_registry_mutex);
	FILE* fp = NULL;
	if (file_slots[index].fp != NULL && file_slots[index].gen == gen) {
		fp = file_slots[index].fp;
	}
	pthread_mutex_unlock(&file_registry_mutex);
	return fp;
}

// Invalidate a token's slot and return the FILE* it held (so the caller can
// fclose outside the lock), or NULL if the token was already invalid.
static FILE* file_handle_release(void* token) {
	uintptr_t t = (uintptr_t)token;
	size_t index = (size_t)(t & FILE_TOKEN_INDEX_MASK);
	uint32_t gen = (uint32_t)(t >> FILE_TOKEN_INDEX_BITS);
	if (index >= FILE_REGISTRY_SIZE) {
		return NULL;
	}
	pthread_mutex_lock(&file_registry_mutex);
	FILE* fp = NULL;
	if (file_slots[index].fp != NULL && file_slots[index].gen == gen) {
		fp = file_slots[index].fp;
		file_slots[index].fp = NULL;
		file_slots[index].gen++; // bump so stale tokens to this slot stop resolving
		if (file_slots[index].gen == 0) {
			file_slots[index].gen = 1;
		}
	}
	pthread_mutex_unlock(&file_registry_mutex);
	return fp;
}

// Validate a resolved FILE handle before use. fp is the result of
// file_handle_resolve - NULL when the token was stale, invalid, or closed.
static int is_valid_file_handle(FILE* fp, qd_context* ctx, const char* op_name) {
	if (!fp) {
		ctx->error_code = IO_ERR_INVALID_HANDLE;
		char err_buf[128];
		snprintf(err_buf, sizeof(err_buf), "%s: invalid or already closed file handle", op_name);
		qd_set_error_msg(ctx, err_buf);
		return 0;
	}
	return 1;
}

int usr_io_open(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in io::open: Stack underflow (need 2, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t mode_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &mode_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::open: Failed to pop mode\n");
		abort();
	}
	if (mode_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::open: Expected string for mode, got %d\n", mode_elem.type);
		abort();
	}
	qd_stack_element_t path_elem;
	err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(mode_elem.value.s);
		fprintf(stderr, "Fatal error in io::open: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(mode_elem.value.s);
		fprintf(stderr, "Fatal error in io::open: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	const char* mode = qd_string_data(mode_elem.value.s);
	if (!path || !mode) {
		qd_string_release(path_elem.value.s);
		qd_string_release(mode_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::open: null path or mode string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}

	FILE* fp = fopen(path, mode);
	qd_string_release(path_elem.value.s);
	qd_string_release(mode_elem.value.s);

	if (!fp) {
		ctx->error_code = IO_ERR_NOT_FOUND;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_NOT_FOUND);
		return (int){IO_ERR_NOT_FOUND};
	}

	// Register the file handle and hand back an opaque token, not the raw FILE*.
	void* token = file_handle_register(fp);
	if (!token) {
		fclose(fp);
		ctx->error_code = IO_ERR_INVALID_HANDLE;
		qd_set_error_msg(ctx, "io::open: too many open files");
		qd_push_i(ctx, IO_ERR_INVALID_HANDLE);
		return (int){IO_ERR_INVALID_HANDLE};
	}

	int push_result = qd_push_p(ctx, token);
	if (push_result != 0) {
		file_handle_release(token);
		fclose(fp);
		fprintf(stderr, "Fatal error in io::open: Failed to push handle to stack\n");
		abort();
	}
	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

int usr_io_close(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::close: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::close: Failed to pop handle\n");
		abort();
	}
	if (elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::close: Expected pointer for handle, got %d\n", elem.type);
		abort();
	}

	// Invalidate the slot first (bumps its generation) so any stale copies of
	// this token stop resolving, then close the underlying file.
	FILE* fp = file_handle_release(elem.value.p);
	if (!fp) {
		// Closing an invalid/already-closed handle is a no-op; record the error
		// for debugging but still report success.
		ctx->error_code = IO_ERR_INVALID_HANDLE;
		qd_set_error_msg(ctx, "io::close: invalid or already closed file handle");
		return (int){0};
	}

	fclose(fp);

	return (int){0};
}

int usr_io_read_string(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in io::read: Stack underflow (need 2, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t count_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::read: Failed to pop count\n");
		abort();
	}
	if (count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in io::read: Expected integer for count, got %d\n", count_elem.type);
		abort();
	}

	qd_stack_element_t handle_elem;
	err = qd_stack_pop(ctx->st, &handle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::read: Failed to pop handle\n");
		abort();
	}
	if (handle_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::read: Expected pointer for handle, got %d\n", handle_elem.type);
		abort();
	}

	FILE* fp = file_handle_resolve(handle_elem.value.p);
	int64_t count = count_elem.value.i;

	// Validate file handle using registry
	if (!is_valid_file_handle(fp, ctx, "io::read_string")) {
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_HANDLE};
	}

// Validate count is within reasonable bounds (max 1GB to prevent DoS)
#define IO_MAX_READ_SIZE (1024 * 1024 * 1024)
	if (count < 0 || count > IO_MAX_READ_SIZE) {
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::read_string: invalid count (max 1GB)");
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_ARG};
	}

	if (count == 0) {
		qd_push_s(ctx, "");
		qd_push_i(ctx, 0);
		qd_push_i(ctx, IO_ERR_OK);
		return (int){0};
	}

	char* buffer = malloc((size_t)count + 1);
	if (!buffer) {
		ctx->error_code = IO_ERR_READ;
		qd_set_error_msg(ctx, "io::read_string: allocation failed");
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_READ};
	}

	size_t bytes_read = fread(buffer, 1, (size_t)count, fp);
	buffer[bytes_read] = '\0';

	if (bytes_read < (size_t)count && ferror(fp)) {
		ctx->error_code = IO_ERR_READ;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		free(buffer);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_READ};
	}

	qd_push_s(ctx, buffer);
	qd_push_i(ctx, (int64_t)bytes_read);
	qd_push_i(ctx, IO_ERR_OK);

	free(buffer);

	return (int){0};
}

int usr_io_write_string(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in io::write: Stack underflow (need 2, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::write: Failed to pop data\n");
		abort();
	}
	if (data_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::write: Expected string for data, got %d\n", data_elem.type);
		abort();
	}
	qd_stack_element_t handle_elem;
	err = qd_stack_pop(ctx->st, &handle_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(data_elem.value.s);
		fprintf(stderr, "Fatal error in io::write: Failed to pop handle\n");
		abort();
	}
	if (handle_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(data_elem.value.s);
		fprintf(stderr, "Fatal error in io::write: Expected pointer for handle, got %d\n", handle_elem.type);
		abort();
	}

	FILE* fp = file_handle_resolve(handle_elem.value.p);
	const char* data = qd_string_data(data_elem.value.s);
	if (!data) {
		qd_string_release(data_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::write_string: null data string");
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_ARG};
	}
	size_t len = strlen(data);

	if (!is_valid_file_handle(fp, ctx, "io::write_string")) {
		qd_string_release(data_elem.value.s);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_HANDLE};
	}

	size_t written = fwrite(data, 1, len, fp);
	qd_string_release(data_elem.value.s);

	if (written < len) {
		ctx->error_code = IO_ERR_WRITE;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_WRITE};
	}

	qd_push_i(ctx, (int64_t)written);
	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

int usr_io_seekg(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in io::seekg: Stack underflow (need 3, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}
	qd_stack_element_t whence_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &whence_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::seekg: Failed to pop whence\n");
		abort();
	}
	if (whence_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in io::seekg: Expected integer for whence, got %d\n", whence_elem.type);
		abort();
	}
	qd_stack_element_t offset_elem;
	err = qd_stack_pop(ctx->st, &offset_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::seekg: Failed to pop offset\n");
		abort();
	}
	if (offset_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in io::seekg: Expected integer for offset, got %d\n", offset_elem.type);
		abort();
	}
	qd_stack_element_t handle_elem;
	err = qd_stack_pop(ctx->st, &handle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::seekg: Failed to pop handle\n");
		abort();
	}
	if (handle_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::seekg: Expected pointer for handle, got %d\n", handle_elem.type);
		abort();
	}

	FILE* fp = file_handle_resolve(handle_elem.value.p);
	int64_t offset = offset_elem.value.i;
	int whence = (int)whence_elem.value.i;

	if (!is_valid_file_handle(fp, ctx, "io::seek")) {
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_HANDLE};
	}

	// Map whence values: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END
	int seek_whence;
	switch (whence) {
	case 0:
		seek_whence = SEEK_SET;
		break;
	case 1:
		seek_whence = SEEK_CUR;
		break;
	case 2:
		seek_whence = SEEK_END;
		break;
	default:
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::seek: invalid whence value");
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_ARG};
	}

	if (fseek(fp, offset, seek_whence) != 0) {
		ctx->error_code = IO_ERR_SEEK;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_SEEK};
	}

	int64_t position = ftell(fp);
	if (position < 0) {
		ctx->error_code = IO_ERR_SEEK;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_SEEK};
	}

	qd_push_i(ctx, position);
	qd_push_i(ctx, IO_ERR_OK);

	return (int){0};
}

int usr_io_eof(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::eof: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_peek(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::eof: Failed to peek handle\n");
		abort();
	}
	if (elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::eof: Expected pointer for handle, got %d\n", elem.type);
		abort();
	}

	FILE* fp = file_handle_resolve(elem.value.p);
	int64_t is_eof = 0;

	if (is_valid_file_handle(fp, ctx, "io::eof")) {
		is_eof = feof(fp) ? 1 : 0;
	}

	qd_push_i(ctx, is_eof);

	return (int){0};
}

// New unified API names
int usr_io_seek(qd_context* ctx) {
	return usr_io_seekg(ctx); // Just call the existing implementation
}

int usr_io_tell(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::tell: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	// Peek at the handle (don't pop it)
	qd_stack_error err = qd_stack_element(ctx->st, qd_stack_size(ctx->st) - 1, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::tell: Failed to peek handle\n");
		abort();
	}
	if (elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::tell: Expected pointer for handle, got %d\n", elem.type);
		abort();
	}

	FILE* fp = file_handle_resolve(elem.value.p);
	if (!is_valid_file_handle(fp, ctx, "io::tell")) {
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_HANDLE};
	}

	int64_t position = ftell(fp);
	if (position < 0) {
		ctx->error_code = IO_ERR_SEEK;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_SEEK};
	}

	qd_push_i(ctx, position);
	qd_push_i(ctx, IO_ERR_OK);

	return (int){0};
}

// Unified buffer-based read (new primary API)
int usr_io_read(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in io::read_bytes: Stack underflow (need 3, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}
	qd_stack_element_t count_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::read_bytes: Failed to pop count\n");
		abort();
	}
	if (count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in io::read_bytes: Expected integer for count, got %d\n", count_elem.type);
		abort();
	}
	qd_stack_element_t buffer_elem;
	err = qd_stack_pop(ctx->st, &buffer_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::read_bytes: Failed to pop buffer\n");
		abort();
	}
	if (buffer_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::read_bytes: Expected pointer for buffer, got %d\n", buffer_elem.type);
		abort();
	}
	qd_stack_element_t handle_elem;
	err = qd_stack_pop(ctx->st, &handle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::read_bytes: Failed to pop handle\n");
		abort();
	}
	if (handle_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::read_bytes: Expected pointer for handle, got %d\n", handle_elem.type);
		abort();
	}

	FILE* fp = file_handle_resolve(handle_elem.value.p);
	void* buffer = buffer_elem.value.p;
	int64_t count = count_elem.value.i;

	if (!is_valid_file_handle(fp, ctx, "io::read")) {
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_HANDLE};
	}

	if (!buffer || count < 0) {
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::read: invalid buffer or count");
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_ARG};
	}

	if (count == 0) {
		qd_push_i(ctx, 0);
		qd_push_i(ctx, IO_ERR_OK);
		return (int){0};
	}

	size_t bytes_read = fread(buffer, 1, (size_t)count, fp);

	if (bytes_read < (size_t)count && ferror(fp)) {
		ctx->error_code = IO_ERR_READ;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_READ};
	}

	qd_push_i(ctx, (int64_t)bytes_read);
	qd_push_i(ctx, IO_ERR_OK);

	return (int){0};
}

// Read a line from stdin
int usr_io_readline(qd_context* ctx) {
	char* line = NULL;
	size_t len = 0;
	ssize_t nread = getline(&line, &len, stdin);

	if (nread == -1) {
		ctx->error_code = IO_ERR_EOF;
		qd_set_error_msg(ctx, "io::readline: read error or EOF");
		free(line);
		qd_push_i(ctx, IO_ERR_EOF);
		return (int){IO_ERR_EOF};
	}

	if (nread > 0 && line[nread - 1] == '\n') {
		line[nread - 1] = '\0';
		nread--;
	}

	qd_push_s(ctx, line);
	qd_push_i(ctx, IO_ERR_OK);
	free(line);

	return (int){0};
}

// Unified buffer-based write (new primary API)
int usr_io_write(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in io::write_bytes: Stack underflow (need 3, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}
	qd_stack_element_t count_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::write_bytes: Failed to pop count\n");
		abort();
	}
	if (count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in io::write_bytes: Expected integer for count, got %d\n", count_elem.type);
		abort();
	}
	qd_stack_element_t buffer_elem;
	err = qd_stack_pop(ctx->st, &buffer_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::write_bytes: Failed to pop buffer\n");
		abort();
	}
	if (buffer_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::write_bytes: Expected pointer for buffer, got %d\n", buffer_elem.type);
		abort();
	}
	qd_stack_element_t handle_elem;
	err = qd_stack_pop(ctx->st, &handle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::write_bytes: Failed to pop handle\n");
		abort();
	}
	if (handle_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in io::write_bytes: Expected pointer for handle, got %d\n", handle_elem.type);
		abort();
	}

	FILE* fp = file_handle_resolve(handle_elem.value.p);
	void* buffer = buffer_elem.value.p;
	int64_t count = count_elem.value.i;

	if (!is_valid_file_handle(fp, ctx, "io::write")) {
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_HANDLE};
	}

	if (!buffer || count < 0) {
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::write: invalid buffer or count");
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_INVALID_ARG};
	}

	if (count == 0) {
		qd_push_i(ctx, 0);
		qd_push_i(ctx, IO_ERR_OK);
		return (int){0};
	}

	size_t bytes_written = fwrite(buffer, 1, (size_t)count, fp);

	if (bytes_written < (size_t)count && ferror(fp)) {
		ctx->error_code = IO_ERR_WRITE;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int){IO_ERR_WRITE};
	}

	qd_push_i(ctx, (int64_t)bytes_written);
	qd_push_i(ctx, IO_ERR_OK);

	return (int){0};
}

int usr_io_read_file(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::read_file: Stack underflow (need 1, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}
	qd_stack_element_t path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::read_file: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::read_file: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	if (!path) {
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::read_file: null path string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}

	FILE* fp = fopen(path, "rb");
	if (!fp) {
		int saved_errno = errno;
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_NOT_FOUND;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_NOT_FOUND);
		return (int){IO_ERR_NOT_FOUND};
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		int saved_errno = errno;
		fclose(fp);
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_SEEK;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_SEEK);
		return (int){IO_ERR_SEEK};
	}

	long file_size = ftell(fp);
	if (file_size < 0) {
		int saved_errno = errno;
		fclose(fp);
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_SEEK;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_SEEK);
		return (int){IO_ERR_SEEK};
	}

	if (fseek(fp, 0, SEEK_SET) != 0) {
		int saved_errno = errno;
		fclose(fp);
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_SEEK;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_SEEK);
		return (int){IO_ERR_SEEK};
	}

	char* buffer = malloc((size_t)file_size + 1);
	if (!buffer) {
		fclose(fp);
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_READ;
		qd_set_error_msg(ctx, "io::read_file: allocation failed");
		qd_push_i(ctx, IO_ERR_READ);
		return (int){IO_ERR_READ};
	}

	size_t bytes_read = fread(buffer, 1, (size_t)file_size, fp);
	buffer[bytes_read] = '\0';

	// Check for read error before closing file (ferror requires open file)
	int had_read_error = (bytes_read < (size_t)file_size && ferror(fp));
	int saved_errno = errno;
	fclose(fp);
	qd_string_release(path_elem.value.s);

	if (had_read_error) {
		free(buffer);
		ctx->error_code = IO_ERR_READ;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_READ);
		return (int){IO_ERR_READ};
	}

	qd_push_s(ctx, buffer);
	qd_push_i(ctx, IO_ERR_OK);

	free(buffer);

	return (int){0};
}

int usr_io_write_file(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in io::write_file: Stack underflow (need 2, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}
	qd_stack_element_t contents_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &contents_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::write_file: Failed to pop contents\n");
		abort();
	}
	if (contents_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::write_file: Expected string for contents, got %d\n", contents_elem.type);
		abort();
	}
	qd_stack_element_t path_elem;
	err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(contents_elem.value.s);
		fprintf(stderr, "Fatal error in io::write_file: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(contents_elem.value.s);
		fprintf(stderr, "Fatal error in io::write_file: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	const char* contents = qd_string_data(contents_elem.value.s);
	if (!path || !contents) {
		qd_string_release(path_elem.value.s);
		qd_string_release(contents_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::write_file: null path or contents string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}
	size_t len = strlen(contents);

	// Open file for writing (truncate if exists)
	FILE* fp = fopen(path, "wb");
	if (!fp) {
		int saved_errno = errno;
		qd_string_release(path_elem.value.s);
		qd_string_release(contents_elem.value.s);
		ctx->error_code = IO_ERR_PERMISSION;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_PERMISSION);
		return (int){IO_ERR_PERMISSION};
	}

	size_t written = fwrite(contents, 1, len, fp);
	int saved_errno = errno;
	fclose(fp);

	qd_string_release(path_elem.value.s);
	qd_string_release(contents_elem.value.s);

	if (written < len) {
		ctx->error_code = IO_ERR_WRITE;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_WRITE);
		return (int){IO_ERR_WRITE};
	}

	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

int usr_io_flush(qd_context* ctx) {
	(void)ctx;
	fflush(stdout);
	return (int){0};
}

int usr_io_stat_size(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::stat_size: Stack underflow (need 1, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::stat_size: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::stat_size: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	if (!path) {
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::stat_size: null path string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}

	struct stat st;
	if (stat(path, &st) != 0) {
		int saved_errno = errno;
		qd_string_release(path_elem.value.s);
		if (saved_errno == ENOENT) {
			ctx->error_code = IO_ERR_NOT_FOUND;
		} else if (saved_errno == EACCES) {
			ctx->error_code = IO_ERR_PERMISSION;
		} else {
			ctx->error_code = IO_ERR_STAT;
		}
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int)ctx->error_code;
	}

	qd_string_release(path_elem.value.s);
	qd_push_i(ctx, (int64_t)st.st_size);
	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

int usr_io_stat_mtime(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::stat_mtime: Stack underflow (need 1, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::stat_mtime: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::stat_mtime: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	if (!path) {
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::stat_mtime: null path string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}

	struct stat st;
	if (stat(path, &st) != 0) {
		int saved_errno = errno;
		qd_string_release(path_elem.value.s);
		if (saved_errno == ENOENT) {
			ctx->error_code = IO_ERR_NOT_FOUND;
		} else if (saved_errno == EACCES) {
			ctx->error_code = IO_ERR_PERMISSION;
		} else {
			ctx->error_code = IO_ERR_STAT;
		}
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int)ctx->error_code;
	}

	qd_string_release(path_elem.value.s);
	qd_push_i(ctx, (int64_t)st.st_mtime);
	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

int usr_io_stat_atime(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::stat_atime: Stack underflow (need 1, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::stat_atime: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::stat_atime: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	if (!path) {
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::stat_atime: null path string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}

	struct stat st;
	if (stat(path, &st) != 0) {
		int saved_errno = errno;
		qd_string_release(path_elem.value.s);
		if (saved_errno == ENOENT) {
			ctx->error_code = IO_ERR_NOT_FOUND;
		} else if (saved_errno == EACCES) {
			ctx->error_code = IO_ERR_PERMISSION;
		} else {
			ctx->error_code = IO_ERR_STAT;
		}
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int)ctx->error_code;
	}

	qd_string_release(path_elem.value.s);
	qd_push_i(ctx, (int64_t)st.st_atime);
	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

int usr_io_stat_mode(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in io::stat_mode: Stack underflow (need 1, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::stat_mode: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::stat_mode: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	if (!path) {
		qd_string_release(path_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::stat_mode: null path string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}

	struct stat st;
	if (stat(path, &st) != 0) {
		int saved_errno = errno;
		qd_string_release(path_elem.value.s);
		if (saved_errno == ENOENT) {
			ctx->error_code = IO_ERR_NOT_FOUND;
		} else if (saved_errno == EACCES) {
			ctx->error_code = IO_ERR_PERMISSION;
		} else {
			ctx->error_code = IO_ERR_STAT;
		}
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, ctx->error_code);
		return (int)ctx->error_code;
	}

	qd_string_release(path_elem.value.s);
	// Return just the permission bits (lower 12 bits of mode)
	qd_push_i(ctx, (int64_t)(st.st_mode & 07777));
	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

int usr_io_append_file(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in io::append_file: Stack underflow (need 2, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}
	qd_stack_element_t contents_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &contents_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in io::append_file: Failed to pop contents\n");
		abort();
	}
	if (contents_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in io::append_file: Expected string for contents, got %d\n", contents_elem.type);
		abort();
	}
	qd_stack_element_t path_elem;
	err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(contents_elem.value.s);
		fprintf(stderr, "Fatal error in io::append_file: Failed to pop path\n");
		abort();
	}
	if (path_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(contents_elem.value.s);
		fprintf(stderr, "Fatal error in io::append_file: Expected string for path, got %d\n", path_elem.type);
		abort();
	}

	const char* path = qd_string_data(path_elem.value.s);
	const char* contents = qd_string_data(contents_elem.value.s);
	if (!path || !contents) {
		qd_string_release(path_elem.value.s);
		qd_string_release(contents_elem.value.s);
		ctx->error_code = IO_ERR_INVALID_ARG;
		qd_set_error_msg(ctx, "io::append_file: null path or contents string");
		qd_push_i(ctx, IO_ERR_INVALID_ARG);
		return (int){IO_ERR_INVALID_ARG};
	}
	size_t len = strlen(contents);

	FILE* fp = fopen(path, "ab");
	if (!fp) {
		int saved_errno = errno;
		qd_string_release(path_elem.value.s);
		qd_string_release(contents_elem.value.s);
		ctx->error_code = IO_ERR_PERMISSION;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_PERMISSION);
		return (int){IO_ERR_PERMISSION};
	}

	size_t written = fwrite(contents, 1, len, fp);
	int saved_errno = errno;
	fclose(fp);

	qd_string_release(path_elem.value.s);
	qd_string_release(contents_elem.value.s);

	if (written < len) {
		ctx->error_code = IO_ERR_WRITE;
		char err_buf[256];
		snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
		qd_set_error_msg(ctx, err_buf);
		qd_push_i(ctx, IO_ERR_WRITE);
		return (int){IO_ERR_WRITE};
	}

	qd_push_i(ctx, IO_ERR_OK);
	return (int){0};
}

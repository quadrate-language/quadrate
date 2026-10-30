#define _DEFAULT_SOURCE
/**
 * @file io.c
 * @brief Implementation of file I/O operations
 */

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <quadrate/io/io.h>
#include <quadrate/rt/runtime.h>
#include <stdbool.h>
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

// An errno as the io error it is: a missing file or directory, or a refusal, are told
// apart from other failures, which take `fallback`
static int io_code(int err, int fallback) {
	switch (err) {
	case ENOENT:
	case ENOTDIR:
		return IO_ERR_NOT_FOUND;
	case EACCES:
	case EPERM:
	case EROFS:
		return IO_ERR_PERMISSION;
	default:
		return fallback;
	}
}

static int fail_errno(qd_context* ctx, const char* op, int err, int fallback) {
	return qd_fail(ctx, io_code(err, fallback), "%s: %s", op, strerror(err));
}

static int fail_handle(qd_context* ctx, const char* op) {
	return qd_fail(ctx, IO_ERR_INVALID_HANDLE, "%s: invalid or already closed file handle", op);
}

// Stack signature: ( path:str mode:str -- handle:ptr )!
int usr_io_open(qd_context* ctx) {
	qd_string_t* path;
	qd_string_t* mode;
	qd_args(ctx, "io::open", "ss", &path, &mode);
	FILE* fp = fopen(qd_string_data(path), qd_string_data(mode));
	const int err = errno;
	qd_string_release(path);
	qd_string_release(mode);
	if (!fp) {
		return fail_errno(ctx, "io::open", err, err == EINVAL ? IO_ERR_INVALID_ARG : IO_ERR_NOT_FOUND);
	}

	// Register the file handle and hand back an opaque token, not the raw FILE*.
	void* token = file_handle_register(fp);
	if (!token) {
		fclose(fp);
		return qd_fail(ctx, IO_ERR_INVALID_HANDLE, "io::open: too many open files");
	}
	qd_push_p(ctx, token);
	return qd_push_i(ctx, IO_ERR_OK);
}

// Stack signature: ( handle:ptr -- )
// Closing an invalid or already closed handle does nothing.
int usr_io_close(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "io::close", "p", &handle);
	// Invalidate the slot first (bumps its generation) so any stale copies of
	// this token stop resolving, then close the underlying file.
	FILE* fp = file_handle_release(handle);
	if (fp) {
		fclose(fp);
	}
	return 0;
}

// Stack signature: ( handle:ptr offset:i64 whence:i64 -- position:i64 )!
int usr_io_seek(qd_context* ctx) {
	void* handle;
	int64_t offset;
	int64_t whence;
	qd_args(ctx, "io::seek", "pii", &handle, &offset, &whence);
	FILE* fp = file_handle_resolve(handle);
	if (!fp) {
		return fail_handle(ctx, "io::seek");
	}
	static const int whences[] = {SEEK_SET, SEEK_CUR, SEEK_END};
	if (whence < 0 || whence > 2) {
		return qd_fail(
				ctx, IO_ERR_INVALID_ARG, "io::seek: whence %" PRId64 " is not SeekSet, SeekCur or SeekEnd", whence);
	}
	if (fseeko(fp, (off_t)offset, whences[whence]) != 0) {
		return fail_errno(ctx, "io::seek", errno, IO_ERR_SEEK);
	}
	const off_t position = ftello(fp);
	if (position < 0) {
		return fail_errno(ctx, "io::seek", errno, IO_ERR_SEEK);
	}
	qd_push_i(ctx, (int64_t)position);
	return qd_push_i(ctx, IO_ERR_OK);
}

// Stack signature: ( handle:ptr -- handle:ptr is_eof:i64 )
// An invalid handle is never at the end.
int usr_io_eof(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "io::eof", "p", &handle);
	FILE* fp = file_handle_resolve(handle);
	qd_push_p(ctx, handle);
	return qd_push_i(ctx, fp && feof(fp) ? 1 : 0);
}

// Stack signature: ( handle:ptr -- position:i64 )!
int usr_io_tell(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "io::tell", "p", &handle);
	FILE* fp = file_handle_resolve(handle);
	if (!fp) {
		return fail_handle(ctx, "io::tell");
	}
	const off_t position = ftello(fp);
	if (position < 0) {
		return fail_errno(ctx, "io::tell", errno, IO_ERR_SEEK);
	}
	qd_push_i(ctx, (int64_t)position);
	return qd_push_i(ctx, IO_ERR_OK);
}

// Stack signature: ( handle:ptr buffer:ptr count:i64 -- bytes_read:i64 )!
int usr_io_read(qd_context* ctx) {
	void* handle;
	void* buffer;
	int64_t count;
	qd_args(ctx, "io::read", "ppi", &handle, &buffer, &count);
	FILE* fp = file_handle_resolve(handle);
	if (!fp) {
		return fail_handle(ctx, "io::read");
	}
	if (!buffer || count < 0) {
		return qd_fail(ctx, IO_ERR_INVALID_ARG, "io::read: null buffer or negative count");
	}
	const size_t bytes_read = count > 0 ? fread(buffer, 1, (size_t)count, fp) : 0;
	if (bytes_read < (size_t)count && ferror(fp)) {
		clearerr(fp);
		return fail_errno(ctx, "io::read", errno, IO_ERR_READ);
	}
	qd_push_i(ctx, (int64_t)bytes_read);
	return qd_push_i(ctx, IO_ERR_OK);
}

/* ( -- line:str ok:i64 )!
 *
 * Three outcomes, kept distinct:
 *
 *   a line      -> line, ok=1, status OK
 *   end of input-> "",   ok=0, status OK   (no error state set)
 *   read failure-> status IO_ERR_READ, error state set
 *
 * Running out of input is how every read loop ends, so it is not an error. This
 * used to set error_code = IO_ERR_EOF at EOF, and because Quadrate has no way to
 * clear the error state, a *successful* loop left it set: a fallible function
 * that drained stdin reported failure to its caller. examples/wc/wc.qd carried a
 * comment explaining that it hand-rolled an ok flag for exactly this reason.
 *
 * getline() returns -1 for both cases, so feof/ferror decide which. Previously
 * they were collapsed into one code whose message read "read error or EOF" --
 * the API admitting it could not tell them apart. */
int usr_io_readline(qd_context* ctx) {
	char* line = NULL;
	size_t len = 0;
	errno = 0;
	ssize_t nread = getline(&line, &len, stdin);

	if (nread == -1) {
		const int err = errno;
		free(line);
		if (ferror(stdin)) {
			clearerr(stdin);
			return qd_fail(ctx, IO_ERR_READ, "io::readline: %s", strerror(err));
		}
		// Ordinary end of input.
		qd_push_s(ctx, "");
		qd_push_i(ctx, 0);
		return qd_push_i(ctx, IO_ERR_OK);
	}

	if (nread > 0 && line[nread - 1] == '\n') {
		nread--;
	}
	qd_string_t* s = qd_string_create_with_length(line, (size_t)nread);
	free(line);
	qd_push_s_ref(ctx, s);
	qd_string_release(s);
	qd_push_i(ctx, 1);
	return qd_push_i(ctx, IO_ERR_OK);
}

// Stack signature: ( handle:ptr buffer:ptr count:i64 -- bytes_written:i64 )!
int usr_io_write(qd_context* ctx) {
	void* handle;
	void* buffer;
	int64_t count;
	qd_args(ctx, "io::write", "ppi", &handle, &buffer, &count);
	FILE* fp = file_handle_resolve(handle);
	if (!fp) {
		return fail_handle(ctx, "io::write");
	}
	if (!buffer || count < 0) {
		return qd_fail(ctx, IO_ERR_INVALID_ARG, "io::write: null buffer or negative count");
	}
	const size_t bytes_written = count > 0 ? fwrite(buffer, 1, (size_t)count, fp) : 0;
	if (bytes_written < (size_t)count) {
		clearerr(fp);
		return fail_errno(ctx, "io::write", errno, IO_ERR_WRITE);
	}
	qd_push_i(ctx, (int64_t)bytes_written);
	return qd_push_i(ctx, IO_ERR_OK);
}

// Stack signature: ( path:str -- contents:str )!
// Read to the end rather than trusting the size stat or ftell gives: a pipe has none,
// and a file under /proc says 0 and then has contents.
int usr_io_read_file(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "io::read_file", "s", &path);
	FILE* fp = fopen(qd_string_data(path), "rb");
	const int open_err = errno;
	qd_string_release(path);
	if (!fp) {
		return fail_errno(ctx, "io::read_file", open_err, IO_ERR_READ);
	}

	char* data = NULL;
	size_t len = 0;
	size_t cap = 0;
	bool out_of_memory = false;
	for (;;) {
		if (len == cap) {
			const size_t new_cap = cap ? cap * 2 : 8192;
			char* grown = new_cap > cap ? realloc(data, new_cap) : NULL;
			if (!grown) {
				out_of_memory = true;
				break;
			}
			data = grown;
			cap = new_cap;
		}
		const size_t got = fread(data + len, 1, cap - len, fp);
		len += got;
		if (got == 0) {
			break;
		}
	}
	const bool failed = !out_of_memory && ferror(fp);
	const int read_err = errno;
	fclose(fp);

	if (out_of_memory || failed) {
		free(data);
		return out_of_memory ? qd_fail(ctx, IO_ERR_READ, "io::read_file: out of memory")
							 : fail_errno(ctx, "io::read_file", read_err, IO_ERR_READ);
	}
	qd_string_t* contents = qd_string_create_with_length(data ? data : "", len);
	free(data);
	qd_push_s_ref(ctx, contents);
	qd_string_release(contents);
	return qd_push_i(ctx, IO_ERR_OK);
}

// write_file and append_file. A full disk often shows only when the buffered data is
// flushed, so a failed fclose fails the write too.
static int write_whole_file(qd_context* ctx, const char* op, const char* mode) {
	qd_string_t* path;
	qd_string_t* contents;
	qd_args(ctx, op, "ss", &path, &contents);
	FILE* fp = fopen(qd_string_data(path), mode);
	int err = errno;
	qd_string_release(path);
	if (!fp) {
		qd_string_release(contents);
		return fail_errno(ctx, op, err, IO_ERR_WRITE);
	}
	const size_t len = qd_string_length(contents);
	const bool wrote = fwrite(qd_string_data(contents), 1, len, fp) == len;
	err = errno;
	qd_string_release(contents);
	const bool closed = fclose(fp) == 0;
	if (wrote && !closed) {
		err = errno;
	}
	if (!wrote || !closed) {
		return fail_errno(ctx, op, err, IO_ERR_WRITE);
	}
	return qd_push_i(ctx, IO_ERR_OK);
}

// Stack signature: ( path:str contents:str -- )!
int usr_io_write_file(qd_context* ctx) {
	return write_whole_file(ctx, "io::write_file", "wb");
}

// Stack signature: ( path:str contents:str -- )!
int usr_io_append_file(qd_context* ctx) {
	return write_whole_file(ctx, "io::append_file", "ab");
}

int usr_io_flush(qd_context* ctx) {
	(void)ctx;
	fflush(stdout);
	return 0;
}

// The stat words: ( path:str -- value:i64 )!
static int stat_word(qd_context* ctx, const char* op, int64_t (*field)(const struct stat*)) {
	qd_string_t* path;
	qd_args(ctx, op, "s", &path);
	struct stat st;
	const int result = stat(qd_string_data(path), &st);
	const int err = errno;
	qd_string_release(path);
	if (result != 0) {
		return fail_errno(ctx, op, err, IO_ERR_STAT);
	}
	qd_push_i(ctx, field(&st));
	return qd_push_i(ctx, IO_ERR_OK);
}

static int64_t stat_size(const struct stat* st) {
	return (int64_t)st->st_size;
}

static int64_t stat_mtime(const struct stat* st) {
	return (int64_t)st->st_mtime;
}

static int64_t stat_atime(const struct stat* st) {
	return (int64_t)st->st_atime;
}

// Just the permission bits, the lower 12 of the mode
static int64_t stat_mode(const struct stat* st) {
	return (int64_t)(st->st_mode & 07777);
}

int usr_io_stat_size(qd_context* ctx) {
	return stat_word(ctx, "io::stat_size", stat_size);
}

int usr_io_stat_mtime(qd_context* ctx) {
	return stat_word(ctx, "io::stat_mtime", stat_mtime);
}

int usr_io_stat_atime(qd_context* ctx) {
	return stat_word(ctx, "io::stat_atime", stat_atime);
}

int usr_io_stat_mode(qd_context* ctx) {
	return stat_word(ctx, "io::stat_mode", stat_mode);
}

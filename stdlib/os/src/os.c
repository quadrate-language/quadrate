#define _DEFAULT_SOURCE
#include "os_fs.h"
#include <errno.h>
#include <quadrate/os/os.h>
#include <quadrate/rt/array.h>
#include <quadrate/rt/runtime.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @brief Translate errno, after a call failed, to an os module error code
 */
static int errno_to_os_error(int err) {
	switch (err) {
	case ENOENT:
		return OS_ERR_NOT_FOUND;
	case EACCES:
	case EPERM:
		return OS_ERR_PERMISSION;
	case EEXIST:
		return OS_ERR_EXISTS;
	case ENOTDIR:
		return OS_ERR_NOT_DIRECTORY;
	case EISDIR:
		return OS_ERR_IS_DIRECTORY;
	case ENOSPC:
		return OS_ERR_NO_SPACE;
	case EROFS:
		return OS_ERR_READ_ONLY;
	case ENAMETOOLONG:
		return OS_ERR_NAME_TOO_LONG;
	case ENOMEM:
		return OS_ERR_OUT_OF_MEMORY;
	case EINVAL:
		return OS_ERR_INVALID_ARG;
	default:
		// EIO, and anything unmapped. A failure must never map to OS_ERR_OK, so errno 0 lands
		// here too.
		return OS_ERR_IO;
	}
}

// Fail with the error for `err` (an errno value), naming its cause
static int fail_errno(qd_context* ctx, const char* op, int err) {
	return qd_fail(ctx, errno_to_os_error(err), "%s: %s", op, strerror(err));
}

// A child's exit status from a wait status, or -1 when it did not exit normally (killed by a
// signal, or the shell could not be started)
static int64_t exit_status(int status) {
	return (status != -1 && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
}

// A Quadrate []str holding `count` entries from a malloc'd list, which is freed. `entries`
// may be NULL when `count` is 0.
static qd_array_t* string_array_from(qd_context* ctx, const char* op, char** entries, size_t count) {
	qd_array_t* arr = qd_array_create(count, QD_ARRAY_TYPE_STR);
	if (arr != NULL) {
		for (size_t j = 0; j < count; j++) {
			arr->data.p[j] = qd_string_create(entries[j] != NULL ? entries[j] : "");
		}
		arr->length = count;
	}
	for (size_t j = 0; j < count; j++) {
		free(entries[j]);
	}
	free(entries);
	if (arr == NULL) {
		qd_fatal_raise(ctx, op, "Failed to allocate entry array");
	}
	return arr;
}

int usr_os_exit(qd_context* ctx) {
	int64_t code;
	qd_args(ctx, "os::exit", "i", &code);
	exit((int)code);
}

int usr_os_system(qd_context* ctx) {
	qd_string_t* cmd;
	qd_args(ctx, "os::system", "s", &cmd);
	// system() returns a wait status, not the exit status: `false` came back as 256
	const int status = system(qd_string_data(cmd));
	qd_string_release(cmd);
	qd_push_i(ctx, exit_status(status));
	return 0;
}

int usr_os_getenv(qd_context* ctx) {
	qd_string_t* name;
	qd_args(ctx, "os::getenv", "s", &name);
	const char* value = getenv(qd_string_data(name));
	qd_push_s(ctx, value ? value : "");
	qd_string_release(name);
	return 0;
}

int usr_os_setenv(qd_context* ctx) {
	qd_string_t* name;
	qd_string_t* value;
	qd_args(ctx, "os::setenv", "ss", &name, &value);
	setenv(qd_string_data(name), qd_string_data(value), 1);
	qd_string_release(name);
	qd_string_release(value);
	return 0;
}

int usr_os_unsetenv(qd_context* ctx) {
	qd_string_t* name;
	qd_args(ctx, "os::unsetenv", "s", &name);
	unsetenv(qd_string_data(name));
	qd_string_release(name);
	return 0;
}

int usr_os_exists(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::exists", "s", &path);
	const int exists = os_fs_exists(qd_string_data(path));
	qd_string_release(path);
	qd_push_i(ctx, exists);
	return 0;
}

int usr_os_delete(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::delete", "s", &path);
	const int result = remove(qd_string_data(path));
	const int err = errno;
	qd_string_release(path);
	if (result != 0) {
		return fail_errno(ctx, "os::delete", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_rename(qd_context* ctx) {
	qd_string_t* oldpath;
	qd_string_t* newpath;
	qd_args(ctx, "os::rename", "ss", &oldpath, &newpath);
	const int result = rename(qd_string_data(oldpath), qd_string_data(newpath));
	const int err = errno;
	qd_string_release(oldpath);
	qd_string_release(newpath);
	if (result != 0) {
		return fail_errno(ctx, "os::rename", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

// 0, or the errno of the first thing that went wrong
static int copy_file(const char* srcpath, const char* dstpath) {
	FILE* src = fopen(srcpath, "rb");
	if (!src) {
		return errno;
	}
	FILE* dst = fopen(dstpath, "wb");
	if (!dst) {
		const int err = errno;
		fclose(src);
		return err;
	}

	int err = 0;
	char buffer[4096];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
		if (fwrite(buffer, 1, n, dst) != n) {
			err = errno;
			break;
		}
	}
	if (err == 0 && ferror(src)) {
		err = errno ? errno : EIO;
	}
	fclose(src);
	// Buffered writes can first fail here, on a full disk
	if (fclose(dst) != 0 && err == 0) {
		err = errno;
	}
	return err;
}

int usr_os_copy(qd_context* ctx) {
	qd_string_t* srcpath;
	qd_string_t* dstpath;
	qd_args(ctx, "os::copy", "ss", &srcpath, &dstpath);
	const int err = copy_file(qd_string_data(srcpath), qd_string_data(dstpath));
	qd_string_release(srcpath);
	qd_string_release(dstpath);
	if (err != 0) {
		return fail_errno(ctx, "os::copy", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_mkdir(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::mkdir", "s", &path);
	// Like mkdir -p: parents are created as needed
	const int err = os_fs_mkdir_p(qd_string_data(path));
	qd_string_release(path);
	if (err != 0) {
		return fail_errno(ctx, "os::mkdir", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_list(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::list", "s", &path);
	size_t count = 0;
	char** entries = os_fs_list_dir(qd_string_data(path), &count);
	if (!entries) {
		// os_fs_list_dir does not say why; ErrNotDirectory and ErrNotFound are the documented
		// causes, and anything else (permissions, a read error) is an I/O error
		const char* p = qd_string_data(path);
		const int code = !os_fs_exists(p) ? OS_ERR_NOT_FOUND : !os_fs_is_dir(p) ? OS_ERR_NOT_DIRECTORY : OS_ERR_IO;
		const int result = qd_fail(ctx, code, "os::list: cannot list '%s'", p);
		qd_string_release(path);
		return result;
	}
	qd_string_release(path);

	qd_push_p(ctx, string_array_from(ctx, "os::list", entries, count));
	qd_push_i(ctx, (int64_t)count);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_args(qd_context* ctx) {
	// The command-line arguments, excluding the program name, as a Quadrate string
	// array. Replaces the `read` builtin, which splayed them across the operand stack
	// and so had no expressible stack effect: the validator modelled it by clearing the
	// type stack, pushing sixteen synthetic strings and giving up on the enclosing
	// function's declared effect.
	int count = ctx->argc > 1 ? ctx->argc - 1 : 0;

	qd_array_t* args = qd_array_create((size_t)count, QD_ARRAY_TYPE_STR);
	if (args == NULL) {
		qd_fatal_raise(ctx, "os::args", "Failed to allocate argument array");
	}

	for (int i = 0; i < count; i++) {
		const char* arg = ctx->argv != NULL && ctx->argv[i + 1] != NULL ? ctx->argv[i + 1] : "";
		args->data.p[i] = qd_string_create(arg);
	}
	args->length = (size_t)count;

	qd_push_p(ctx, args);
	return 0;
}

int usr_os_program_name(qd_context* ctx) {
	// argv[0]. `read` used to stash this in ctx->program_name as a side effect; nothing
	// read it back, so it is exposed properly here instead.
	const char* name = ctx->argc > 0 && ctx->argv != NULL && ctx->argv[0] != NULL ? ctx->argv[0] : "";
	qd_push_s(ctx, name);
	return 0;
}

int usr_os_popen(qd_context* ctx) {
	qd_string_t* cmd;
	void* callback;
	qd_args(ctx, "os::popen", "sp", &cmd, &callback);

	FILE* pipe = popen(qd_string_data(cmd), "r");
	const int err = errno;
	qd_string_release(cmd);
	if (pipe == NULL) {
		return fail_errno(ctx, "os::popen", err);
	}

	// Each line, without its newline, goes to the callback as it arrives
	char* line = NULL;
	size_t line_cap = 0;
	ssize_t line_len;
	while ((line_len = getline(&line, &line_cap, pipe)) != -1) {
		if (line_len > 0 && line[line_len - 1] == '\n') {
			line[line_len - 1] = '\0';
		}
		qd_push_s(ctx, line);
		qd_push_p(ctx, callback);
		if (qd_call(ctx) != 0) {
			free(line);
			pclose(pipe);
			return qd_fail(ctx, OS_ERR_IO, "os::popen: the callback failed");
		}
	}
	free(line);

	qd_push_i(ctx, exit_status(pclose(pipe)));
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

// exec(cmd:str -- stdout:str exitcode:i64)!: run a command and capture its output
int usr_os_exec(qd_context* ctx) {
	qd_string_t* cmd;
	qd_args(ctx, "os::exec", "s", &cmd);

	FILE* pipe = popen(qd_string_data(cmd), "r");
	const int err = errno;
	qd_string_release(cmd);
	if (pipe == NULL) {
		return fail_errno(ctx, "os::exec", err);
	}

	size_t buf_cap = 4096;
	size_t buf_len = 0;
	char* buf = malloc(buf_cap);
	if (!buf) {
		pclose(pipe);
		return qd_fail(ctx, OS_ERR_OUT_OF_MEMORY, "os::exec: out of memory");
	}

	char chunk[1024];
	size_t bytes_read;
	while ((bytes_read = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
		if (buf_len + bytes_read >= buf_cap) {
			buf_cap *= 2;
			char* new_buf = realloc(buf, buf_cap);
			if (!new_buf) {
				free(buf);
				pclose(pipe);
				return qd_fail(ctx, OS_ERR_OUT_OF_MEMORY, "os::exec: out of memory");
			}
			buf = new_buf;
		}
		memcpy(buf + buf_len, chunk, bytes_read);
		buf_len += bytes_read;
	}
	buf[buf_len] = '\0';

	const int64_t code = exit_status(pclose(pipe));
	qd_push_s(ctx, buf);
	free(buf);
	qd_push_i(ctx, code);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_getpid(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)getpid());
	return 0;
}

int usr_os_is_dir(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::is_dir", "s", &path);
	const int is_dir = os_fs_is_dir(qd_string_data(path));
	qd_string_release(path);
	qd_push_i(ctx, is_dir);
	return 0;
}

int usr_os_is_file(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::is_file", "s", &path);
	const int is_file = os_fs_is_file(qd_string_data(path));
	qd_string_release(path);
	qd_push_i(ctx, is_file);
	return 0;
}

int usr_os_rmdir(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::rmdir", "s", &path);
	// Like rm -rf: the contents go too
	const int err = os_fs_rmdir_r(qd_string_data(path));
	qd_string_release(path);
	if (err != 0) {
		return fail_errno(ctx, "os::rmdir", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

// Walk callback context for passing to C++ code
typedef struct {
	qd_context* ctx;
	void* callback_ptr;
	bool callback_failed;
} walk_callback_ctx;

// C callback that calls the Quadrate callback
static int walk_c_callback(const char* path, int is_dir, int depth, void* user_data) {
	walk_callback_ctx* wctx = (walk_callback_ctx*)user_data;
	qd_context* ctx = wctx->ctx;
	qd_push_s(ctx, path);
	qd_push_i(ctx, is_dir);
	qd_push_i(ctx, depth);
	qd_push_p(ctx, wctx->callback_ptr);
	if (qd_call(ctx) != 0) {
		// Stop the walk. Recorded here because the stop code itself would read as an errno.
		wctx->callback_failed = true;
		return 1;
	}
	return 0;
}

int usr_os_walk(qd_context* ctx) {
	qd_string_t* path;
	void* callback;
	qd_args(ctx, "os::walk", "sp", &path, &callback);
	walk_callback_ctx wctx = {ctx, callback, false};
	const int err = os_fs_walk(qd_string_data(path), walk_c_callback, &wctx);
	qd_string_release(path);
	if (wctx.callback_failed) {
		return qd_fail(ctx, OS_ERR_IO, "os::walk: the callback failed");
	}
	if (err != 0) {
		return fail_errno(ctx, "os::walk", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_glob(qd_context* ctx) {
	qd_string_t* pattern;
	qd_args(ctx, "os::glob", "s", &pattern);
	size_t count = 0;
	char** entries = os_fs_glob(qd_string_data(pattern), &count);
	qd_string_release(pattern);

	// No matches is an empty array, not a null one: `entries len` used to be fatal
	qd_push_p(ctx, string_array_from(ctx, "os::glob", entries, entries ? count : 0));
	qd_push_i(ctx, entries ? (int64_t)count : 0);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_mktemp(qd_context* ctx) {
	char template[] = "/tmp/qd_XXXXXX";
	if (!mkdtemp(template)) {
		return fail_errno(ctx, "os::mktemp", errno);
	}
	qd_push_s(ctx, template);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_cwd(qd_context* ctx) {
	char* cwd = getcwd(NULL, 0);
	if (!cwd) {
		return fail_errno(ctx, "os::cwd", errno);
	}
	qd_push_s(ctx, cwd);
	free(cwd);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_symlink(qd_context* ctx) {
	qd_string_t* target;
	qd_string_t* linkpath;
	qd_args(ctx, "os::symlink", "ss", &target, &linkpath);
	const int result = symlink(qd_string_data(target), qd_string_data(linkpath));
	const int err = errno;
	qd_string_release(target);
	qd_string_release(linkpath);
	if (result != 0) {
		return fail_errno(ctx, "os::symlink", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_readlink(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::readlink", "s", &path);
	char buf[4096];
	const ssize_t len = readlink(qd_string_data(path), buf, sizeof(buf) - 1);
	const int err = errno;
	qd_string_release(path);
	if (len < 0) {
		return fail_errno(ctx, "os::readlink", err);
	}
	buf[len] = '\0';
	qd_push_s(ctx, buf);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_is_symlink(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::is_symlink", "s", &path);
	// lstat, so the link itself is looked at and not followed
	struct stat st;
	const int is_symlink = lstat(qd_string_data(path), &st) == 0 && S_ISLNK(st.st_mode);
	qd_string_release(path);
	qd_push_i(ctx, is_symlink);
	return 0;
}

int usr_os_chmod(qd_context* ctx) {
	qd_string_t* path;
	int64_t mode;
	qd_args(ctx, "os::chmod", "si", &path, &mode);
	const int result = chmod(qd_string_data(path), (mode_t)(mode & 07777));
	const int err = errno;
	qd_string_release(path);
	if (result != 0) {
		return fail_errno(ctx, "os::chmod", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_chown(qd_context* ctx) {
	qd_string_t* path;
	int64_t uid;
	int64_t gid;
	qd_args(ctx, "os::chown", "sii", &path, &uid, &gid);
	// -1 leaves that id unchanged, which is what chown itself takes it to mean
	const int result = chown(qd_string_data(path), (uid_t)uid, (gid_t)gid);
	const int err = errno;
	qd_string_release(path);
	if (result != 0) {
		return fail_errno(ctx, "os::chown", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_getuid(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)getuid());
	return 0;
}

int usr_os_getgid(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)getgid());
	return 0;
}

// urandom( -- n:i64)! - cryptographically secure random 64-bit value sourced
// from the OS entropy pool (/dev/urandom). Portable across Linux and Haiku.
int usr_os_urandom(qd_context* ctx) {
	uint64_t value = 0;
	FILE* f = fopen("/dev/urandom", "rb");
	if (!f) {
		return fail_errno(ctx, "os::urandom", errno);
	}
	const size_t got = fread(&value, 1, sizeof(value), f);
	fclose(f);
	if (got != sizeof(value)) {
		return qd_fail(ctx, OS_ERR_IO, "os::urandom: short read from /dev/urandom");
	}
	qd_push_i(ctx, (int64_t)value);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_hostname(qd_context* ctx) {
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) != 0) {
		return fail_errno(ctx, "os::hostname", errno);
	}
	hostname[sizeof(hostname) - 1] = '\0'; // gethostname need not terminate a truncated name
	qd_push_s(ctx, hostname);
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

int usr_os_chdir(qd_context* ctx) {
	qd_string_t* path;
	qd_args(ctx, "os::chdir", "s", &path);
	const int result = chdir(qd_string_data(path));
	const int err = errno;
	qd_string_release(path);
	if (result != 0) {
		return fail_errno(ctx, "os::chdir", err);
	}
	qd_push_i(ctx, OS_ERR_OK);
	return 0;
}

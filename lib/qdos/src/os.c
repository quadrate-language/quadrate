#include <qdos/os.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include "os_fs.h"

/**
 * @brief Translate errno to os module error code
 */
static int errno_to_os_error(int err) {
	switch (err) {
	case 0:
		return OS_ERR_OK;
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
	case EIO:
		return OS_ERR_IO;
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
		return OS_ERR_IO; // Default to I/O error for unknown errors
	}
}

qd_exec_result usr_os_exit(qd_context* ctx) {
	// Check stack has at least 1 element
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::exit: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the exit code
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exit: Failed to pop exit code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's an integer
	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in os::exit: Expected integer exit code, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Exit the program with the given code
	exit((int)elem.value.i);

	// This line is never reached, but needed for compiler
	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_system(qd_context* ctx) {
	// Check stack has at least 1 element
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::system: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the command string
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::system: Failed to pop command string\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's a string
	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::system: Expected string command, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Execute the command
	int exit_code = system(qd_string_data(elem.value.s));

	// Free the string
	qd_string_release(elem.value.s);

	// Push the exit code back onto the stack
	err = qd_stack_push_int(ctx->st, (int64_t)exit_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::system: Failed to push exit code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_getenv(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::getenv: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the variable name
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::getenv: Failed to pop variable name\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::getenv: Expected string command, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the environment variable
	const char* value = getenv(qd_string_data(elem.value.s));
	qd_string_release(elem.value.s);

	err = qd_stack_push_str(ctx->st, value ? value : "");
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::getenv: Failed to push environment variable value\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_exists(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::exists: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exists: Failed to pop path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::exists: Expected string path, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check if file exists using C++17 filesystem wrapper
	int exists = os_fs_exists(qd_string_data(elem.value.s));
	qd_string_release(elem.value.s);

	err = qd_stack_push_int(ctx->st, (int64_t)exists);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exists: Failed to push result\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_delete(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::delete: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::delete: Failed to pop path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::delete: Expected string path, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Delete the file (remove is C standard, works on all platforms)
	int result = remove(qd_string_data(elem.value.s));
	int error_code = (result != 0) ? errno_to_os_error(errno) : OS_ERR_OK;
	qd_string_release(elem.value.s);

	// Push error code (OS_ERR_OK = success, or specific error code)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::delete: Failed to push error code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error code
	return (qd_exec_result){error_code};
}

qd_exec_result usr_os_rename(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in os::rename: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop newpath (top of stack)
	qd_stack_element_t newpath_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &newpath_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::rename: Failed to pop newpath\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (newpath_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::rename: Expected string newpath, got type %d\n", newpath_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop oldpath
	qd_stack_element_t oldpath_elem;
	err = qd_stack_pop(ctx->st, &oldpath_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(newpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::rename: Failed to pop oldpath\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (oldpath_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(newpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::rename: Expected string oldpath, got type %d\n", oldpath_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Rename the file
	int result = rename(qd_string_data(oldpath_elem.value.s), qd_string_data(newpath_elem.value.s));
	int error_code = (result == -1) ? errno_to_os_error(errno) : OS_ERR_OK;
	qd_string_release(oldpath_elem.value.s);
	qd_string_release(newpath_elem.value.s);

	// Push error code (OS_ERR_OK = success, or specific error code)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::rename: Failed to push error code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error code
	return (qd_exec_result){error_code};
}

qd_exec_result usr_os_copy(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in os::copy: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop dstpath (top of stack)
	qd_stack_element_t dstpath_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &dstpath_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::copy: Failed to pop dstpath\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (dstpath_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::copy: Expected string dstpath, got type %d\n", dstpath_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop srcpath
	qd_stack_element_t srcpath_elem;
	err = qd_stack_pop(ctx->st, &srcpath_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(dstpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::copy: Failed to pop srcpath\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (srcpath_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(dstpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::copy: Expected string srcpath, got type %d\n", srcpath_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Copy the file
	FILE* src = fopen(qd_string_data(srcpath_elem.value.s), "rb");
	int error_code = OS_ERR_OK;
	if (!src) {
		error_code = errno_to_os_error(errno);
	} else {
		FILE* dst = fopen(qd_string_data(dstpath_elem.value.s), "wb");
		if (!dst) {
			error_code = errno_to_os_error(errno);
			fclose(src);
		} else {
			char buffer[4096];
			size_t n;
			while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
				if (fwrite(buffer, 1, n, dst) != n) {
					error_code = errno_to_os_error(errno);
					break;
				}
			}
			fclose(dst);
			fclose(src);
		}
	}

	qd_string_release(srcpath_elem.value.s);
	qd_string_release(dstpath_elem.value.s);

	// Push error code (OS_ERR_OK = success, or specific error code)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::copy: Failed to push error code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error code
	return (qd_exec_result){error_code};
}

qd_exec_result usr_os_mkdir(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::mkdir: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::mkdir: Failed to pop path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::mkdir: Expected string path, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Create directory and parents using C++17 filesystem wrapper (cross-platform)
	// This behaves like mkdir -p, creating parent directories as needed
	int fs_error = os_fs_mkdir_p(qd_string_data(elem.value.s));
	int error_code = (fs_error != 0) ? errno_to_os_error(fs_error) : OS_ERR_OK;
	qd_string_release(elem.value.s);

	// Push error code (OS_ERR_OK = success, or specific error code)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::mkdir: Failed to push error code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error code
	return (qd_exec_result){error_code};
}

qd_exec_result usr_os_setenv(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in os::setenv: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop value (top of stack)
	qd_stack_element_t value_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::setenv: Failed to pop value\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (value_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::setenv: Expected string value, got type %d\n", value_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop name
	qd_stack_element_t name_elem;
	err = qd_stack_pop(ctx->st, &name_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(value_elem.value.s);
		fprintf(stderr, "Fatal error in os::setenv: Failed to pop name\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (name_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(value_elem.value.s);
		fprintf(stderr, "Fatal error in os::setenv: Expected string name, got type %d\n", name_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Set the environment variable (1 = overwrite existing)
	int result = setenv(qd_string_data(name_elem.value.s), qd_string_data(value_elem.value.s), 1);
	qd_string_release(name_elem.value.s);
	qd_string_release(value_elem.value.s);

	if (result != 0) {
		return (qd_exec_result){errno};
	}

	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_popen(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in os::popen: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop callback (top of stack)
	qd_stack_element_t callback_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &callback_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::popen: Failed to pop callback\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (callback_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in os::popen: Expected pointer for callback, got type %d\n", callback_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop command string
	qd_stack_element_t cmd_elem;
	err = qd_stack_pop(ctx->st, &cmd_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::popen: Failed to pop command\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (cmd_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::popen: Expected string for command, got type %d\n", cmd_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Open pipe to command
	FILE* pipe = popen(qd_string_data(cmd_elem.value.s), "r");
	qd_string_release(cmd_elem.value.s);

	if (pipe == NULL) {
		// Push exit code -1 to indicate failure
		qd_stack_push_int(ctx->st, -1);
		return (qd_exec_result){OS_ERR_IO};
	}

	// Read output line by line and call callback for each
	char* line = NULL;
	size_t line_cap = 0;
	ssize_t line_len;

	while ((line_len = getline(&line, &line_cap, pipe)) != -1) {
		// Remove trailing newline if present
		if (line_len > 0 && line[line_len - 1] == '\n') {
			line[line_len - 1] = '\0';
			line_len--;
		}

		// Push line string to stack
		err = qd_stack_push_str(ctx->st, line);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in os::popen: Failed to push line\n");
			free(line);
			pclose(pipe);
			qd_print_stack_trace(ctx);
			abort();
		}

		// Push callback to stack
		err = qd_stack_push_ptr(ctx->st, callback_elem.value.p);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in os::popen: Failed to push callback\n");
			free(line);
			pclose(pipe);
			qd_print_stack_trace(ctx);
			abort();
		}

		// Call the callback
		qd_exec_result call_result = qd_call(ctx);
		if (call_result.code != 0) {
			// Callback returned an error
			free(line);
			pclose(pipe);
			return call_result;
		}
	}

	free(line);

	// Get exit code
	int status = pclose(pipe);
	int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

	// Push exit code (the return value)
	err = qd_stack_push_int(ctx->st, (int64_t)exit_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::popen: Failed to push exit code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push Ok marker (1) for switch/if to match
	err = qd_stack_push_int(ctx->st, OS_ERR_OK);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::popen: Failed to push Ok marker\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};  // 0 = success
}

// os::exec - execute command and capture output: (cmd:str -- stdout:str exitcode:i64)!
qd_exec_result usr_os_exec(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::exec: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop command string
	qd_stack_element_t cmd_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &cmd_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exec: Failed to pop command\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (cmd_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::exec: Expected string for command, got type %d\n", cmd_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Open pipe to command
	FILE* pipe = popen(qd_string_data(cmd_elem.value.s), "r");
	qd_string_release(cmd_elem.value.s);

	if (pipe == NULL) {
		int error_code = errno_to_os_error(errno);
		ctx->error_code = error_code;
		qd_set_error_msg(ctx, "os::exec: failed to execute command");
		qd_stack_push_int(ctx->st, (int64_t)error_code);
		return (qd_exec_result){error_code};
	}

	// Read all output into a buffer
	size_t buf_cap = 4096;
	size_t buf_len = 0;
	char* buf = malloc(buf_cap);
	if (!buf) {
		pclose(pipe);
		ctx->error_code = OS_ERR_OUT_OF_MEMORY;
		qd_set_error_msg(ctx, "os::exec: out of memory");
		qd_stack_push_int(ctx->st, (int64_t)OS_ERR_OUT_OF_MEMORY);
		return (qd_exec_result){OS_ERR_OUT_OF_MEMORY};
	}

	char chunk[1024];
	size_t bytes_read;
	while ((bytes_read = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
		// Grow buffer if needed
		if (buf_len + bytes_read >= buf_cap) {
			buf_cap *= 2;
			char* new_buf = realloc(buf, buf_cap);
			if (!new_buf) {
				free(buf);
				pclose(pipe);
				ctx->error_code = OS_ERR_OUT_OF_MEMORY;
				qd_set_error_msg(ctx, "os::exec: out of memory");
				qd_stack_push_int(ctx->st, (int64_t)OS_ERR_OUT_OF_MEMORY);
				return (qd_exec_result){OS_ERR_OUT_OF_MEMORY};
			}
			buf = new_buf;
		}
		memcpy(buf + buf_len, chunk, bytes_read);
		buf_len += bytes_read;
	}
	buf[buf_len] = '\0';

	// Get exit code
	int status = pclose(pipe);
	int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

	// Push stdout string
	qd_stack_push_str(ctx->st, buf);
	free(buf);

	// Push exit code
	err = qd_stack_push_int(ctx->st, (int64_t)exit_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exec: Failed to push exit code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push Ok marker
	err = qd_stack_push_int(ctx->st, OS_ERR_OK);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exec: Failed to push Ok marker\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_os_list(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::list: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::list: Failed to pop path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::list: Expected string path, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// List directory using C++17 filesystem wrapper (cross-platform)
	size_t count = 0;
	char** entries = os_fs_list_dir(qd_string_data(elem.value.s), &count);

	if (!entries) {
		// Failed to list directory
		qd_string_release(elem.value.s);
		// Push empty array and count 0 on error
		err = qd_stack_push_ptr(ctx->st, NULL);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in os::list: Failed to push entries pointer\n");
			qd_print_stack_trace(ctx);
			abort();
		}
		err = qd_stack_push_int(ctx->st, 0);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in os::list: Failed to push count\n");
			qd_print_stack_trace(ctx);
			abort();
		}
		// Push error code (not found / not a directory)
		qd_stack_push_int(ctx->st, OS_ERR_NOT_FOUND);
		// Return error since listing failed
		return (qd_exec_result){OS_ERR_NOT_FOUND};
	}

	qd_string_release(elem.value.s);

	// Push entries pointer and count
	err = qd_stack_push_ptr(ctx->st, entries);
	if (err != QD_STACK_OK) {
		// Cleanup on error
		for (size_t j = 0; j < count; j++) {
			free(entries[j]);
		}
		free(entries);
		fprintf(stderr, "Fatal error in os::list: Failed to push entries pointer\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_push_int(ctx->st, (int64_t)count);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::list: Failed to push count\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push error code (OS_ERR_OK for success)
	qd_stack_push_int(ctx->st, OS_ERR_OK);
	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_getpid(qd_context* ctx) {
	// Get process ID
	pid_t pid = getpid();

	// Push PID to stack
	qd_stack_error err = qd_stack_push_int(ctx->st, (int64_t)pid);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::getpid: Failed to push pid\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_os_is_dir(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::is_dir: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::is_dir: Expected string path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	int is_dir = os_fs_is_dir(qd_string_data(elem.value.s));
	qd_string_release(elem.value.s);

	qd_stack_push_int(ctx->st, (int64_t)is_dir);
	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_is_file(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::is_file: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::is_file: Expected string path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	int is_file = os_fs_is_file(qd_string_data(elem.value.s));
	qd_string_release(elem.value.s);

	qd_stack_push_int(ctx->st, (int64_t)is_file);
	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_rmdir(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::rmdir: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::rmdir: Expected string path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Remove directory and all contents recursively (like rm -rf)
	int fs_error = os_fs_rmdir_r(qd_string_data(elem.value.s));
	int error_code = (fs_error != 0) ? errno_to_os_error(fs_error) : OS_ERR_OK;
	qd_string_release(elem.value.s);

	qd_stack_push_int(ctx->st, (int64_t)error_code);
	return (qd_exec_result){error_code};
}

// Walk callback context for passing to C++ code
typedef struct {
	qd_context* ctx;
	void* callback_ptr;
} walk_callback_ctx;

// C callback that calls the Quadrate callback
static int walk_c_callback(const char* path, int is_dir, int depth, void* user_data) {
	walk_callback_ctx* wctx = (walk_callback_ctx*)user_data;
	qd_context* ctx = wctx->ctx;

	// Push arguments: path, is_dir, depth
	qd_stack_push_str(ctx->st, path);
	qd_stack_push_int(ctx->st, (int64_t)is_dir);
	qd_stack_push_int(ctx->st, (int64_t)depth);

	// Push callback and call it
	qd_stack_push_ptr(ctx->st, wctx->callback_ptr);
	qd_exec_result result = qd_call(ctx);

	return (result.code != 0) ? 1 : 0;
}

qd_exec_result usr_os_walk(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in os::walk: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop callback
	qd_stack_element_t callback_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &callback_elem);
	if (err != QD_STACK_OK || callback_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in os::walk: Expected callback pointer\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop path
	qd_stack_element_t path_elem;
	err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK || path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::walk: Expected string path\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	walk_callback_ctx wctx = {ctx, callback_elem.value.p};
	int fs_error = os_fs_walk(qd_string_data(path_elem.value.s), walk_c_callback, &wctx);
	int error_code = (fs_error != 0) ? errno_to_os_error(fs_error) : OS_ERR_OK;
	qd_string_release(path_elem.value.s);

	qd_stack_push_int(ctx->st, (int64_t)error_code);
	return (qd_exec_result){error_code};
}

qd_exec_result usr_os_glob(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::glob: Stack underflow\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::glob: Expected string pattern\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	size_t count = 0;
	char** entries = os_fs_glob(qd_string_data(elem.value.s), &count);
	qd_string_release(elem.value.s);

	if (!entries) {
		// No matches or error - push empty results
		qd_stack_push_ptr(ctx->st, NULL);
		qd_stack_push_int(ctx->st, 0);
		qd_stack_push_int(ctx->st, OS_ERR_OK);
		return (qd_exec_result){OS_ERR_OK};
	}

	// Push entries pointer and count
	qd_stack_push_ptr(ctx->st, entries);
	qd_stack_push_int(ctx->st, (int64_t)count);
	qd_stack_push_int(ctx->st, OS_ERR_OK);
	return (qd_exec_result){OS_ERR_OK};
}

qd_exec_result usr_os_mktemp(qd_context* ctx) {
	// Create template for mkdtemp
	// Use /tmp/qd_XXXXXX format
	char template[] = "/tmp/qd_XXXXXX";

	// Create the temporary directory
	char* result = mkdtemp(template);
	if (!result) {
		int error_code = errno_to_os_error(errno);
		ctx->error_code = error_code;
		qd_set_error_msg(ctx, "os::mktemp: failed to create temp directory");
		qd_stack_push_int(ctx->st, (int64_t)error_code);
		return (qd_exec_result){error_code};
	}

	// Push the path and Ok
	qd_stack_push_str(ctx->st, result);
	qd_stack_push_int(ctx->st, OS_ERR_OK);
	return (qd_exec_result){0};
}

qd_exec_result usr_os_cwd(qd_context* ctx) {
	char* cwd = getcwd(NULL, 0);
	if (!cwd) {
		int error_code = errno_to_os_error(errno);
		ctx->error_code = error_code;
		qd_set_error_msg(ctx, "os::cwd: failed to get current directory");
		qd_stack_push_int(ctx->st, (int64_t)error_code);
		return (qd_exec_result){error_code};
	}

	// Push the path and Ok
	qd_stack_push_str(ctx->st, cwd);
	free(cwd);
	qd_stack_push_int(ctx->st, OS_ERR_OK);
	return (qd_exec_result){0};
}

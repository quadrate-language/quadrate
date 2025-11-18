#define _POSIX_C_SOURCE 200809L

#include <stdosqd/os.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

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
	return (qd_exec_result){0};
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
	int exit_code = system(elem.value.s);

	// Free the string
	free(elem.value.s);

	// Push the exit code back onto the stack
	err = qd_stack_push_int(ctx->st, (int64_t)exit_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::system: Failed to push exit code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
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
	const char* value = getenv(elem.value.s);
	free(elem.value.s);

	err = qd_stack_push_str(ctx->st, value ? value : "");
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::getenv: Failed to push environment variable value\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
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

	// Check if file exists using access()
	int exists = (access(elem.value.s, F_OK) == 0) ? 1 : 0;
	free(elem.value.s);

	err = qd_stack_push_int(ctx->st, (int64_t)exists);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exists: Failed to push result\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
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

	// Delete the file
	int result = unlink(elem.value.s);
	int error_code = (result == -1) ? errno : 0;
	free(elem.value.s);

	// Push errno (0 = success, or errno value on error)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::delete: Failed to push errno\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error if operation failed
	return (qd_exec_result){error_code != 0 ? 1 : 0};
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
		free(newpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::rename: Failed to pop oldpath\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (oldpath_elem.type != QD_STACK_TYPE_STR) {
		free(newpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::rename: Expected string oldpath, got type %d\n", oldpath_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Rename the file
	int result = rename(oldpath_elem.value.s, newpath_elem.value.s);
	int error_code = (result == -1) ? errno : 0;
	free(oldpath_elem.value.s);
	free(newpath_elem.value.s);

	// Push errno (0 = success, or errno value on error)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::rename: Failed to push errno\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error if operation failed
	return (qd_exec_result){error_code != 0 ? 1 : 0};
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
		free(dstpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::copy: Failed to pop srcpath\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (srcpath_elem.type != QD_STACK_TYPE_STR) {
		free(dstpath_elem.value.s);
		fprintf(stderr, "Fatal error in os::copy: Expected string srcpath, got type %d\n", srcpath_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Copy the file
	FILE* src = fopen(srcpath_elem.value.s, "rb");
	int result = -1;
	if (src) {
		FILE* dst = fopen(dstpath_elem.value.s, "wb");
		if (dst) {
			char buffer[4096];
			size_t n;
			result = 0;
			while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
				if (fwrite(buffer, 1, n, dst) != n) {
					result = -1;
					break;
				}
			}
			fclose(dst);
		}
		fclose(src);
	}

	int error_code = (result == -1) ? errno : 0;
	free(srcpath_elem.value.s);
	free(dstpath_elem.value.s);

	// Push errno (0 = success, or errno value on error)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::copy: Failed to push errno\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error if operation failed
	return (qd_exec_result){error_code != 0 ? 1 : 0};
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

	// Create directory with permissions 0755
	int result = mkdir(elem.value.s, 0755);
	int error_code = (result == -1) ? errno : 0;
	free(elem.value.s);

	// Push errno (0 = success, or errno value on error)
	err = qd_stack_push_int(ctx->st, (int64_t)error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::mkdir: Failed to push errno\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Return error if operation failed
	return (qd_exec_result){error_code != 0 ? 1 : 0};
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

	// Open directory
	DIR* dir = opendir(elem.value.s);
	if (!dir) {
		int error_code = errno;
		free(elem.value.s);
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
		// Push errno value
		qd_stack_push_int(ctx->st, (int64_t)error_code);
		// Return error since opendir failed
		return (qd_exec_result){1};
	}

	// Count entries first
	size_t count = 0;
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		// Skip . and ..
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		count++;
	}

	// Allocate array for strings
	char** entries = malloc(count * sizeof(char*));
	if (!entries) {
		closedir(dir);
		free(elem.value.s);
		fprintf(stderr, "Fatal error in os::list: Failed to allocate entries array\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Rewind and read entries
	rewinddir(dir);
	size_t i = 0;
	while ((entry = readdir(dir)) != NULL && i < count) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		entries[i] = strdup(entry->d_name);
		if (!entries[i]) {
			// Cleanup on error
			for (size_t j = 0; j < i; j++) {
				free(entries[j]);
			}
			free(entries);
			closedir(dir);
			free(elem.value.s);
			fprintf(stderr, "Fatal error in os::list: Failed to allocate entry string\n");
			qd_print_stack_trace(ctx);
			abort();
		}
		i++;
	}

	closedir(dir);
	free(elem.value.s);

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

	// Push errno (0 for success)
	qd_stack_push_int(ctx->st, 0);
	return (qd_exec_result){0};
}

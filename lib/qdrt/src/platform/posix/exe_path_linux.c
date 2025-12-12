// Linux implementation of exe_path_platform using /proc/self/exe
#define _POSIX_C_SOURCE 200809L
#include "../exe_path_platform.h"
#include <unistd.h>

int exe_path_platform_get(char* buffer, size_t buffer_size) {
	if (!buffer || buffer_size == 0) {
		return -1;
	}

	ssize_t len = readlink("/proc/self/exe", buffer, buffer_size);
	if (len == -1) {
		return -1;
	}

	// Null-terminate if there's room
	if ((size_t)len < buffer_size) {
		buffer[len] = '\0';
	}

	return (int)len;
}

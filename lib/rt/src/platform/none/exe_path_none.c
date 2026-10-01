// exe_path_platform for a target with no filesystem path to its own image
#include "../exe_path_platform.h"

int exe_path_platform_get(char* buffer, size_t buffer_size) {
	if (buffer && buffer_size > 0) {
		buffer[0] = '\0';
	}
	return -1;
}

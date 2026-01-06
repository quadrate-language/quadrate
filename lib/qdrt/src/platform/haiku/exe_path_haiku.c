// Haiku implementation of exe_path_platform using image_info API
#include "../exe_path_platform.h"
#include <image.h>
#include <string.h>

int exe_path_platform_get(char* buffer, size_t buffer_size) {
	if (!buffer || buffer_size == 0) {
		return -1;
	}

	image_info info;
	int32 cookie = 0;

	// Find the app image (first image, type B_APP_IMAGE)
	while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
		if (info.type == B_APP_IMAGE) {
			size_t len = strlen(info.name);
			if (len >= buffer_size) {
				// Buffer too small, return what we can
				len = buffer_size - 1;
			}
			memcpy(buffer, info.name, len);
			buffer[len] = '\0';
			return (int)len;
		}
	}

	return -1;
}

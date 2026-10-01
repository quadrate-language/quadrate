// dynlib_platform for a target with no dynamic linker, such as a
// microcontroller: nothing can be opened, and saying so is all there is to do
#include "../dynlib_platform.h"
#include <stddef.h>

dynlib_handle_t dynlib_platform_open(const char* path) {
	(void)path;
	return NULL;
}

void* dynlib_platform_symbol(dynlib_handle_t handle, const char* symbol) {
	(void)handle;
	(void)symbol;
	return NULL;
}

void dynlib_platform_close(dynlib_handle_t handle) {
	(void)handle;
}

const char* dynlib_platform_error(void) {
	return "dynamic libraries are not supported on this platform";
}

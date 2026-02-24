// POSIX implementation of dynlib_platform using dlopen/dlsym
#include "../dynlib_platform.h"
#include <dlfcn.h>
#include <stddef.h>

dynlib_handle_t dynlib_platform_open(const char* path) {
	if (!path) {
		return NULL;
	}
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* dynlib_platform_symbol(dynlib_handle_t handle, const char* symbol) {
	if (!handle || !symbol) {
		return NULL;
	}
	return dlsym(handle, symbol);
}

void dynlib_platform_close(dynlib_handle_t handle) {
	if (handle) {
		dlclose(handle);
	}
}

const char* dynlib_platform_error(void) {
	return dlerror();
}

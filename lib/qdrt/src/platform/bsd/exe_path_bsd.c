// FreeBSD/NetBSD/OpenBSD implementation of exe_path_platform using sysctl
#include "../exe_path_platform.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <string.h>

int exe_path_platform_get(char* buffer, size_t buffer_size) {
	if (!buffer || buffer_size == 0) {
		return -1;
	}

#if defined(__FreeBSD__)
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
	size_t len = buffer_size;
	if (sysctl(mib, 4, buffer, &len, NULL, 0) == 0) {
		return (int)len - 1; // Exclude null terminator
	}
#elif defined(__NetBSD__)
	// NetBSD: try /proc/curproc/exe first
	ssize_t len = readlink("/proc/curproc/exe", buffer, buffer_size - 1);
	if (len != -1) {
		buffer[len] = '\0';
		return (int)len;
	}
#elif defined(__OpenBSD__)
	// OpenBSD: no reliable way, try argv[0] fallback elsewhere
	// For now, return error
#endif

	return -1;
}

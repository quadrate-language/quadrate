// SPDX-License-Identifier: GPL-3.0-or-later
// Platform detection and common definitions for Quadrate

#ifndef QDPLATFORM_PLATFORM_H
#define QDPLATFORM_PLATFORM_H

// Platform detection
#if defined(__linux__)
	#define QD_PLATFORM_LINUX 1
	#define QD_PLATFORM_POSIX 1
	#define QD_PLATFORM_NAME "linux"
#elif defined(__APPLE__) && defined(__MACH__)
	#define QD_PLATFORM_MACOS 1
	#define QD_PLATFORM_POSIX 1
	#define QD_PLATFORM_NAME "macos"
#elif defined(__FreeBSD__)
	#define QD_PLATFORM_FREEBSD 1
	#define QD_PLATFORM_POSIX 1
	#define QD_PLATFORM_NAME "freebsd"
#elif defined(__OpenBSD__)
	#define QD_PLATFORM_OPENBSD 1
	#define QD_PLATFORM_POSIX 1
	#define QD_PLATFORM_NAME "openbsd"
#elif defined(__NetBSD__)
	#define QD_PLATFORM_NETBSD 1
	#define QD_PLATFORM_POSIX 1
	#define QD_PLATFORM_NAME "netbsd"
#elif defined(__HAIKU__)
	#define QD_PLATFORM_HAIKU 1
	#define QD_PLATFORM_POSIX 1
	#define QD_PLATFORM_NAME "haiku"
#elif defined(_WIN32) || defined(_WIN64)
	#define QD_PLATFORM_WINDOWS 1
	#define QD_PLATFORM_NAME "windows"
#else
	#define QD_PLATFORM_UNKNOWN 1
	#define QD_PLATFORM_NAME "unknown"
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
	#define QD_ARCH_X86_64 1
	#define QD_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
	#define QD_ARCH_X86 1
	#define QD_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
	#define QD_ARCH_ARM64 1
	#define QD_ARCH_NAME "arm64"
#elif defined(__arm__) || defined(_M_ARM)
	#define QD_ARCH_ARM 1
	#define QD_ARCH_NAME "arm"
#elif defined(__riscv)
	#define QD_ARCH_RISCV 1
	#if __riscv_xlen == 64
		#define QD_ARCH_NAME "riscv64"
	#else
		#define QD_ARCH_NAME "riscv32"
	#endif
#else
	#define QD_ARCH_UNKNOWN 1
	#define QD_ARCH_NAME "unknown"
#endif

// Pointer size
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
	#define QD_POINTER_SIZE 8
#else
	#define QD_POINTER_SIZE 4
#endif

// Export/import macros for shared libraries
#if defined(QD_PLATFORM_WINDOWS)
	#ifdef QD_BUILDING_DLL
		#define QD_EXPORT __declspec(dllexport)
	#else
		#define QD_EXPORT __declspec(dllimport)
	#endif
#else
	#define QD_EXPORT __attribute__((visibility("default")))
#endif

// Unused parameter macro
#define QD_UNUSED(x) (void)(x)

// Static assertion that works in C11 and C++11
#ifdef __cplusplus
	#define QD_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
	#define QD_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

#endif // QDPLATFORM_PLATFORM_H

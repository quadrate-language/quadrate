/**
 * @file http_internal.h
 * @brief Helpers the HTTP client and server share
 */

#ifndef QD_HTTP_INTERNAL_H
#define QD_HTTP_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

/** A growable, always NUL-terminated byte buffer; zero-initialise it, free its `data` */
typedef struct {
	char* data;
	size_t len;
	size_t cap;
} http_buf_t;

bool http_buf_add(http_buf_t* b, const char* s, size_t n);

__attribute__((format(printf, 2, 3))) bool http_buf_printf(http_buf_t* b, const char* fmt, ...);

/** The value of header `name` among "Name: value\r\n" lines, trimmed, or NULL */
const char* http_find_header(const char* headers, size_t len, const char* name, size_t* value_len);

#endif

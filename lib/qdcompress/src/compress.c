/**
 * @file compress.c
 * @brief Compression library implementation using zlib
 */

#include "qdcompress/compress.h"
#include <qdrt/context.h>
#include <qdrt/qd_string.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <zlib.h>
#include <stdlib.h>
#include <string.h>

/* Error codes matching module.qd */
#define COMPRESS_ERR_OK 1
#define COMPRESS_ERR_ALLOC 2
#define COMPRESS_ERR_INVALID_ARG 3
#define COMPRESS_ERR_COMPRESS 4
#define COMPRESS_ERR_DECOMPRESS 5

/** Helper to safely set error message */
static void set_error_msg(qd_context* ctx, const char* msg) {
	if (ctx->error_msg) free(ctx->error_msg);
	ctx->error_msg = strdup(msg);
}

/**
 * gzip - Compress string using gzip format with default level
 * Stack: (data:str -- compressed:str)!
 */
qd_exec_result usr_compress_gzip(qd_context* ctx) {
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);

	if (err != QD_STACK_OK || data_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "gzip: expected string argument");
		ctx->error_code = COMPRESS_ERR_INVALID_ARG;
		return (qd_exec_result){COMPRESS_ERR_INVALID_ARG};
	}

	const char* input = qd_string_data(data_elem.value.s);
	size_t input_len = qd_string_length(data_elem.value.s);

	/* Estimate output buffer size */
	size_t bound = compressBound(input_len) + 18; /* gzip header/trailer */
	uint8_t* output = malloc(bound);
	if (!output) {
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gzip: memory allocation failed");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	z_stream strm;
	memset(&strm, 0, sizeof(strm));

	/* windowBits = 15 + 16 = gzip format, level 6 (default) */
	int ret = deflateInit2(&strm, 6, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gzip: deflateInit2 failed");
		ctx->error_code = COMPRESS_ERR_COMPRESS;
		return (qd_exec_result){COMPRESS_ERR_COMPRESS};
	}

	strm.next_in = (Bytef*)input;
	strm.avail_in = (uInt)input_len;
	strm.next_out = output;
	strm.avail_out = (uInt)bound;

	ret = deflate(&strm, Z_FINISH);
	if (ret != Z_STREAM_END) {
		deflateEnd(&strm);
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gzip: deflate failed");
		ctx->error_code = COMPRESS_ERR_COMPRESS;
		return (qd_exec_result){COMPRESS_ERR_COMPRESS};
	}

	size_t output_len = strm.total_out;
	deflateEnd(&strm);
	qd_string_release(data_elem.value.s);

	/* Create result string from compressed data */
	qd_string_t* result = qd_string_create_with_length((const char*)output, output_len);
	free(output);

	if (!result) {
		set_error_msg(ctx, "gzip: failed to create result string");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	qd_push_s_ref(ctx, result);
	qd_string_release(result);
	qd_push_i(ctx, COMPRESS_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * gzip_level - Compress with specific compression level
 * Stack: (data:str level:i64 -- compressed:str)!
 */
qd_exec_result usr_compress_gzip_level(qd_context* ctx) {
	qd_stack_element_t level_elem, data_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &level_elem);
	if (err != QD_STACK_OK || level_elem.type != QD_STACK_TYPE_INT) {
		set_error_msg(ctx, "gzip_level: expected integer level");
		ctx->error_code = COMPRESS_ERR_INVALID_ARG;
		return (qd_exec_result){COMPRESS_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK || data_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "gzip_level: expected string data");
		ctx->error_code = COMPRESS_ERR_INVALID_ARG;
		return (qd_exec_result){COMPRESS_ERR_INVALID_ARG};
	}

	int level = (int)level_elem.value.i;
	if (level < 1) level = 1;
	if (level > 9) level = 9;

	const char* input = qd_string_data(data_elem.value.s);
	size_t input_len = qd_string_length(data_elem.value.s);

	size_t bound = compressBound(input_len) + 18;
	uint8_t* output = malloc(bound);
	if (!output) {
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gzip_level: memory allocation failed");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	z_stream strm;
	memset(&strm, 0, sizeof(strm));

	int ret = deflateInit2(&strm, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gzip_level: deflateInit2 failed");
		ctx->error_code = COMPRESS_ERR_COMPRESS;
		return (qd_exec_result){COMPRESS_ERR_COMPRESS};
	}

	strm.next_in = (Bytef*)input;
	strm.avail_in = (uInt)input_len;
	strm.next_out = output;
	strm.avail_out = (uInt)bound;

	ret = deflate(&strm, Z_FINISH);
	if (ret != Z_STREAM_END) {
		deflateEnd(&strm);
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gzip_level: deflate failed");
		ctx->error_code = COMPRESS_ERR_COMPRESS;
		return (qd_exec_result){COMPRESS_ERR_COMPRESS};
	}

	size_t output_len = strm.total_out;
	deflateEnd(&strm);
	qd_string_release(data_elem.value.s);

	qd_string_t* result = qd_string_create_with_length((const char*)output, output_len);
	free(output);

	if (!result) {
		set_error_msg(ctx, "gzip_level: failed to create result string");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	qd_push_s_ref(ctx, result);
	qd_string_release(result);
	qd_push_i(ctx, COMPRESS_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * gunzip - Decompress gzip data
 * Stack: (compressed:str -- data:str)!
 */
qd_exec_result usr_compress_gunzip(qd_context* ctx) {
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);

	if (err != QD_STACK_OK || data_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "gunzip: expected string argument");
		ctx->error_code = COMPRESS_ERR_INVALID_ARG;
		return (qd_exec_result){COMPRESS_ERR_INVALID_ARG};
	}

	const char* input = qd_string_data(data_elem.value.s);
	size_t input_len = qd_string_length(data_elem.value.s);

	/* Start with 4x input size estimate */
	size_t buf_size = input_len * 4;
	if (buf_size < 256) buf_size = 256;
	uint8_t* output = malloc(buf_size);
	if (!output) {
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gunzip: memory allocation failed");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	z_stream strm;
	memset(&strm, 0, sizeof(strm));

	/* windowBits = 15 + 16 = gzip format */
	int ret = inflateInit2(&strm, 15 + 16);
	if (ret != Z_OK) {
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "gunzip: inflateInit2 failed");
		ctx->error_code = COMPRESS_ERR_DECOMPRESS;
		return (qd_exec_result){COMPRESS_ERR_DECOMPRESS};
	}

	strm.next_in = (Bytef*)input;
	strm.avail_in = (uInt)input_len;
	strm.next_out = output;
	strm.avail_out = (uInt)buf_size;

	while (1) {
		ret = inflate(&strm, Z_NO_FLUSH);

		if (ret == Z_STREAM_END) {
			break;
		}

		if (ret != Z_OK) {
			inflateEnd(&strm);
			free(output);
			qd_string_release(data_elem.value.s);
			set_error_msg(ctx, "gunzip: inflate failed");
			ctx->error_code = COMPRESS_ERR_DECOMPRESS;
			return (qd_exec_result){COMPRESS_ERR_DECOMPRESS};
		}

		/* Need more output space */
		if (strm.avail_out == 0) {
			size_t new_size = buf_size * 2;
			uint8_t* new_buf = realloc(output, new_size);
			if (!new_buf) {
				inflateEnd(&strm);
				free(output);
				qd_string_release(data_elem.value.s);
				set_error_msg(ctx, "gunzip: reallocation failed");
				ctx->error_code = COMPRESS_ERR_ALLOC;
				return (qd_exec_result){COMPRESS_ERR_ALLOC};
			}
			output = new_buf;
			strm.next_out = output + buf_size;
			strm.avail_out = (uInt)(new_size - buf_size);
			buf_size = new_size;
		}
	}

	size_t output_len = strm.total_out;
	inflateEnd(&strm);
	qd_string_release(data_elem.value.s);

	qd_string_t* result = qd_string_create_with_length((const char*)output, output_len);
	free(output);

	if (!result) {
		set_error_msg(ctx, "gunzip: failed to create result string");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	qd_push_s_ref(ctx, result);
	qd_string_release(result);
	qd_push_i(ctx, COMPRESS_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * deflate - Compress with raw deflate (no header)
 * Stack: (data:str -- compressed:str)!
 */
qd_exec_result usr_compress_deflate(qd_context* ctx) {
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);

	if (err != QD_STACK_OK || data_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "deflate: expected string argument");
		ctx->error_code = COMPRESS_ERR_INVALID_ARG;
		return (qd_exec_result){COMPRESS_ERR_INVALID_ARG};
	}

	const char* input = qd_string_data(data_elem.value.s);
	size_t input_len = qd_string_length(data_elem.value.s);

	size_t bound = compressBound(input_len);
	uint8_t* output = malloc(bound);
	if (!output) {
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "deflate: memory allocation failed");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	z_stream strm;
	memset(&strm, 0, sizeof(strm));

	/* windowBits = -15 = raw deflate (no header) */
	int ret = deflateInit2(&strm, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "deflate: deflateInit2 failed");
		ctx->error_code = COMPRESS_ERR_COMPRESS;
		return (qd_exec_result){COMPRESS_ERR_COMPRESS};
	}

	strm.next_in = (Bytef*)input;
	strm.avail_in = (uInt)input_len;
	strm.next_out = output;
	strm.avail_out = (uInt)bound;

	ret = deflate(&strm, Z_FINISH);
	if (ret != Z_STREAM_END) {
		deflateEnd(&strm);
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "deflate: compress failed");
		ctx->error_code = COMPRESS_ERR_COMPRESS;
		return (qd_exec_result){COMPRESS_ERR_COMPRESS};
	}

	size_t output_len = strm.total_out;
	deflateEnd(&strm);
	qd_string_release(data_elem.value.s);

	qd_string_t* result = qd_string_create_with_length((const char*)output, output_len);
	free(output);

	if (!result) {
		set_error_msg(ctx, "deflate: failed to create result string");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	qd_push_s_ref(ctx, result);
	qd_string_release(result);
	qd_push_i(ctx, COMPRESS_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * inflate - Decompress raw deflate data
 * Stack: (compressed:str -- data:str)!
 */
qd_exec_result usr_compress_inflate(qd_context* ctx) {
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);

	if (err != QD_STACK_OK || data_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "inflate: expected string argument");
		ctx->error_code = COMPRESS_ERR_INVALID_ARG;
		return (qd_exec_result){COMPRESS_ERR_INVALID_ARG};
	}

	const char* input = qd_string_data(data_elem.value.s);
	size_t input_len = qd_string_length(data_elem.value.s);

	size_t buf_size = input_len * 4;
	if (buf_size < 256) buf_size = 256;
	uint8_t* output = malloc(buf_size);
	if (!output) {
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "inflate: memory allocation failed");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	z_stream strm;
	memset(&strm, 0, sizeof(strm));

	/* windowBits = -15 = raw deflate */
	int ret = inflateInit2(&strm, -15);
	if (ret != Z_OK) {
		free(output);
		qd_string_release(data_elem.value.s);
		set_error_msg(ctx, "inflate: inflateInit2 failed");
		ctx->error_code = COMPRESS_ERR_DECOMPRESS;
		return (qd_exec_result){COMPRESS_ERR_DECOMPRESS};
	}

	strm.next_in = (Bytef*)input;
	strm.avail_in = (uInt)input_len;
	strm.next_out = output;
	strm.avail_out = (uInt)buf_size;

	while (1) {
		ret = inflate(&strm, Z_NO_FLUSH);

		if (ret == Z_STREAM_END) {
			break;
		}

		if (ret != Z_OK) {
			inflateEnd(&strm);
			free(output);
			qd_string_release(data_elem.value.s);
			set_error_msg(ctx, "inflate: decompress failed");
			ctx->error_code = COMPRESS_ERR_DECOMPRESS;
			return (qd_exec_result){COMPRESS_ERR_DECOMPRESS};
		}

		if (strm.avail_out == 0) {
			size_t new_size = buf_size * 2;
			uint8_t* new_buf = realloc(output, new_size);
			if (!new_buf) {
				inflateEnd(&strm);
				free(output);
				qd_string_release(data_elem.value.s);
				set_error_msg(ctx, "inflate: reallocation failed");
				ctx->error_code = COMPRESS_ERR_ALLOC;
				return (qd_exec_result){COMPRESS_ERR_ALLOC};
			}
			output = new_buf;
			strm.next_out = output + buf_size;
			strm.avail_out = (uInt)(new_size - buf_size);
			buf_size = new_size;
		}
	}

	size_t output_len = strm.total_out;
	inflateEnd(&strm);
	qd_string_release(data_elem.value.s);

	qd_string_t* result = qd_string_create_with_length((const char*)output, output_len);
	free(output);

	if (!result) {
		set_error_msg(ctx, "inflate: failed to create result string");
		ctx->error_code = COMPRESS_ERR_ALLOC;
		return (qd_exec_result){COMPRESS_ERR_ALLOC};
	}

	qd_push_s_ref(ctx, result);
	qd_string_release(result);
	qd_push_i(ctx, COMPRESS_ERR_OK);
	return (qd_exec_result){0};
}

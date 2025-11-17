#include <stdbase64qd/base64.h>
#include <qdrt/stack.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Base64 alphabet
static const char base64_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Reverse lookup table for base64 decoding
// Returns 0-63 for valid chars, -1 for invalid, -2 for padding ('=')
static const int8_t base64_decode_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0-15
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 16-31
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,  // 32-47  ('+' at 43, '/' at 47)
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,  // 48-63  ('0'-'9' at 48-57, '=' at 61)
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  // 64-79  ('A'-'O')
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,  // 80-95  ('P'-'Z')
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  // 96-111 ('a'-'o')
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,  // 112-127 ('p'-'z')
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 128-143
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 144-159
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 160-175
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 176-191
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 192-207
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 208-223
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 224-239
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1   // 240-255
};

/* Helper: Pop integer from stack */
static qd_stack_error pop_int(qd_context* ctx, int64_t* value) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return err;
	}
	if (elem.type != QD_STACK_TYPE_INT) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.i;
	return QD_STACK_OK;
}

/* Helper: Pop pointer from stack */
static qd_stack_error pop_ptr(qd_context* ctx, void** value) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return err;
	}
	if (elem.type != QD_STACK_TYPE_PTR) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.p;
	return QD_STACK_OK;
}

/* Encode binary data to base64 string */
qd_exec_result usr_base64_encode(qd_context* ctx) {
	int64_t len;
	void* data;

	// Pop arguments: data:p len:i
	if (pop_int(ctx, &len) != QD_STACK_OK ||
			pop_ptr(ctx, &data) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (data == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in base64::encode";
		return (qd_exec_result){-1};
	}

	if (len < 0) {
		ctx->error_code = -1;
		ctx->error_msg = "Negative length in base64::encode";
		return (qd_exec_result){-1};
	}

	// Calculate output length: ceil(len/3)*4
	size_t out_len = ((size_t)len + 2) / 3 * 4;

	// Allocate output buffer (+1 for null terminator)
	char* out = malloc(out_len + 1);
	if (!out) {
		ctx->error_code = -1;
		ctx->error_msg = "Allocation failed in base64::encode";
		return (qd_exec_result){-1};
	}

	const uint8_t* in = (const uint8_t*)data;
	size_t out_pos = 0;
	size_t i;

	// Process complete 3-byte groups
	for (i = 0; i + 2 < (size_t)len; i += 3) {
		uint8_t b1 = in[i];
		uint8_t b2 = in[i + 1];
		uint8_t b3 = in[i + 2];

		// Split 24 bits into 4 6-bit values
		out[out_pos++] = base64_alphabet[(b1 >> 2) & 0x3F];
		out[out_pos++] = base64_alphabet[((b1 & 0x03) << 4) | ((b2 >> 4) & 0x0F)];
		out[out_pos++] = base64_alphabet[((b2 & 0x0F) << 2) | ((b3 >> 6) & 0x03)];
		out[out_pos++] = base64_alphabet[b3 & 0x3F];
	}

	// Handle remaining bytes (1 or 2)
	size_t remaining = (size_t)len - i;
	if (remaining == 1) {
		uint8_t b1 = in[i];
		out[out_pos++] = base64_alphabet[(b1 >> 2) & 0x3F];
		out[out_pos++] = base64_alphabet[(b1 & 0x03) << 4];
		out[out_pos++] = '=';
		out[out_pos++] = '=';
	} else if (remaining == 2) {
		uint8_t b1 = in[i];
		uint8_t b2 = in[i + 1];
		out[out_pos++] = base64_alphabet[(b1 >> 2) & 0x3F];
		out[out_pos++] = base64_alphabet[((b1 & 0x03) << 4) | ((b2 >> 4) & 0x0F)];
		out[out_pos++] = base64_alphabet[(b2 & 0x0F) << 2];
		out[out_pos++] = '=';
	}

	// Null terminate
	out[out_pos] = '\0';

	// Push result as string
	qd_exec_result result = qd_push_s(ctx, out);

	// qd_push_s makes a copy, so free the original
	free(out);

	return result;
}

/* Decode base64 string to binary data */
qd_exec_result usr_base64_decode(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in base64::decode: Failed to pop string\n");
		abort();
	}
	if (str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in base64::decode: Expected string, got %d\n", str_elem.type);
		free(str_elem.value.s);
		abort();
	}

	char* encoded = str_elem.value.s;
	size_t in_len = strlen(encoded);

	// Validate length (must be multiple of 4)
	if (in_len % 4 != 0) {
		fprintf(stderr, "Fatal error in base64::decode: Invalid base64 length (must be multiple of 4)\n");
		free(encoded);
		abort();
	}

	// Calculate maximum output length
	size_t max_out_len = in_len / 4 * 3;

	// Allocate output buffer
	uint8_t* out = malloc(max_out_len);
	if (!out) {
		fprintf(stderr, "Fatal error in base64::decode: Allocation failed\n");
		free(encoded);
		abort();
	}

	size_t out_pos = 0;

	// Decode 4-character groups
	for (size_t i = 0; i < in_len; i += 4) {
		int8_t v1 = base64_decode_table[(uint8_t)encoded[i]];
		int8_t v2 = base64_decode_table[(uint8_t)encoded[i + 1]];
		int8_t v3 = base64_decode_table[(uint8_t)encoded[i + 2]];
		int8_t v4 = base64_decode_table[(uint8_t)encoded[i + 3]];

		// Check for invalid characters
		if (v1 < 0 || v2 < 0) {
			fprintf(stderr, "Fatal error in base64::decode: Invalid base64 character\n");
			free(out);
			free(encoded);
			abort();
		}

		// Handle padding
		if (v3 == -2) {  // Third char is '='
			// Must be last group
			if (i + 4 != in_len) {
				fprintf(stderr, "Fatal error in base64::decode: Padding character not at end\n");
				free(out);
				free(encoded);
				abort();
			}
			// Only decode first byte
			out[out_pos++] = (uint8_t)((v1 << 2) | (v2 >> 4));
			break;
		}

		if (v4 == -2) {  // Fourth char is '='
			// Must be last group
			if (i + 4 != in_len) {
				fprintf(stderr, "Fatal error in base64::decode: Padding character not at end\n");
				free(out);
				free(encoded);
				abort();
			}
			// Decode first two bytes
			out[out_pos++] = (uint8_t)((v1 << 2) | (v2 >> 4));
			out[out_pos++] = (uint8_t)((v2 << 4) | (v3 >> 2));
			break;
		}

		// Check for other invalid values
		if (v3 < 0 || v4 < 0) {
			fprintf(stderr, "Fatal error in base64::decode: Invalid base64 character\n");
			free(out);
			free(encoded);
			abort();
		}

		// Decode 3 bytes from 4 chars
		out[out_pos++] = (uint8_t)((v1 << 2) | (v2 >> 4));
		out[out_pos++] = (uint8_t)((v2 << 4) | (v3 >> 2));
		out[out_pos++] = (uint8_t)((v3 << 6) | v4);
	}

	free(encoded);

	// Push data and length: data:p data_len:i
	qd_push_p(ctx, out);
	return qd_push_i(ctx, (int64_t)out_pos);
}

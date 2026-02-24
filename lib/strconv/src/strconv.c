#define _POSIX_C_SOURCE 200809L

#include <quadrate/strconv/strconv.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>

#define STRCONV_ERR_OK 1
#define STRCONV_ERR_INVALID 2

// Helper to convert digit value to character
static char digit_to_char(int digit) {
	if (digit < 10) {
		return (char)('0' + digit);
	}
	return (char)('a' + (digit - 10));
}

// Helper to convert character to digit value
static int char_to_digit(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'z') {
		return 10 + (c - 'a');
	}
	if (c >= 'A' && c <= 'Z') {
		return 10 + (c - 'A');
	}
	return -1;
}

// format_int - format integer in given base ( value:i base:i -- str:s )
int usr_strconv_format_int(qd_context* ctx) {
	qd_stack_element_t base_elem, value_elem;

	// Pop base
	qd_stack_error err = qd_stack_pop(ctx->st, &base_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::format_int: Stack underflow\n");
		abort();
	}

	// Pop value
	err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::format_int: Stack underflow\n");
		abort();
	}

	if (value_elem.type != QD_STACK_TYPE_INT || base_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strconv::format_int: Expected two integers\n");
		abort();
	}

	int64_t value = value_elem.value.i;
	int64_t base = base_elem.value.i;

	if (base < 2 || base > 36) {
		fprintf(stderr, "Fatal error in strconv::format_int: Base must be 2-36, got %ld\n", base);
		abort();
	}

	// Handle zero specially
	if (value == 0) {
		qd_push_s(ctx, "0");
		return (int){0};
	}

	// Buffer for result (64 bits = max 64 binary digits + sign + null)
	char buffer[66];
	char* ptr = buffer + sizeof(buffer) - 1;
	*ptr = '\0';

	int negative = 0;
	uint64_t uvalue;

	if (value < 0) {
		negative = 1;
		uvalue = (uint64_t)(-(value + 1)) + 1;
	} else {
		uvalue = (uint64_t)value;
	}

	// Convert digits in reverse
	while (uvalue > 0) {
		ptr--;
		*ptr = digit_to_char((int)(uvalue % (uint64_t)base));
		uvalue /= (uint64_t)base;
	}

	// Add negative sign if needed
	if (negative) {
		ptr--;
		*ptr = '-';
	}

	qd_push_s(ctx, ptr);
	return (int){0};
}

// parse_int - parse integer from string in given base ( str:s base:i -- value:i )
int usr_strconv_parse_int(qd_context* ctx) {
	qd_stack_element_t base_elem, str_elem;

	// Pop base
	qd_stack_error err = qd_stack_pop(ctx->st, &base_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::parse_int: Stack underflow\n");
		abort();
	}

	// Pop string
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::parse_int: Stack underflow\n");
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strconv::parse_int: Expected string\n");
		abort();
	}

	if (base_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strconv::parse_int: Expected integer base\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	int64_t base = base_elem.value.i;
	if (base < 2 || base > 36) {
		fprintf(stderr, "Fatal error in strconv::parse_int: Base must be 2-36, got %ld\n", base);
		qd_string_release(str_elem.value.s);
		abort();
	}

	const char* str = qd_string_data(str_elem.value.s);
	const char* ptr = str;

	// Skip leading whitespace
	while (*ptr && isspace((unsigned char)*ptr)) {
		ptr++;
	}

	// Check for negative sign
	int negative = 0;
	if (*ptr == '-') {
		negative = 1;
		ptr++;
	} else if (*ptr == '+') {
		ptr++;
	}

	// Parse digits
	int64_t result = 0;
	int digits_parsed = 0;

	while (*ptr) {
		int digit = char_to_digit(*ptr);
		if (digit < 0 || digit >= base) {
			break;
		}
		result = result * base + digit;
		digits_parsed++;
		ptr++;
	}

	if (digits_parsed == 0) {
		qd_string_release(str_elem.value.s);
		ctx->error_code = STRCONV_ERR_INVALID;
		qd_set_error_msg(ctx, "strconv::parse_int: invalid number format");
		qd_stack_push_int(ctx->st, (int64_t)STRCONV_ERR_INVALID);
		return (int){STRCONV_ERR_INVALID};
	}

	if (negative) {
		result = -result;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	qd_stack_push_int(ctx->st, STRCONV_ERR_OK);
	return (int){0};
}

// itoa - convert integer to string (base 10) ( value:i -- str:s )
int usr_strconv_itoa(qd_context* ctx) {
	qd_stack_element_t value_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::itoa: Stack underflow\n");
		abort();
	}

	if (value_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strconv::itoa: Expected integer\n");
		abort();
	}

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%ld", value_elem.value.i);
	qd_push_s(ctx, buffer);

	return (int){0};
}

// atoi - convert string to integer (base 10) ( str:s -- value:i )
int usr_strconv_atoi(qd_context* ctx) {
	qd_stack_element_t str_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::atoi: Stack underflow\n");
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strconv::atoi: Expected string\n");
		abort();
	}

	const char* str = qd_string_data(str_elem.value.s);
	char* endptr;
	int64_t result = strtol(str, &endptr, 10);

	// Check if any valid digits were parsed
	if (endptr == str) {
		qd_string_release(str_elem.value.s);
		ctx->error_code = STRCONV_ERR_INVALID;
		qd_set_error_msg(ctx, "strconv::atoi: invalid number format");
		qd_stack_push_int(ctx->st, (int64_t)STRCONV_ERR_INVALID);
		return (int){STRCONV_ERR_INVALID};
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	qd_stack_push_int(ctx->st, STRCONV_ERR_OK);
	return (int){0};
}

// format_float - format float to string ( value:f -- str:s )
int usr_strconv_format_float(qd_context* ctx) {
	qd_stack_element_t value_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::format_float: Stack underflow\n");
		abort();
	}

	if (value_elem.type != QD_STACK_TYPE_FLOAT) {
		fprintf(stderr, "Fatal error in strconv::format_float: Expected float\n");
		abort();
	}

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%g", value_elem.value.f);
	qd_push_s(ctx, buffer);

	return (int){0};
}

// parse_float - parse string to float ( str:s -- value:f )
int usr_strconv_parse_float(qd_context* ctx) {
	qd_stack_element_t str_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::parse_float: Stack underflow\n");
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strconv::parse_float: Expected string\n");
		abort();
	}

	const char* str = qd_string_data(str_elem.value.s);
	char* endptr;
	double result = strtod(str, &endptr);

	// Check if any valid digits were parsed
	if (endptr == str) {
		qd_string_release(str_elem.value.s);
		ctx->error_code = STRCONV_ERR_INVALID;
		qd_set_error_msg(ctx, "strconv::parse_float: invalid number format");
		qd_stack_push_int(ctx->st, (int64_t)STRCONV_ERR_INVALID);
		return (int){STRCONV_ERR_INVALID};
	}

	qd_string_release(str_elem.value.s);
	qd_push_f(ctx, result);
	qd_stack_push_int(ctx->st, STRCONV_ERR_OK);
	return (int){0};
}

// format_bool - format boolean to string ( value:i -- str:s )
int usr_strconv_format_bool(qd_context* ctx) {
	qd_stack_element_t value_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::format_bool: Stack underflow\n");
		abort();
	}

	if (value_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strconv::format_bool: Expected integer\n");
		abort();
	}

	qd_push_s(ctx, value_elem.value.i != 0 ? "true" : "false");

	return (int){0};
}

// parse_bool - parse string to boolean ( str:s -- value:i )
int usr_strconv_parse_bool(qd_context* ctx) {
	qd_stack_element_t str_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strconv::parse_bool: Stack underflow\n");
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strconv::parse_bool: Expected string\n");
		abort();
	}

	const char* str = qd_string_data(str_elem.value.s);

	int64_t result;
	if (strcasecmp(str, "true") == 0 || strcmp(str, "1") == 0) {
		result = 1;
	} else if (strcasecmp(str, "false") == 0 || strcmp(str, "0") == 0) {
		result = 0;
	} else {
		qd_string_release(str_elem.value.s);
		ctx->error_code = STRCONV_ERR_INVALID;
		qd_set_error_msg(ctx, "strconv::parse_bool: invalid boolean format");
		qd_stack_push_int(ctx->st, (int64_t)STRCONV_ERR_INVALID);
		return (int){STRCONV_ERR_INVALID};
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	qd_stack_push_int(ctx->st, STRCONV_ERR_OK);
	return (int){0};
}

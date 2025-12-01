#define _POSIX_C_SOURCE 200809L

#include <qdstrconv/strconv.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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
qd_exec_result usr_strconv_format_int(qd_context* ctx) {
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
		return (qd_exec_result){0};
	}

	// Buffer for result (64 bits = max 64 binary digits + sign + null)
	char buffer[66];
	char* ptr = buffer + sizeof(buffer) - 1;
	*ptr = '\0';

	int negative = 0;
	uint64_t uvalue;

	if (value < 0) {
		negative = 1;
		uvalue = (uint64_t)(-value);
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
	return (qd_exec_result){0};
}

// parse_int - parse integer from string in given base ( str:s base:i -- value:i )
qd_exec_result usr_strconv_parse_int(qd_context* ctx) {
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
		fprintf(stderr, "Fatal error in strconv::parse_int: Invalid number format: \"%s\"\n", str);
		qd_string_release(str_elem.value.s);
		abort();
	}

	if (negative) {
		result = -result;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}

// itoa - convert integer to string (base 10) ( value:i -- str:s )
qd_exec_result usr_strconv_itoa(qd_context* ctx) {
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

	return (qd_exec_result){0};
}

// atoi - convert string to integer (base 10) ( str:s -- value:i )
qd_exec_result usr_strconv_atoi(qd_context* ctx) {
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
		fprintf(stderr, "Fatal error in strconv::atoi: Invalid number format: \"%s\"\n", str);
		qd_string_release(str_elem.value.s);
		abort();
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);

	return (qd_exec_result){0};
}

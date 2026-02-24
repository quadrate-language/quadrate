#include <quadrate/math/math.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int usr_math_sin(qd_context* ctx) {
	// Compute sine of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::sin: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::sin: Failed to peek stack\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::sin: Type error (expected int or float, got %s)\n", type_name);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = sin(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int usr_math_cos(qd_context* ctx) {
	// Compute cosine of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::cos: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::cos: Failed to peek stack\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::cos: Type error (expected int or float, got %s)\n", type_name);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = cos(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int usr_math_tan(qd_context* ctx) {
	// Compute tangent of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::tan: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::tan: Failed to peek stack\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::tan: Type error (expected int or float, got %s)\n", type_name);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = tan(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int usr_math_asin(qd_context* ctx) {
	// Compute arcsine of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::asin: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::asin: Failed to peek stack\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::asin: Type error (expected int or float, got %s)\n", type_name);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: asin requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		fprintf(stderr, "Fatal error in math::asin: Domain error (value %f is outside [-1, 1])\n", value);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = asin(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int usr_math_acos(qd_context* ctx) {
	// Compute arccosine of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::acos: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::acos: Failed to peek stack\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::acos: Type error (expected int or float, got %s)\n", type_name);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: acos requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		fprintf(stderr, "Fatal error in math::acos: Domain error (value %f is outside [-1, 1])\n", value);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = acos(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int usr_math_atan(qd_context* ctx) {
	// Compute arctangent of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::atan: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::atan: Failed to peek stack\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::atan: Type error (expected int or float, got %s)\n", type_name);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = atan(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// sqrt - square root
int usr_math_sqrt(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::sqrt: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::sqrt: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::sqrt: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: sqrt requires non-negative values
	if (value < 0.0) {
		fprintf(stderr, "Fatal error in math::sqrt: Domain error (requires non-negative value, got %f)\n", value);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = sqrt(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// cbrt - cube root
int usr_math_cbrt(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::cbrt: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::cbrt: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::cbrt: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = cbrt(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// pow - exponentiation (base^exponent)
int usr_math_pow(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in math::pow: Stack underflow (requires 2 values)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t exponent_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &exponent_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::pow: Failed to pop exponent\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t base_elem;
	err = qd_stack_pop(ctx->st, &base_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::pow: Failed to pop base\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double base, exponent;
	if (base_elem.type == QD_STACK_TYPE_INT) {
		base = (double)base_elem.value.i;
	} else if (base_elem.type == QD_STACK_TYPE_FLOAT) {
		base = base_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::pow: Invalid base type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (exponent_elem.type == QD_STACK_TYPE_INT) {
		exponent = (double)exponent_elem.value.i;
	} else if (exponent_elem.type == QD_STACK_TYPE_FLOAT) {
		exponent = exponent_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::pow: Invalid exponent type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = pow(base, exponent);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// ln - natural logarithm (base e)
int usr_math_ln(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::ln: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::ln: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::ln: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: ln requires positive values
	if (value <= 0.0) {
		fprintf(stderr, "Fatal error in math::ln: Domain error (requires positive value, got %f)\n", value);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = log(value);  // log() is natural logarithm in C

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// log10 - base 10 logarithm
int usr_math_log10(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::log10: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::log10: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::log10: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: log10 requires positive values
	if (value <= 0.0) {
		fprintf(stderr, "Fatal error in math::log10: Domain error (requires positive value, got %f)\n", value);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = log10(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// ceil - ceiling (round up)
int usr_math_ceil(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::ceil: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::ceil: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::ceil: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = ceil(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// floor - floor (round down)
int usr_math_floor(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::floor: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::floor: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::floor: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = floor(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// round - round to nearest integer
int usr_math_round(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::round: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::round: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::round: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Round to nearest integer
	double result = round(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// atan2 - two-argument arctangent
int usr_math_atan2(qd_context* ctx) {
	// Stack: ( y x -- atan2(y,x) )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in math::atan2: Stack underflow (requires 2 values)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t x_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &x_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::atan2: Failed to pop x\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t y_elem;
	err = qd_stack_pop(ctx->st, &y_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::atan2: Failed to pop y\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double x, y;
	if (x_elem.type == QD_STACK_TYPE_INT) {
		x = (double)x_elem.value.i;
	} else if (x_elem.type == QD_STACK_TYPE_FLOAT) {
		x = x_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::atan2: Invalid x type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (y_elem.type == QD_STACK_TYPE_INT) {
		y = (double)y_elem.value.i;
	} else if (y_elem.type == QD_STACK_TYPE_FLOAT) {
		y = y_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::atan2: Invalid y type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = atan2(y, x);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// hypot - hypotenuse (sqrt(x^2 + y^2) without overflow)
int usr_math_hypot(qd_context* ctx) {
	// Stack: ( x y -- hypot(x,y) )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in math::hypot: Stack underflow (requires 2 values)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t y_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &y_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::hypot: Failed to pop y\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t x_elem;
	err = qd_stack_pop(ctx->st, &x_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::hypot: Failed to pop x\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double x, y;
	if (x_elem.type == QD_STACK_TYPE_INT) {
		x = (double)x_elem.value.i;
	} else if (x_elem.type == QD_STACK_TYPE_FLOAT) {
		x = x_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::hypot: Invalid x type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (y_elem.type == QD_STACK_TYPE_INT) {
		y = (double)y_elem.value.i;
	} else if (y_elem.type == QD_STACK_TYPE_FLOAT) {
		y = y_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::hypot: Invalid y type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = hypot(x, y);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// exp - exponential function (e^x)
int usr_math_exp(qd_context* ctx) {
	// Stack: ( x -- e^x )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::exp: Stack underflow (requires 1 value)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::exp: Failed to pop value\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::exp: Invalid type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = exp(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// fmod - floating-point modulo
int usr_math_fmod(qd_context* ctx) {
	// Stack: ( x y -- x mod y )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in math::fmod: Stack underflow (requires 2 values)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t y_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &y_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::fmod: Failed to pop y\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t x_elem;
	err = qd_stack_pop(ctx->st, &x_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::fmod: Failed to pop x\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double x, y;
	if (x_elem.type == QD_STACK_TYPE_INT) {
		x = (double)x_elem.value.i;
	} else if (x_elem.type == QD_STACK_TYPE_FLOAT) {
		x = x_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::fmod: Invalid x type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (y_elem.type == QD_STACK_TYPE_INT) {
		y = (double)y_elem.value.i;
	} else if (y_elem.type == QD_STACK_TYPE_FLOAT) {
		y = y_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::fmod: Invalid y type (expected int or float)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check for division by zero
	if (y == 0.0) {
		fprintf(stderr, "Fatal error in math::fmod: Division by zero\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = fmod(x, y);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// sinh - hyperbolic sine
int usr_math_sinh(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::sinh: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_float(ctx->st, sinh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// cosh - hyperbolic cosine
int usr_math_cosh(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::cosh: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_float(ctx->st, cosh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// tanh - hyperbolic tangent
int usr_math_tanh(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::tanh: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_float(ctx->st, tanh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// asinh - inverse hyperbolic sine
int usr_math_asinh(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::asinh: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_float(ctx->st, asinh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// acosh - inverse hyperbolic cosine
int usr_math_acosh(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::acosh: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	if (value < 1.0) {
		fprintf(stderr, "Fatal error in math::acosh: Domain error (requires value >= 1)\n");
		abort();
	}
	err = qd_stack_push_float(ctx->st, acosh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// atanh - inverse hyperbolic tangent
int usr_math_atanh(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::atanh: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	if (value <= -1.0 || value >= 1.0) {
		fprintf(stderr, "Fatal error in math::atanh: Domain error (requires -1 < value < 1)\n");
		abort();
	}
	err = qd_stack_push_float(ctx->st, atanh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// log2 - base-2 logarithm
int usr_math_log2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::log2: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	if (value <= 0.0) {
		fprintf(stderr, "Fatal error in math::log2: Domain error (requires positive value)\n");
		abort();
	}
	err = qd_stack_push_float(ctx->st, log2(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// exp2 - 2^x
int usr_math_exp2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::exp2: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_float(ctx->st, exp2(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// trunc - truncate toward zero
int usr_math_trunc(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::trunc: Stack underflow\n");
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_float(ctx->st, trunc(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}


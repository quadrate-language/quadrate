#include <quadrate/math/math.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/runtime.h>
#include <math.h>
#include <stdint.h>

int usr_math_sin(qd_context* ctx) {
	// Compute sine of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qd_fatal_raise(ctx, "math::sin", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::sin", "Failed to peek stack");
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		qd_fatal_raise(ctx, "math::sin", "Type error (expected int or float, got %s)", type_name);
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
		qd_fatal_raise(ctx, "math::cos", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::cos", "Failed to peek stack");
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		qd_fatal_raise(ctx, "math::cos", "Type error (expected int or float, got %s)", type_name);
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
		qd_fatal_raise(ctx, "math::tan", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::tan", "Failed to peek stack");
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		qd_fatal_raise(ctx, "math::tan", "Type error (expected int or float, got %s)", type_name);
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
		qd_fatal_raise(ctx, "math::asin", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::asin", "Failed to peek stack");
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		qd_fatal_raise(ctx, "math::asin", "Type error (expected int or float, got %s)", type_name);
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: asin requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		qd_fatal_raise(ctx, "math::asin", "Domain error (value %f is outside [-1, 1])", value);
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
		qd_fatal_raise(ctx, "math::acos", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::acos", "Failed to peek stack");
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		qd_fatal_raise(ctx, "math::acos", "Type error (expected int or float, got %s)", type_name);
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: acos requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		qd_fatal_raise(ctx, "math::acos", "Domain error (value %f is outside [-1, 1])", value);
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
		qd_fatal_raise(ctx, "math::atan", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::atan", "Failed to peek stack");
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		qd_fatal_raise(ctx, "math::atan", "Type error (expected int or float, got %s)", type_name);
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
		qd_fatal_raise(ctx, "math::sqrt", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::sqrt", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::sqrt", "Invalid type (expected int or float)");
	}

	// Check domain: sqrt requires non-negative values
	if (value < 0.0) {
		qd_fatal_raise(ctx, "math::sqrt", "Domain error (requires non-negative value, got %f)", value);
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
		qd_fatal_raise(ctx, "math::cbrt", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::cbrt", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::cbrt", "Invalid type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::pow", "Stack underflow (requires 2 values)");
	}

	qd_stack_element_t exponent_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &exponent_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::pow", "Failed to pop exponent");
	}

	qd_stack_element_t base_elem;
	err = qd_stack_pop(ctx->st, &base_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::pow", "Failed to pop base");
	}

	double base, exponent;
	if (base_elem.type == QD_STACK_TYPE_INT) {
		base = (double)base_elem.value.i;
	} else if (base_elem.type == QD_STACK_TYPE_FLOAT) {
		base = base_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::pow", "Invalid base type (expected int or float)");
	}

	if (exponent_elem.type == QD_STACK_TYPE_INT) {
		exponent = (double)exponent_elem.value.i;
	} else if (exponent_elem.type == QD_STACK_TYPE_FLOAT) {
		exponent = exponent_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::pow", "Invalid exponent type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::ln", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::ln", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::ln", "Invalid type (expected int or float)");
	}

	// Check domain: ln requires positive values
	if (value <= 0.0) {
		qd_fatal_raise(ctx, "math::ln", "Domain error (requires positive value, got %f)", value);
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
		qd_fatal_raise(ctx, "math::log10", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::log10", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::log10", "Invalid type (expected int or float)");
	}

	// Check domain: log10 requires positive values
	if (value <= 0.0) {
		qd_fatal_raise(ctx, "math::log10", "Domain error (requires positive value, got %f)", value);
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
		qd_fatal_raise(ctx, "math::ceil", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::ceil", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::ceil", "Invalid type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::floor", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::floor", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::floor", "Invalid type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::round", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::round", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::round", "Invalid type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::atan2", "Stack underflow (requires 2 values)");
	}

	qd_stack_element_t x_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &x_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::atan2", "Failed to pop x");
	}

	qd_stack_element_t y_elem;
	err = qd_stack_pop(ctx->st, &y_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::atan2", "Failed to pop y");
	}

	double x, y;
	if (x_elem.type == QD_STACK_TYPE_INT) {
		x = (double)x_elem.value.i;
	} else if (x_elem.type == QD_STACK_TYPE_FLOAT) {
		x = x_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::atan2", "Invalid x type (expected int or float)");
	}

	if (y_elem.type == QD_STACK_TYPE_INT) {
		y = (double)y_elem.value.i;
	} else if (y_elem.type == QD_STACK_TYPE_FLOAT) {
		y = y_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::atan2", "Invalid y type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::hypot", "Stack underflow (requires 2 values)");
	}

	qd_stack_element_t y_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &y_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::hypot", "Failed to pop y");
	}

	qd_stack_element_t x_elem;
	err = qd_stack_pop(ctx->st, &x_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::hypot", "Failed to pop x");
	}

	double x, y;
	if (x_elem.type == QD_STACK_TYPE_INT) {
		x = (double)x_elem.value.i;
	} else if (x_elem.type == QD_STACK_TYPE_FLOAT) {
		x = x_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::hypot", "Invalid x type (expected int or float)");
	}

	if (y_elem.type == QD_STACK_TYPE_INT) {
		y = (double)y_elem.value.i;
	} else if (y_elem.type == QD_STACK_TYPE_FLOAT) {
		y = y_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::hypot", "Invalid y type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::exp", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::exp", "Failed to pop value");
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::exp", "Invalid type (expected int or float)");
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
		qd_fatal_raise(ctx, "math::fmod", "Stack underflow (requires 2 values)");
	}

	qd_stack_element_t y_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &y_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::fmod", "Failed to pop y");
	}

	qd_stack_element_t x_elem;
	err = qd_stack_pop(ctx->st, &x_elem);
	if (err != QD_STACK_OK) {
		qd_fatal_raise(ctx, "math::fmod", "Failed to pop x");
	}

	double x, y;
	if (x_elem.type == QD_STACK_TYPE_INT) {
		x = (double)x_elem.value.i;
	} else if (x_elem.type == QD_STACK_TYPE_FLOAT) {
		x = x_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::fmod", "Invalid x type (expected int or float)");
	}

	if (y_elem.type == QD_STACK_TYPE_INT) {
		y = (double)y_elem.value.i;
	} else if (y_elem.type == QD_STACK_TYPE_FLOAT) {
		y = y_elem.value.f;
	} else {
		qd_fatal_raise(ctx, "math::fmod", "Invalid y type (expected int or float)");
	}

	// Check for division by zero
	if (y == 0.0) {
		qd_fatal_raise(ctx, "math::fmod", "Division by zero");
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
		qd_fatal_raise(ctx, "math::sinh", "Stack underflow");
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
		qd_fatal_raise(ctx, "math::cosh", "Stack underflow");
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
		qd_fatal_raise(ctx, "math::tanh", "Stack underflow");
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
		qd_fatal_raise(ctx, "math::asinh", "Stack underflow");
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
		qd_fatal_raise(ctx, "math::acosh", "Stack underflow");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	if (value < 1.0) {
		qd_fatal_raise(ctx, "math::acosh", "Domain error (requires value >= 1)");
	}
	err = qd_stack_push_float(ctx->st, acosh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// atanh - inverse hyperbolic tangent
int usr_math_atanh(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qd_fatal_raise(ctx, "math::atanh", "Stack underflow");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	if (value <= -1.0 || value >= 1.0) {
		qd_fatal_raise(ctx, "math::atanh", "Domain error (requires -1 < value < 1)");
	}
	err = qd_stack_push_float(ctx->st, atanh(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// log2 - base-2 logarithm
int usr_math_log2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qd_fatal_raise(ctx, "math::log2", "Stack underflow");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	if (value <= 0.0) {
		qd_fatal_raise(ctx, "math::log2", "Domain error (requires positive value)");
	}
	err = qd_stack_push_float(ctx->st, log2(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// exp2 - 2^x
int usr_math_exp2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qd_fatal_raise(ctx, "math::exp2", "Stack underflow");
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
		qd_fatal_raise(ctx, "math::trunc", "Stack underflow");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_float(ctx->st, trunc(value));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// sq - square (x^2)

// cb - cube (x^3)

// abs - absolute value

// min - minimum of two values

// max - maximum of two values

// fac - factorial
int usr_math_fac(qd_context* ctx) {
	// Stack: ( n:i -- n!:i )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qd_fatal_raise(ctx, "math::fac", "Stack underflow");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	if (elem.type != QD_STACK_TYPE_INT) {
		qd_fatal_raise(ctx, "math::fac", "Type error (expected int)");
	}

	int64_t n = elem.value.i;
	if (n < 0) {
		qd_fatal_raise(ctx, "math::fac", "Domain error (requires non-negative integer)");
	}

	int64_t result = 1;
	for (int64_t i = 2; i <= n; i++) {
		// Detect overflow before it happens (21! already exceeds int64).
		if (result > INT64_MAX / i) {
			qd_fatal_raise(ctx, "math::fac", "Result overflow (n too large)");
		}
		result *= i;
	}

	err = qd_stack_push_int(ctx->st, result);
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// inv - reciprocal (1/x)


// inf / nan - name the two IEEE 754 values that have no literal spelling.
// Division produces them (1.0 0.0 / is +infinity, 0.0 0.0 / is NaN), but without
// these a program cannot write one down -- which matters for the usual seeds:
// "smallest so far" starts at +infinity, "no result yet" at NaN.
int usr_math_inf(qd_context* ctx) {
	// HUGE_VAL, not INFINITY: the latter is a float and -Wdouble-promotion rejects widening it.
	qd_stack_error err = qd_stack_push_float(ctx->st, HUGE_VAL);
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

int usr_math_nan(qd_context* ctx) {
	// nan("") is double; the NAN macro is a float.
	qd_stack_error err = qd_stack_push_float(ctx->st, nan(""));
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// Classification predicates. NaN is not equal to itself, so `x x ==` is the only
// portable NaN test without these, and it reads as a mistake.
static int math_classify(qd_context* ctx, const char* name, int (*test)(double)) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qd_fatal_raise(ctx, name, "Stack underflow");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) return (int){-2};

	if (elem.type != QD_STACK_TYPE_INT && elem.type != QD_STACK_TYPE_FLOAT) {
		qd_fatal_raise(ctx, name, "Type error (expected a number)");
	}

	double value = (elem.type == QD_STACK_TYPE_INT) ? (double)elem.value.i : elem.value.f;
	err = qd_stack_push_int(ctx->st, test(value) ? 1 : 0);
	return (err != QD_STACK_OK) ? (int){-2} : (int){0};
}

// isnan/isinf/isfinite are macros, so they need real functions to take the address of.
static int math_test_is_nan(double v) {
	return isnan(v);
}

static int math_test_is_inf(double v) {
	return isinf(v);
}

static int math_test_is_finite(double v) {
	return isfinite(v);
}

int usr_math_is_nan(qd_context* ctx) {
	return math_classify(ctx, "math::is_nan", math_test_is_nan);
}

int usr_math_is_inf(qd_context* ctx) {
	return math_classify(ctx, "math::is_inf", math_test_is_inf);
}

int usr_math_is_finite(qd_context* ctx) {
	return math_classify(ctx, "math::is_finite", math_test_is_finite);
}

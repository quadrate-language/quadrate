#include <stdmathqd/math.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Helper function to dump stack contents for error messages
static void dump_stack(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	fprintf(stderr, "\nStack dump (%zu elements):\n", stack_size);

	if (stack_size == 0) {
		fprintf(stderr, "  (empty)\n");
		return;
	}

	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t elem;
		qd_stack_error err = qd_stack_element(ctx->st, i, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "  [%zu]: <error reading element>\n", i);
			continue;
		}

		fprintf(stderr, "  [%zu]: ", i);
		switch (elem.type) {
			case QD_STACK_TYPE_INT:
				fprintf(stderr, "int = %ld\n", elem.value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				fprintf(stderr, "float = %f\n", elem.value.f);
				break;
			case QD_STACK_TYPE_STR:
				fprintf(stderr, "str = \"%s\"\n", elem.value.s);
				break;
			case QD_STACK_TYPE_PTR:
				fprintf(stderr, "ptr = %p\n", elem.value.p);
				break;
			default:
				fprintf(stderr, "<unknown type>\n");
				break;
		}
	}
}

qd_exec_result usr_math_sin(qd_context* ctx) {
	// Compute sine of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::sin: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check it's a numeric type (int or float)
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::sin: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::sin: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = sin(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_math_cos(qd_context* ctx) {
	// Compute cosine of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::cos: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::cos: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::cos: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = cos(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_math_tan(qd_context* ctx) {
	// Compute tangent of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::tan: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::tan: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::tan: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = tan(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_math_asin(qd_context* ctx) {
	// Compute arcsine of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::asin: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::asin: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::asin: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: asin requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		fprintf(stderr, "Fatal error in math::asin: Domain error (value %f is outside [-1, 1])\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = asin(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_math_acos(qd_context* ctx) {
	// Compute arccosine of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::acos: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::acos: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::acos: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: acos requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		fprintf(stderr, "Fatal error in math::acos: Domain error (value %f is outside [-1, 1])\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = acos(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_math_atan(qd_context* ctx) {
	// Compute arctangent of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::atan: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::atan: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in math::atan: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = atan(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// sqrt - square root
qd_exec_result usr_math_sqrt(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::sqrt: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::sqrt: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: sqrt requires non-negative values
	if (value < 0.0) {
		fprintf(stderr, "Fatal error in math::sqrt: Domain error (requires non-negative value, got %f)\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = sqrt(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// sq - REPLACED BY QUADRATE IMPLEMENTATION (see lib/stdmathqd/qd/math/module.qd)

// cb - REPLACED BY QUADRATE IMPLEMENTATION (see lib/stdmathqd/qd/math/module.qd)

// cbrt - cube root
qd_exec_result usr_math_cbrt(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::cbrt: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::cbrt: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = cbrt(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// pow - exponentiation (base^exponent)
qd_exec_result usr_math_pow(qd_context* ctx) {
	// Pop two numeric values: base, then exponent
	// Push result as float
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in math::pow: Stack underflow (requires 2 values)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop exponent (top of stack)
	qd_stack_element_t exponent_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &exponent_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::pow: Failed to pop exponent\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop base
	qd_stack_element_t base_elem;
	err = qd_stack_pop(ctx->st, &base_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::pow: Failed to pop base\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Convert both to double
	double base, exponent;
	if (base_elem.type == QD_STACK_TYPE_INT) {
		base = (double)base_elem.value.i;
	} else if (base_elem.type == QD_STACK_TYPE_FLOAT) {
		base = base_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::pow: Invalid base type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (exponent_elem.type == QD_STACK_TYPE_INT) {
		exponent = (double)exponent_elem.value.i;
	} else if (exponent_elem.type == QD_STACK_TYPE_FLOAT) {
		exponent = exponent_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in math::pow: Invalid exponent type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Compute base^exponent
	double result = pow(base, exponent);

	// Push result as float
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// ln - natural logarithm (base e)
qd_exec_result usr_math_ln(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::ln: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::ln: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: ln requires positive values
	if (value <= 0.0) {
		fprintf(stderr, "Fatal error in math::ln: Domain error (requires positive value, got %f)\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = log(value);  // log() is natural logarithm in C

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// log10 - base 10 logarithm
qd_exec_result usr_math_log10(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::log10: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::log10: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: log10 requires positive values
	if (value <= 0.0) {
		fprintf(stderr, "Fatal error in math::log10: Domain error (requires positive value, got %f)\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = log10(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// ceil - ceiling (round up)
qd_exec_result usr_math_ceil(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::ceil: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::ceil: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = ceil(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// floor - floor (round down)
qd_exec_result usr_math_floor(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::floor: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::floor: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = floor(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// round - round to nearest integer
qd_exec_result usr_math_round(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in math::round: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in math::round: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Round to nearest integer
	double result = round(value);

	// Push result as float
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// abs - REPLACED BY QUADRATE IMPLEMENTATION (see lib/stdmathqd/qd/math/module.qd)
// min - REPLACED BY QUADRATE IMPLEMENTATION (see lib/stdmathqd/qd/math/module.qd)
// max - REPLACED BY QUADRATE IMPLEMENTATION (see lib/stdmathqd/qd/math/module.qd)
// fac - REPLACED BY QUADRATE IMPLEMENTATION (see lib/stdmathqd/qd/math/module.qd)
// inv - REPLACED BY QUADRATE IMPLEMENTATION (see lib/stdmathqd/qd/math/module.qd)


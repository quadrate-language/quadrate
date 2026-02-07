#include <qdtesting/testing.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <qdrt/qd_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to release a stack element (only strings need releasing)
static void release_element(qd_stack_element_t* elem) {
	if (elem->type == QD_STACK_TYPE_STR && elem->value.s != NULL) {
		qd_string_release(elem->value.s);
	}
}

// Helper function to print a stack element value
static void print_element_value(FILE* f, qd_stack_element_t* elem) {
	switch (elem->type) {
		case QD_STACK_TYPE_INT:
			fprintf(f, "%ld", elem->value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			fprintf(f, "%g", elem->value.f);
			break;
		case QD_STACK_TYPE_STR:
			fprintf(f, "\"%s\"", qd_string_data(elem->value.s));
			break;
		case QD_STACK_TYPE_PTR:
			fprintf(f, "ptr(%p)", elem->value.p);
			break;
		default:
			fprintf(f, "<unknown>");
			break;
	}
}

// Helper function to get type name
static const char* get_type_name(qd_stack_type type) {
	switch (type) {
		case QD_STACK_TYPE_INT: return "i64";
		case QD_STACK_TYPE_FLOAT: return "f64";
		case QD_STACK_TYPE_STR: return "str";
		case QD_STACK_TYPE_PTR: return "ptr";
		default: return "unknown";
	}
}

// Helper function to compare two stack elements for equality
static int elements_equal(qd_stack_element_t* a, qd_stack_element_t* b) {
	if (a->type != b->type) {
		return 0;
	}

	switch (a->type) {
		case QD_STACK_TYPE_INT:
			return a->value.i == b->value.i;
		case QD_STACK_TYPE_FLOAT:
			return a->value.f == b->value.f;
		case QD_STACK_TYPE_STR:
			return strcmp(qd_string_data(a->value.s), qd_string_data(b->value.s)) == 0;
		case QD_STACK_TYPE_PTR:
			return a->value.p == b->value.p;
		default:
			return 0;
	}
}

// assert_eq - Assert two values are equal: ( a b -- )
int usr_testing_assert_eq(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Assertion error: assert_eq requires 2 values on stack, have %zu\n", stack_size);
		qd_print_stack_trace(ctx);
		return (int){1};
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		release_element(&b);
		return (int){-2};
	}

	int equal = elements_equal(&a, &b);

	if (!equal) {
		fprintf(stderr, "Assertion failed: assert_eq\n");
		fprintf(stderr, "  expected: ");
		print_element_value(stderr, &a);
		fprintf(stderr, " (%s)\n", get_type_name(a.type));
		fprintf(stderr, "       got: ");
		print_element_value(stderr, &b);
		fprintf(stderr, " (%s)\n", get_type_name(b.type));
		qd_print_stack_trace(ctx);
		release_element(&a);
		release_element(&b);
		return (int){1};
	}

	release_element(&a);
	release_element(&b);
	return (int){0};
}

// assert_ne - Assert two values are not equal: ( a b -- )
int usr_testing_assert_ne(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Assertion error: assert_ne requires 2 values on stack, have %zu\n", stack_size);
		qd_print_stack_trace(ctx);
		return (int){1};
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		release_element(&b);
		return (int){-2};
	}

	int equal = elements_equal(&a, &b);

	if (equal) {
		fprintf(stderr, "Assertion failed: assert_ne\n");
		fprintf(stderr, "  both values are: ");
		print_element_value(stderr, &a);
		fprintf(stderr, " (%s)\n", get_type_name(a.type));
		qd_print_stack_trace(ctx);
		release_element(&a);
		release_element(&b);
		return (int){1};
	}

	release_element(&a);
	release_element(&b);
	return (int){0};
}

// assert_true - Assert value is truthy: ( v -- )
int usr_testing_assert_true(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Assertion error: assert_true requires 1 value on stack, have %zu\n", stack_size);
		qd_print_stack_trace(ctx);
		return (int){1};
	}

	qd_stack_element_t v;
	qd_stack_error err = qd_stack_pop(ctx->st, &v);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int truthy = 0;
	switch (v.type) {
		case QD_STACK_TYPE_INT:
			truthy = v.value.i != 0;
			break;
		case QD_STACK_TYPE_FLOAT:
			truthy = v.value.f != 0.0;
			break;
		case QD_STACK_TYPE_STR:
			truthy = qd_string_data(v.value.s)[0] != '\0';
			break;
		case QD_STACK_TYPE_PTR:
			truthy = v.value.p != NULL;
			break;
		default:
			truthy = 0;
			break;
	}

	if (!truthy) {
		fprintf(stderr, "Assertion failed: assert_true\n");
		fprintf(stderr, "  value: ");
		print_element_value(stderr, &v);
		fprintf(stderr, " (%s) is not truthy\n", get_type_name(v.type));
		qd_print_stack_trace(ctx);
		release_element(&v);
		return (int){1};
	}

	release_element(&v);
	return (int){0};
}

// assert_false - Assert value is falsy: ( v -- )
int usr_testing_assert_false(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Assertion error: assert_false requires 1 value on stack, have %zu\n", stack_size);
		qd_print_stack_trace(ctx);
		return (int){1};
	}

	qd_stack_element_t v;
	qd_stack_error err = qd_stack_pop(ctx->st, &v);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int falsy = 0;
	switch (v.type) {
		case QD_STACK_TYPE_INT:
			falsy = v.value.i == 0;
			break;
		case QD_STACK_TYPE_FLOAT:
			falsy = v.value.f == 0.0;
			break;
		case QD_STACK_TYPE_STR:
			falsy = qd_string_data(v.value.s)[0] == '\0';
			break;
		case QD_STACK_TYPE_PTR:
			falsy = v.value.p == NULL;
			break;
		default:
			falsy = 1;
			break;
	}

	if (!falsy) {
		fprintf(stderr, "Assertion failed: assert_false\n");
		fprintf(stderr, "  value: ");
		print_element_value(stderr, &v);
		fprintf(stderr, " (%s) is not falsy\n", get_type_name(v.type));
		qd_print_stack_trace(ctx);
		release_element(&v);
		return (int){1};
	}

	release_element(&v);
	return (int){0};
}

// fail - Unconditionally fail a test: ( msg -- )
int usr_testing_fail(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Test failed (no message provided)\n");
		qd_print_stack_trace(ctx);
		return (int){1};
	}

	qd_stack_element_t msg;
	qd_stack_error err = qd_stack_pop(ctx->st, &msg);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Test failed (error reading message)\n");
		qd_print_stack_trace(ctx);
		return (int){1};
	}

	if (msg.type == QD_STACK_TYPE_STR) {
		fprintf(stderr, "Test failed: %s\n", qd_string_data(msg.value.s));
	} else {
		fprintf(stderr, "Test failed: ");
		print_element_value(stderr, &msg);
		fprintf(stderr, "\n");
	}
	qd_print_stack_trace(ctx);

	release_element(&msg);
	return (int){1};
}

// Helper to pop two strings: ( haystack needle -- )
// Returns 0 on success, non-zero on failure
static int pop_two_strings(qd_context* ctx, const char* func_name,
                           qd_stack_element_t* haystack, qd_stack_element_t* needle) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Assertion error: %s requires 2 strings on stack, have %zu\n", func_name, stack_size);
		qd_print_stack_trace(ctx);
		return 1;
	}

	qd_stack_error err = qd_stack_pop(ctx->st, needle);
	if (err != QD_STACK_OK) {
		return -2;
	}
	err = qd_stack_pop(ctx->st, haystack);
	if (err != QD_STACK_OK) {
		release_element(needle);
		return -2;
	}

	if (haystack->type != QD_STACK_TYPE_STR || needle->type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Assertion error: %s requires 2 strings, got %s and %s\n",
				func_name, get_type_name(haystack->type), get_type_name(needle->type));
		qd_print_stack_trace(ctx);
		release_element(haystack);
		release_element(needle);
		return 1;
	}

	return 0;
}

// assert_contains - Assert string contains substring: ( haystack needle -- )
int usr_testing_assert_contains(qd_context* ctx) {
	qd_stack_element_t haystack, needle;
	int rc = pop_two_strings(ctx, "assert_contains", &haystack, &needle);
	if (rc != 0) {
		return rc;
	}

	const char* h = qd_string_data(haystack.value.s);
	const char* n = qd_string_data(needle.value.s);

	if (strstr(h, n) == NULL) {
		fprintf(stderr, "Assertion failed: assert_contains\n");
		fprintf(stderr, "  string: \"%s\"\n", h);
		fprintf(stderr, "  does not contain: \"%s\"\n", n);
		qd_print_stack_trace(ctx);
		release_element(&haystack);
		release_element(&needle);
		return (int){1};
	}

	release_element(&haystack);
	release_element(&needle);
	return (int){0};
}

// assert_starts_with - Assert string starts with prefix: ( str prefix -- )
int usr_testing_assert_starts_with(qd_context* ctx) {
	qd_stack_element_t haystack, needle;
	int rc = pop_two_strings(ctx, "assert_starts_with", &haystack, &needle);
	if (rc != 0) {
		return rc;
	}

	const char* h = qd_string_data(haystack.value.s);
	const char* n = qd_string_data(needle.value.s);
	size_t nlen = strlen(n);

	if (strncmp(h, n, nlen) != 0) {
		fprintf(stderr, "Assertion failed: assert_starts_with\n");
		fprintf(stderr, "  string: \"%s\"\n", h);
		fprintf(stderr, "  does not start with: \"%s\"\n", n);
		qd_print_stack_trace(ctx);
		release_element(&haystack);
		release_element(&needle);
		return (int){1};
	}

	release_element(&haystack);
	release_element(&needle);
	return (int){0};
}

// assert_ends_with - Assert string ends with suffix: ( str suffix -- )
int usr_testing_assert_ends_with(qd_context* ctx) {
	qd_stack_element_t haystack, needle;
	int rc = pop_two_strings(ctx, "assert_ends_with", &haystack, &needle);
	if (rc != 0) {
		return rc;
	}

	const char* h = qd_string_data(haystack.value.s);
	const char* n = qd_string_data(needle.value.s);
	size_t hlen = strlen(h);
	size_t nlen = strlen(n);

	if (nlen > hlen || strcmp(h + hlen - nlen, n) != 0) {
		fprintf(stderr, "Assertion failed: assert_ends_with\n");
		fprintf(stderr, "  string: \"%s\"\n", h);
		fprintf(stderr, "  does not end with: \"%s\"\n", n);
		qd_print_stack_trace(ctx);
		release_element(&haystack);
		release_element(&needle);
		return (int){1};
	}

	release_element(&haystack);
	release_element(&needle);
	return (int){0};
}

// assert_approx_eq - Assert two floats are approximately equal: ( a b epsilon -- )
int usr_testing_assert_approx_eq(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Assertion error: assert_approx_eq requires 3 values on stack (a b epsilon), have %zu\n", stack_size);
		qd_print_stack_trace(ctx);
		return (int){1};
	}

	qd_stack_element_t epsilon, b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &epsilon);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		release_element(&epsilon);
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		release_element(&epsilon);
		release_element(&b);
		return (int){-2};
	}

	// All three must be floats
	if (a.type != QD_STACK_TYPE_FLOAT || b.type != QD_STACK_TYPE_FLOAT || epsilon.type != QD_STACK_TYPE_FLOAT) {
		fprintf(stderr, "Assertion error: assert_approx_eq requires 3 floats\n");
		fprintf(stderr, "  got: %s, %s, %s\n", get_type_name(a.type), get_type_name(b.type), get_type_name(epsilon.type));
		qd_print_stack_trace(ctx);
		release_element(&a);
		release_element(&b);
		release_element(&epsilon);
		return (int){1};
	}

	double diff = a.value.f - b.value.f;
	if (diff < 0) {
		diff = -diff;
	}

	int approx_equal = diff <= epsilon.value.f;

	if (!approx_equal) {
		fprintf(stderr, "Assertion failed: assert_approx_eq\n");
		fprintf(stderr, "  expected: %g\n", a.value.f);
		fprintf(stderr, "       got: %g\n", b.value.f);
		fprintf(stderr, "      diff: %g\n", diff);
		fprintf(stderr, "   epsilon: %g\n", epsilon.value.f);
		qd_print_stack_trace(ctx);
		release_element(&a);
		release_element(&b);
		release_element(&epsilon);
		return (int){1};
	}

	release_element(&a);
	release_element(&b);
	release_element(&epsilon);
	return (int){0};
}

#define _DEFAULT_SOURCE
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/testing/testing.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// The assertions take the value under test first and the expected value second, as in
// `2 3 + 5 testing::assert_eq`, and report them that way round.

static void release_element(qd_stack_element_t* elem) {
	if (elem->type == QD_STACK_TYPE_STR && elem->value.s != NULL) {
		qd_string_release(elem->value.s);
	}
}

static void print_string(FILE* f, const qd_string_t* s) {
	fputc('"', f);
	fwrite(qd_string_data(s), 1, qd_string_length(s), f);
	fputc('"', f);
}

static void print_element(FILE* f, const qd_stack_element_t* elem) {
	switch (elem->type) {
	case QD_STACK_TYPE_INT:
		fprintf(f, "%ld", elem->value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		fprintf(f, "%g", elem->value.f);
		break;
	case QD_STACK_TYPE_STR:
		print_string(f, elem->value.s);
		break;
	case QD_STACK_TYPE_PTR:
		fprintf(f, "ptr(%p)", elem->value.p);
		break;
	case QD_STACK_TYPE_D128: {
		char text[QD_D128_STRING_MAX];
		qd_d128_format(elem->value.d, text, sizeof text);
		fprintf(f, "%sd", text);
		break;
	}
	}
	fprintf(f, " (%s)", qd_type_name(elem->type));
}

static bool strings_equal(const qd_string_t* a, const qd_string_t* b) {
	return qd_string_length(a) == qd_string_length(b) &&
		   memcmp(qd_string_data(a), qd_string_data(b), qd_string_length(a)) == 0;
}

static bool elements_equal(const qd_stack_element_t* a, const qd_stack_element_t* b) {
	if (a->type != b->type) {
		return false;
	}
	switch (a->type) {
	case QD_STACK_TYPE_INT:
		return a->value.i == b->value.i;
	case QD_STACK_TYPE_FLOAT:
		return a->value.f == b->value.f;
	case QD_STACK_TYPE_STR:
		return strings_equal(a->value.s, b->value.s);
	case QD_STACK_TYPE_PTR:
		return a->value.p == b->value.p;
	case QD_STACK_TYPE_D128:
		// IEEE equality, as `==` has it: 1.5 and 1.50 are equal, NaN is equal to nothing.
		return qd_d128_compare(a->value.d, b->value.d) == 0;
	}
	return false;
}

static bool truthy(const qd_stack_element_t* v) {
	switch (v->type) {
	case QD_STACK_TYPE_INT:
		return v->value.i != 0;
	case QD_STACK_TYPE_FLOAT:
		return v->value.f != 0.0;
	case QD_STACK_TYPE_STR:
		return qd_string_length(v->value.s) > 0;
	case QD_STACK_TYPE_PTR:
		return v->value.p != NULL;
	case QD_STACK_TYPE_D128:
		return !qd_d128_is_zero(v->value.d);
	}
	return false;
}

// What the test printed came before the failure, so it goes out first
static void begin_failure(const char* what) {
	fflush(stdout);
	fprintf(stderr, "Assertion failed: %s\n", what);
}

static int end_failure(qd_context* ctx) {
	qd_print_stack_trace(ctx);
	qd_assertion_failed(ctx);
	return 1;
}

// assert_eq: ( actual:any expected:any -- )
int usr_testing_assert_eq(qd_context* ctx) {
	qd_stack_element_t actual;
	qd_stack_element_t expected;
	qd_args(ctx, "testing::assert_eq", "vv", &actual, &expected);
	int rc = 0;
	if (!elements_equal(&actual, &expected)) {
		begin_failure("assert_eq");
		fputs("  expected: ", stderr);
		print_element(stderr, &expected);
		fputs("\n       got: ", stderr);
		print_element(stderr, &actual);
		fputc('\n', stderr);
		rc = end_failure(ctx);
	}
	release_element(&actual);
	release_element(&expected);
	return rc;
}

// assert_ne: ( a:any b:any -- )
int usr_testing_assert_ne(qd_context* ctx) {
	qd_stack_element_t a;
	qd_stack_element_t b;
	qd_args(ctx, "testing::assert_ne", "vv", &a, &b);
	int rc = 0;
	if (elements_equal(&a, &b)) {
		begin_failure("assert_ne");
		fputs("  both values are: ", stderr);
		print_element(stderr, &a);
		fputc('\n', stderr);
		rc = end_failure(ctx);
	}
	release_element(&a);
	release_element(&b);
	return rc;
}

static int assert_truth(qd_context* ctx, const char* op, const char* name, bool want) {
	qd_stack_element_t v;
	qd_args(ctx, op, "v", &v);
	int rc = 0;
	if (truthy(&v) != want) {
		begin_failure(name);
		fputs("  value: ", stderr);
		print_element(stderr, &v);
		fprintf(stderr, " is not %s\n", want ? "truthy" : "falsy");
		rc = end_failure(ctx);
	}
	release_element(&v);
	return rc;
}

// assert_true: ( v:any -- )
int usr_testing_assert_true(qd_context* ctx) {
	return assert_truth(ctx, "testing::assert_true", "assert_true", true);
}

// assert_false: ( v:any -- )
int usr_testing_assert_false(qd_context* ctx) {
	return assert_truth(ctx, "testing::assert_false", "assert_false", false);
}

// fail: ( msg:str -- )
int usr_testing_fail(qd_context* ctx) {
	qd_string_t* msg;
	qd_args(ctx, "testing::fail", "s", &msg);
	fflush(stdout);
	fputs("Test failed: ", stderr);
	fwrite(qd_string_data(msg), 1, qd_string_length(msg), stderr);
	fputc('\n', stderr);
	qd_string_release(msg);
	return end_failure(ctx);
}

// The string assertions: ( s:str part:str -- ), `found` says whether `part` is where the
// assertion wants it
static int assert_string(qd_context* ctx, const char* op, const char* name, const char* relation,
		bool (*found)(const qd_string_t* s, const qd_string_t* part)) {
	qd_string_t* s;
	qd_string_t* part;
	qd_args(ctx, op, "ss", &s, &part);
	int rc = 0;
	if (!found(s, part)) {
		begin_failure(name);
		fputs("  string: ", stderr);
		print_string(stderr, s);
		fprintf(stderr, "\n  %s: ", relation);
		print_string(stderr, part);
		fputc('\n', stderr);
		rc = end_failure(ctx);
	}
	qd_string_release(s);
	qd_string_release(part);
	return rc;
}

static bool contains(const qd_string_t* s, const qd_string_t* part) {
	const size_t n = qd_string_length(part);
	return n == 0 || memmem(qd_string_data(s), qd_string_length(s), qd_string_data(part), n) != NULL;
}

static bool starts_with(const qd_string_t* s, const qd_string_t* part) {
	const size_t n = qd_string_length(part);
	return n <= qd_string_length(s) && memcmp(qd_string_data(s), qd_string_data(part), n) == 0;
}

static bool ends_with(const qd_string_t* s, const qd_string_t* part) {
	const size_t n = qd_string_length(part);
	const size_t len = qd_string_length(s);
	return n <= len && memcmp(qd_string_data(s) + len - n, qd_string_data(part), n) == 0;
}

int usr_testing_assert_contains(qd_context* ctx) {
	return assert_string(ctx, "testing::assert_contains", "assert_contains", "does not contain", contains);
}

int usr_testing_assert_starts_with(qd_context* ctx) {
	return assert_string(ctx, "testing::assert_starts_with", "assert_starts_with", "does not start with", starts_with);
}

int usr_testing_assert_ends_with(qd_context* ctx) {
	return assert_string(ctx, "testing::assert_ends_with", "assert_ends_with", "does not end with", ends_with);
}

// assert_approx_eq: ( actual:f64 expected:f64 epsilon:f64 -- )
int usr_testing_assert_approx_eq(qd_context* ctx) {
	double actual;
	double expected;
	double epsilon;
	qd_args(ctx, "testing::assert_approx_eq", "fff", &actual, &expected, &epsilon);
	const double diff = actual > expected ? actual - expected : expected - actual;
	// A NaN anywhere fails: `diff <= epsilon` is false for it
	if (diff <= epsilon) {
		return 0;
	}
	begin_failure("assert_approx_eq");
	fprintf(stderr, "  expected: %g\n       got: %g\n      diff: %g\n   epsilon: %g\n", expected, actual, diff,
			epsilon);
	return end_failure(ctx);
}

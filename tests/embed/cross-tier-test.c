// A function registered through lib/qd must be callable from lib/interp when
// both share a context. This is what qd_interp_attach() is for.

#include <quadrate/interp/interp.h>
#include <quadrate/qd/qd.h>
#include <quadrate/rt/runtime.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int cond, const char* what) {
	if (!cond) {
		failures++;
		fprintf(stderr, "FAIL: %s\n", what);
	}
}

static int native_answer(qd_context* ctx, void* userdata) {
	(void)userdata;
	return qd_push_i(ctx, 42);
}

int main(void) {
	qd_context* ctx = qd_create_context(256);
	check(ctx != NULL, "context created");

	qd_module* mod = qd_get_module(ctx, "hw");
	check(mod != NULL, "module created");
	qd_register_function(mod, "answer", "( -- v:i64)", native_answer, NULL);

	// The interpreter shares the context, so it sees the registration
	qd_interp* interp = qd_interp_attach(ctx);
	check(interp != NULL, "interpreter attached");

	check(qd_interp_eval(interp, "hw::answer"), "qd-registered function runs under interp");
	if (!qd_interp_eval(interp, "2 *")) {
		check(0, "result composes with builtins");
	}

	qd_interp_value value;
	check(qd_interp_peek(interp, 0, &value), "a value is on the stack");
	check(value.i == 84, "42 from lib/qd, doubled by lib/interp");

	// An unregistered name still fails cleanly
	check(!qd_interp_eval(interp, "hw::missing"), "unregistered name is refused");

	// This binary links qd_dep (shared runtime) and interp_dep (static one), so
	// it holds two copies of the runtime's file-scope state. Recovery lives on
	// the context precisely so it survives that; reaching here at all means it
	// did, because the alternative is _exit(1).
	check(!qd_interp_eval(interp, "clear 1 0 /"), "division by zero is refused");
	check(strstr(qd_interp_error(interp), "Division by zero") != NULL, "and recovered, not fatal");

    qd_interp_destroy(interp);
	qd_release_modules(ctx);
	qd_free_context(ctx);

	if (failures == 0) {
		printf("cross-tier: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}

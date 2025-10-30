#include <quadrate/qd.h>
#include <stdlib.h>

qd_context* qd_create_context(size_t stack_size) {
	qd_context* ctx = static_cast<qd_context*>(malloc(sizeof(qd_context)));
	if (ctx) {
		qd_stack_error err = qd_stack_init(&ctx->st, stack_size);
		if (err != QD_STACK_OK) {
			free(ctx);
			return NULL;
		}
	}
	return ctx;
}

void qd_free_context(qd_context* ctx) {
	if (ctx == NULL) {
		return;
	}
	qd_stack_destroy(ctx->st);
	free(ctx);
}

qd_module* qd_get_module(qd_context*, const char*) {
	return NULL;
}

void qd_add_script(qd_module*, const char*) {
}

void qd_register_function(qd_module*, const char*, void (*)()) {
}

void qd_build(qd_module*) {
}

void qd_execute(qd_context*, const char*) {
}

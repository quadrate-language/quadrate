#include <quadrate/qd.h>
#include <stdlib.h>

qd_context_t* qd_create_context() {
	return NULL;
}

void qd_free_context(qd_context_t* ctx) {
}

qd_module_t* qd_get_module(qd_context_t* ctx, const char* name) {
	return NULL;
}

void qd_add_script(qd_module_t* mod, const char* script) {
}

void qd_register_function(qd_module_t* mod, const char* name, void(*fn)()) {
}

void qd_build(qd_module_t* mod) {
}

void qd_execute(qd_context_t* ctx, const char* fn) {
}


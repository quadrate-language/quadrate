#include <quadrate/qd.h>
#include <stdlib.h>

qd_context_t* qd_create_context() {
	return NULL;
}

void qd_free_context(qd_context_t*) {
}

qd_module_t* qd_get_module(qd_context_t*, const char*) {
	return NULL;
}

void qd_add_script(qd_module_t*, const char*) {
}

void qd_register_function(qd_module_t*, const char*, void (*)()) {
}

void qd_build(qd_module_t*) {
}

void qd_execute(qd_context_t*, const char*) {
}

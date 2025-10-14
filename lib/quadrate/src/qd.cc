#include <quadrate/qd.h>
#include <stdlib.h>

qd_context* qd_create_context() {
	return NULL;
}

void qd_free_context(qd_context*) {
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

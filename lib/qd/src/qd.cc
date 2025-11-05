#include <qd/qd.h>

// Context management functions (qd_create_context, qd_free_context) are now in libqdrt

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

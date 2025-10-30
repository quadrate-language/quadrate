#ifndef QUADRATE_QD_H
#define QUADRATE_QD_H

#include <quadrate/runtime/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qd_module qd_module;

qd_context* qd_create_context();
void qd_free_context(qd_context* ctx);
qd_module* qd_get_module(qd_context* ctx, const char* name);
void qd_add_script(qd_module* mod, const char* script);
void qd_register_function(qd_module* mod, const char* name, void (*fn)());
void qd_build(qd_module* mod);
void qd_execute(qd_context* ctx, const char* fn);

#ifdef __cplusplus
}
#endif

#endif

#ifndef QUADRATE_QD_H
#define QUADRATE_QD_H

#include <qdrt/exec_result.h>
#include <qdrt/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qd_module qd_module;

// Context management functions are now in qdrt/runtime.h (included above)

qd_module* qd_get_module(qd_context* ctx, const char* name);
void qd_add_script(qd_module* mod, const char* script);
void qd_register_function(qd_module* mod, const char* name, void (*fn)(void));
void qd_build(qd_module* mod);
void qd_execute(qd_context* ctx, const char* fn);

#ifdef __cplusplus
}
#endif

#endif

#ifndef QUADRATE_QD_H
#define QUADRATE_QD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qd_context qd_context_t;

qd_context_t* qd_create_context();
void qd_free_context(qd_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif


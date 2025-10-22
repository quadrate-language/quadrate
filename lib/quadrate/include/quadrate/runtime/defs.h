#ifndef QD_QUADRATE_RUNTIME_DEFS_H
#define QD_QUADRATE_RUNTIME_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#define QD_REQUIRE_STACK(ctx, n)                                                                                       \
	do {                                                                                                               \
		if (qd_stack_size((ctx)->st) < (n)) {                                                                          \
			qd_err_push((ctx), QD_STACK_ERR_UNDERFLOW);                                                                \
			return (qd_exec_result){.code = QD_STACK_ERR_UNDERFLOW};                                                   \
		}                                                                                                              \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif

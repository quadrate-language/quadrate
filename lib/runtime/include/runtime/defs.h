#ifndef QD_RUNTIME_DEFS_H
#define QD_RUNTIME_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#define QD_REQUIRE_STACK(ctx, n)                                                                                       \
	do {                                                                                                               \
		if (qd_stack_depth((ctx)->st) < (n)) {                                                                         \
			qd_err_push((ctx), QD_STACK_UNDERFLOW);                                                                    \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif

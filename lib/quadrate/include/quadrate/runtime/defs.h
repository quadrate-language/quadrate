#ifndef QD_QUADRATE_RUNTIME_DEFS_H
#define QD_QUADRATE_RUNTIME_DEFS_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_REQUIRE_STACK(ctx, n)                                                                                       \
	do {                                                                                                               \
		if (qd_stack_size((ctx)->st) < (n)) {                                                                          \
			fprintf(stderr, "Fatal error in %s: Stack underflow (required %zu elements, have %zu)\n",                  \
				__func__, (size_t)(n), qd_stack_size((ctx)->st));                                                      \
			abort();                                                                                                    \
		}                                                                                                              \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif

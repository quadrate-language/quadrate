#include <stdarg.h>

#define QD_STACK_DEPTH 1024
#define QD_STACK_ELEMENT_SIZE 4
#define __qd_real_t double

__qd_real_t __qd_stack[QD_STACK_DEPTH][QD_STACK_ELEMENT_SIZE] = {0};
int __qd_stack_ptr = 0;

void __qd_arg_push(__qd_real_t a, __qd_real_t b, __qd_real_t c, __qd_real_t d) {
	__qd_stack[__qd_stack_ptr][0] = a;
	__qd_stack[__qd_stack_ptr][1] = b;
	__qd_stack[__qd_stack_ptr][2] = c;
	__qd_stack[__qd_stack_ptr][3] = d;
	__qd_stack_ptr++;
}

void __qd_push(int n, ...) {
	va_list args;
	va_start(args, n);
	for (int i = 0; i < n; ++i) {
		__qd_arg_push(va_arg(args, __qd_real_t), 0.0, 0.0, 0.0);
	}
	va_end(args);
}

void __qd_pop() {
	--__qd_stack_ptr;
}


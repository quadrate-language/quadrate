#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "qd_base.h"

__qd_real_t __qd_stack[QD_STACK_DEPTH][QD_STACK_ELEMENT_SIZE] = {0};
int __qd_stack_ptr = 0;

void __qd_arg_push(__qd_real_t a, __qd_real_t b, __qd_real_t c, __qd_real_t d) {
	if (__qd_stack_ptr >= QD_STACK_DEPTH) {
		__qd_panic_stack_overflow();
	}
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
	if (__qd_stack_ptr == 0) {
		__qd_panic_stack_underflow();
	}
	--__qd_stack_ptr;
}

void __qd_add() {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2][0] += __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 2][1] += __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 2][2] += __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 2][3] += __qd_stack[__qd_stack_ptr - 1][3];
	__qd_pop();
}

void __qd_panic_stack_underflow() {
	fprintf(stderr, "panic: stack underflow\n");
	exit(1);
}

void __qd_panic_stack_overflow() {
	fprintf(stderr, "panic: stack overflow\n");
	exit(1);
}

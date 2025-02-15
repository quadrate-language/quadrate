#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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

void __qd_pop(int n) {
	if (__qd_stack_ptr == 0) {
		__qd_panic_stack_underflow();
	}
	--__qd_stack_ptr;
}

void __qd_add(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2][0] += __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 2][1] += __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 2][2] += __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 2][3] += __qd_stack[__qd_stack_ptr - 1][3];
	__qd_pop(0);
}

void __qd_sub(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2][0] -= __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 2][1] -= __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 2][2] -= __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 2][3] -= __qd_stack[__qd_stack_ptr - 1][3];
	__qd_pop(0);
}

void __qd_mul(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2][0] *= __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 2][1] *= __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 2][2] *= __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 2][3] *= __qd_stack[__qd_stack_ptr - 1][3];
	__qd_pop(0);
}

void __qd_div(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1][0] != 0.0) {
		__qd_stack[__qd_stack_ptr - 2][0] /= __qd_stack[__qd_stack_ptr - 1][0];
	}
	if (__qd_stack[__qd_stack_ptr - 1][1] != 0.0) {
		__qd_stack[__qd_stack_ptr - 2][1] /= __qd_stack[__qd_stack_ptr - 1][1];
	}
	if (__qd_stack[__qd_stack_ptr - 1][2] != 0.0) {
		__qd_stack[__qd_stack_ptr - 2][2] /= __qd_stack[__qd_stack_ptr - 1][2];
	}
	if (__qd_stack[__qd_stack_ptr - 1][3] != 0.0) {
		__qd_stack[__qd_stack_ptr - 2][3] /= __qd_stack[__qd_stack_ptr - 1][3];
	}
	__qd_pop(0);
}

void __qd_swap(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_real_t tmp[QD_STACK_ELEMENT_SIZE];
	tmp[0] = __qd_stack[__qd_stack_ptr - 2][0];
	tmp[1] = __qd_stack[__qd_stack_ptr - 2][1];
	tmp[2] = __qd_stack[__qd_stack_ptr - 2][2];
	tmp[3] = __qd_stack[__qd_stack_ptr - 2][3];
	__qd_stack[__qd_stack_ptr - 2][0] = __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 2][1] = __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 2][2] = __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 2][3] = __qd_stack[__qd_stack_ptr - 1][3];
	__qd_stack[__qd_stack_ptr - 1][0] = tmp[0];
	__qd_stack[__qd_stack_ptr - 1][1] = tmp[1];
	__qd_stack[__qd_stack_ptr - 1][2] = tmp[2];
	__qd_stack[__qd_stack_ptr - 1][3] = tmp[3];
}

void __qd_abs(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = fabs(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = fabs(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = fabs(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = fabs(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_acos(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = acos(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = acos(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = acos(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = acos(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_asin(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = asin(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = asin(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = asin(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = asin(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_atan(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = atan(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = atan(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = atan(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = atan(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_cos(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = cos(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = cos(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = cos(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = cos(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_sin(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = sin(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = sin(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = sin(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = sin(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_tan(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = tan(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = tan(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = tan(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = tan(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_cbrt(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = cbrt(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = cbrt(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = cbrt(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = cbrt(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_ceil(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = ceil(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = ceil(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = ceil(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = ceil(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_floor(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = floor(__qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 1][1] = floor(__qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 1][2] = floor(__qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 1][3] = floor(__qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_clear(int n) {
	__qd_stack_ptr = 0;
}

void __qd_dec(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	--__qd_stack[__qd_stack_ptr - 1][0];
	--__qd_stack[__qd_stack_ptr - 1][1];
	--__qd_stack[__qd_stack_ptr - 1][2];
	--__qd_stack[__qd_stack_ptr - 1][3];
}

void __qd_inc(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	++__qd_stack[__qd_stack_ptr - 1][0];
	++__qd_stack[__qd_stack_ptr - 1][1];
	++__qd_stack[__qd_stack_ptr - 1][2];
	++__qd_stack[__qd_stack_ptr - 1][3];
}

void __qd_dup(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 1][0], __qd_stack[__qd_stack_ptr - 1][1], __qd_stack[__qd_stack_ptr - 1][2], __qd_stack[__qd_stack_ptr - 1][3]);
}

void __qd_inv(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1][0] != 0.0) {
		__qd_stack[__qd_stack_ptr - 1][0] = 1.0 / __qd_stack[__qd_stack_ptr - 1][0];
	}
	if (__qd_stack[__qd_stack_ptr - 1][1] != 0.0) {
		__qd_stack[__qd_stack_ptr - 1][1] = 1.0 / __qd_stack[__qd_stack_ptr - 1][1];
	}
	if (__qd_stack[__qd_stack_ptr - 1][2] != 0.0) {
		__qd_stack[__qd_stack_ptr - 1][2] = 1.0 / __qd_stack[__qd_stack_ptr - 1][2];
	}
	if (__qd_stack[__qd_stack_ptr - 1][3] != 0.0) {
		__qd_stack[__qd_stack_ptr - 1][3] = 1.0 / __qd_stack[__qd_stack_ptr - 1][3];
	}
}

void __qd_ln(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1][0] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][0] = log(__qd_stack[__qd_stack_ptr - 1][0]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][1] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][1] = log(__qd_stack[__qd_stack_ptr - 1][1]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][2] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][2] = log(__qd_stack[__qd_stack_ptr - 1][2]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][3] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][3] = log(__qd_stack[__qd_stack_ptr - 1][3]);
	}
}

void __qd_log10(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1][0] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][0] = log10(__qd_stack[__qd_stack_ptr - 1][0]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][1] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][1] = log10(__qd_stack[__qd_stack_ptr - 1][1]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][2] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][2] = log10(__qd_stack[__qd_stack_ptr - 1][2]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][3] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1][3] = log10(__qd_stack[__qd_stack_ptr - 1][3]);
	}
}

void __qd_neg(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] = -__qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 1][1] = -__qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 1][2] = -__qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 1][3] = -__qd_stack[__qd_stack_ptr - 1][3];
}

void __qd_over(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 2][0], __qd_stack[__qd_stack_ptr - 2][1], __qd_stack[__qd_stack_ptr - 2][2], __qd_stack[__qd_stack_ptr - 2][3]);
}

void __qd_pow(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2][0] = pow(__qd_stack[__qd_stack_ptr - 2][0], __qd_stack[__qd_stack_ptr - 1][0]);
	__qd_stack[__qd_stack_ptr - 2][1] = pow(__qd_stack[__qd_stack_ptr - 2][1], __qd_stack[__qd_stack_ptr - 1][1]);
	__qd_stack[__qd_stack_ptr - 2][2] = pow(__qd_stack[__qd_stack_ptr - 2][2], __qd_stack[__qd_stack_ptr - 1][2]);
	__qd_stack[__qd_stack_ptr - 2][3] = pow(__qd_stack[__qd_stack_ptr - 2][3], __qd_stack[__qd_stack_ptr - 1][3]);
	__qd_pop(0);
}

void __qd_rot(int n) {
	if (__qd_stack_ptr < 3) {
		__qd_panic_stack_underflow();
	}
	__qd_real_t tmp[QD_STACK_ELEMENT_SIZE];
	tmp[0] = __qd_stack[__qd_stack_ptr - 3][0];
	tmp[1] = __qd_stack[__qd_stack_ptr - 3][1];
	tmp[2] = __qd_stack[__qd_stack_ptr - 3][2];
	tmp[3] = __qd_stack[__qd_stack_ptr - 3][3];
	__qd_stack[__qd_stack_ptr - 3][0] = __qd_stack[__qd_stack_ptr - 2][0];
	__qd_stack[__qd_stack_ptr - 3][1] = __qd_stack[__qd_stack_ptr - 2][1];
	__qd_stack[__qd_stack_ptr - 3][2] = __qd_stack[__qd_stack_ptr - 2][2];
	__qd_stack[__qd_stack_ptr - 3][3] = __qd_stack[__qd_stack_ptr - 2][3];
	__qd_stack[__qd_stack_ptr - 2][0] = __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 2][1] = __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 2][2] = __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 2][3] = __qd_stack[__qd_stack_ptr - 1][3];
	__qd_stack[__qd_stack_ptr - 1][0] = tmp[0];
	__qd_stack[__qd_stack_ptr - 1][1] = tmp[1];
	__qd_stack[__qd_stack_ptr - 1][2] = tmp[2];
	__qd_stack[__qd_stack_ptr - 1][3] = tmp[3];
}

void __qd_rot2(int n) {
	if (__qd_stack_ptr < 4) {
		__qd_panic_stack_underflow();
	}
	__qd_real_t tmp[QD_STACK_ELEMENT_SIZE];
	tmp[0] = __qd_stack[__qd_stack_ptr - 4][0];
	tmp[1] = __qd_stack[__qd_stack_ptr - 4][1];
	tmp[2] = __qd_stack[__qd_stack_ptr - 4][2];
	tmp[3] = __qd_stack[__qd_stack_ptr - 4][3];
	__qd_stack[__qd_stack_ptr - 4][0] = __qd_stack[__qd_stack_ptr - 2][0];
	__qd_stack[__qd_stack_ptr - 4][1] = __qd_stack[__qd_stack_ptr - 2][1];
	__qd_stack[__qd_stack_ptr - 4][2] = __qd_stack[__qd_stack_ptr - 2][2];
	__qd_stack[__qd_stack_ptr - 4][3] = __qd_stack[__qd_stack_ptr - 2][3];
	__qd_stack[__qd_stack_ptr - 2][0] = tmp[0];
	__qd_stack[__qd_stack_ptr - 2][1] = tmp[1];
	__qd_stack[__qd_stack_ptr - 2][2] = tmp[2];
	__qd_stack[__qd_stack_ptr - 2][3] = tmp[3];
	tmp[0] = __qd_stack[__qd_stack_ptr - 3][0];
	tmp[1] = __qd_stack[__qd_stack_ptr - 3][1];
	tmp[2] = __qd_stack[__qd_stack_ptr - 3][2];
	tmp[3] = __qd_stack[__qd_stack_ptr - 3][3];
	__qd_stack[__qd_stack_ptr - 3][0] = __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 3][1] = __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 3][2] = __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 3][3] = __qd_stack[__qd_stack_ptr - 1][3];
	__qd_stack[__qd_stack_ptr - 1][0] = tmp[0];
}

void __qd_sq(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1][0] *= __qd_stack[__qd_stack_ptr - 1][0];
	__qd_stack[__qd_stack_ptr - 1][1] *= __qd_stack[__qd_stack_ptr - 1][1];
	__qd_stack[__qd_stack_ptr - 1][2] *= __qd_stack[__qd_stack_ptr - 1][2];
	__qd_stack[__qd_stack_ptr - 1][3] *= __qd_stack[__qd_stack_ptr - 1][3];
}

void __qd_sqrt(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1][0] >= 0.0) {
		__qd_stack[__qd_stack_ptr - 1][0] = sqrt(__qd_stack[__qd_stack_ptr - 1][0]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][1] >= 0.0) {
		__qd_stack[__qd_stack_ptr - 1][1] = sqrt(__qd_stack[__qd_stack_ptr - 1][1]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][2] >= 0.0) {
		__qd_stack[__qd_stack_ptr - 1][2] = sqrt(__qd_stack[__qd_stack_ptr - 1][2]);
	}
	if (__qd_stack[__qd_stack_ptr - 1][3] >= 0.0) {
		__qd_stack[__qd_stack_ptr - 1][3] = sqrt(__qd_stack[__qd_stack_ptr - 1][3]);
	}
}

void __qd_panic_stack_underflow() {
	fprintf(stderr, "panic: stack underflow\n");
	exit(1);
}

void __qd_panic_stack_overflow() {
	fprintf(stderr, "panic: stack overflow\n");
	exit(1);
}

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "qd_base.h"

__qd_real_t __qd_stack[QD_STACK_DEPTH] = {0};
int __qd_stack_ptr = 0;

void __qd_arg_push(__qd_real_t x) {
	if (__qd_stack_ptr >= QD_STACK_DEPTH) {
		__qd_panic_stack_overflow();
	}
	__qd_stack[__qd_stack_ptr] = x;
	__qd_stack_ptr++;
}

void __qd_push(int n, ...) {
	va_list args;
	va_start(args, n);
	for (int i = 0; i < n; ++i) {
		__qd_arg_push(va_arg(args, __qd_real_t));
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
	__qd_stack[__qd_stack_ptr - 2] += __qd_stack[__qd_stack_ptr - 1];
	__qd_pop(0);
}

void __qd_sub(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2] -= __qd_stack[__qd_stack_ptr - 1];
	__qd_pop(0);
}

void __qd_mul(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2] *= __qd_stack[__qd_stack_ptr - 1];
	__qd_pop(0);
}

void __qd_div(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1] != 0.0) {
		__qd_stack[__qd_stack_ptr - 2] /= __qd_stack[__qd_stack_ptr - 1];
	}
	__qd_pop(0);
}

void __qd_swap(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 2];
	__qd_stack[__qd_stack_ptr - 2] = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack[__qd_stack_ptr - 1] = tmp;
}

void __qd_abs(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = fabs(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_acos(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = acos(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_asin(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = asin(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_atan(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = atan(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_cos(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = cos(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_sin(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = sin(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_tan(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = tan(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_cbrt(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = cbrt(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_ceil(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = ceil(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_floor(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = floor(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_clear(int n) {
	__qd_stack_ptr = 0;
}

void __qd_dec(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	--__qd_stack[__qd_stack_ptr - 1];
}

void __qd_inc(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	++__qd_stack[__qd_stack_ptr - 1];
}

void __qd_dup(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_inv(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1] != 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = 1.0 / __qd_stack[__qd_stack_ptr - 1];
	}
}

void __qd_ln(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = log(__qd_stack[__qd_stack_ptr - 1]);
	}
}

void __qd_log10(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = log10(__qd_stack[__qd_stack_ptr - 1]);
	}
}

void __qd_neg(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] = -__qd_stack[__qd_stack_ptr - 1];
}

void __qd_over(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 2]);
}

void __qd_pow(int n) {
	if (__qd_stack_ptr < 2) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 2] = pow(__qd_stack[__qd_stack_ptr - 2], __qd_stack[__qd_stack_ptr - 1]);
	__qd_pop(0);
}

void __qd_read(int n) {
	char input[1024];
	if (fgets(input, sizeof(input), stdin) != NULL) {
		char* token = strtok(input, " ");
		while (token != NULL) {
			__qd_arg_push(atof(token));
			token = strtok(NULL, " ");
		}
	}
}

void __qd_rot(int n) {
	if (__qd_stack_ptr < 3) {
		__qd_panic_stack_underflow();
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 3];
	__qd_stack[__qd_stack_ptr - 3] = __qd_stack[__qd_stack_ptr - 2];
	__qd_stack[__qd_stack_ptr - 2] = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack[__qd_stack_ptr - 1] = tmp;
}

void __qd_rot2(int n) {
	if (__qd_stack_ptr < 4) {
		__qd_panic_stack_underflow();
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 4];
	__qd_stack[__qd_stack_ptr - 4] = __qd_stack[__qd_stack_ptr - 2];
	__qd_stack[__qd_stack_ptr - 2] = tmp;
	tmp = __qd_stack[__qd_stack_ptr - 3];
	__qd_stack[__qd_stack_ptr - 3] = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack[__qd_stack_ptr - 1] = tmp;
}

void __qd_sq(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	__qd_stack[__qd_stack_ptr - 1] *= __qd_stack[__qd_stack_ptr - 1];
}

void __qd_sqrt(int n) {
	if (__qd_stack_ptr < 1) {
		__qd_panic_stack_underflow();
	}
	if (__qd_stack[__qd_stack_ptr - 1] >= 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = sqrt(__qd_stack[__qd_stack_ptr - 1]);
	}
}

void __qd_write(int n) {
	for (int i = 0; i < __qd_stack_ptr; ++i) {
		if (i != 0) {
			printf(" ");
		}
		printf("%f", __qd_stack[i]);
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


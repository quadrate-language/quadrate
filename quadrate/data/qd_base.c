// Copyright 2025 Joachim Klahr
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "qd_base.h"

__qd_real_t __qd_stack[QD_STACK_DEPTH] = {0};
__qd_real_t __qd_mark_stacks[QD_MARK_STACK_DEPTH][QD_STACK_DEPTH] = {0};
__qd_real_t __qd_err = 0.0;
int __qd_stack_ptr = 0;
int __qd_mark_stack_ptr = 0;
int __qd_mark_stacks_ptrs[QD_MARK_STACK_DEPTH] = {0};
int __qd_precision = 2;

__qd_real_t __qd_ptr_to_real(void* ptr) {
	__qd_real_t result = 0;
	memcpy(&result, &ptr, sizeof(result));
	return result;
}

void* __qd_real_to_ptr(__qd_real_t ptr) {
	void* result;
	memcpy(&result, &ptr, sizeof(result));
	return result;
}

__qd_real_t __qd_fnptr_to_real(void (*fn)(int, ...)) {
	__qd_real_t result = 0;
	memcpy(&result, &fn, sizeof(result));
	return result;
}

void (*__qd_real_to_fnptr(__qd_real_t ptr))(int, ...) {
	void (*result)(int, ...);
	memcpy(&result, &ptr, sizeof(result));
	return result;
}

void __qd_arg_push(__qd_real_t x) {
	if (__qd_stack_ptr >= QD_STACK_DEPTH) {
		__qd___panic_stack_overflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr] = x;
	__qd_stack_ptr++;
}

void __qd_error(int n, ...) {
	__qd_arg_push(__qd_err);
	__qd_err = 0.0;
}

void __qd_push(int n, ...) {
	va_list args;
	va_start(args, n);
	for (int i = 0; i < n; ++i) {
		__qd_arg_push(va_arg(args, __qd_real_t));
	}
	va_end(args);
}

void __qd_pop(int n, ...) {
	va_list args;
	va_start(args, n);
	if (n == 0) {
		if (__qd_stack_ptr == 0) {
			__qd___panic_stack_underflow(0);
			return;
		}
		--__qd_stack_ptr;
	} else {
		int x = (int)va_arg(args, __qd_real_t);
		for (int i = 0; i < x; ++i) {
			if (__qd_stack_ptr == 0) {
				__qd___panic_stack_underflow(0);
				return;
			}
			--__qd_stack_ptr;
		}
	}
	va_end(args);
}

void __qd_avg(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd_stack_ptr = 0;
		__qd_arg_push(0);
		return;
	}

	__qd_real_t summed = 0;
	for (int i = 0; i < __qd_stack_ptr; ++i) {
		summed += __qd_stack[i];
	}

	summed /= (__qd_stack_ptr);
	__qd_stack_ptr = 0;
	__qd_arg_push(summed);
}

void __qd_call(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	void (*fn_ptr)(int, ...) = __qd_real_to_fnptr(__qd_stack[--__qd_stack_ptr]);
	if (fn_ptr == NULL) {
		__qd_panic_invalid_input();
		return;
	}
	fn_ptr(0);
}

void __qd_cell(int n, ...) {
	size_t cell_size = sizeof(__qd_real_t);
	__qd_arg_push((__qd_real_t)cell_size);
}

void __qd_sum(int n, ...) {
	__qd_real_t summed = 0;
	for (int i = 0; i < __qd_stack_ptr; ++i) {
		summed += __qd_stack[i];
	}
	__qd_arg_push(summed);
}

void __qd_depth(int n, ...) {
	__qd_arg_push(__qd_stack_ptr);
}

void __qd_add(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 2] += __qd_stack[__qd_stack_ptr - 1];
	__qd_pop(0);
}

void __qd_sub(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 2] -= __qd_stack[__qd_stack_ptr - 1];
	__qd_pop(0);
}

void __qd_mul(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 2] *= __qd_stack[__qd_stack_ptr - 1];
	__qd_pop(0);
}

void __qd_div(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	if (__qd_stack[__qd_stack_ptr - 1] == 0.0) {
		__qd_stack_ptr -= 2;
		__qd_panic_division_by_zero();
		return;
	}
	__qd_stack[__qd_stack_ptr - 2] /= __qd_stack[__qd_stack_ptr - 1];
	__qd_pop(0);
}

void __qd_swap(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 2];
	__qd_stack[__qd_stack_ptr - 2] = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack[__qd_stack_ptr - 1] = tmp;
}

void __qd_swap2(int n, ...) {
	if (__qd_stack_ptr < 4) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 4];
	__qd_real_t tmp2 =  __qd_stack[__qd_stack_ptr - 3];
	__qd_stack[__qd_stack_ptr - 4] = __qd_stack[__qd_stack_ptr - 2];
	__qd_stack[__qd_stack_ptr - 3] = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack[__qd_stack_ptr - 2] = tmp;
	__qd_stack[__qd_stack_ptr - 1] = tmp2;
}

void __qd_abs(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = fabs(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_acos(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = acos(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_asin(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = asin(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_atan(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = atan(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_cos(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = cos(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_sin(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = sin(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_tan(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = tan(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_fac(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	if (__qd_stack[__qd_stack_ptr - 1] < 0.0) {
		__qd_stack_ptr -= 1;
		__qd_panic_invalid_data();
		return;
	}
	__qd_real_t result = 1;
	for (__qd_real_t i = 1; i <= __qd_stack[__qd_stack_ptr - 1]; ++i) {
		result *= i;
	}
	__qd_stack[__qd_stack_ptr - 1] = result;
}

void __qd_tuck(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 1];
	__qd_swap(0);
	__qd_arg_push(tmp);
}

void __qd_cb(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = __qd_stack[__qd_stack_ptr - 1] * __qd_stack[__qd_stack_ptr - 1] * __qd_stack[__qd_stack_ptr - 1];
}

void __qd_cbrt(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = cbrt(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_ceil(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = ceil(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_floor(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = floor(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_clear(int n, ...) {
	__qd_stack_ptr = 0;
}

void __qd_dec(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	--__qd_stack[__qd_stack_ptr - 1];
}

void __qd_inc(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	++__qd_stack[__qd_stack_ptr - 1];
}

void __qd_dup(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_dup2(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 2]);
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 2]);
}

void __qd_inv(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	if (__qd_stack[__qd_stack_ptr - 1] != 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = 1.0 / __qd_stack[__qd_stack_ptr - 1];
	}
}

void __qd_ln(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	if (__qd_stack[__qd_stack_ptr - 1] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = log(__qd_stack[__qd_stack_ptr - 1]);
	}
}

void __qd_log10(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	if (__qd_stack[__qd_stack_ptr - 1] > 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = log10(__qd_stack[__qd_stack_ptr - 1]);
	}
}

void __qd_max(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_real_t max = __qd_stack[__qd_stack_ptr - 2] > __qd_stack[__qd_stack_ptr - 1] ? __qd_stack[__qd_stack_ptr - 2] : __qd_stack[__qd_stack_ptr - 1];
	__qd_stack_ptr -= 2;
	__qd_arg_push(max);
}

void __qd_min(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_real_t min = __qd_stack[__qd_stack_ptr - 2] < __qd_stack[__qd_stack_ptr - 1] ? __qd_stack[__qd_stack_ptr - 2] : __qd_stack[__qd_stack_ptr - 1];
	__qd_stack_ptr -= 2;
	__qd_arg_push(min);
}

void __qd_neg(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = -__qd_stack[__qd_stack_ptr - 1];
}

void __qd_nip(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_swap(0);
	__qd_pop(0);
}

void __qd_over(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - 2]);
}

void __qd_pick(int n, ...) {
	if (n == 0) {
		__qd_panic_invalid_input();
		return;
	}
	va_list args;
	va_start(args, n);
	int x = (int)va_arg(args, __qd_real_t);
	if (x < 0 || x >= __qd_stack_ptr) {
		__qd_panic_invalid_data();
		return;
	}
	__qd_arg_push(__qd_stack[__qd_stack_ptr - x - 1]);
	va_end(args);
}

void __qd_pow(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 2] = pow(__qd_stack[__qd_stack_ptr - 2], __qd_stack[__qd_stack_ptr - 1]);
	__qd_pop(0);
}

void __qd_mark(int n, ...) {
	if (__qd_mark_stack_ptr >= QD_MARK_STACK_DEPTH) {
		__qd_panic_mark_stack_overflow();
		return;
	}
	__qd_mark_stacks_ptrs[__qd_mark_stack_ptr] = __qd_stack_ptr;
	memcpy(__qd_mark_stacks[__qd_mark_stack_ptr], __qd_stack, sizeof(__qd_stack));
	__qd_mark_stack_ptr++;
}

void __qd_revert(int n, ...) {
	if (__qd_mark_stack_ptr == 0) {
		__qd_panic_mark_stack_underflow();
		return;
	}
	__qd_stack_ptr = __qd_mark_stacks_ptrs[--__qd_mark_stack_ptr];
	memcpy(__qd_stack, __qd_mark_stacks[__qd_mark_stack_ptr], sizeof(__qd_stack));
}

void __qd_roll(int n, ...) {
	if (n == 0) {
		__qd_panic_invalid_input();
		return;
	}
	va_list args;
	va_start(args, n);
	int x = (int)va_arg(args, __qd_real_t);
	if (x < 0 || x >= __qd_stack_ptr) {
		__qd_panic_invalid_data();
		return;
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - x - 1];
	for (int i = __qd_stack_ptr - x - 1; i < __qd_stack_ptr - 1; ++i) {
		__qd_stack[i] = __qd_stack[i + 1];
	}
	__qd_pop(0);
	__qd_arg_push(tmp);
	va_end(args);
}

void __qd_mod(int n, ...) {
	if (__qd_stack_ptr < 2) {
		__qd___panic_stack_underflow(0);
		return;
	}
	if (__qd_stack[__qd_stack_ptr - 1] == 0.0) {
		__qd_stack_ptr -= 2;
		__qd_panic_division_by_zero();
		return;
	}
	__qd_stack[__qd_stack_ptr - 2] = fmod(__qd_stack[__qd_stack_ptr - 2], __qd_stack[__qd_stack_ptr - 1]);
	__qd_pop(0);
}

void trim(char* str) {
	int start = 0;
	while (isspace(str[start])) {
		start++;
	}

	int end = strlen(str) - 1;
	while (end >= start && isspace(str[end])) {
		end--;
	}

	int length = end - start + 1;
	memmove(str, str + start, length);

	str[length] = '\0';
}

void __qd_read(int n, ...) {
	char input[1024] = {0};
	if (fgets(input, sizeof(input), stdin) != NULL) {
		trim(input);
		__qd_eval(0, input);
	}
}

void __qd_reduce_add(int n, ...) {
	__qd_real_t result = 0;
	for (int i = 0; i < __qd_stack_ptr; ++i) {
		result += __qd_stack[i];
	}
	__qd_stack_ptr = 0;
	__qd_arg_push(result);
}

void __qd_reduce_div(int n, ...) {
	__qd_real_t result = __qd_stack[0];
	for (int i = 1; i < __qd_stack_ptr; ++i) {
		if (__qd_stack[i] == 0.0) {
			__qd_stack_ptr = 0;
			__qd_arg_push(0);
			__qd_panic_division_by_zero();
			return;
		}
		result /= __qd_stack[i];
	}
	__qd_stack_ptr = 0;
	__qd_arg_push(result);
}

void __qd_reduce_mul(int n, ...) {
	__qd_real_t result = 1;
	for (int i = 0; i < __qd_stack_ptr; ++i) {
		result *= __qd_stack[i];
	}
	__qd_stack_ptr = 0;
	__qd_arg_push(result);
}

void __qd_reduce_sub(int n, ...) {
	__qd_real_t result = __qd_stack[0];
	for (int i = 1; i < __qd_stack_ptr; ++i) {
		result -= __qd_stack[i];
	}
	__qd_stack_ptr = 0;
	__qd_arg_push(result);
}

void __qd_rot(int n, ...) {
	if (__qd_stack_ptr < 3) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 3];
	__qd_stack[__qd_stack_ptr - 3] = __qd_stack[__qd_stack_ptr - 2];
	__qd_stack[__qd_stack_ptr - 2] = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack[__qd_stack_ptr - 1] = tmp;
}

void __qd_rot2(int n, ...) {
	if (__qd_stack_ptr < 4) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_real_t tmp = __qd_stack[__qd_stack_ptr - 4];
	__qd_stack[__qd_stack_ptr - 4] = __qd_stack[__qd_stack_ptr - 2];
	__qd_stack[__qd_stack_ptr - 2] = tmp;
	tmp = __qd_stack[__qd_stack_ptr - 3];
	__qd_stack[__qd_stack_ptr - 3] = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack[__qd_stack_ptr - 1] = tmp;
}

void __qd_round(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] = round(__qd_stack[__qd_stack_ptr - 1]);
}

void __qd_sq(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_stack[__qd_stack_ptr - 1] *= __qd_stack[__qd_stack_ptr - 1];
}

void __qd_sqrt(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	if (__qd_stack[__qd_stack_ptr - 1] >= 0.0) {
		__qd_stack[__qd_stack_ptr - 1] = sqrt(__qd_stack[__qd_stack_ptr - 1]);
	}
}

void __qd_test(int n, ...) {
	if (n == 0) {
		__qd_panic_invalid_data();
		exit(1);
	}
	va_list args;
	va_start(args, n);
	const char* name = va_arg(args, const char*);
	if (n - 1 != __qd_stack_ptr) {
		printf("test: [%s] expected stack depth %d, got %d\n", name, n - 1, __qd_stack_ptr);
		exit(1);
	}

	for (int i = 0; i < n-1; ++i) {
		__qd_real_t x = va_arg(args, __qd_real_t);
		if (x != __qd_stack[i]) {
			printf("test: [%s] expected %.*f, got %.*f\n", name, __qd_precision, x, __qd_precision, __qd_stack[i]);
			exit(1);
		}
	}
	va_end(args);
	__qd_stack_ptr = 0;
}

void __qd_write(int n, ...) {
	for (int i = 0; i < __qd_stack_ptr; ++i) {
		if (i != 0) {
			printf(" ");
		}
		printf("%.*f", __qd_precision, __qd_stack[i]);
	}
}

void __qd_print(int n, ...) {
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	printf("%.*f\n", __qd_precision, __qd_stack[__qd_stack_ptr - 1]);
}

void __qd_eval(int n, const char* expression, ...) {
	char* expr_copy = strdup(expression);
	if (expr_copy == NULL) {
		return;
	}
	char* token = strtok((char*)expr_copy, " \t\r\n");
	while (token != NULL) {
		if (isdigit(token[0]) || (token[0] == '-' && isdigit(token[1]))) {
			__qd_arg_push(atof(token));
		} else if (strcmp(token, "+") == 0) {
			__qd_add(0);
		} else if (strcmp(token, "-") == 0) {
			__qd_sub(0);
		} else if (strcmp(token, "*") == 0) {
			__qd_mul(0);
		} else if (strcmp(token, "/") == 0) {
			__qd_div(0);
		} else if (strcmp(token, "d") == 0) {
			__qd_dup(0);
		} else if (strcmp(token, "v") == 0) {
			__qd_sqrt(0);
		} else if (strcmp(token, "^") == 0) {
			__qd_pow(0);
		} else if (strcmp(token, "%") == 0) {
			__qd_mod(0);
		} else if (strcmp(token, "p") == 0) {
			__qd_print(0);
		} else if (strcmp(token, "k") == 0) {
			__qd_scale(0);
		} else if (strcmp(token, "z") == 0) {
			__qd_depth(0);
		} else {
			__qd_panic_invalid_input();
			return;
		}
		token = strtok(NULL, " \t\r\n");
	}
}

void __qd_scale(int n, ...) {
	va_list args;
	va_start(args, n);
	if (n > 0) {
		__qd_push(n, va_arg(args, __qd_real_t));
	}
	if (__qd_stack_ptr < 1) {
		__qd___panic_stack_underflow(0);
		return;
	}
	__qd_precision = (int)__qd_stack[--__qd_stack_ptr];
}

void __qd_within(int n, ...) {
	if (__qd_stack_ptr < 3) {
		__qd___panic_stack_underflow(0);
		return;
	}

	__qd_real_t x = __qd_stack[__qd_stack_ptr - 3];
	__qd_real_t low = __qd_stack[__qd_stack_ptr - 2];
	__qd_real_t high = __qd_stack[__qd_stack_ptr - 1];
	__qd_stack_ptr -= 3;
	if (x >= low && x <= high) {
		__qd_arg_push(1);
	} else {
		__qd_arg_push(0);
	}
}

void __qd___panic_stack_overflow(int n, ...) {
	__qd_err = 2.1;
	fprintf(stderr, "panic: stack overflow\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_mark_stack_overflow() {
	__qd_err = 2.4;
	fprintf(stderr, "panic: mark stack overflow\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_mark_stack_underflow() {
	__qd_err = 2.5;
	fprintf(stderr, "panic: mark stack underflow\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd___panic_stack_underflow(int n, ...) {
	__qd_err = 2.2;
	fprintf(stderr, "panic: stack underflow\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_stack_underflow() {
	__qd_err = 2.2;
	fprintf(stderr, "panic: stack underflow\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_stack_overflow() {
	__qd_err = 2.1;
	fprintf(stderr, "panic: stack overflow\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_value_infinity() {
	__qd_err = 2.3;
	fprintf(stderr, "panic: value infinity\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_division_by_zero() {
	__qd_err = 1.1;
	fprintf(stderr, "panic: division by zero\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_invalid_input() {
	__qd_err = 3.1;
	fprintf(stderr, "panic: invalid input\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_invalid_data() {
	__qd_err = 3.2;
	fprintf(stderr, "panic: invalid data\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}

void __qd_panic_out_of_memory() {
	__qd_err = 4.1;
	fprintf(stderr, "panic: out of memory\n");
#ifdef QD_ENABLE_PANIC
	exit(1);
#endif
}


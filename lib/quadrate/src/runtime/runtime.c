#include <quadrate/runtime/runtime.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

qd_exec_result qd_push_i(qd_context* ctx, int64_t value) {
	qd_stack_error err = qd_stack_push_int(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_f(qd_context* ctx, double value) {
	qd_stack_error err = qd_stack_push_float(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_s(qd_context* ctx, const char* value) {
	qd_stack_error err = qd_stack_push_str(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_print(qd_context* ctx) {
	const int64_t stack_size = (int64_t)qd_stack_size(ctx->st);
	for (int64_t i = stack_size - 1; i >= 0; i--) {
		qd_stack_element_t val;
		qd_stack_error err = qd_stack_element(ctx->st, (size_t)i, &val);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
		if (i < stack_size - 1) {
			printf(" ");
		}
		switch (val.type) {
			case QD_STACK_TYPE_INT:
				printf("%ld", val.value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				printf("%f", val.value.f);
				break;
			case QD_STACK_TYPE_STR:
				printf("\"%s\"", val.value.s);
				break;
			default:
				return (qd_exec_result){-3};
		}
	}
	printf("\n");
	return (qd_exec_result){0};
}

qd_exec_result qd_printv(qd_context* ctx) {
	const int64_t stack_size = (int64_t)qd_stack_size(ctx->st);
	for (int64_t i = stack_size - 1; i >= 0; i--) {
		qd_stack_element_t val;
		qd_stack_error err = qd_stack_element(ctx->st, (size_t)i, &val);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
		if (i < stack_size - 1) {
			printf(" ");
		}
		switch (val.type) {
			case QD_STACK_TYPE_INT:
				printf("int:%ld", val.value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				printf("flt:%f", val.value.f);
				break;
			case QD_STACK_TYPE_STR:
				printf("str:\"%s\"", val.value.s);
				break;
			default:
				return (qd_exec_result){-3};
		}
	}
	printf("\n");
	return (qd_exec_result){0};
}

qd_exec_result qd_peek(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_peek(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	switch (val.type) {
		case QD_STACK_TYPE_INT:
			printf("%ld\n", val.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			printf("%f\n", val.value.f);
			break;
		case QD_STACK_TYPE_STR:
			printf("%s\n", val.value.s);
			break;
		default:
			return (qd_exec_result){-3};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_err_push(qd_context* ctx, qd_stack_error value) {
	(void)ctx;
	(void)value;
	return (qd_exec_result){0};
}

qd_exec_result qd_div(qd_context* ctx) {
	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		if (b.value.i == 0) {
			return (qd_exec_result){-4};
		}
		int64_t result = a.value.i / b.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if ((a.type == QD_STACK_TYPE_INT || a.type == QD_STACK_TYPE_FLOAT) &&
	           (b.type == QD_STACK_TYPE_INT || b.type == QD_STACK_TYPE_FLOAT)) {
		double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
		double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;
		if (bf == 0.0) {
			return (qd_exec_result){-4};
		}
		double result = af / bf;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		if (b.type == QD_STACK_TYPE_STR) {
			free(b.value.s);
		}
		if (a.type == QD_STACK_TYPE_STR) {
			free(a.value.s);
		}
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_sq(qd_context* ctx) {
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (a.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i * a.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if (a.type == QD_STACK_TYPE_FLOAT) {
		double result = a.value.f * a.value.f;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

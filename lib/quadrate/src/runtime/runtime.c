#include <quadrate/runtime/runtime.h>
#include <stdio.h>

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
	const ssize_t stack_size = (ssize_t)qd_stack_size(ctx->st);
	for (ssize_t i = stack_size - 1; i >= 0; i--) {
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
	const ssize_t stack_size = (ssize_t)qd_stack_size(ctx->st);
	for (ssize_t i = stack_size - 1; i >= 0; i--) {
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

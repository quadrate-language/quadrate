#include <qdrt/stack.h>
#include <stdlib.h>
#include <string.h>

struct qd_stack {
	qd_stack_element_t* data;
	size_t capacity;
	size_t size;
};

qd_stack_error qd_stack_init(qd_stack** stack, size_t capacity) {
	if (stack == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (capacity == 0) {
		return QD_STACK_ERR_INVALID_CAPACITY;
	}

	qd_stack* s = (qd_stack*)malloc(sizeof(qd_stack));
	if (s == NULL) {
		return QD_STACK_ERR_ALLOC;
	}

	s->data = (qd_stack_element_t*)malloc(sizeof(qd_stack_element_t) * capacity);
	if (s->data == NULL) {
		free(s);
		return QD_STACK_ERR_ALLOC;
	}

	s->capacity = capacity;
	s->size = 0;
	*stack = s;
	return QD_STACK_OK;
}

void qd_stack_destroy(qd_stack* stack) {
	if (stack == NULL) {
		return;
	}

	/* Free all string allocations */
	for (size_t i = 0; i < stack->size; i++) {
		if (stack->data[i].type == QD_STACK_TYPE_STR) {
			free(stack->data[i].value.s);
		}
	}

	free(stack->data);
	free(stack);
}

qd_stack_error qd_stack_push_int(qd_stack* stack, int64_t value) {
	if (stack == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size >= stack->capacity) {
		return QD_STACK_ERR_OVERFLOW;
	}

	stack->data[stack->size].value.i = value;
	stack->data[stack->size].type = QD_STACK_TYPE_INT;
	stack->size++;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_push_float(qd_stack* stack, double value) {
	if (stack == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size >= stack->capacity) {
		return QD_STACK_ERR_OVERFLOW;
	}

	stack->data[stack->size].value.f = value;
	stack->data[stack->size].type = QD_STACK_TYPE_FLOAT;
	stack->size++;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_push_ptr(qd_stack* stack, void* value) {
	if (stack == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size >= stack->capacity) {
		return QD_STACK_ERR_OVERFLOW;
	}

	stack->data[stack->size].value.p = value;
	stack->data[stack->size].type = QD_STACK_TYPE_PTR;
	stack->size++;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_push_str(qd_stack* stack, const char* value) {
	if (stack == NULL || value == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size >= stack->capacity) {
		return QD_STACK_ERR_OVERFLOW;
	}

	/* Copy the string to own the data */
	size_t len = strlen(value);
	char* copy = (char*)malloc(len + 1);
	if (copy == NULL) {
		return QD_STACK_ERR_ALLOC;
	}
	memcpy(copy, value, len + 1);

	stack->data[stack->size].value.s = copy;
	stack->data[stack->size].type = QD_STACK_TYPE_STR;
	stack->size++;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_element(qd_stack* stack, size_t index, qd_stack_element_t* element) {
	if (stack == NULL || element == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (index >= stack->size) {
		return QD_STACK_ERR_UNDERFLOW;
	}

	*element = stack->data[index];
	return QD_STACK_OK;
}

qd_stack_error qd_stack_peek(qd_stack* stack, qd_stack_element_t* element) {
	if (stack == NULL || element == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size == 0) {
		return QD_STACK_ERR_UNDERFLOW;
	}

	*element = stack->data[stack->size - 1];
	return QD_STACK_OK;
}

qd_stack_error qd_stack_pop(qd_stack* stack, qd_stack_element_t* element) {
	if (stack == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size == 0) {
		return QD_STACK_ERR_UNDERFLOW;
	}
	stack->size--;
	if (element != NULL) {
		*element = stack->data[stack->size];
	} else {
		if (stack->data[stack->size].type == QD_STACK_TYPE_STR) {
			free(stack->data[stack->size].value.s);
		}
	}
	return QD_STACK_OK;
}

qd_stack_error qd_stack_top_type(const qd_stack* stack, qd_stack_type* type) {
	if (stack == NULL || type == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size == 0) {
		return QD_STACK_ERR_UNDERFLOW;
	}

	*type = stack->data[stack->size - 1].type;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_top_int(const qd_stack* stack, int64_t* value) {
	if (stack == NULL || value == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size == 0) {
		return QD_STACK_ERR_UNDERFLOW;
	}
	if (stack->data[stack->size - 1].type != QD_STACK_TYPE_INT) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}

	*value = stack->data[stack->size - 1].value.i;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_top_double(const qd_stack* stack, double* value) {
	if (stack == NULL || value == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size == 0) {
		return QD_STACK_ERR_UNDERFLOW;
	}
	if (stack->data[stack->size - 1].type != QD_STACK_TYPE_FLOAT) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}

	*value = stack->data[stack->size - 1].value.f;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_top_ptr(const qd_stack* stack, void** value) {
	if (stack == NULL || value == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size == 0) {
		return QD_STACK_ERR_UNDERFLOW;
	}
	if (stack->data[stack->size - 1].type != QD_STACK_TYPE_PTR) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}

	*value = stack->data[stack->size - 1].value.p;
	return QD_STACK_OK;
}

qd_stack_error qd_stack_top_str(const qd_stack* stack, const char** value) {
	if (stack == NULL || value == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (stack->size == 0) {
		return QD_STACK_ERR_UNDERFLOW;
	}
	if (stack->data[stack->size - 1].type != QD_STACK_TYPE_STR) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}

	*value = stack->data[stack->size - 1].value.s;
	return QD_STACK_OK;
}

size_t qd_stack_size(const qd_stack* stack) {
	if (stack == NULL) {
		return 0;
	}
	return stack->size;
}

size_t qd_stack_capacity(const qd_stack* stack) {
	if (stack == NULL) {
		return 0;
	}
	return stack->capacity;
}

bool qd_stack_is_empty(const qd_stack* stack) {
	if (stack == NULL) {
		return true;
	}
	return stack->size == 0;
}

bool qd_stack_is_full(const qd_stack* stack) {
	if (stack == NULL) {
		return true;
	}
	return stack->size >= stack->capacity;
}

const char* qd_stack_error_string(qd_stack_error error) {
	switch (error) {
		case QD_STACK_OK:
			return "Success";
		case QD_STACK_ERR_INVALID_CAPACITY:
			return "Invalid capacity: must be greater than 0";
		case QD_STACK_ERR_OVERFLOW:
			return "Stack overflow: cannot push, stack is full";
		case QD_STACK_ERR_UNDERFLOW:
			return "Stack underflow: cannot access empty stack";
		case QD_STACK_ERR_TYPE_MISMATCH:
			return "Type mismatch: top element has different type";
		case QD_STACK_ERR_NULL_POINTER:
			return "Null pointer provided";
		case QD_STACK_ERR_ALLOC:
			return "Memory allocation failed";
		default:
			return "Unknown error";
	}
}

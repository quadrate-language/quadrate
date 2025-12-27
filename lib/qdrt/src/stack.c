#define _POSIX_C_SOURCE 200809L

#include <qdrt/stack.h>
#include <qdrt/qd_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stack structure is now defined in the header for inline access

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

	/* Release all string references */
	for (size_t i = 0; i < stack->size; i++) {
		if (stack->data[i].type == QD_STACK_TYPE_STR) {
			qd_string_release(stack->data[i].value.s);
		}
	}

	free(stack->data);
	free(stack);
}

qd_stack_error qd_stack_clone(qd_stack** dest, const qd_stack* src) {
	if (dest == NULL || src == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}

	/* Create new stack with same capacity */
	qd_stack_error err = qd_stack_init(dest, src->capacity);
	if (err != QD_STACK_OK) {
		return err;
	}

	qd_stack* d = *dest;

	/* Copy all elements */
	for (size_t i = 0; i < src->size; i++) {
		d->data[i].type = src->data[i].type;
		d->data[i].is_error_tainted = src->data[i].is_error_tainted;

		/* Deep copy strings (for ctx isolation) */
		if (src->data[i].type == QD_STACK_TYPE_STR) {
			const char* str_data = qd_string_data(src->data[i].value.s);
			size_t str_len = qd_string_length(src->data[i].value.s);
			d->data[i].value.s = qd_string_create_with_length(str_data, str_len);
			if (d->data[i].value.s == NULL) {
				/* Cleanup on allocation failure */
				for (size_t j = 0; j < i; j++) {
					if (d->data[j].type == QD_STACK_TYPE_STR) {
						qd_string_release(d->data[j].value.s);
					}
				}
				qd_stack_destroy(d);
				*dest = NULL;
				return QD_STACK_ERR_ALLOC;
			}
		} else {
			/* Shallow copy for non-string types */
			d->data[i].value = src->data[i].value;
		}
	}

	d->size = src->size;
	return QD_STACK_OK;
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
	stack->data[stack->size].is_error_tainted = false;
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
	stack->data[stack->size].is_error_tainted = false;
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
	stack->data[stack->size].is_error_tainted = false;
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

	/* Create reference-counted string */
	qd_string_t* qd_str = qd_string_create(value);
	if (qd_str == NULL) {
		return QD_STACK_ERR_ALLOC;
	}

	stack->data[stack->size].value.s = qd_str;
	stack->data[stack->size].type = QD_STACK_TYPE_STR;
	stack->data[stack->size].is_error_tainted = false;
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
		/* Release string reference if element not saved */
		if (stack->data[stack->size].type == QD_STACK_TYPE_STR) {
			qd_string_release(stack->data[stack->size].value.s);
		}
	}
	return QD_STACK_OK;
}

qd_stack_error qd_stack_remove_at(qd_stack* stack, size_t index, qd_stack_element_t* element) {
	if (stack == NULL) {
		return QD_STACK_ERR_NULL_POINTER;
	}
	if (index >= stack->size) {
		return QD_STACK_ERR_UNDERFLOW;
	}

	/* Save the element if requested */
	if (element != NULL) {
		*element = stack->data[index];
	} else {
		/* Release string reference if element not saved */
		if (stack->data[index].type == QD_STACK_TYPE_STR) {
			qd_string_release(stack->data[index].value.s);
		}
	}

	/* Shift all elements above this index down by one */
	for (size_t i = index; i < stack->size - 1; i++) {
		stack->data[i] = stack->data[i + 1];
	}

	stack->size--;
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

	*value = qd_string_data(stack->data[stack->size - 1].value.s);
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

bool qd_stack_is_top_tainted(const qd_stack* stack) {
	if (stack == NULL || stack->size == 0) {
		return false;
	}
	return stack->data[stack->size - 1].is_error_tainted;
}

void qd_stack_mark_top_tainted(qd_stack* stack) {
	if (stack == NULL || stack->size == 0) {
		return;
	}
	stack->data[stack->size - 1].is_error_tainted = true;
}

void qd_stack_clear_top_taint(qd_stack* stack) {
	if (stack == NULL || stack->size == 0) {
		return;
	}
	stack->data[stack->size - 1].is_error_tainted = false;
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

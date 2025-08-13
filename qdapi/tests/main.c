#include <quadrate/qd.h>
#include <stdio.h>

int main(void) {
	qd_context_t* ctx = qd_create_context();
	qd_free_context(ctx);

	printf("Hello, Quadrate!\n");

	return 0;
}


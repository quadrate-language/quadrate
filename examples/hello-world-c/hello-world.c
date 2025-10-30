#include <quadrate/qd.h>

int main(void) {
	qd_context* ctx = qd_create_context();

	qd_push_s(ctx, "Hello, World!");
	qd_print(ctx);

	qd_free_context(ctx);

	return 0;
}

#include <quadrate/qd.h>
#include <stdio.h>

void my_func(void) {
	printf("This is a custom function!\n");
}

int main(void) {
	qd_context_t* ctx = qd_create_context();

	qd_module_t* os = qd_get_module(ctx, "os");
	qd_add_script(os, "fn exit() {}");
	qd_register_function(os, "my_func", my_func);
	qd_build(os);
	qd_execute(ctx, "os::exit");

	qd_free_context(ctx);

	printf("Hello, Quadrate!\n");

	return 0;
}


#include <qd/qd.h>
#include <stdio.h>

void my_func() {
	printf("This is a custom function!\n");
}

int main(void) {
	qd_context* ctx = qd_create_context(1024);

	qd_module* hello = qd_get_module(ctx, "hello");
	qd_add_script(hello, "fn world( -- ) { \"Hello, World!\" . nl }");
	//	qd_register_function(hello, "my_func", my_func);
	qd_build(hello);
	qd_execute(ctx, "123.34 . nl hello::world");

	qd_free_context(ctx);

	printf("Hello, Quadrate!\n");

	return 0;
}

#include <qd/qd.h>
#include <stdio.h>

int main(void) {
	qd_context* ctx = qd_create_context(1024);

	// Module 1: math operations
	qd_module* math_mod = qd_get_module(ctx, "math");
	qd_add_script(math_mod, "fn square(x:i64 -- result:i64) { dup mul }");
	qd_add_script(math_mod, "fn double(x:i64 -- result:i64) { 2 * }");
	qd_build(math_mod);

	// Module 2: string operations
	qd_module* str_mod = qd_get_module(ctx, "str");
	qd_add_script(str_mod, "fn greet( -- ) { \"Hello from str module!\" . nl }");
	qd_add_script(str_mod, "fn farewell( -- ) { \"Goodbye!\" . nl }");
	qd_build(str_mod);

	// Module 3: calculator
	qd_module* calc_mod = qd_get_module(ctx, "calc");
	qd_add_script(calc_mod, "fn add_and_print(a:i64 b:i64 -- ) { + dup \"Result: \" . . nl }");
	qd_build(calc_mod);

	printf("=== Testing Multiple Modules ===\n\n");

	// Test math module
	printf("Math module:\n");
	qd_execute(ctx, "5 math::square . nl");        // 5 * 5 = 25
	qd_execute(ctx, "7 math::double . nl");        // 7 * 2 = 14

	printf("\n");

	// Test string module
	printf("String module:\n");
	qd_execute(ctx, "str::greet");
	qd_execute(ctx, "str::farewell");

	printf("\n");

	// Test calculator module
	printf("Calculator module:\n");
	qd_execute(ctx, "10 20 calc::add_and_print");  // 10 + 20 = 30

	printf("\n");

	// Mix operations from different modules
	printf("Mixed operations:\n");
	qd_execute(ctx, "3 math::square 2 math::double + . nl");  // (3*3) + (2*2) = 9 + 4 = 13

	qd_free_context(ctx);

	printf("\n=== Done ===\n");
	return 0;
}

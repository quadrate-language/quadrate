#include <qd/qd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Native C function that can be called from Quadrate
qd_exec_result native_get_timestamp(qd_context* ctx) {
	time_t now = time(nullptr);
	return qd_push_i(ctx, static_cast<int64_t>(now));
}

// Another native function
qd_exec_result native_random(qd_context* ctx) {
	int random_val = rand() % 100;
	return qd_push_i(ctx, static_cast<int64_t>(random_val));
}

int main(void) {
	srand(static_cast<unsigned int>(time(nullptr)));
	qd_context* ctx = qd_create_context(1024);

	// Create module with both Quadrate and native functions
	qd_module* utils = qd_get_module(ctx, "utils");

	// Add Quadrate functions
	qd_add_script(utils, "fn double(x:i64 -- result:i64) { 2 * }");

	// Register native C functions
	qd_register_function(utils, "get_timestamp", reinterpret_cast<void (*)()>(native_get_timestamp));
	qd_register_function(utils, "random", reinterpret_cast<void (*)()>(native_random));

	qd_build(utils);

	printf("=== Native Functions Test ===\n\n");

	// Call native function
	printf("Current timestamp: ");
	qd_execute(ctx, "utils::get_timestamp . nl");

	// Mix native and compiled functions
	printf("Random number doubled: ");
	qd_execute(ctx, "utils::random utils::double . nl");

	qd_free_context(ctx);
	return 0;
}

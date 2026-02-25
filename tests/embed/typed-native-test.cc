#include <quadrate/qd/qd.h>
#include <stdio.h>
#include <string.h>

// Native function that pushes a f64 (latitude)
int native_lat(qd_context* ctx, void*) {
	return qd_push_f(ctx, 59.3293);
}

// Native function that pushes a f64 (altitude)
int native_alt(qd_context* ctx, void*) {
	return qd_push_f(ctx, 125.5);
}

// Native function that pushes a string (sensor name)
int native_name(qd_context* ctx, void*) {
	return qd_push_s(ctx, "GPS-1");
}

// Native function that pushes an i64
int native_count(qd_context* ctx, void*) {
	return qd_push_i(ctx, 42);
}

int main(void) {
	qd_context* ctx = qd_create_context(1024);

	// Create sensor module with typed native functions
	qd_module* sensors = qd_get_module(ctx, "sensors");
	qd_register_function(sensors, "lat", "( -- v:f64)", native_lat, NULL);
	qd_register_function(sensors, "alt", "( -- v:f64)", native_alt, NULL);
	qd_register_function(sensors, "name", "( -- s:str)", native_name, NULL);
	qd_register_function(sensors, "count", "( -- v:i64)", native_count, NULL);
	qd_build(sensors);

	// Create main module that uses typed sensor functions with stdlib
	qd_module* main_mod = qd_get_module(ctx, "main");
	qd_add_script(main_mod,
		"use sensors\n"
		"use strconv\n"
		"\n"
		"fn report( -- ) {\n"
		"    // sensors::lat returns f64, strconv::format_float expects f64 — type checks!\n"
		"    \"lat=\" print sensors::lat strconv::format_float print nl\n"
		"    \"alt=\" print sensors::alt strconv::format_float print nl\n"
		"    \"name=\" print sensors::name print nl\n"
		"    \"count=\" print sensors::count print nl\n"
		"}\n"
	);
	qd_build(main_mod);

	printf("=== Typed Native Functions Test ===\n");

	if (qd_is_compiled(main_mod)) {
		qd_execute(ctx, "main::report");
		printf("=== All typed native tests passed ===\n");
	} else {
		printf("FAIL: main module failed to compile\n");
	}

	qd_free_context(ctx);
	return 0;
}

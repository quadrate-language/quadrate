#include <quadrate/qd/qd.h>
#include <stdio.h>

int main(void) {
	qd_context* ctx = qd_create_context(1024);

	qd_module* hello = qd_get_module(ctx, "hello");
	qd_add_script(hello,
			"use strings\n"
			"use json\n"
			"fn world() { \"Hello, World!\" print nl }\n"
			"fn shout() { \"hello\" strings::upper print nl }\n"
			"fn parse() { \"{\\\"name\\\":\\\"Alice\\\"}\" \"name\" json::get_string -> _ -> val val print nl }");
	qd_build(hello);
	qd_execute(ctx, "123.34 print nl hello::world");
	qd_execute(ctx, "hello::shout");
	qd_execute(ctx, "hello::parse");

	qd_free_context(ctx);

	printf("Hello, Quadrate!\n");

	return 0;
}

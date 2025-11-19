#include <qd/qd.h>
#include <stdio.h>

int main(void) {
	qd_context* ctx = qd_create_context(1024);

	// Create a module
	qd_module* app = qd_get_module(ctx, "app");

	// Add functions incrementally (all added before building)
	printf("=== Building Module Incrementally ===\n\n");

	printf("Adding base functions...\n");
	qd_add_script(app, "fn add(a:i64 b:i64 -- result:i64) { + }");
	qd_add_script(app, "fn sub(a:i64 b:i64 -- result:i64) { - }");

	printf("Adding higher-level functions that use base functions...\n");
	qd_add_script(app, "fn double_sum(a:i64 b:i64 -- result:i64) { + 2 * }");

	printf("Building all at once...\n");
	qd_build(app);

	printf("\nTesting:\n");
	qd_execute(ctx, "10 5 app::add . nl");          // 15
	qd_execute(ctx, "10 5 app::sub . nl");          // 5
	qd_execute(ctx, "10 5 app::double_sum . nl");  // (10+5)*2 = 30

	qd_free_context(ctx);
	return 0;
}

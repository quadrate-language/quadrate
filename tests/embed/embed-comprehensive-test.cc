#include <quadrate/qd/qd.h>
#include <quadrate/rt/stack.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void check(const char* name, bool condition) {
	if (condition) {
		g_tests_passed++;
	} else {
		g_tests_failed++;
		printf("  FAIL: %s\n", name);
	}
}

int native_add(qd_context* ctx, void*) {
	qd_stack_element_t b, a;
	qd_stack_pop(ctx->st, &b);
	qd_stack_pop(ctx->st, &a);
	return qd_push_i(ctx, a.value.i + b.value.i);
}

int native_hypot(qd_context* ctx, void*) {
	qd_stack_element_t y, x;
	qd_stack_pop(ctx->st, &y);
	qd_stack_pop(ctx->st, &x);
	return qd_push_f(ctx, sqrt(x.value.f * x.value.f + y.value.f * y.value.f));
}

int native_strlen(qd_context* ctx, void*) {
	qd_stack_element_t s;
	qd_stack_pop(ctx->st, &s);
	int64_t len = 0;
	if (s.type == QD_STACK_TYPE_STR) {
		len = static_cast<int64_t>(strlen(qd_string_data(s.value.s)));
		qd_string_release(s.value.s);
	}
	return qd_push_i(ctx, len);
}

struct Config {
	const char* prefix;
	int multiplier;
};

int native_get_prefix(qd_context* ctx, void* userdata) {
	Config* cfg = static_cast<Config*>(userdata);
	return qd_push_s(ctx, cfg->prefix);
}

int native_scale(qd_context* ctx, void* userdata) {
	Config* cfg = static_cast<Config*>(userdata);
	qd_stack_element_t x;
	qd_stack_pop(ctx->st, &x);
	return qd_push_i(ctx, x.value.i * cfg->multiplier);
}

int native_origin(qd_context* ctx, void*) {
	qd_push_f(ctx, 1.0);
	qd_push_f(ctx, 2.0);
	return qd_push_f(ctx, 3.0);
}

int native_concat(qd_context* ctx, void*) {
	qd_stack_element_t b_elem, a_elem;
	qd_stack_pop(ctx->st, &b_elem);
	qd_stack_pop(ctx->st, &a_elem);

	const char* a_str = (a_elem.type == QD_STACK_TYPE_STR) ? qd_string_data(a_elem.value.s) : "";
	const char* b_str = (b_elem.type == QD_STACK_TYPE_STR) ? qd_string_data(b_elem.value.s) : "";

	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s", a_str, b_str);

	if (a_elem.type == QD_STACK_TYPE_STR) qd_string_release(a_elem.value.s);
	if (b_elem.type == QD_STACK_TYPE_STR) qd_string_release(b_elem.value.s);

	return qd_push_s(ctx, buf);
}

void test_native_with_inputs(qd_context* ctx) {
	printf("--- Native functions with input parameters ---\n");

	qd_module* math = qd_get_module(ctx, "mymath");
	qd_register_function(math, "add", "(a:i64 b:i64 -- sum:i64)", native_add, NULL);
	qd_register_function(math, "hypot", "(x:f64 y:f64 -- dist:f64)", native_hypot, NULL);
	qd_register_function(math, "strlen", "(s:str -- len:i64)", native_strlen, NULL);
	qd_build(math);

	qd_module* t1 = qd_get_module(ctx, "t1");
	qd_add_script(t1,
		"use mymath\n"
		"fn test_add( -- ) {\n"
		"    10 20 mymath::add print nl\n"
		"}\n"
		"fn test_hypot( -- ) {\n"
		"    3.0 4.0 mymath::hypot print nl\n"
		"}\n"
		"fn test_strlen( -- ) {\n"
		"    \"hello\" mymath::strlen print nl\n"
		"}\n"
	);
	qd_build(t1);
	check("t1 compiled", qd_is_compiled(t1));

	qd_execute(ctx, "t1::test_add");
	qd_execute(ctx, "t1::test_hypot");
	qd_execute(ctx, "t1::test_strlen");
}

void test_userdata() {
	printf("--- Userdata passing ---\n");

	qd_context* ctx = qd_create_context(1024);
	Config cfg = {"[LOG] ", 10};

	qd_module* config_mod = qd_get_module(ctx, "config");
	qd_register_function(config_mod, "prefix", "( -- s:str)", native_get_prefix, &cfg);
	qd_register_function(config_mod, "scale", "(x:i64 -- result:i64)", native_scale, &cfg);
	qd_build(config_mod);

	qd_module* t2 = qd_get_module(ctx, "t2");
	qd_add_script(t2,
		"use config\n"
		"fn test_prefix( -- ) {\n"
		"    config::prefix print nl\n"
		"}\n"
		"fn test_scale( -- ) {\n"
		"    5 config::scale print nl\n"
		"}\n"
	);
	qd_build(t2);
	check("t2 compiled", qd_is_compiled(t2));

	qd_execute(ctx, "t2::test_prefix");
	qd_execute(ctx, "t2::test_scale");

	qd_free_context(ctx);
}

void test_multiple_returns() {
	printf("--- Multiple return values ---\n");

	qd_context* ctx = qd_create_context(1024);

	qd_module* geo = qd_get_module(ctx, "geo");
	qd_register_function(geo, "origin", "( -- x:f64 y:f64 z:f64)", native_origin, NULL);
	qd_build(geo);

	qd_module* t3 = qd_get_module(ctx, "t3");
	qd_add_script(t3,
		"use geo\n"
		"fn test_origin( -- ) {\n"
		"    geo::origin\n"
		"    \"z=\" print print nl\n"
		"    \"y=\" print print nl\n"
		"    \"x=\" print print nl\n"
		"}\n"
	);
	qd_build(t3);
	check("t3 compiled", qd_is_compiled(t3));

	qd_execute(ctx, "t3::test_origin");

	qd_free_context(ctx);
}

void test_string_roundtrip() {
	printf("--- String passing ---\n");

	qd_context* ctx = qd_create_context(1024);

	qd_module* strmod = qd_get_module(ctx, "mystr");
	qd_register_function(strmod, "concat", "(a:str b:str -- result:str)", native_concat, NULL);
	qd_build(strmod);

	qd_module* t4 = qd_get_module(ctx, "t4");
	qd_add_script(t4,
		"use mystr\n"
		"fn test_concat( -- ) {\n"
		"    \"Hello, \" \"World!\" mystr::concat print nl\n"
		"}\n"
	);
	qd_build(t4);
	check("t4 compiled", qd_is_compiled(t4));

	qd_execute(ctx, "t4::test_concat");

	qd_free_context(ctx);
}

void test_native_only_module() {
	printf("--- Native-only module ---\n");

	qd_context* ctx = qd_create_context(1024);

	qd_module* hw = qd_get_module(ctx, "hw");
	qd_register_function(hw, "answer", "( -- v:i64)", [](qd_context* c, void*) -> int {
		return qd_push_i(c, 42);
	}, NULL);
	qd_register_function(hw, "pi", "( -- v:f64)", [](qd_context* c, void*) -> int {
		return qd_push_f(c, 3.14159);
	}, NULL);

	qd_module* t5 = qd_get_module(ctx, "t5");
	qd_add_script(t5,
		"use hw\n"
		"fn run( -- ) {\n"
		"    hw::answer print nl\n"
		"    hw::pi print nl\n"
		"}\n"
	);
	qd_build(t5);
	check("t5 compiled", qd_is_compiled(t5));

	qd_execute(ctx, "t5::run");

	qd_free_context(ctx);
}

void test_mixed_native_and_script() {
	printf("--- Native + Quadrate cooperating modules ---\n");

	qd_context* ctx = qd_create_context(1024);

	qd_module* base = qd_get_module(ctx, "base");
	qd_register_function(base, "value", "( -- v:i64)", [](qd_context* c, void*) -> int {
		return qd_push_i(c, 100);
	}, NULL);
	qd_build(base);

	qd_module* mix = qd_get_module(ctx, "mix");
	qd_add_script(mix,
		"use base\n"
		"fn doubled( -- result:i64) { base::value 2 * }\n"
	);
	qd_build(mix);
	check("mix compiled", qd_is_compiled(mix));

	qd_execute(ctx, "mix::doubled print nl");

	qd_free_context(ctx);
}

void test_cross_module_typed_stdlib() {
	printf("--- Cross-module typed native + stdlib interop ---\n");

	qd_context* ctx = qd_create_context(1024);

	qd_module* sensors = qd_get_module(ctx, "sens");
	qd_register_function(sensors, "temp", "( -- v:f64)", [](qd_context* c, void*) -> int {
		return qd_push_f(c, 21.75);
	}, NULL);
	qd_register_function(sensors, "label", "( -- s:str)", [](qd_context* c, void*) -> int {
		return qd_push_s(c, "TEMP");
	}, NULL);
	qd_build(sensors);

	qd_module* app = qd_get_module(ctx, "app");
	qd_add_script(app,
		"use sens\n"
		"use strconv\n"
		"fn report( -- ) {\n"
		"    sens::label print \"=\" print\n"
		"    sens::temp strconv::format_float print nl\n"
		"}\n"
	);
	qd_build(app);
	check("app compiled (stdlib interop)", qd_is_compiled(app));

	qd_execute(ctx, "app::report");

	qd_free_context(ctx);
}

void test_context_userdata() {
	printf("--- Context-level userdata ---\n");

	qd_context* ctx = qd_create_context(1024);

	int game_score = 999;
	qd_set_userdata(ctx, &game_score);

	qd_module* g = qd_get_module(ctx, "game");
	qd_register_function(g, "score", "( -- v:i64)", [](qd_context* c, void*) -> int {
		int* score = static_cast<int*>(qd_get_userdata(c));
		return qd_push_i(c, *score);
	}, NULL);
	qd_build(g);

	qd_module* t6 = qd_get_module(ctx, "t6");
	qd_add_script(t6,
		"use game\n"
		"fn show( -- ) { game::score print nl }\n"
	);
	qd_build(t6);
	check("t6 compiled", qd_is_compiled(t6));

	qd_execute(ctx, "t6::show");

	qd_free_context(ctx);
}

void test_many_functions() {
	printf("--- Module with many native functions ---\n");

	qd_context* ctx = qd_create_context(1024);

	qd_module* ops = qd_get_module(ctx, "ops");
	qd_register_function(ops, "one",   "( -- v:i64)", [](qd_context* c, void*) -> int { return qd_push_i(c, 1); }, NULL);
	qd_register_function(ops, "two",   "( -- v:i64)", [](qd_context* c, void*) -> int { return qd_push_i(c, 2); }, NULL);
	qd_register_function(ops, "three", "( -- v:i64)", [](qd_context* c, void*) -> int { return qd_push_i(c, 3); }, NULL);
	qd_register_function(ops, "four",  "( -- v:i64)", [](qd_context* c, void*) -> int { return qd_push_i(c, 4); }, NULL);
	qd_register_function(ops, "five",  "( -- v:i64)", [](qd_context* c, void*) -> int { return qd_push_i(c, 5); }, NULL);
	qd_register_function(ops, "sum2",  "(a:i64 b:i64 -- r:i64)", native_add, NULL);
	qd_build(ops);

	qd_module* t7 = qd_get_module(ctx, "t7");
	qd_add_script(t7,
		"use ops\n"
		"fn run( -- ) {\n"
		"    ops::one ops::two ops::sum2\n"
		"    ops::three ops::sum2\n"
		"    ops::four ops::sum2\n"
		"    ops::five ops::sum2\n"
		"    print nl\n"
		"}\n"
	);
	qd_build(t7);
	check("t7 compiled", qd_is_compiled(t7));

	qd_execute(ctx, "t7::run");

	qd_free_context(ctx);
}

int main(void) {
	printf("=== Comprehensive Embedding API Test ===\n\n");

	qd_context* ctx = qd_create_context(1024);
	test_native_with_inputs(ctx);
	qd_free_context(ctx);

	test_userdata();
	test_multiple_returns();
	test_string_roundtrip();
	test_native_only_module();
	test_mixed_native_and_script();
	test_cross_module_typed_stdlib();
	test_context_userdata();
	test_many_functions();

	printf("\n=== Results: %d passed, %d failed ===\n", g_tests_passed, g_tests_failed);
	if (g_tests_failed == 0) {
		printf("=== All comprehensive tests passed ===\n");
	}
	return g_tests_failed > 0 ? 1 : 0;
}

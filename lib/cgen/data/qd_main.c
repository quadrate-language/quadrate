#include <runtime/context.h>
#include <runtime/exec_result.h>

extern qd_exec_result main_main(qd_context* ctx);

int main(void) {
	qd_context ctx;
	main_main(&ctx);
	return 0;
}

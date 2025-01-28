#include "qd_base.h"

int main() {
	// push 1.0
	__qd_push(1, 1.0);

	// add 2.0
	__qd_arg_push(2.0, 0.0, 0.0, 0.0);
	__qd_add();

	return 0;
}

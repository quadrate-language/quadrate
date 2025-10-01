#include "compiler.h"
#include <u8t/scanner.h>

void Compiler::compile(const char* source) {
	u8t_scanner scanner;
	if (!u8t_scanner_init(&scanner, source)) {
		return;
	}

	while (true) {
		char32_t token = u8t_scanner_scan(&scanner);
		if (token == U8T_EOF) {
			break;
		}

		switch (token) {
		case U8T_IDENTIFIER:
			printf("Identifier: %s\n", scanner._token_text);
			break;
		case U8T_INTEGER:
			printf("Integer: %s\n", scanner._token_text);
			break;
		case U8T_FLOAT:
			printf("Float: %s\n", scanner._token_text);
			break;
		case U8T_STRING:
			printf("String: %s\n", scanner._token_text);
			break;
		default:
			break;
		}
	}
}

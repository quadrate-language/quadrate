#include <qc/ast.h>
#include <qc/ast_node.h>
#include <u8t/scanner.h>

namespace Qd {
	IAstNode* Ast::generate(const char* src) const {
		u8t_scanner scanner;
		u8t_scanner_init(&scanner, src);

		return nullptr;
	}
}


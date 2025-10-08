#include "compiler.h"
#include <qc/ast.h>
#include <qc/ast_printer.h>

void Compiler::compile(const char* source) {
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(source);

	printf("\n=== AST ===\n");
	Qd::AstPrinter::print(root);
}

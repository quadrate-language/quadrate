#include "compiler.h"
#include <cgen/writer.h>
#include <qc/ast.h>
#include <qc/ast_printer.h>

void Compiler::compile(const char* source) {
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(source);

	Qd::AstPrinter::print(root);

	Qd::Writer writer;
	writer.write(root, "out.c");
}

#include "compiler.h"
#include <cgen/writer.h>
#include <filesystem>
#include <qc/ast.h>
#include <qc/ast_printer.h>

void Compiler::compile(const char* source) {
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(source);

	Qd::AstPrinter::print(root);

	std::string outputFolder = ".out";
	// Create output folder if it doesn't exist
	std::filesystem::remove_all(outputFolder);
	std::filesystem::create_directory(outputFolder);

	Qd::Writer writer;
	writer.write(root, "main", ".out/out.c");

	writer.writeMain(".out/qd_main.c");
}

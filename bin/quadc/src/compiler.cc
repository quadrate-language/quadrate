#include "compiler.h"
#include <cgen/writer.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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

	// Compile C files to object files
	std::cout << "Compiling out.c..." << std::endl;
	int ret1 = std::system("gcc -c .out/out.c -o .out/out.o -I./lib/quadrate/include");
	if (ret1 != 0) {
		std::cerr << "Error: Failed to compile out.c" << std::endl;
		exit(1);
	}

	std::cout << "Compiling qd_main.c..." << std::endl;
	int ret2 = std::system("gcc -c .out/qd_main.c -o .out/qd_main.o -I./lib/quadrate/include");
	if (ret2 != 0) {
		std::cerr << "Error: Failed to compile qd_main.c" << std::endl;
		exit(1);
	}

	std::cout << "Compilation successful. Object files generated." << std::endl;

	// Link object files into executable
	std::cout << "Linking..." << std::endl;
	int ret3 = std::system("gcc .out/out.o .out/qd_main.o -o ./main -L./dist/lib -lquadrate");
	if (ret3 != 0) {
		std::cerr << "Error: Failed to link" << std::endl;
		exit(1);
	}

	std::cout << "Build successful. Executable: ./main" << std::endl;
}

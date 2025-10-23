#include "compiler.h"
#include <cgen/writer.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_printer.h>
#include <sstream>

Compiler::Compiler(const char* outputDir) : mOutputDir(outputDir) {
	std::filesystem::remove_all(mOutputDir);
	std::filesystem::create_directory(mOutputDir);
}

bool Compiler::transpile(const char* filename, const char* package, const char* source) {
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(source);
	Qd::AstPrinter::print(root);

	Qd::Writer writer;
	std::filesystem::create_directory(mOutputDir + "/" + std::string(package));
	std::string mainFilename = mOutputDir + "/" + std::string(package) + "/" + std::string(filename) + ".c";
	writer.write(root, package, mainFilename.c_str());

	return true;
}

std::optional<TranslationUnit> Compiler::compile(const char* filename, const char* flags) {
	std::string cmd = "gcc -c " + std::string(filename) + " -o " + std::string(filename) + ".o " + std::string(flags);
	std::cout << "Compiling: " << cmd << std::endl;
	int ret = std::system(cmd.c_str());
	if (ret != 0) {
		return std::nullopt;
	}
	return TranslationUnit{std::string(filename) + ".o"};
}

bool Compiler::link(
		const std::vector<TranslationUnit>& translationUnits, const char* outputFilename, const char* flags) {
	std::stringstream cmd;
	cmd << "gcc ";
	for (const auto& obj : translationUnits) {
		cmd << obj.objectFilename << " ";
	}
	cmd << "-o " << outputFilename << " " << flags;

	std::cout << "Linking: " << cmd.str() << std::endl;
	int ret = std::system(cmd.str().c_str());
	if (ret != 0) {
		return false;
	}

	return true;
}

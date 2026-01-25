#ifndef QUADC_PARSED_MODULE_H
#define QUADC_PARSED_MODULE_H

#include <memory>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <string>
#include <vector>

// Helper structure to hold parsed AST data
struct ParsedModule {
	std::string name;
	std::string package;
	std::string sourceDirectory;
	std::string packageDirectory; // For third-party packages, e.g., /path/to/packages/color@master
	std::unique_ptr<Qd::Ast> ast;
	Qd::IAstNode* root;
	std::vector<std::string> importedModules;
	bool hasFFIImports = false;			 // True if module uses FFI (import statement)
	bool hasNativeStdlibModules = false; // True if module uses stdlib modules with native code
	bool mergeIntoMain = false;			 // True if this is a local file import that should merge into main namespace
};

#endif

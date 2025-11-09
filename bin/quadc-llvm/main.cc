#include <fstream>
#include <iostream>
#include <llvmgen/generator.h>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_use.h>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

// Helper function to find a module file
static std::string findModuleFile(const std::string& moduleName) {
	std::vector<std::string> searchPaths;

	// Check environment variable
	const char* quadrateRoot = std::getenv("QUADRATE_ROOT");
	if (quadrateRoot) {
		searchPaths.push_back(std::string(quadrateRoot));
	}

	// Check home directory
	const char* home = std::getenv("HOME");
	if (home) {
		searchPaths.push_back(std::string(home) + "/quadrate");
	}

	// Check system path
	searchPaths.push_back("/usr/share/quadrate");

	// Check local paths
	searchPaths.push_back(".");  // Current directory (for tests run from modules dir)
	searchPaths.push_back("tests/qd/modules");
	searchPaths.push_back("lib/stdqd/qd");

	// Try each search path
	for (const auto& basePath : searchPaths) {
		std::string modulePath = basePath + "/" + moduleName + "/module.qd";
		struct stat buffer;
		if (stat(modulePath.c_str(), &buffer) == 0) {
			return modulePath;
		}
	}

	return "";
}

// Helper function to collect all use statements from an AST
static void collectUseStatements(Qd::IAstNode* node, std::set<std::string>& modules) {
	if (!node) return;

	if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
		auto useNode = static_cast<Qd::AstNodeUse*>(node);
		modules.insert(useNode->module());
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectUseStatements(node->child(i), modules);
	}
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: quadc-llvm <file.qd> [-r|--run] [-o output]" << std::endl;
		return 1;
	}

	std::string inputFile;
	std::string outputFile = "output";
	bool runAfterCompile = false;

	// Parse command line arguments
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "-r" || arg == "--run") {
			runAfterCompile = true;
		} else if (arg == "-o" && i + 1 < argc) {
			outputFile = argv[++i];
		} else if (arg[0] != '-') {
			inputFile = arg;
		}
	}

	if (inputFile.empty()) {
		std::cerr << "Error: No input file specified" << std::endl;
		return 1;
	}

	// Read the input file
	std::ifstream file(inputFile);
	if (!file) {
		std::cerr << "Error: Cannot open file " << inputFile << std::endl;
		return 1;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string source = buffer.str();

	// Parse the source code
	Qd::Ast ast;
	auto root = ast.generate(source.c_str(), false, inputFile.c_str());
	if (!root || ast.hasErrors()) {
		std::cerr << "Parsing failed with " << ast.errorCount() << " errors" << std::endl;
		return 1;
	}

	// Load and parse all required modules (with transitive dependencies)
	// Keep Ast objects alive to prevent AST destruction
	// Use vectors to preserve loading order (dependencies first)
	std::vector<std::pair<std::string, Qd::IAstNode*>> moduleASTs;
	std::map<std::string, std::unique_ptr<Qd::Ast>> moduleAstObjects;
	int astCounter = 0; // Unique counter for AST object storage

	// Collect all use statements from main file
	std::set<std::string> modulesToLoad;
	collectUseStatements(root, modulesToLoad);

	// Process modules with transitive dependencies
	std::set<std::string> loadedModules;
	std::map<std::string, std::string> moduleDirectories; // Track module directories for .qd file loading

	while (!modulesToLoad.empty()) {
		// Take first unloaded module
		auto it = modulesToLoad.begin();
		std::string moduleName = *it;
		modulesToLoad.erase(it);

		// Skip if already loaded
		if (loadedModules.count(moduleName) > 0) {
			continue;
		}

		// Check if this is a .qd file (intra-module import)
		if (moduleName.size() > 3 && moduleName.substr(moduleName.size() - 3) == ".qd") {
			// This is a .qd file that should be loaded from a module's directory
			// We need to find which module directory to search
			// For now, skip these - they'll be handled when processing the parent module
			continue;
		}

		// Find and load module
		std::string modulePath = findModuleFile(moduleName);
		if (modulePath.empty()) {
			std::cerr << "Error: Module '" << moduleName << "' not found" << std::endl;
			return 1;
		}

		// Extract module directory for later .qd file loading
		size_t lastSlash = modulePath.find_last_of('/');
		std::string moduleDir = (lastSlash != std::string::npos) ? modulePath.substr(0, lastSlash) : ".";
		moduleDirectories[moduleName] = moduleDir;

		// Read module file
		std::ifstream moduleFile(modulePath);
		if (!moduleFile) {
			std::cerr << "Error: Cannot open module file " << modulePath << std::endl;
			return 1;
		}

		std::stringstream moduleBuffer;
		moduleBuffer << moduleFile.rdbuf();
		std::string moduleSource = moduleBuffer.str();

		// Parse module - keep Ast object alive
		auto moduleAstObj = std::make_unique<Qd::Ast>();
		auto moduleRoot = moduleAstObj->generate(moduleSource.c_str(), false, modulePath.c_str());
		if (!moduleRoot || moduleAstObj->hasErrors()) {
			std::cerr << "Error parsing module '" << moduleName << "': "
					  << moduleAstObj->errorCount() << " errors" << std::endl;
			return 1;
		}

		moduleASTs.push_back({moduleName, moduleRoot});
		std::string astKey = moduleName + "_" + std::to_string(astCounter++);
		moduleAstObjects[astKey] = std::move(moduleAstObj);
		loadedModules.insert(moduleName);

		// Collect dependencies from this module
		std::set<std::string> moduleDeps;
		collectUseStatements(moduleRoot, moduleDeps);
		for (const auto& dep : moduleDeps) {
			if (loadedModules.count(dep) == 0) {
				// Check if this is a .qd file
				if (dep.size() > 3 && dep.substr(dep.size() - 3) == ".qd") {
					// Load .qd file from module's directory
					std::string qdPath = moduleDir + "/" + dep;
					struct stat statBuf;
					if (stat(qdPath.c_str(), &statBuf) != 0) {
						std::cerr << "Error: Cannot find " << dep << " in module " << moduleName << std::endl;
						return 1;
					}

					// Read .qd file
					std::ifstream qdFile(qdPath);
					if (!qdFile) {
						std::cerr << "Error: Cannot open " << qdPath << std::endl;
						return 1;
					}

					std::stringstream qdBuffer;
					qdBuffer << qdFile.rdbuf();
					std::string qdSource = qdBuffer.str();

					// Parse .qd file as part of the same module
					auto qdAstObj = std::make_unique<Qd::Ast>();
					auto qdRoot = qdAstObj->generate(qdSource.c_str(), false, qdPath.c_str());
					if (!qdRoot || qdAstObj->hasErrors()) {
						std::cerr << "Error parsing " << qdPath << ": "
								  << qdAstObj->errorCount() << " errors" << std::endl;
						return 1;
					}

					// Add .qd file AST with the same module name
					moduleASTs.push_back({moduleName, qdRoot});
					astKey = moduleName + "_" + std::to_string(astCounter++);
					moduleAstObjects[astKey] = std::move(qdAstObj);
				} else {
					// Regular module dependency
					modulesToLoad.insert(dep);
				}
			}
		}
	}

	// Generate LLVM IR
	Qd::LlvmGenerator generator;

	// Add all module ASTs in reverse order (dependencies first)
	// Modules were loaded in breadth-first order (dependents before dependencies)
	// but we need to generate them depth-first (dependencies before dependents)
	for (auto it = moduleASTs.rbegin(); it != moduleASTs.rend(); ++it) {
		generator.addModuleAST(it->first, it->second);
	}

	if (!generator.generate(root, "quadrate_module")) {
		std::cerr << "LLVM generation failed" << std::endl;
		return 1;
	}

	// Print IR to stdout for inspection
	std::cout << "=== Generated LLVM IR ===" << std::endl;
	std::cout << generator.getIRString() << std::endl;

	// Write IR to file
	std::string irFile = outputFile + ".ll";
	if (!generator.writeIR(irFile)) {
		std::cerr << "Failed to write IR file" << std::endl;
		return 1;
	}
	std::cout << "Written IR to " << irFile << std::endl;

	// Write executable
	if (!generator.writeExecutable(outputFile)) {
		std::cerr << "Failed to create executable" << std::endl;
		return 1;
	}
	std::cout << "Written executable to " << outputFile << std::endl;

	// Run the executable if requested
	if (runAfterCompile) {
		std::cout << "\n=== Running " << outputFile << " ===" << std::endl;
		int exitCode = system(("./" + outputFile).c_str());
		return WEXITSTATUS(exitCode);
	}

	return 0;
}

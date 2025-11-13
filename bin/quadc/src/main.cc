#include "cxxopts.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <llvmgen/generator.h>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_use.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <random>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define QUADC_VERSION "0.1.0"

std::string createTempDir(bool useCwd) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);

	// Determine base directory: use cwd if --save-temps, otherwise use system temp
	std::filesystem::path baseDir;
	if (useCwd) {
		baseDir = std::filesystem::current_path();
	} else {
		baseDir = std::filesystem::temp_directory_path();
	}

	// Try up to 10 times to create a unique directory
	for (int attempt = 0; attempt < 10; attempt++) {
		std::stringstream ss;
		ss << "qd_";
		for (int i = 0; i < 8; i++) {
			ss << std::hex << dis(gen);
		}

		std::filesystem::path tmpDir = baseDir / ss.str();

		// Try to create the directory atomically (no TOCTOU race)
		std::error_code ec;
		if (std::filesystem::create_directory(tmpDir, ec)) {
			return tmpDir.string();
		}

		// If directory already exists, try again with a different name
		if (ec.value() == EEXIST) {
			continue;
		}

		// For other errors, fail immediately
		std::cerr << "quadc: failed to create temporary directory: " << ec.message() << std::endl;
		exit(1);
	}

	// Failed to create directory after multiple attempts
	std::cerr << "quadc: failed to create temporary directory after 10 attempts" << std::endl;
	exit(1);
}

// RAII guard for automatic temp directory cleanup
class TempDirGuard {
public:
	explicit TempDirGuard(const std::string& path) : mPath(path), mShouldDelete(true) {
	}

	~TempDirGuard() {
		if (mShouldDelete && !mPath.empty()) {
			std::filesystem::remove_all(mPath);
		}
	}

	void release() {
		mShouldDelete = false;
	}

	// Prevent copying
	TempDirGuard(const TempDirGuard&) = delete;
	TempDirGuard& operator=(const TempDirGuard&) = delete;

private:
	std::string mPath;
	bool mShouldDelete;
};

// Find a module file, searching in multiple locations
// Returns the full path to the module file, or empty string if not found
std::string findModuleFile(const std::string& moduleName, const std::string& sourceDir) {
	// Check if this is a direct file import (ends with .qd)
	bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isDirectFile) {
		// Try 1: Local path (relative to source file)
		std::string localPath = sourceDir + "/" + moduleName;
		if (std::filesystem::exists(localPath)) {
			return localPath;
		}

		// Try 2: QUADRATE_ROOT environment variable
		const char* quadrateRoot = getenv("QUADRATE_ROOT");
		if (quadrateRoot) {
			std::string rootPath = std::string(quadrateRoot) + "/" + moduleName;
			if (std::filesystem::exists(rootPath)) {
				return rootPath;
			}
		}

		// Try 3: $HOME/quadrate directory
		const char* home = getenv("HOME");
		if (home) {
			std::string homePath = std::string(home) + "/quadrate/" + moduleName;
			if (std::filesystem::exists(homePath)) {
				return homePath;
			}
		}

		// Try 4: System-wide installation
		std::string systemPath = "/usr/share/quadrate/" + moduleName;
		if (std::filesystem::exists(systemPath)) {
			return systemPath;
		}
	} else {
		// Module directory import (original behavior)
		// Try 1: Local path (relative to source file)
		std::string localPath = sourceDir + "/" + moduleName + "/module.qd";
		if (std::filesystem::exists(localPath)) {
			return localPath;
		}

		// Try 2: QUADRATE_ROOT environment variable
		const char* quadrateRoot = getenv("QUADRATE_ROOT");
		if (quadrateRoot) {
			std::string rootPath = std::string(quadrateRoot) + "/" + moduleName + "/module.qd";
			if (std::filesystem::exists(rootPath)) {
				return rootPath;
			}
		}

		// Try 3: Standard library directories (e.g., lib/stdmathqd/qd/math for "math" module)
		std::string stdLibPath = "lib/std" + moduleName + "qd/qd/" + moduleName + "/module.qd";
		if (std::filesystem::exists(stdLibPath)) {
			return stdLibPath;
		}

		// Try 4: $HOME/quadrate directory
		const char* home = getenv("HOME");
		if (home) {
			std::string homePath = std::string(home) + "/quadrate/" + moduleName + "/module.qd";
			if (std::filesystem::exists(homePath)) {
				return homePath;
			}
		}

		// Try 5: System-wide installation
		std::string systemPath = "/usr/share/quadrate/" + moduleName + "/module.qd";
		if (std::filesystem::exists(systemPath)) {
			return systemPath;
		}
	}

	return ""; // Not found
}

// Helper structure to hold parsed AST data
struct ParsedModule {
	std::string name;
	std::string package;
	std::string sourceDirectory;
	std::unique_ptr<Qd::Ast> ast;
	Qd::IAstNode* root;
	std::vector<std::string> importedModules;
};

int main(int argc, char** argv) {
	cxxopts::Options options("quadc", "Quadrate compiler (LLVM backend)");
	options.add_options()("h,help", "Display help.")("v,version", "Display compiler version.")(
			"o", "Output filename", cxxopts::value<std::string>()->default_value("main"))("save-temps",
			"Save temporary files", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("verbose",
			"Print compilation commands",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("dump-tokens", "Print tokens",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("r,run", "Run the compiled program",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"dump-ir", "Print LLVM IR", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"files", "Input files", cxxopts::value<std::vector<std::string>>());

	options.parse_positional({"files"});
	auto result = options.parse(argc, argv);

	if (result.count("help") || argc == 1) {
		std::cout << options.help() << std::endl;
		return 0;
	}

	if (result.count("version")) {
		std::cout << QUADC_VERSION << std::endl;
		return 0;
	}

	// Configure colored output - check NO_COLOR environment variable
	const bool noColors = std::getenv("NO_COLOR") != nullptr;
	Qd::Colors::setEnabled(!noColors);

	std::string outputFilename = result["o"].as<std::string>();
	const bool run = result["run"].as<bool>();

	// Check if we should preserve temp files (needed for temp dir location)
	const bool saveTemps = result["save-temps"].as<bool>();

	const std::string outputDir = createTempDir(saveTemps);
	TempDirGuard tempGuard(outputDir);

	// When running, place executable in temp directory's bin subdirectory
	std::string outputPath;
	if (run) {
		std::filesystem::path binDir = std::filesystem::path(outputDir) / "bin";
		std::filesystem::create_directory(binDir);
		outputPath = (binDir / outputFilename).string();
	} else {
		outputPath = outputFilename;
	}

	// Preserve temp files if requested
	if (saveTemps) {
		tempGuard.release();
		std::cout << "Temporary files saved in: " << outputDir << std::endl;
	}

	// Check if verbose output is enabled
	const bool verbose = result["verbose"].as<bool>();

	// Check if token dumping is enabled
	const bool dumpTokens = result["dump-tokens"].as<bool>();

	// Check if IR dumping is enabled
	const bool dumpIR = result["dump-ir"].as<bool>();

	if (result.count("files")) {
		auto files = result["files"].as<std::vector<std::string>>();
		std::vector<ParsedModule> parsedModules;

		// Parse all main source files
		for (const auto& file : files) {
			std::ifstream qdFile(file);
			if (!qdFile.is_open()) {
				std::cerr << "quadc: cannot find " << file << ": No such file or directory" << std::endl;
				continue;
			}
			qdFile.seekg(0, std::ios::end);
			auto pos = qdFile.tellg();
			qdFile.seekg(0);
			if (pos < 0) {
				std::cerr << "quadc: error reading " << file << std::endl;
				continue;
			}
			size_t size = static_cast<size_t>(pos);
			std::string buffer(size, ' ');
			qdFile.read(&buffer[0], static_cast<std::streamsize>(size));

			// Parse the source
			auto ast = std::make_unique<Qd::Ast>();
			auto root = ast->generate(buffer.c_str(), dumpTokens, file.c_str());
			if (!root || ast->hasErrors()) {
				std::cerr << "quadc: parsing failed for " << file << " with " << ast->errorCount() << " errors"
						  << std::endl;
				return 1;
			}

			// Semantic validation - catch errors before LLVM generation
			Qd::SemanticValidator validator;
			size_t errorCount = validator.validate(root, file.c_str());
			if (errorCount > 0) {
				// Validation failed - do not proceed
				return 1;
			}

			// Get source directory for module resolution
			std::filesystem::path filePath(file);
			std::string sourceDirectory = filePath.parent_path().string();
			if (sourceDirectory.empty()) {
				sourceDirectory = ".";
			}

			ParsedModule module;
			module.name = file;
			module.package = "main";
			module.sourceDirectory = sourceDirectory;
			module.root = root;
			module.ast = std::move(ast);

			// Collect imported modules
			std::function<void(Qd::IAstNode*)> collectImports = [&](Qd::IAstNode* node) {
				if (!node) {
					return;
				}
				// Check for USE statement nodes (type == USE_STATEMENT)
				if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
					auto* useNode = static_cast<Qd::AstNodeUse*>(node);
					module.importedModules.push_back(useNode->module());
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					collectImports(node->child(i));
				}
			};
			collectImports(root);

			parsedModules.push_back(std::move(module));
		}

		// Collect all imported modules from main sources
		// Track which package each import belongs to (for .qd file imports)
		std::unordered_set<std::string> allModules;
		std::unordered_set<std::string> processedModules;
		std::unordered_map<std::string, std::string> moduleToPackage;	// moduleName -> packageName
		std::unordered_map<std::string, std::string> moduleToSourceDir; // moduleName -> sourceDirectory
		std::string sourceDirectory;
		for (const auto& module : parsedModules) {
			for (const auto& importedModule : module.importedModules) {
				allModules.insert(importedModule);

				// Check if this is a .qd file import
				bool isDirectFile =
						importedModule.size() >= 3 && importedModule.substr(importedModule.size() - 3) == ".qd";
				if (isDirectFile) {
					// .qd file imports use the parent package name
					moduleToPackage[importedModule] = module.package;
					moduleToSourceDir[importedModule] = module.sourceDirectory;
				} else {
					// Regular module imports get their own package
					moduleToPackage[importedModule] = importedModule;
					moduleToSourceDir[importedModule] = module.sourceDirectory;
				}
			}
			// Use source directory from first source (they should all be the same for now)
			if (sourceDirectory.empty()) {
				sourceDirectory = module.sourceDirectory;
			}
		}

		// Transpile all imported modules (including transitive imports)
		// Keep processing until no new modules are discovered
		while (!allModules.empty()) {
			// Get next unprocessed module
			std::string moduleName = *allModules.begin();
			allModules.erase(allModules.begin());

			// Skip if already processed
			if (processedModules.count(moduleName)) {
				continue;
			}
			processedModules.insert(moduleName);

			// Get the package name and source directory for this module
			std::string packageName = moduleToPackage.count(moduleName) ? moduleToPackage[moduleName] : moduleName;
			std::string moduleSourceDir =
					moduleToSourceDir.count(moduleName) ? moduleToSourceDir[moduleName] : sourceDirectory;

			std::string moduleFilePath = findModuleFile(moduleName, moduleSourceDir);
			if (moduleFilePath.empty()) {
				// Module file not found - skip silently (already validated)
				continue;
			}

			// Read module file
			std::ifstream moduleFile(moduleFilePath);
			if (!moduleFile.is_open()) {
				continue;
			}
			moduleFile.seekg(0, std::ios::end);
			auto pos = moduleFile.tellg();
			moduleFile.seekg(0);
			if (pos < 0) {
				continue;
			}
			size_t size = static_cast<size_t>(pos);
			std::string buffer(size, ' ');
			moduleFile.read(&buffer[0], static_cast<std::streamsize>(size));

			// Parse the module
			auto ast = std::make_unique<Qd::Ast>();
			auto root = ast->generate(buffer.c_str(), false, moduleFilePath.c_str());
			if (!root || ast->hasErrors()) {
				std::cerr << "quadc: failed to parse module: " << moduleName << std::endl;
				return 1;
			}

			// Semantic validation - catch errors before LLVM generation
			// Pass true for isModuleFile to skip reporting errors for missing nested module imports
			Qd::SemanticValidator validator;
			size_t errorCount = validator.validate(root, moduleFilePath.c_str(), true);
			if (errorCount > 0) {
				// Validation failed - do not proceed
				return 1;
			}

			// Get module's source directory
			std::filesystem::path moduleFilePathObj(moduleFilePath);
			std::string moduleFileSourceDir = moduleFilePathObj.parent_path().string();
			if (moduleFileSourceDir.empty()) {
				moduleFileSourceDir = ".";
			}

			ParsedModule parsedMod;
			parsedMod.name = moduleName;
			parsedMod.package = packageName;
			parsedMod.sourceDirectory = moduleFileSourceDir;
			parsedMod.root = root;
			parsedMod.ast = std::move(ast);

			// Collect imports from this module
			std::function<void(Qd::IAstNode*)> collectImports = [&](Qd::IAstNode* node) {
				if (!node) {
					return;
				}
				if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
					auto* useNode = static_cast<Qd::AstNodeUse*>(node);
					parsedMod.importedModules.push_back(useNode->module());
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					collectImports(node->child(i));
				}
			};
			collectImports(root);

			// Add any modules imported by this module to the set
			for (const auto& transitiveModule : parsedMod.importedModules) {
				if (!processedModules.count(transitiveModule)) {
					allModules.insert(transitiveModule);

					// Determine package for transitive imports
					bool isDirectFile = transitiveModule.size() >= 3 &&
										transitiveModule.substr(transitiveModule.size() - 3) == ".qd";
					if (isDirectFile) {
						// .qd file imports use the same package as the module that imported them
						moduleToPackage[transitiveModule] = packageName;
						moduleToSourceDir[transitiveModule] = moduleFileSourceDir;
					} else {
						// Regular module imports get their own package and search from original source dir
						moduleToPackage[transitiveModule] = transitiveModule;
						moduleToSourceDir[transitiveModule] = sourceDirectory;
					}
				}
			}

			parsedModules.push_back(std::move(parsedMod));
		}

		// Now generate LLVM IR from all parsed modules
		Qd::LlvmGenerator generator;

		// Add all dependency modules in REVERSE order (dependencies first)
		// Modules were loaded in breadth-first order (main first, then dependents, then their dependencies)
		// but we need to generate them depth-first (deep dependencies first, then their dependents)
		for (auto it = parsedModules.rbegin(); it != parsedModules.rend(); ++it) {
			if (it->package != "main") {
				generator.addModuleAST(it->package, it->root);
			}
		}

		// Generate main module last
		Qd::IAstNode* mainRoot = nullptr;
		for (auto& module : parsedModules) {
			if (module.package == "main") {
				mainRoot = module.root;
				break;
			}
		}

		if (!mainRoot) {
			std::cerr << "quadc: no main module found" << std::endl;
			return 1;
		}

		if (!generator.generate(mainRoot, "main")) {
			std::cerr << "quadc: LLVM generation failed" << std::endl;
			return 1;
		}

		// Print IR to stdout if requested
		if (dumpIR || verbose) {
			std::cout << "=== Generated LLVM IR ===" << std::endl;
			std::cout << generator.getIRString() << std::endl;
		}

		// Write IR to file if save-temps is enabled
		if (saveTemps) {
			std::string irFile = (std::filesystem::path(outputDir) / (outputFilename + ".ll")).string();
			if (!generator.writeIR(irFile)) {
				std::cerr << "quadc: failed to write IR file" << std::endl;
				return 1;
			}
			if (verbose) {
				std::cout << "Written IR to " << irFile << std::endl;
			}
		}

		// Write executable
		if (!generator.writeExecutable(outputPath)) {
			std::cerr << "quadc: failed to create executable" << std::endl;
			return 1;
		}

		if (verbose) {
			std::cout << "Written executable to " << outputPath << std::endl;
		}

		// Run the program if requested
		if (run) {
			if (verbose) {
				std::cout << "\n=== Running " << outputPath << " ===" << std::endl;
			}
			// Execute using system() and get exit code
			int status = system(outputPath.c_str());
			if (status == -1) {
				std::cerr << "quadc: failed to execute program" << std::endl;
				return 1;
			}
			// Check if process exited normally or was killed by signal
			int exitCode;
			if (WIFEXITED(status)) {
				exitCode = WEXITSTATUS(status);
			} else {
				// Process was terminated by a signal
				exitCode = -1;
			}
			if (exitCode != 0) {
				std::cerr << "quadc: program exited with code " << exitCode << std::endl;
			}
			return exitCode;
		}
	}

	return 0;
}

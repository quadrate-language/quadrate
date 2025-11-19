#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <llvmgen/generator.h>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_use.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define QUADC_VERSION "0.1.0"

// Global module version pins (set from command-line -l flags)
static std::unordered_map<std::string, std::string> g_moduleVersionPins;

struct Options {
	std::vector<std::string> files;
	std::string outputName = "main";
	int optLevel = 0; // 0-3 for -O0 through -O3
	bool help = false;
	bool version = false;
	bool saveTemps = false;
	bool verbose = false;
	bool dumpTokens = false;
	bool run = false;
	bool dumpIR = false;
	bool debugInfo = false;
	bool werror = false;
	std::unordered_map<std::string, std::string> moduleVersions; // module name -> version
};

void printHelp() {
	std::cout << "quadc - Quadrate compiler\n\n";
	std::cout << "Compiles .qd source files to native executables via LLVM.\n\n";
	std::cout << "Usage: quadc [options] <file>...\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help         Show this help message\n";
	std::cout << "  -v, --version      Show version information\n";
	std::cout << "  -o <name>          Output executable name (default: main)\n";
	std::cout << "  -O0, -O1, -O2, -O3 Set optimization level (default: -O0)\n";
	std::cout << "  -g                 Generate debug information for GDB/LLDB\n";
	std::cout << "  -l <mod@ver>       Pin module to specific version (e.g., -l color@1.0.0)\n";
	std::cout << "  --save-temps       Keep temporary files for debugging\n";
	std::cout << "  --verbose          Show detailed compilation steps\n";
	std::cout << "  --dump-tokens      Print lexer tokens\n";
	std::cout << "  -r, --run          Compile and run immediately\n";
	std::cout << "  --dump-ir          Print generated LLVM IR\n";
	std::cout << "  --werror           Treat warnings as errors\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quadc main.qd              Compile to executable 'main'\n";
	std::cout << "  quadc -o prog main.qd      Compile to executable 'prog'\n";
	std::cout << "  quadc -r main.qd           Compile and run immediately\n";
}

void printVersion() {
	std::cout << QUADC_VERSION << "\n";
}

bool parseArgs(int argc, char* argv[], Options& opts) {
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help") {
			opts.help = true;
			return true;
		} else if (arg == "-v" || arg == "--version") {
			opts.version = true;
			return true;
		} else if (arg == "-o") {
			if (i + 1 >= argc) {
				std::cerr << "quadc: option '-o' requires an argument\n";
				std::cerr << "Try 'quadc --help' for more information.\n";
				return false;
			}
			opts.outputName = argv[++i];
		} else if (arg == "--save-temps") {
			opts.saveTemps = true;
		} else if (arg == "--verbose") {
			opts.verbose = true;
		} else if (arg == "--dump-tokens") {
			opts.dumpTokens = true;
		} else if (arg == "-r" || arg == "--run") {
			opts.run = true;
		} else if (arg == "--dump-ir") {
			opts.dumpIR = true;
		} else if (arg == "-g") {
			opts.debugInfo = true;
		} else if (arg == "-l") {
			if (i + 1 >= argc) {
				std::cerr << "quadc: option '-l' requires an argument (module@version)\n";
				std::cerr << "Try 'quadc --help' for more information.\n";
				return false;
			}
			std::string moduleSpec = argv[++i];

			// Parse module@version format
			size_t atPos = moduleSpec.find('@');
			if (atPos == std::string::npos || atPos == 0 || atPos == moduleSpec.size() - 1) {
				std::cerr << "quadc: invalid format for '-l': '" << moduleSpec << "'\n";
				std::cerr << "Expected format: module@version (e.g., color@1.0.0)\n";
				return false;
			}

			std::string moduleName = moduleSpec.substr(0, atPos);
			std::string version = moduleSpec.substr(atPos + 1);
			opts.moduleVersions[moduleName] = version;
		} else if (arg == "--werror") {
			opts.werror = true;
		} else if (arg == "-O0") {
			opts.optLevel = 0;
		} else if (arg == "-O1") {
			opts.optLevel = 1;
		} else if (arg == "-O2") {
			opts.optLevel = 2;
		} else if (arg == "-O3") {
			opts.optLevel = 3;
		} else if (arg[0] == '-') {
			std::cerr << "quadc: unknown option: " << arg << "\n";
			std::cerr << "Try 'quadc --help' for more information.\n";
			return false;
		} else {
			opts.files.push_back(arg);
		}
	}

	if (opts.files.empty() && !opts.help && !opts.version) {
		std::cerr << "quadc: no input files\n";
		std::cerr << "Try 'quadc --help' for more information.\n";
		return false;
	}

	return true;
}

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

// Expand tilde (~) in file paths
std::string expandTilde(const std::string& path) {
	if (path.empty() || path[0] != '~') {
		return path;
	}

	const char* home = std::getenv("HOME");
	if (!home) {
		return path; // Can't expand, return as-is
	}

	// Replace ~ or ~/ with home directory
	if (path.length() == 1) {
		return std::string(home);
	} else if (path[1] == '/') {
		return std::string(home) + path.substr(1);
	}

	// ~username syntax not supported, return as-is
	return path;
}

// Get packages directory path (where quadpm installs third-party modules)
std::string getPackagesDir() {
	// Check QUADRATE_PATH environment variable first
	const char* quadratePath = std::getenv("QUADRATE_PATH");
	if (quadratePath) {
		return std::string(quadratePath);
	}

	// Check if XDG_DATA_HOME is set
	const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
	if (xdgDataHome) {
		return std::string(xdgDataHome) + "/quadrate/packages";
	}

	// Default to ~/quadrate/packages
	const char* home = std::getenv("HOME");
	if (home) {
		return std::string(home) + "/quadrate/packages";
	}

	return ""; // No packages directory available
}

// Find a package in the packages directory
// Checks g_moduleVersionPins first for pinned versions from -l flags
// Returns the full path to the package directory, or empty string if not found
std::string findLatestPackageVersion(const std::string& moduleName) {
	std::string packagesDir = getPackagesDir();
	if (packagesDir.empty() || !std::filesystem::exists(packagesDir)) {
		return "";
	}

	// Check if this module has a pinned version from -l flag
	std::string pinnedVersion;
	if (g_moduleVersionPins.count(moduleName)) {
		pinnedVersion = g_moduleVersionPins.at(moduleName);
	}

	// If version is pinned, look for exact match
	if (!pinnedVersion.empty()) {
		std::string exactPath = packagesDir + "/" + moduleName + "@" + pinnedVersion;
		if (std::filesystem::exists(exactPath) && std::filesystem::is_directory(exactPath)) {
			return exactPath;
		}
		return ""; // Pinned version not found
	}

	// No version specified - find any version
	std::string latestVersion;
	std::string latestPath;

	try {
		for (const auto& entry : std::filesystem::directory_iterator(packagesDir)) {
			if (!entry.is_directory()) {
				continue;
			}

			std::string dirName = entry.path().filename().string();

			// Check if directory name starts with "moduleName@"
			std::string prefix = moduleName + "@";
			if (dirName.size() > prefix.size() && dirName.substr(0, prefix.size()) == prefix) {
				std::string foundVersion = dirName.substr(prefix.size());

				// For now, just use the first match (later could implement version comparison)
				// If multiple versions exist, we use the last one found by directory iterator
				latestVersion = foundVersion;
				latestPath = entry.path().string();
			}
		}
	} catch (...) {
		// Ignore errors iterating directory
		return "";
	}

	return latestPath;
}

// Get package name from a module identifier
// For file paths (ending in .qd), returns the filename without extension
// For module names, returns the module name as-is
std::string getPackageFromModuleName(const std::string& moduleName) {
	// Check if this is a file path (ends with .qd)
	bool isFilePath = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isFilePath) {
		// Extract filename from path
		size_t lastSlash = moduleName.find_last_of('/');
		std::string filename = (lastSlash != std::string::npos) ? moduleName.substr(lastSlash + 1) : moduleName;

		// Remove .qd extension
		if (filename.size() >= 3 && filename.substr(filename.size() - 3) == ".qd") {
			filename = filename.substr(0, filename.size() - 3);
		}

		return filename;
	}

	// Not a file path, return as-is
	return moduleName;
}

// Find a module file, searching in multiple locations
// Returns the full path to the module file, or empty string if not found
std::string findModuleFile(const std::string& moduleName, const std::string& sourceDir) {
	// Check if this is a direct file import (ends with .qd)
	bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isDirectFile) {
		// Expand tilde in the path
		std::string expandedModuleName = expandTilde(moduleName);

		// If it's an absolute path (starts with / or was expanded from ~), use it directly
		if (!expandedModuleName.empty() && expandedModuleName[0] == '/') {
			if (std::filesystem::exists(expandedModuleName)) {
				return expandedModuleName;
			}
			return ""; // Absolute path doesn't exist
		}

		// For relative paths (including ./ and ../), resolve relative to source directory
		std::filesystem::path filePath;
		if (moduleName.size() >= 2 && (moduleName.substr(0, 2) == "./" || moduleName.substr(0, 3) == "../")) {
			// Explicit relative path
			filePath = std::filesystem::path(sourceDir) / moduleName;
		} else {
			// Implicit relative path (no leading ./ or ../)
			filePath = std::filesystem::path(sourceDir) / moduleName;
		}

		// Normalize the path (resolve .. and .)
		try {
			filePath = std::filesystem::weakly_canonical(filePath);
			if (std::filesystem::exists(filePath)) {
				return filePath.string();
			}
		} catch (...) {
			// If path resolution fails, try simple concatenation
			std::string localPath = sourceDir + "/" + moduleName;
			if (std::filesystem::exists(localPath)) {
				return localPath;
			}
		}

		// File not found
		return "";
	} else {
		// Module directory import (original behavior)
		// Try 1: Local path (relative to source file)
		std::string localPath = sourceDir + "/" + moduleName + "/module.qd";
		if (std::filesystem::exists(localPath)) {
			return localPath;
		}

		// Try 2: Third-party packages directory (installed via quadpm)
		std::string packagePath = findLatestPackageVersion(moduleName);
		if (!packagePath.empty()) {
			std::string moduleFile = packagePath + "/module.qd";
			if (std::filesystem::exists(moduleFile)) {
				return moduleFile;
			}
		}

		// Try 3: QUADRATE_ROOT environment variable
		const char* quadrateRoot = getenv("QUADRATE_ROOT");
		if (quadrateRoot) {
			std::string rootPath = std::string(quadrateRoot) + "/" + moduleName + "/module.qd";
			if (std::filesystem::exists(rootPath)) {
				return rootPath;
			}
		}

		// Try 4: Standard library directories relative to current directory (for development)
		std::string stdLibPath = "lib/std" + moduleName + "qd/qd/" + moduleName + "/module.qd";
		if (std::filesystem::exists(stdLibPath)) {
			return stdLibPath;
		}

		// Try 5: Standard library relative to executable (for installed binaries)
		// Get executable path and look for ../share/quadrate/<module>/module.qd
		try {
			std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe");
			std::filesystem::path exeDir = exePath.parent_path();
			std::filesystem::path sharePath = exeDir / ".." / "share" / "quadrate" / moduleName / "module.qd";
			if (std::filesystem::exists(sharePath)) {
				return sharePath.string();
			}
		} catch (...) {
			// Ignore errors reading executable path
		}

		// Try 6: $HOME/quadrate directory
		const char* home = getenv("HOME");
		if (home) {
			std::string homePath = std::string(home) + "/quadrate/" + moduleName + "/module.qd";
			if (std::filesystem::exists(homePath)) {
				return homePath;
			}
		}

		// Try 7: System-wide installation
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
	std::string packageDirectory; // For third-party packages, e.g., /path/to/packages/color@master
	std::unique_ptr<Qd::Ast> ast;
	Qd::IAstNode* root;
	std::vector<std::string> importedModules;
};

int main(int argc, char** argv) {
	Options opts;

	// Show help if no arguments provided
	if (argc == 1) {
		printHelp();
		return 0;
	}

	if (!parseArgs(argc, argv, opts)) {
		return 1;
	}

	if (opts.help) {
		printHelp();
		return 0;
	}

	if (opts.version) {
		printVersion();
		return 0;
	}

	// Set global module version pins from command-line options
	g_moduleVersionPins = opts.moduleVersions;

	// Configure colored output - check NO_COLOR environment variable
	const bool noColors = std::getenv("NO_COLOR") != nullptr;
	Qd::Colors::setEnabled(!noColors);

	const std::string outputDir = createTempDir(opts.saveTemps);
	TempDirGuard tempGuard(outputDir);

	// When running, place executable in temp directory's bin subdirectory
	std::string outputPath;
	if (opts.run) {
		std::filesystem::path binDir = std::filesystem::path(outputDir) / "bin";
		std::filesystem::create_directory(binDir);
		outputPath = (binDir / opts.outputName).string();
	} else {
		outputPath = opts.outputName;
	}

	// Preserve temp files if requested
	if (opts.saveTemps) {
		tempGuard.release();
		std::cout << "Temporary files saved in: " << outputDir << std::endl;
	}

	if (!opts.files.empty()) {
		std::vector<ParsedModule> parsedModules;

		// Parse all main source files
		for (const auto& file : opts.files) {
			std::ifstream qdFile(file);
			if (!qdFile.is_open()) {
				std::cerr << "quadc: " << file << ": No such file or directory" << std::endl;
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
			auto root = ast->generate(buffer.c_str(), opts.dumpTokens, file.c_str());
			if (!root || ast->hasErrors()) {
				std::cerr << "quadc: parsing failed for " << file << " with " << ast->errorCount() << " errors"
						  << std::endl;
				return 1;
			}

			// Semantic validation - catch errors before LLVM generation
			Qd::SemanticValidator validator;
			size_t errorCount = validator.validate(root, file.c_str(), false, opts.werror);
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

				// Check if this is a .qd file import (direct file import)
				bool isDirectFile =
						importedModule.size() >= 3 && importedModule.substr(importedModule.size() - 3) == ".qd";
				if (isDirectFile) {
					// Direct file imports: determine if this is an intra-module import or a top-level import
					// Intra-module: parent is a module directory, file inherits parent's namespace
					// Top-level: parent is standalone .qd, file gets its own namespace from filename

					// Check if parent is a module directory (doesn't end in .qd)
					bool parentIsModuleDirectory =
							!(module.name.size() >= 3 && module.name.substr(module.name.size() - 3) == ".qd");

					if (parentIsModuleDirectory) {
						// Intra-module import: use parent's package
						// e.g., split_module module imports helper.qd → helper functions are split_module::*
						moduleToPackage[importedModule] = module.package;
					} else {
						// Top-level import: derive package from imported filename
						// e.g., main.qd imports calculator.qd → calculator functions are calculator::*
						moduleToPackage[importedModule] = getPackageFromModuleName(importedModule);
					}
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
			size_t errorCount = validator.validate(root, moduleFilePath.c_str(), true, opts.werror);
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

			// Detect if this module is from a third-party package
			// Package paths look like: /path/to/packages/modulename@version/module.qd
			std::string packageDir;
			std::string packagesDir = getPackagesDir();
			if (!packagesDir.empty()) {
				std::string normalizedModulePath = std::filesystem::absolute(moduleFilePath).string();
				std::string normalizedPackagesDir = std::filesystem::absolute(packagesDir).string();

				// Check if module path starts with packages directory
				if (normalizedModulePath.size() > normalizedPackagesDir.size() &&
						normalizedModulePath.substr(0, normalizedPackagesDir.size()) == normalizedPackagesDir) {
					// Extract the package directory (e.g., /path/to/packages/color@master)
					std::string relativePath = normalizedModulePath.substr(normalizedPackagesDir.size());
					if (!relativePath.empty() && relativePath[0] == '/') {
						relativePath = relativePath.substr(1);
					}
					// Get the first path component (modulename@version)
					size_t slashPos = relativePath.find('/');
					if (slashPos != std::string::npos) {
						std::string packageDirName = relativePath.substr(0, slashPos);
						packageDir = normalizedPackagesDir + "/" + packageDirName;
					}
				}
			}

			ParsedModule parsedMod;
			parsedMod.name = moduleName;
			parsedMod.package = packageName;
			parsedMod.sourceDirectory = moduleFileSourceDir;
			parsedMod.packageDirectory = packageDir;
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
						// Check if importing file is a module directory (doesn't end in .qd)
						bool importerIsModuleDirectory =
								!(moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd");

						if (importerIsModuleDirectory) {
							// Intra-module import: use importer's package
							moduleToPackage[transitiveModule] = packageName;
						} else {
							// Top-level import: derive package from filename
							moduleToPackage[transitiveModule] = getPackageFromModuleName(transitiveModule);
						}
						// File imports use the importing module's source directory
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

		// Enable debug info if requested
		if (opts.debugInfo) {
			generator.setDebugInfo(true);
		}

		// Set optimization level
		generator.setOptimizationLevel(opts.optLevel);

		// Add library search paths for third-party packages
		// Track which packages we've already added to avoid duplicates
		std::set<std::string> addedPackagePaths;
		for (const auto& module : parsedModules) {
			if (!module.packageDirectory.empty() && addedPackagePaths.find(module.packageDirectory) == addedPackagePaths.end()) {
				// Add the package's lib directory to the linker search paths
				std::string libPath = module.packageDirectory + "/lib";
				if (std::filesystem::exists(libPath)) {
					generator.addLibrarySearchPath(libPath);
					addedPackagePaths.insert(module.packageDirectory);
				}
			}
		}

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
		std::string mainSourceFile;
		for (auto& module : parsedModules) {
			if (module.package == "main") {
				mainRoot = module.root;
				mainSourceFile = module.name;
				break;
			}
		}

		if (!mainRoot) {
			std::cerr << "quadc: no main module found" << std::endl;
			return 1;
		}

		// Check if main function exists in main module
		bool hasMainFunction = false;
		for (size_t i = 0; i < mainRoot->childCount(); i++) {
			Qd::IAstNode* child = mainRoot->child(i);
			if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				auto* funcDecl = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
				if (funcDecl->name() == "main") {
					hasMainFunction = true;
					break;
				}
			}
		}

		if (!hasMainFunction) {
			std::cerr << "quadc: error: no 'main' function found in main module" << std::endl;
			std::cerr << "quadc: note: a Quadrate program must have a 'main' function as the entry point" << std::endl;
			return 1;
		}

		// Pass the actual source file path for debug info
		if (!generator.generate(mainRoot, mainSourceFile)) {
			std::cerr << "quadc: LLVM generation failed" << std::endl;
			return 1;
		}

		// Print IR to stdout if requested
		if (opts.dumpIR || opts.verbose) {
			std::cout << "=== Generated LLVM IR ===" << std::endl;
			std::cout << generator.getIRString() << std::endl;
		}

		// Write IR to file if save-temps is enabled
		if (opts.saveTemps) {
			std::string irFile = (std::filesystem::path(outputDir) / (opts.outputName + ".ll")).string();
			if (!generator.writeIR(irFile)) {
				std::cerr << "quadc: failed to write IR file" << std::endl;
				return 1;
			}
			if (opts.verbose) {
				std::cout << "Written IR to " << irFile << std::endl;
			}
		}

		// Write executable
		if (!generator.writeExecutable(outputPath)) {
			std::cerr << "quadc: failed to create executable" << std::endl;
			return 1;
		}

		if (opts.verbose) {
			std::cout << "Written executable to " << outputPath << std::endl;
		}

		// Run the program if requested
		if (opts.run) {
			if (opts.verbose) {
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

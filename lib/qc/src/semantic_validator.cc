#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_ctx.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_use.h>
#include <qc/colors.h>
#include <qc/instructions.h>
#include <qc/semantic_validator.h>
#include <sstream>

namespace Qd {

	// Helper function to expand tilde (~) in file paths
	static std::string expandTilde(const std::string& path) {
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

	// Helper function to extract package name from module identifier
	// For file paths (ending in .qd), returns the filename without extension
	// For module names, returns the module name as-is
	static std::string getPackageFromModuleName(const std::string& moduleName) {
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

	// Helper function to serialize a case value for comparison
	static std::string serializeCaseValue(IAstNode* node) {
		if (!node) {
			return "";
		}

		// Handle literals
		if (node->type() == IAstNode::Type::LITERAL) {
			AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(node);
			switch (lit->literalType()) {
			case AstNodeLiteral::LiteralType::INTEGER:
				return "int:" + lit->value();
			case AstNodeLiteral::LiteralType::FLOAT:
				return "float:" + lit->value();
			case AstNodeLiteral::LiteralType::STRING:
				return "string:" + lit->value();
			}
		}

		// For other node types, use a generic representation
		// This is a simple approach - could be enhanced for complex expressions
		return "node:" + std::to_string(reinterpret_cast<std::uintptr_t>(node));
	}

	static const char* stackValueTypeToString(StackValueType type) {
		switch (type) {
		case StackValueType::INT:
			return "int";
		case StackValueType::FLOAT:
			return "float";
		case StackValueType::STRING:
			return "string";
		case StackValueType::PTR:
			return "ptr";
		case StackValueType::ANY:
			return "any";
		case StackValueType::UNKNOWN:
			return "unknown";
		default:
			return "unknown";
		}
	}

	// Check if actual type can be implicitly cast to expected type
	// Returns true if implicit cast is allowed (int <-> float)
	static bool isImplicitCastAllowed(StackValueType actual, StackValueType expected) {
		// Allow int -> float and float -> int implicit conversions
		if ((actual == StackValueType::INT && expected == StackValueType::FLOAT) ||
				(actual == StackValueType::FLOAT && expected == StackValueType::INT)) {
			return true;
		}
		return false;
	}

	SemanticValidator::SemanticValidator()
		: mFilename(nullptr), mErrorCount(0), mWarningCount(0), mWerror(false), mIsModuleFile(false) {
	}

	bool SemanticValidator::isBuiltInInstruction(const char* name) const {
		// Use the extended validator list which includes stdlib imports
		return Qd::isKnownInstruction(name);
	}

	void SemanticValidator::reportError(const char* message) {
		reportErrorConditional(message, true);
	}

	void SemanticValidator::reportError(const IAstNode* node, const char* message) {
		reportErrorConditional(node, message, true);
	}

	void SemanticValidator::reportErrorConditional(const char* message, bool shouldReport) {
		if (!shouldReport) {
			return;
		}
		// GCC/Clang style: quadc: filename: error: message
		std::cerr << Colors::bold() << "quadc: " << Colors::reset();
		if (mFilename) {
			std::cerr << Colors::bold() << mFilename << ":" << Colors::reset() << " ";
		}
		std::cerr << Colors::bold() << Colors::red() << "error:" << Colors::reset() << " ";
		std::cerr << Colors::bold() << message << Colors::reset() << std::endl;
		mErrorCount++;
	}

	void SemanticValidator::reportErrorConditional(const IAstNode* node, const char* message, bool shouldReport) {
		if (!shouldReport) {
			return;
		}
		// GCC/Clang style: quadc: filename:line:column: error: message
		std::cerr << Colors::bold() << "quadc: " << Colors::reset();
		if (mFilename && node) {
			std::cerr << Colors::bold() << mFilename << ":" << node->line() << ":" << node->column() << ":"
					  << Colors::reset() << " ";
		} else if (mFilename) {
			std::cerr << Colors::bold() << mFilename << ":" << Colors::reset() << " ";
		}
		std::cerr << Colors::bold() << Colors::red() << "error:" << Colors::reset() << " ";
		std::cerr << Colors::bold() << message << Colors::reset() << std::endl;
		mErrorCount++;
	}

	void SemanticValidator::reportWarning(const IAstNode* node, const char* message) {
		// If werror is enabled, treat warnings as errors
		if (mWerror) {
			reportError(node, message);
			return;
		}

		// GCC/Clang style: quadc: filename:line:column: warning: message
		std::cerr << Colors::bold() << "quadc: " << Colors::reset();
		if (mFilename && node) {
			std::cerr << Colors::bold() << mFilename << ":" << node->line() << ":" << node->column() << ":"
					  << Colors::reset() << " ";
		} else if (mFilename) {
			std::cerr << Colors::bold() << mFilename << ":" << Colors::reset() << " ";
		}
		std::cerr << Colors::bold() << Colors::magenta() << "warning:" << Colors::reset() << " ";
		std::cerr << Colors::bold() << message << Colors::reset() << std::endl;
		mWarningCount++;
	}

	size_t SemanticValidator::validate(IAstNode* program, const char* filename, bool isModuleFile, bool werror) {
		mErrorCount = 0;
		mWarningCount = 0;
		mWerror = werror;
		mFilename = filename;
		mIsModuleFile = isModuleFile;
		mDefinedFunctions.clear();
		mFunctionSignatures.clear();
		mImportedModules.clear();
		mImportedLibraries.clear();
		mImportedLibraryFunctions.clear();
		mLoadedModuleFiles.clear();
		mModuleFunctions.clear();

		// Extract source directory and package name from filename
		if (filename) {
			std::string fullPath(filename);
			size_t lastSlash = fullPath.find_last_of('/');
			if (lastSlash != std::string::npos) {
				mSourceDirectory = fullPath.substr(0, lastSlash);

				// Extract package name from directory structure
				// If file is math_utils/module.qd, package is "math_utils"
				// If file is just main.qd, package is "main"
				size_t secondLastSlash = mSourceDirectory.find_last_of('/');
				if (secondLastSlash != std::string::npos) {
					mCurrentPackage = mSourceDirectory.substr(secondLastSlash + 1);
				} else {
					// File is in current directory, use "main" as default
					mCurrentPackage = "main";
				}
			} else {
				mSourceDirectory = ".";
				mCurrentPackage = "main";
			}
		} else {
			mSourceDirectory = ".";
			mCurrentPackage = "main";
		}

		// Pass 1: Collect all function definitions
		collectDefinitions(program);

		// Pass 2: Validate all references
		validateReferences(program);

		// Pass 3a: Analyze function signatures (stack effects)
		// Use iterative analysis until signatures converge (fixed point)
		static const int MAX_SIGNATURE_ANALYSIS_ITERATIONS = 100; // Prevent infinite loops in signature inference
		bool signaturesChanged = true;
		int iteration = 0;

		while (signaturesChanged && iteration < MAX_SIGNATURE_ANALYSIS_ITERATIONS) {
			signaturesChanged = false;

			// Store old signatures to detect changes
			auto oldSignatures = mFunctionSignatures;

			// Re-analyze all functions with current signatures
			analyzeFunctionSignatures(program);

			// Check if any signatures changed
			for (const auto& pair : mFunctionSignatures) {
				auto oldIt = oldSignatures.find(pair.first);
				if (oldIt == oldSignatures.end() || oldIt->second.produces.size() != pair.second.produces.size()) {
					signaturesChanged = true;
					break;
				}
				// Check if types differ
				for (size_t i = 0; i < pair.second.produces.size(); i++) {
					if (oldIt->second.produces[i] != pair.second.produces[i]) {
						signaturesChanged = true;
						break;
					}
				}
				if (signaturesChanged) {
					break;
				}
			}

			iteration++;
		}

		// Warn if we didn't converge
		if (iteration >= MAX_SIGNATURE_ANALYSIS_ITERATIONS) {
			std::cerr << Colors::bold() << Colors::magenta() << "Warning: " << Colors::reset()
					  << "Function signature analysis did not converge after " << MAX_SIGNATURE_ANALYSIS_ITERATIONS
					  << " iterations. Type checking may be incomplete." << std::endl;
		}

		// Pass 3b: Type check using function signatures
		typeCheckFunction(program);

		return mErrorCount;
	}

	void SemanticValidator::collectDefinitions(IAstNode* node) {
		if (!node) {
			return;
		}

		// If this is a function declaration, add it to the symbol table
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			mDefinedFunctions.insert(func->name());
		}

		// If this is a use statement, add the module to imported modules and load its definitions
		if (node->type() == IAstNode::Type::USE_STATEMENT) {
			AstNodeUse* use = static_cast<AstNodeUse*>(node);
			std::string moduleName = use->module();
			mImportedModules.insert(moduleName);

			// For top-level file imports (not intra-module imports), also register the derived namespace
			// This allows "use "calculator.qd"" to work with "calculator::function"
			// But for intra-module imports like "use helper.qd" inside a module, we want functions
			// to remain in the parent module's namespace, not create a new "helper" namespace
			if (!mIsModuleFile) {
				std::string packageName = getPackageFromModuleName(moduleName);
				if (packageName != moduleName) {
					mImportedModules.insert(packageName);
				}
			}

			// Only report errors for missing modules if this is the main entry point (not a module file)
			loadModuleDefinitions(moduleName, mCurrentPackage, !mIsModuleFile);
		}

		// If this is an import statement, collect imported library functions
		if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			AstNodeImport* import = static_cast<AstNodeImport*>(node);
			mImportedLibraries[import->namespaceName()] = import->library();
			// Register all imported functions as namespace::function
			for (const auto* func : import->functions()) {
				std::string qualifiedName = import->namespaceName() + "::" + func->name;
				mImportedLibraryFunctions.insert(qualifiedName);
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectDefinitions(node->child(i));
		}
	}

	void SemanticValidator::loadModuleDefinitions(
			const std::string& moduleName, const std::string& currentPackage, bool reportErrors) {
		// Check if we've already loaded this specific file (prevent duplicate loads)
		if (mLoadedModuleFiles.count(moduleName)) {
			return;
		}
		mLoadedModuleFiles.insert(moduleName);

		// Check if this is a direct file import (ends with .qd)
		bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

		std::string effectiveModuleName;
		if (isDirectFile) {
			// For .qd file imports from top-level files, derive package name from filename
			// For intra-module imports (when currentPackage != mCurrentPackage), use the current package name
			if (!mIsModuleFile && currentPackage == mCurrentPackage) {
				effectiveModuleName = getPackageFromModuleName(moduleName);
			} else {
				effectiveModuleName = currentPackage;
			}
		} else {
			effectiveModuleName = moduleName;
		}

		std::string modulePath;
		std::ifstream file;

		if (isDirectFile) {
			// Direct file import: look for file.qd
			// Expand tilde (~) in the path if present
			std::string expandedModuleName = expandTilde(moduleName);

			// Check if it's an absolute path (starts with / or was expanded from ~)
			if (!expandedModuleName.empty() && expandedModuleName[0] == '/') {
				// Absolute path - use directly
				file.open(expandedModuleName);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					// Parse and add to effective module namespace
					parseModuleAndCollectFunctions(effectiveModuleName, source);
					return;
				}
				file.close();
				// Absolute path doesn't exist - will fail below
			} else {
				// Relative path - search in parent module's directory
				// First check if we have a directory for the current package (parent module)
				std::string searchDir = mSourceDirectory;
				if (mModuleDirectories.count(currentPackage)) {
					searchDir = mModuleDirectories[currentPackage];
				}

				// Try 1: Same directory as parent module
				modulePath = searchDir + "/" + moduleName;
				file.open(modulePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					// Parse and add to effective module namespace
					parseModuleAndCollectFunctions(effectiveModuleName, source);
					return;
				}
				file.close();
			}

			// If not found in same directory, try standard paths
			// Try 2: Third-party packages directory (installed via quadpm)
			// Get packages directory
			std::string packagesDir;
			const char* quadrateCache = std::getenv("QUADRATE_CACHE");
			if (quadrateCache) {
				packagesDir = quadrateCache;
			} else {
				const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
				if (xdgDataHome) {
					packagesDir = std::string(xdgDataHome) + "/quadrate/packages";
				} else {
					const char* pkgHome = std::getenv("HOME");
					if (pkgHome) {
						packagesDir = std::string(pkgHome) + "/quadrate/packages";
					}
				}
			}

			if (!packagesDir.empty() && std::filesystem::exists(packagesDir)) {
				// Look for directories matching moduleName@*
				try {
					for (const auto& entry : std::filesystem::directory_iterator(packagesDir)) {
						if (!entry.is_directory()) {
							continue;
						}
						std::string dirName = entry.path().filename().string();
						std::string prefix = moduleName + "@";
						if (dirName.size() > prefix.size() && dirName.substr(0, prefix.size()) == prefix) {
							// Found a matching package
							modulePath = entry.path().string() + "/module.qd";
							file.open(modulePath);
							if (file.good()) {
								mModuleDirectories[moduleName] = entry.path().string();
								std::stringstream buffer;
								buffer << file.rdbuf();
								std::string source = buffer.str();
								file.close();
								parseModuleAndCollectFunctions(effectiveModuleName, source);
								return;
							}
							file.close();
							break; // Only try first matching version
						}
					}
				} catch (...) {
					// Ignore errors iterating directory
				}
			}

			// Try 3: QUADRATE_ROOT
			const char* quadrateRoot = std::getenv("QUADRATE_ROOT");
			if (quadrateRoot) {
				modulePath = std::string(quadrateRoot) + "/" + moduleName;
				file.open(modulePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					parseModuleAndCollectFunctions(effectiveModuleName, source);
					return;
				}
				file.close();
			}

			// Try 3: $HOME/quadrate
			const char* home = std::getenv("HOME");
			if (home) {
				modulePath = std::string(home) + "/quadrate/" + moduleName;
				file.open(modulePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					parseModuleAndCollectFunctions(effectiveModuleName, source);
					return;
				}
				file.close();
			}
		} else {
			// Module directory import: use moduleName (looks for moduleName/module.qd)
			// Try 1: Local path (relative to source file)
			modulePath = mSourceDirectory + "/" + moduleName + "/module.qd";
			file.open(modulePath);
			if (file.good()) {
				// Found it locally - store the module directory
				mModuleDirectories[moduleName] = mSourceDirectory + "/" + moduleName;
				std::stringstream buffer;
				buffer << file.rdbuf();
				std::string source = buffer.str();
				file.close();
				parseModuleAndCollectFunctions(moduleName, source);
				return;
			}
			file.close();

			// Try 2: Third-party packages directory (installed via quadpm)
			std::string packagesDir;
			const char* quadrateCache = std::getenv("QUADRATE_CACHE");
			if (quadrateCache) {
				packagesDir = quadrateCache;
			} else {
				const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
				if (xdgDataHome) {
					packagesDir = std::string(xdgDataHome) + "/quadrate/packages";
				} else {
					const char* pkgHome = std::getenv("HOME");
					if (pkgHome) {
						packagesDir = std::string(pkgHome) + "/quadrate/packages";
					}
				}
			}

			if (!packagesDir.empty() && std::filesystem::exists(packagesDir)) {
				// Look for directories matching moduleName@*
				try {
					for (const auto& entry : std::filesystem::directory_iterator(packagesDir)) {
						if (!entry.is_directory()) {
							continue;
						}
						std::string dirName = entry.path().filename().string();
						std::string prefix = moduleName + "@";
						if (dirName.size() > prefix.size() && dirName.substr(0, prefix.size()) == prefix) {
							// Found a matching package
							modulePath = entry.path().string() + "/module.qd";
							file.open(modulePath);
							if (file.good()) {
								mModuleDirectories[moduleName] = entry.path().string();
								std::stringstream buffer;
								buffer << file.rdbuf();
								std::string source = buffer.str();
								file.close();
								parseModuleAndCollectFunctions(moduleName, source);
								return;
							}
							file.close();
							break; // Only try first matching version
						}
					}
				} catch (...) {
					// Ignore errors iterating directory
				}
			}

			// Try 3: QUADRATE_ROOT environment variable
			const char* quadrateRoot = std::getenv("QUADRATE_ROOT");
			if (quadrateRoot) {
				modulePath = std::string(quadrateRoot) + "/" + moduleName + "/module.qd";
				file.open(modulePath);
				if (file.good()) {
					// Store the module directory
					mModuleDirectories[moduleName] = std::string(quadrateRoot) + "/" + moduleName;
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					parseModuleAndCollectFunctions(moduleName, source);
					return;
				}
				file.close();
			}

			// Try 4: Standard library directories relative to current directory (for development)
			std::string stdLibPath = "lib/std" + moduleName + "qd/qd/" + moduleName + "/module.qd";
			file.open(stdLibPath);
			if (file.good()) {
				// Store the module directory
				mModuleDirectories[moduleName] = "lib/std" + moduleName + "qd/qd/" + moduleName;
				std::stringstream buffer;
				buffer << file.rdbuf();
				std::string source = buffer.str();
				file.close();
				parseModuleAndCollectFunctions(moduleName, source);
				return;
			}
			file.close();

			// Try 5: Standard library relative to executable (for installed binaries)
			// Get executable path and look for ../share/quadrate/<module>/module.qd
			try {
				std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe");
				std::filesystem::path exeDir = exePath.parent_path();
				std::filesystem::path sharePath = exeDir / ".." / "share" / "quadrate" / moduleName / "module.qd";
				if (std::filesystem::exists(sharePath)) {
					modulePath = sharePath.string();
					file.open(modulePath);
					if (file.good()) {
						mModuleDirectories[moduleName] = sharePath.parent_path().string();
						std::stringstream buffer;
						buffer << file.rdbuf();
						std::string source = buffer.str();
						file.close();
						parseModuleAndCollectFunctions(moduleName, source);
						return;
					}
					file.close();
				}
			} catch (...) {
				// Ignore errors reading executable path
			}

			// Try 6: $HOME/quadrate directory
			const char* home = std::getenv("HOME");
			if (home) {
				modulePath = std::string(home) + "/quadrate/" + moduleName + "/module.qd";
				file.open(modulePath);
				if (file.good()) {
					// Store the module directory
					mModuleDirectories[moduleName] = std::string(home) + "/quadrate/" + moduleName;
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					parseModuleAndCollectFunctions(moduleName, source);
					return;
				}
				file.close();
			}
		}

		// Module file doesn't exist anywhere
		// Only report error if reportErrors is true (top-level import from user code)
		// Nested imports from other modules silently skip (might be found via different paths)
		if (reportErrors) {
			if (isDirectFile) {
				std::string errorMsg = "File '";
				errorMsg += moduleName;
				errorMsg += "': No such file or directory";
				reportError(errorMsg.c_str());
			} else {
				// Check if a directory with the module name exists but doesn't contain module.qd
				bool foundDirectoryWithoutModuleFile = false;
				std::string directoryPath;

				// Check local path
				std::string localDir = mSourceDirectory + "/" + moduleName;
				if (std::filesystem::exists(localDir) && std::filesystem::is_directory(localDir)) {
					foundDirectoryWithoutModuleFile = true;
					directoryPath = localDir;
				}

				// Check QUADRATE_ROOT
				if (!foundDirectoryWithoutModuleFile) {
					const char* quadrateRoot = std::getenv("QUADRATE_ROOT");
					if (quadrateRoot) {
						std::string rootDir = std::string(quadrateRoot) + "/" + moduleName;
						if (std::filesystem::exists(rootDir) && std::filesystem::is_directory(rootDir)) {
							foundDirectoryWithoutModuleFile = true;
							directoryPath = rootDir;
						}
					}
				}

				// Check $HOME/quadrate
				if (!foundDirectoryWithoutModuleFile) {
					const char* home = std::getenv("HOME");
					if (home) {
						std::string homeDir = std::string(home) + "/quadrate/" + moduleName;
						if (std::filesystem::exists(homeDir) && std::filesystem::is_directory(homeDir)) {
							foundDirectoryWithoutModuleFile = true;
							directoryPath = homeDir;
						}
					}
				}

				std::string errorMsg;
				if (foundDirectoryWithoutModuleFile) {
					errorMsg = "Module '";
					errorMsg += moduleName;
					errorMsg += "' directory found at '";
					errorMsg += directoryPath;
					errorMsg += "', but it does not contain a 'module.qd' file.\n";
					errorMsg += "Module directories must have a 'module.qd' file as the entry point.\n";
					errorMsg += "Either create '";
					errorMsg += directoryPath;
					errorMsg += "/module.qd' or use a direct file import like: use \"";
					errorMsg += moduleName;
					errorMsg += "/filename.qd\"";
				} else {
					errorMsg = "Module '";
					errorMsg += moduleName;
					errorMsg += "' not found in any search path";
				}
				reportError(errorMsg.c_str());
			}
		}
	}

	void SemanticValidator::parseModuleAndCollectFunctions(const std::string& moduleName, const std::string& source) {
		// Parse the module file to AST
		Ast moduleAst;
		IAstNode* moduleAstRoot = moduleAst.generate(source.c_str(), false, nullptr);
		if (!moduleAstRoot) {
			// Failed to parse - skip silently
			return;
		}

		// Process USE statements in the module first (to load .qd file imports)
		// We need to recursively collect definitions, including from imported .qd files
		for (size_t i = 0; i < moduleAstRoot->childCount(); i++) {
			IAstNode* child = moduleAstRoot->child(i);
			if (child && child->type() == IAstNode::Type::USE_STATEMENT) {
				AstNodeUse* use = static_cast<AstNodeUse*>(child);
				// Recursively load this module/file with the current module as context
				// Pass false for reportErrors since this is a nested import
				loadModuleDefinitions(use->module(), moduleName, false);
			}
		}

		// Collect function definitions from the module
		std::unordered_set<std::string> moduleFunctions;
		collectModuleFunctions(moduleAstRoot, moduleFunctions);

		// Store the collected functions
		// If this module already has functions (from .qd file imports), merge with existing set
		if (mModuleFunctions.find(moduleName) != mModuleFunctions.end()) {
			// Merge: add new functions to existing set
			mModuleFunctions[moduleName].insert(moduleFunctions.begin(), moduleFunctions.end());
		} else {
			// Create new entry
			mModuleFunctions[moduleName] = moduleFunctions;
		}

		// Collect constant definitions from the module
		std::unordered_set<std::string> moduleConstants;
		collectModuleConstants(moduleAstRoot, moduleConstants);

		// Store the collected constants
		if (mModuleConstants.find(moduleName) != mModuleConstants.end()) {
			// Merge: add new constants to existing set
			mModuleConstants[moduleName].insert(moduleConstants.begin(), moduleConstants.end());
		} else {
			// Create new entry
			mModuleConstants[moduleName] = moduleConstants;
		}

		// Also collect constant values
		collectModuleConstantValues(moduleAstRoot, moduleName);

		// Analyze function signatures for module functions
		// We use a simplified analysis since we don't need iterative convergence for modules
		analyzeModuleFunctionSignatures(moduleAstRoot, moduleName);
	}

	void SemanticValidator::collectModuleFunctions(IAstNode* node, std::unordered_set<std::string>& functions) {
		if (!node) {
			return;
		}

		// If this is a function declaration, add it to the set
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			functions.insert(func->name());
		}
		// If this is an import statement, add imported functions to the set
		else if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			AstNodeImport* import = static_cast<AstNodeImport*>(node);
			const auto& importedFuncs = import->functions();
			for (const auto* func : importedFuncs) {
				functions.insert(func->name);
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleFunctions(node->child(i), functions);
		}
	}

	void SemanticValidator::collectModuleConstants(IAstNode* node, std::unordered_set<std::string>& constants) {
		if (!node) {
			return;
		}

		// If this is a constant declaration, add it to the set
		if (node->type() == IAstNode::Type::CONSTANT_DECLARATION) {
			AstNodeConstant* constNode = static_cast<AstNodeConstant*>(node);
			constants.insert(constNode->name());
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleConstants(node->child(i), constants);
		}
	}

	void SemanticValidator::collectModuleConstantValues(IAstNode* node, const std::string& moduleName) {
		if (!node) {
			return;
		}

		// If this is a constant declaration, store its value
		if (node->type() == IAstNode::Type::CONSTANT_DECLARATION) {
			AstNodeConstant* constNode = static_cast<AstNodeConstant*>(node);
			std::string qualifiedName = moduleName + "::" + constNode->name();
			mModuleConstantValues[qualifiedName] = constNode->value();
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleConstantValues(node->child(i), moduleName);
		}
	}

	void SemanticValidator::analyzeModuleFunctionSignatures(IAstNode* node, const std::string& moduleName) {
		if (!node) {
			return;
		}

		// Analyze each function definition in the module to determine its stack effect
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			std::vector<StackValueType> typeStack;

			// Initialize type stack with input parameters
			// Input parameters are on the stack when the function starts
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i]);
				const std::string& typeStr = param->typeString();

				if (typeStr == "i") {
					typeStack.push_back(StackValueType::INT);
				} else if (typeStr == "f") {
					typeStack.push_back(StackValueType::FLOAT);
				} else if (typeStr == "s") {
					typeStack.push_back(StackValueType::STRING);
				} else {
					// Untyped or unknown - treat as ANY
					typeStack.push_back(StackValueType::ANY);
				}
			}

			// Analyze the function body in isolation
			if (func->body()) {
				analyzeBlockInIsolation(func->body(), typeStack);
			}

			// Store the signature with qualified name: moduleName::functionName
			FunctionSignature sig;

			// Build consumes list from input parameters
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i]);
				const std::string& typeStr = param->typeString();

				if (typeStr == "i") {
					sig.consumes.push_back(StackValueType::INT);
				} else if (typeStr == "f") {
					sig.consumes.push_back(StackValueType::FLOAT);
				} else if (typeStr == "s") {
					sig.consumes.push_back(StackValueType::STRING);
				} else if (typeStr == "p") {
					sig.consumes.push_back(StackValueType::PTR);
				} else {
					// Untyped or unknown - use ANY
					sig.consumes.push_back(StackValueType::ANY);
				}
			}

			sig.produces = typeStack;
			sig.throws = func->throws();
			std::string qualifiedName = moduleName + "::" + func->name();
			mFunctionSignatures[qualifiedName] = sig;
		}
		// Analyze imported functions and register them with the module's namespace
		else if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			AstNodeImport* import = static_cast<AstNodeImport*>(node);
			const auto& importedFuncs = import->functions();

			for (const auto* func : importedFuncs) {
				FunctionSignature sig;

				// Build consumes list from input parameters
				for (size_t i = 0; i < func->inputParameters.size(); i++) {
					AstNodeParameter* param = func->inputParameters[i];
					const std::string& typeStr = param->typeString();

					if (typeStr == "i") {
						sig.consumes.push_back(StackValueType::INT);
					} else if (typeStr == "f") {
						sig.consumes.push_back(StackValueType::FLOAT);
					} else if (typeStr == "s") {
						sig.consumes.push_back(StackValueType::STRING);
					} else if (typeStr == "p") {
						sig.consumes.push_back(StackValueType::PTR);
					} else {
						// Untyped or unknown - treat as ANY
						sig.consumes.push_back(StackValueType::ANY);
					}
				}

				// Build produces list from output parameters
				for (size_t i = 0; i < func->outputParameters.size(); i++) {
					AstNodeParameter* param = func->outputParameters[i];
					const std::string& typeStr = param->typeString();

					if (typeStr == "i") {
						sig.produces.push_back(StackValueType::INT);
					} else if (typeStr == "f") {
						sig.produces.push_back(StackValueType::FLOAT);
					} else if (typeStr == "s") {
						sig.produces.push_back(StackValueType::STRING);
					} else if (typeStr == "p") {
						sig.produces.push_back(StackValueType::PTR);
					} else {
						// Untyped or unknown - treat as ANY
						sig.produces.push_back(StackValueType::ANY);
					}
				}

				// Store the signature with qualified name: moduleName::functionName
				// This allows imported functions to be called with the module's namespace
				sig.throws = func->throws;
				std::string qualifiedName = moduleName + "::" + func->name;
				mFunctionSignatures[qualifiedName] = sig;
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			analyzeModuleFunctionSignatures(node->child(i), moduleName);
		}
	}

	void SemanticValidator::validateReferences(IAstNode* node, bool insideForLoop) {
		std::unordered_set<std::string> localVariables;
		validateReferencesInternal(node, insideForLoop, localVariables);
	}

	void SemanticValidator::validateReferencesInternal(
			IAstNode* node, bool insideForLoop, std::unordered_set<std::string>& localVariables) {
		if (!node) {
			return;
		}

		// Check if this is a local variable declaration
		if (node->type() == IAstNode::Type::LOCAL) {
			AstNodeLocal* local = static_cast<AstNodeLocal*>(node);
			localVariables.insert(local->name());
			return;
		}

		// Check if this is a break statement
		if (node->type() == IAstNode::Type::BREAK_STATEMENT) {
			if (!insideForLoop) {
				reportError(node, "break statement not within loop or switch");
			}
			return;
		}

		// Check if this is a continue statement
		if (node->type() == IAstNode::Type::CONTINUE_STATEMENT) {
			if (!insideForLoop) {
				reportError(node, "continue statement not within a loop");
			}
			return;
		}

		// Check if this is a switch statement
		if (node->type() == IAstNode::Type::SWITCH_STATEMENT) {
			AstNodeSwitchStatement* switchStmt = static_cast<AstNodeSwitchStatement*>(node);

			// Validate: switch must have at least one case
			if (switchStmt->cases().empty()) {
				reportError(switchStmt, "Switch statement must have at least one case");
			}

			// Validate: no duplicate case values
			std::unordered_set<std::string> seenValues;
			for (const auto* caseNode : switchStmt->cases()) {
				if (!caseNode->isDefault() && caseNode->value()) {
					// Get the case value as a string for comparison
					// We need to serialize the AST node to compare values
					std::string valueStr = serializeCaseValue(caseNode->value());

					if (seenValues.find(valueStr) != seenValues.end()) {
						std::string errorMsg = "Duplicate case value '";
						errorMsg += valueStr;
						errorMsg += "' in switch statement";
						reportError(caseNode, errorMsg.c_str());
					}
					seenValues.insert(valueStr);
				}
			}
		}

		// Check if this is an identifier (function call or local variable reference)
		if (node->type() == IAstNode::Type::IDENTIFIER) {
			AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
			const char* name = ident->name().c_str();

			// Check if it's $ (for loop iterator variable)
			if (strcmp(name, "$") == 0) {
				if (!insideForLoop) {
					reportError(ident, "Iterator variable '$' can only be used inside a for loop");
				}
				return;
			}

			// Check if it's a local variable
			if (localVariables.find(name) != localVariables.end()) {
				// Valid local variable reference
				return;
			}

			// Check if it's a built-in instruction
			if (isBuiltInInstruction(name)) {
				// Valid built-in, no error
				return;
			}

			// Check if it's a defined function
			if (mDefinedFunctions.find(name) == mDefinedFunctions.end()) {
				// Not found - report error
				std::string errorMsg = "Undefined function '";
				errorMsg += name;
				errorMsg += "'";
				reportError(ident, errorMsg.c_str());
			}
		}

		// Check if this is a function pointer reference
		if (node->type() == IAstNode::Type::FUNCTION_POINTER_REFERENCE) {
			AstNodeFunctionPointerReference* funcPtr = static_cast<AstNodeFunctionPointerReference*>(node);
			const char* name = funcPtr->functionName().c_str();

			// Check if the referenced function is defined
			if (mDefinedFunctions.find(name) == mDefinedFunctions.end()) {
				// Not found - report error
				std::string errorMsg = "Undefined function '";
				errorMsg += name;
				errorMsg += "' in function pointer reference";
				reportError(funcPtr, errorMsg.c_str());
			}
		}

		// Check if this is a scoped identifier (module function call like math::sqrt or std::printf)
		if (node->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
			AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(node);
			const std::string& scopeName = scoped->scope();
			const std::string& functionName = scoped->name();
			std::string qualifiedName = scopeName + "::" + functionName;

			// Check if this is an imported library function (e.g., std::printf)
			if (mImportedLibraryFunctions.find(qualifiedName) != mImportedLibraryFunctions.end()) {
				// Valid imported library function
				return;
			}

			// Check if this is an imported library namespace (even if function not declared)
			if (mImportedLibraries.find(scopeName) != mImportedLibraries.end()) {
				// It's a library namespace, but function wasn't declared in import
				std::string errorMsg = "Function '";
				errorMsg += functionName;
				errorMsg += "' not declared in library import '";
				errorMsg += scopeName;
				errorMsg += "'";
				reportError(scoped, errorMsg.c_str());
				return;
			}

			// Check if the module was imported
			if (mImportedModules.find(scopeName) == mImportedModules.end()) {
				std::string errorMsg = "Module '";
				errorMsg += scopeName;
				errorMsg += "' not imported. Add 'use ";
				errorMsg += scopeName;
				errorMsg += "' to use this module";
				reportError(scoped, errorMsg.c_str());
				return;
			}

			// Check if this is a constant (constants take precedence over functions)
			auto constIt = mModuleConstants.find(scopeName);
			if (constIt != mModuleConstants.end()) {
				const auto& constants = constIt->second;
				if (constants.find(functionName) != constants.end()) {
					// This is a valid constant - no further checking needed
					return;
				}
			}

			// Check if the function exists in the module
			auto moduleIt = mModuleFunctions.find(scopeName);
			if (moduleIt != mModuleFunctions.end()) {
				const auto& functions = moduleIt->second;
				if (functions.find(functionName) == functions.end()) {
					std::string errorMsg = "Function or constant '";
					errorMsg += functionName;
					errorMsg += "' not found in module '";
					errorMsg += scopeName;
					errorMsg += "'";
					reportError(scoped, errorMsg.c_str());
				}
			}
			// If module not in mModuleFunctions, it means loadModuleDefinitions failed
			// but we don't report an error here as it was likely already reported
		}

		// Track when we enter a for loop, infinite loop, or switch statement
		bool childrenInsideForLoop = insideForLoop;
		if (node->type() == IAstNode::Type::FOR_STATEMENT || node->type() == IAstNode::Type::LOOP_STATEMENT ||
				node->type() == IAstNode::Type::SWITCH_STATEMENT) {
			childrenInsideForLoop = true;
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			validateReferencesInternal(node->child(i), childrenInsideForLoop, localVariables);
		}
	}

	void SemanticValidator::analyzeFunctionSignatures(IAstNode* node) {
		if (!node) {
			return;
		}

		// Analyze each function definition to determine its stack effect
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			std::vector<StackValueType> typeStack;

			// Initialize type stack with input parameters (they're on the stack when function starts)
			for (auto* paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				std::string typeStr = param->typeString();
				if (typeStr == "i") {
					typeStack.push_back(StackValueType::INT);
				} else if (typeStr == "f") {
					typeStack.push_back(StackValueType::FLOAT);
				} else if (typeStr == "s") {
					typeStack.push_back(StackValueType::STRING);
				} else if (typeStr == "p") {
					typeStack.push_back(StackValueType::PTR);
				} else {
					// Untyped or unknown - use ANY
					typeStack.push_back(StackValueType::ANY);
				}
			}

			// Analyze the function body in isolation (without resolving function calls)
			if (func->body()) {
				analyzeBlockInIsolation(func->body(), typeStack);
			}

			// Store the signature with input parameters as consumes
			FunctionSignature sig;

			// Build consumes list from input parameters
			for (auto* paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				std::string typeStr = param->typeString();
				if (typeStr == "i") {
					sig.consumes.push_back(StackValueType::INT);
				} else if (typeStr == "f") {
					sig.consumes.push_back(StackValueType::FLOAT);
				} else if (typeStr == "s") {
					sig.consumes.push_back(StackValueType::STRING);
				} else if (typeStr == "p") {
					sig.consumes.push_back(StackValueType::PTR);
				} else {
					// Untyped or unknown - use ANY
					sig.consumes.push_back(StackValueType::ANY);
				}
			}

			sig.produces = typeStack;
			sig.throws = func->throws();
			mFunctionSignatures[func->name()] = sig;
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			analyzeFunctionSignatures(node->child(i));
		}
	}

	void SemanticValidator::analyzeBlockInIsolation(IAstNode* node, std::vector<StackValueType>& typeStack) {
		if (!node) {
			return;
		}

		// Process each child in the block
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}

			switch (child->type()) {
			case IAstNode::Type::LITERAL: {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(child);
				switch (lit->literalType()) {
				case AstNodeLiteral::LiteralType::INTEGER:
					typeStack.push_back(StackValueType::INT);
					break;
				case AstNodeLiteral::LiteralType::FLOAT:
					typeStack.push_back(StackValueType::FLOAT);
					break;
				case AstNodeLiteral::LiteralType::STRING:
					typeStack.push_back(StackValueType::STRING);
					break;
				}
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				// During signature analysis, don't report errors - just simulate the stack
				typeCheckInstructionInternal(child, instr->name().c_str(), typeStack, false);
				break;
			}

			case IAstNode::Type::BLOCK: {
				// Recursively analyze nested blocks
				analyzeBlockInIsolation(child, typeStack);
				break;
			}

			case IAstNode::Type::IDENTIFIER: {
				// Apply function signature if known (for iterative analysis)
				AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(child);
				const std::string& name = ident->name();

				auto sigIt = mFunctionSignatures.find(name);
				if (sigIt != mFunctionSignatures.end()) {
					// Apply the known signature
					const FunctionSignature& sig = sigIt->second;
					for (const auto& type : sig.produces) {
						typeStack.push_back(type);
					}
				}
				// If signature not known yet, skip (will be resolved in next iteration)
				break;
			}

			case IAstNode::Type::SCOPED_IDENTIFIER: {
				// Apply module function signature if known
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);
				const std::string& moduleName = scoped->scope();
				const std::string& functionName = scoped->name();
				std::string qualifiedName = moduleName + "::" + functionName;

				auto sigIt = mFunctionSignatures.find(qualifiedName);
				if (sigIt != mFunctionSignatures.end()) {
					// Apply the known signature
					const FunctionSignature& sig = sigIt->second;
					for (const auto& type : sig.produces) {
						typeStack.push_back(type);
					}
				}
				// If signature not known yet, skip (will be resolved in next iteration)
				break;
			}

			case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
				// Function pointer references push a pointer type onto the stack
				typeStack.push_back(StackValueType::PTR);
				break;

			default:
				// Other node types don't affect the type stack during signature analysis
				break;
			}
		}
	}

	void SemanticValidator::typeCheckFunction(IAstNode* node) {
		if (!node) {
			return;
		}

		// Type check each function definition
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			std::vector<StackValueType> typeStack;
			std::unordered_map<std::string, StackValueType> localVariables;

			// Initialize type stack with input parameters
			// Input parameters are on the stack when the function starts
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i]);
				const std::string& typeStr = param->typeString();

				if (typeStr == "i") {
					typeStack.push_back(StackValueType::INT);
				} else if (typeStr == "f") {
					typeStack.push_back(StackValueType::FLOAT);
				} else if (typeStr == "s") {
					typeStack.push_back(StackValueType::STRING);
				} else {
					// Untyped or unknown - treat as ANY
					typeStack.push_back(StackValueType::ANY);
				}
			}

			// Type check the function body
			if (func->body()) {
				typeCheckBlock(func->body(), typeStack, localVariables);
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			typeCheckFunction(node->child(i));
		}
	}

	void SemanticValidator::typeCheckBlock(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables) {
		if (!node) {
			return;
		}

		// Process each child in the block
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}

			switch (child->type()) {
			case IAstNode::Type::LITERAL: {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(child);
				switch (lit->literalType()) {
				case AstNodeLiteral::LiteralType::INTEGER:
					typeStack.push_back(StackValueType::INT);
					break;
				case AstNodeLiteral::LiteralType::FLOAT:
					typeStack.push_back(StackValueType::FLOAT);
					break;
				case AstNodeLiteral::LiteralType::STRING:
					typeStack.push_back(StackValueType::STRING);
					break;
				}
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				typeCheckInstruction(child, instr->name().c_str(), typeStack);
				break;
			}

			case IAstNode::Type::BLOCK: {
				// Recursively check nested blocks
				typeCheckBlock(child, typeStack, localVariables);
				break;
			}

			case IAstNode::Type::IF_STATEMENT: {
				// For now, skip control flow type checking
				// (more complex - would need to merge type states from branches)
				break;
			}

			case IAstNode::Type::FOR_STATEMENT:
			case IAstNode::Type::LOOP_STATEMENT: {
				// For now, skip loop type checking
				break;
			}

			case IAstNode::Type::CTX_STATEMENT: {
				// For now, skip ctx type checking since it's complex
				// (would need full stack effect analysis including clear, drop, etc.)
				// The runtime enforces the single-value constraint anyway
				// Just push a generic type to the parent stack
				typeStack.push_back(StackValueType::INT);
				break;
			}

			case IAstNode::Type::LOCAL: {
				// Handle local variable declaration: pop value from stack and store
				AstNodeLocal* local = static_cast<AstNodeLocal*>(child);
				const std::string& varName = local->name();

				// Check if stack is empty
				if (typeStack.empty()) {
					std::string errorMsg = "Type error in local variable '";
					errorMsg += varName;
					errorMsg += "': Stack underflow (no value to store)";
					reportError(local, errorMsg.c_str());
					break;
				}

				// Pop the value type from the stack and store it as the variable's type
				StackValueType varType = typeStack.back();
				typeStack.pop_back();
				localVariables[varName] = varType;
				break;
			}

			case IAstNode::Type::IDENTIFIER: {
				// Handle function calls - apply their stack effect
				AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(child);
				const std::string& name = ident->name();

				// Check if it's a local variable reference
				auto localIt = localVariables.find(name);
				if (localIt != localVariables.end()) {
					// Push the local variable's type onto the stack
					typeStack.push_back(localIt->second);
					break;
				}

				// Check if this is a user-defined function
				auto sigIt = mFunctionSignatures.find(name);
				if (sigIt != mFunctionSignatures.end()) {
					const FunctionSignature& sig = sigIt->second;

					// Validate '!' and '?' usage: only allowed on fallible functions (marked with '!')
					if (ident->abortOnError() && !sig.throws) {
						std::string errorMsg = "Cannot use '!' operator on function '" + name +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(ident, errorMsg.c_str());
					}
					if (ident->checkError() && !sig.throws) {
						std::string errorMsg = "Cannot use '?' operator on function '" + name +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(ident, errorMsg.c_str());
					}

					// Check fallible functions without ! or ? must be followed by 'if'
					if (sig.throws && !ident->abortOnError() && !ident->checkError()) {
						IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
						if (!nextNode || nextNode->type() != IAstNode::Type::IF_STATEMENT) {
							std::string errorMsg = "Fallible function '" + name +
												   "' must be immediately followed by 'if' to check for errors, or use "
												   "'!' to abort on error";
							reportError(ident, errorMsg.c_str());
						}
					}

					// Check if stack has enough values for function parameters
					if (typeStack.size() < sig.consumes.size()) {
						std::string errorMsg = "Type error in function call '";
						errorMsg += name;
						errorMsg += "': Stack underflow (requires ";
						errorMsg += std::to_string(sig.consumes.size());
						errorMsg += " values, have ";
						errorMsg += std::to_string(typeStack.size());
						errorMsg += ")";
						reportError(ident, errorMsg.c_str());
						break;
					}

					// Track which parameters need casts
					std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);

					// Check if the types match
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
						StackValueType expected = sig.consumes[j];
						StackValueType actual = typeStack[stackIdx];

						// Skip check if expected type is ANY or UNKNOWN
						if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN) {
							continue;
						}

						// Skip check if actual type is UNKNOWN (can't determine type)
						if (actual == StackValueType::UNKNOWN) {
							continue;
						}

						// Check for type mismatch
						if (actual != expected) {
							// Check if implicit cast is allowed (int <-> float)
							if (isImplicitCastAllowed(actual, expected)) {
								// Warn about implicit cast
								std::string warnMsg = "Implicit cast in function call '";
								warnMsg += name;
								warnMsg += "': Parameter ";
								warnMsg += std::to_string(j + 1);
								warnMsg += " expects ";
								warnMsg += stackValueTypeToString(expected);
								warnMsg += ", but got ";
								warnMsg += stackValueTypeToString(actual);
								reportWarning(ident, warnMsg.c_str());

								// Record the cast direction
								if (actual == StackValueType::INT && expected == StackValueType::FLOAT) {
									paramCasts[j] = CastDirection::INT_TO_FLOAT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::FLOAT;
								} else if (actual == StackValueType::FLOAT && expected == StackValueType::INT) {
									paramCasts[j] = CastDirection::FLOAT_TO_INT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::INT;
								}
							} else {
								// Type mismatch error
								std::string errorMsg = "Type error in function call '";
								errorMsg += name;
								errorMsg += "': Parameter ";
								errorMsg += std::to_string(j + 1);
								errorMsg += " expects ";
								errorMsg += stackValueTypeToString(expected);
								errorMsg += ", but got ";
								errorMsg += stackValueTypeToString(actual);
								reportError(ident, errorMsg.c_str());
							}
						}
					}

					// Store cast information in the identifier node
					ident->setParameterCasts(paramCasts);

					// Consume the parameters from the stack
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						typeStack.pop_back();
					}

					// Apply the produces effect
					if (ident->checkError()) {
						// func? - immediately check error
						// Produces: value (untainted) + error_status (INT)
						for (const auto& type : sig.produces) {
							typeStack.push_back(type); // Push untainted value
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
					} else if (sig.throws && !ident->abortOnError()) {
						// func without ! or ? - pushes result + error flag
						for (const auto& type : sig.produces) {
							typeStack.push_back(type);
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
					} else {
						// Normal call or func!
						for (const auto& type : sig.produces) {
							typeStack.push_back(type);
						}
					}
				}
				// If it's not a user function, it must be a built-in (already validated in pass 2)
				// Built-ins are handled as Instructions, not Identifiers in the AST
				break;
			}

			case IAstNode::Type::SCOPED_IDENTIFIER: {
				// Handle module constants or function calls
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);
				const std::string& moduleName = scoped->scope();
				const std::string& functionName = scoped->name();
				std::string qualifiedName = moduleName + "::" + functionName;

				// Check if this is a constant first
				auto constIt = mModuleConstants.find(moduleName);
				if (constIt != mModuleConstants.end()) {
					const auto& constants = constIt->second;
					if (constants.find(functionName) != constants.end()) {
						// This is a constant - determine its type and push onto the stack
						std::string constQualifiedName = moduleName + "::" + functionName;
						auto valueIt = mModuleConstantValues.find(constQualifiedName);
						if (valueIt != mModuleConstantValues.end()) {
							const std::string& value = valueIt->second;
							// Infer type from value
							if (!value.empty() && value[0] == '"') {
								typeStack.push_back(StackValueType::STRING);
							} else if (value.find('.') != std::string::npos || value.find('e') != std::string::npos ||
									   value.find('E') != std::string::npos) {
								typeStack.push_back(StackValueType::FLOAT);
							} else {
								typeStack.push_back(StackValueType::INT);
							}
						} else {
							typeStack.push_back(StackValueType::UNKNOWN);
						}
						break;
					}
				}

				// Look up the module function signature
				auto sigIt = mFunctionSignatures.find(qualifiedName);
				if (sigIt != mFunctionSignatures.end()) {
					const FunctionSignature& sig = sigIt->second;

					// Validate '!' and '?' usage: only allowed on fallible functions (marked with '!')
					if (scoped->abortOnError() && !sig.throws) {
						std::string errorMsg = "Cannot use '!' operator on function '" + qualifiedName +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(scoped, errorMsg.c_str());
					}
					if (scoped->checkError() && !sig.throws) {
						std::string errorMsg = "Cannot use '?' operator on function '" + qualifiedName +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(scoped, errorMsg.c_str());
					}

					// Check fallible functions without ! or ? must be followed by 'if'
					if (sig.throws && !scoped->abortOnError() && !scoped->checkError()) {
						IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
						if (!nextNode || nextNode->type() != IAstNode::Type::IF_STATEMENT) {
							std::string errorMsg = "Fallible function '" + qualifiedName +
												   "' must be immediately followed by 'if' to check for errors, or use "
												   "'!' to abort on error";
							reportError(scoped, errorMsg.c_str());
						}
					}

					// Check if stack has enough values for function parameters
					if (typeStack.size() < sig.consumes.size()) {
						std::string errorMsg = "Type error in function call '";
						errorMsg += qualifiedName;
						errorMsg += "': Stack underflow (requires ";
						errorMsg += std::to_string(sig.consumes.size());
						errorMsg += " values, have ";
						errorMsg += std::to_string(typeStack.size());
						errorMsg += ")";
						reportError(scoped, errorMsg.c_str());
						break;
					}

					// Track which parameters need casts
					std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);

					// Check if the types match
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
						StackValueType expected = sig.consumes[j];
						StackValueType actual = typeStack[stackIdx];

						// Skip check if expected type is ANY or UNKNOWN
						if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN) {
							continue;
						}

						// Skip check if actual type is UNKNOWN (can't determine type)
						if (actual == StackValueType::UNKNOWN) {
							continue;
						}

						// Check for type mismatch
						if (actual != expected) {
							// Check if implicit cast is allowed (int <-> float)
							if (isImplicitCastAllowed(actual, expected)) {
								// Warn about implicit cast
								std::string warnMsg = "Implicit cast in function call '";
								warnMsg += qualifiedName;
								warnMsg += "': Parameter ";
								warnMsg += std::to_string(j + 1);
								warnMsg += " expects ";
								warnMsg += stackValueTypeToString(expected);
								warnMsg += ", but got ";
								warnMsg += stackValueTypeToString(actual);
								reportWarning(scoped, warnMsg.c_str());

								// Record the cast direction
								if (actual == StackValueType::INT && expected == StackValueType::FLOAT) {
									paramCasts[j] = CastDirection::INT_TO_FLOAT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::FLOAT;
								} else if (actual == StackValueType::FLOAT && expected == StackValueType::INT) {
									paramCasts[j] = CastDirection::FLOAT_TO_INT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::INT;
								}
							} else {
								// Type mismatch error
								std::string errorMsg = "Type error in function call '";
								errorMsg += qualifiedName;
								errorMsg += "': Parameter ";
								errorMsg += std::to_string(j + 1);
								errorMsg += " expects ";
								errorMsg += stackValueTypeToString(expected);
								errorMsg += ", but got ";
								errorMsg += stackValueTypeToString(actual);
								reportError(scoped, errorMsg.c_str());
							}
						}
					}

					// Store cast information in the scoped identifier node
					scoped->setParameterCasts(paramCasts);

					// Consume the parameters from the stack
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						typeStack.pop_back();
					}

					// Apply the produces effect
					if (scoped->checkError()) {
						// func? - immediately check error
						// Produces: value (untainted) + error_status (INT)
						for (const auto& type : sig.produces) {
							typeStack.push_back(type); // Push untainted value
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
					} else if (sig.throws && !scoped->abortOnError()) {
						// func without ! or ? - pushes result + error flag
						for (const auto& type : sig.produces) {
							typeStack.push_back(type);
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
					} else {
						// Normal call or func!
						for (const auto& type : sig.produces) {
							typeStack.push_back(type);
						}
					}
				}
				// If signature not found, module wasn't loaded or analyzed
				// This was already checked in validation pass, so we can skip silently
				break;
			}

			case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
				// Function pointer references push a pointer type onto the stack
				typeStack.push_back(StackValueType::PTR);
				break;

			default:
				// Other node types don't affect the type stack
				break;
			}
		}
	}

	void SemanticValidator::typeCheckInstruction(
			IAstNode* node, const char* name, std::vector<StackValueType>& typeStack) {
		typeCheckInstructionInternal(node, name, typeStack, true);
	}

	void SemanticValidator::typeCheckInstructionInternal(
			IAstNode* node, const char* name, std::vector<StackValueType>& typeStack, bool reportErrors) {
		// Handle instruction aliases
		if (strcmp(name, ".") == 0) {
			name = "print";
		} else if (strcmp(name, "/") == 0) {
			name = "div";
		} else if (strcmp(name, "*") == 0) {
			name = "mul";
		} else if (strcmp(name, "+") == 0) {
			name = "add";
		} else if (strcmp(name, "-") == 0) {
			name = "sub";
		}

		// error instruction: sets error flag (for use in 'throws' functions)
		// Stack: [...] -> [...] (unchanged)
		if (strcmp(name, "error") == 0) {
			// No stack changes, just sets ctx->has_error = true at runtime
			return;
		}

		// read instruction: reads command-line arguments
		// Stack: [...] -> [...] arg0 arg1 ... argN argc
		// Since we don't know argc at compile-time, we push multiple values
		// to allow reasonable operations after read (assumes up to 16 arguments)
		static const int READ_INSTRUCTION_MAX_ARGS = 16; // Maximum expected command-line arguments
		static const int READ_INSTRUCTION_STACK_DEPTH = READ_INSTRUCTION_MAX_ARGS + 1; // +1 for argc itself
		if (strcmp(name, "read") == 0) {
			typeStack.clear();
			// Push 16 values (enough for most use cases) + argc
			// This is a workaround for not knowing argc at compile time
			for (int i = 0; i < READ_INSTRUCTION_STACK_DEPTH; i++) {
				typeStack.push_back(StackValueType::INT);
			}
			return;
		}

		// Arithmetic operations: abs, sq (preserve type)
		if (strcmp(name, "abs") == 0 || strcmp(name, "sq") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Type remains the same (already on stack)
		}
		// Trigonometric functions: sin, cos, tan, asin, acos, atan (always return float)
		else if (strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 || strcmp(name, "tan") == 0 ||
				 strcmp(name, "asin") == 0 || strcmp(name, "acos") == 0 || strcmp(name, "atan") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportError(node, errorMsg.c_str());
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Pop and push float (trig functions always return float)
			typeStack.pop_back();
			typeStack.push_back(StackValueType::FLOAT);
		}
		// Math functions: sqrt, cb, cbrt, ceil, floor, ln, log10, round (always return float)
		else if (strcmp(name, "sqrt") == 0 || strcmp(name, "cb") == 0 || strcmp(name, "cbrt") == 0 ||
				 strcmp(name, "ceil") == 0 || strcmp(name, "floor") == 0 || strcmp(name, "ln") == 0 ||
				 strcmp(name, "log10") == 0 || strcmp(name, "round") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportError(node, errorMsg.c_str());
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Pop and push float (math functions always return float)
			typeStack.pop_back();
			typeStack.push_back(StackValueType::FLOAT);
		}
		// Factorial function: fac (integer only, returns integer)
		else if (strcmp(name, "fac") == 0) {
			if (typeStack.empty()) {
				reportError(node, "Type error in 'fac': Stack underflow (requires 1 integer value)");
				return;
			}

			StackValueType top = typeStack.back();
			if (top != StackValueType::INT) {
				std::string errorMsg = "Type error in 'fac': Expected integer type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Type remains integer (already on stack)
		}
		// Increment/Decrement functions: inc, dec (preserve type)
		else if (strcmp(name, "inc") == 0 || strcmp(name, "dec") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportError(node, errorMsg.c_str());
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Type remains the same (already on stack)
		}
		// Inverse function: inv (numeric input, returns float)
		else if (strcmp(name, "inv") == 0) {
			if (typeStack.empty()) {
				reportError(node, "Type error in 'inv': Stack underflow (requires 1 numeric value)");
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in 'inv': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Pop and push float (inv always returns float)
			typeStack.pop_back();
			typeStack.push_back(StackValueType::FLOAT);
		}
		// Binary arithmetic operations: add, sub, mul, div, pow
		else if (strcmp(name, "add") == 0 || strcmp(name, "sub") == 0 || strcmp(name, "mul") == 0 ||
				 strcmp(name, "div") == 0 || strcmp(name, "pow") == 0) {
			if (typeStack.size() < 2) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 2 numeric values)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}

			StackValueType b = typeStack.back();
			typeStack.pop_back();
			StackValueType a = typeStack.back();
			typeStack.pop_back();

			if (!isNumericType(a) || !isNumericType(b)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric types, got ";
				errorMsg += typeToString(a);
				errorMsg += " and ";
				errorMsg += typeToString(b);
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}

			// Result is float if either operand is float, otherwise int
			StackValueType result = (a == StackValueType::FLOAT || b == StackValueType::FLOAT) ? StackValueType::FLOAT
																							   : StackValueType::INT;
			typeStack.push_back(result);
		}
		// Print operations: print, printv
		else if (strcmp(name, "print") == 0 || strcmp(name, "printv") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 value)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			typeStack.pop_back(); // Pop the value
		}
		// Non-destructive print: prints, printsv
		else if (strcmp(name, "prints") == 0 || strcmp(name, "printsv") == 0) {
			// These don't modify the stack
		}
		// Stack operations: dup
		else if (strcmp(name, "dup") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'dup': Stack underflow (requires 1 value)", reportErrors);
				return;
			}
			StackValueType top = typeStack.back();
			typeStack.push_back(top); // Duplicate
		}
		// Stack operations: dup2 ( a b -- a b a b )
		else if (strcmp(name, "dup2") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'dup2': Stack underflow (requires 2 values)");
				return;
			}
			// Get the second and top elements
			StackValueType second = typeStack[typeStack.size() - 2];
			StackValueType top = typeStack.back();
			// Push copies of both
			typeStack.push_back(second);
			typeStack.push_back(top);
		}
		// Stack operations: dupd ( a b -- a a b )
		else if (strcmp(name, "dupd") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'dupd': Stack underflow (requires 2 values)");
				return;
			}
			// Get the second and top elements
			StackValueType second = typeStack[typeStack.size() - 2];
			StackValueType top = typeStack.back();
			// Push: second (duplicate of second), then top
			typeStack.push_back(second);
			typeStack.push_back(top);
		}
		// Stack operations: swapd ( a b c -- b a c )
		else if (strcmp(name, "swapd") == 0) {
			if (typeStack.size() < 3) {
				reportError(node, "Type error in 'swapd': Stack underflow (requires 3 values)");
				return;
			}
			// Get third, second, and top elements
			StackValueType third = typeStack[typeStack.size() - 3];
			StackValueType second = typeStack[typeStack.size() - 2];
			StackValueType top = typeStack.back();
			// Remove all three
			typeStack.pop_back();
			typeStack.pop_back();
			typeStack.pop_back();
			// Push: second, third, top (swapped second and third)
			typeStack.push_back(second);
			typeStack.push_back(third);
			typeStack.push_back(top);
		}
		// Stack operations: overd ( a b c -- a b a c )
		else if (strcmp(name, "overd") == 0) {
			if (typeStack.size() < 3) {
				reportError(node, "Type error in 'overd': Stack underflow (requires 3 values)");
				return;
			}
			// Get the third element
			StackValueType third = typeStack[typeStack.size() - 3];
			// Push a copy of it to the top
			typeStack.push_back(third);
		}
		// Stack operations: nipd ( a b c -- a c )
		else if (strcmp(name, "nipd") == 0) {
			if (typeStack.size() < 3) {
				reportError(node, "Type error in 'nipd': Stack underflow (requires 3 values)");
				return;
			}
			// Get top element
			StackValueType top = typeStack.back();
			typeStack.pop_back();
			// Remove second element
			typeStack.pop_back();
			// Push top back
			typeStack.push_back(top);
		}
		// Stack operations: swap
		else if (strcmp(name, "swap") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(node, "Type error in 'swap': Stack underflow (requires 2 values)", reportErrors);
				return;
			}
			StackValueType a = typeStack.back();
			typeStack.pop_back();
			StackValueType b = typeStack.back();
			typeStack.pop_back();
			typeStack.push_back(a);
			typeStack.push_back(b);
		}
		// Stack operations: over ( a b -- a b a )
		else if (strcmp(name, "over") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'over': Stack underflow (requires 2 values)");
				return;
			}
			// Get the second element
			StackValueType second = typeStack[typeStack.size() - 2];
			// Push a copy of it to the top
			typeStack.push_back(second);
		}
		// Stack operations: nip ( a b -- b )
		else if (strcmp(name, "nip") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'nip': Stack underflow (requires 2 values)");
				return;
			}
			StackValueType top = typeStack.back();
			typeStack.pop_back();
			typeStack.pop_back();	  // Remove second element
			typeStack.push_back(top); // Push top back
		}
		// Stack operations: clear (empties the entire stack)
		else if (strcmp(name, "clear") == 0) {
			// Clear all elements from the type stack
			typeStack.clear();
		}
		// Stack operations: depth (pushes the current stack depth as an integer)
		else if (strcmp(name, "depth") == 0) {
			// Push an int type onto the stack (depth is always an integer)
			typeStack.push_back(StackValueType::INT);
		}
		// call - invoke function pointer from stack
		else if (strcmp(name, "call") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'call': Stack underflow (requires 1 value)", reportErrors);
				return;
			}
			// Pop the function pointer - runtime will verify it's a pointer type
			typeStack.pop_back();
			// We don't know what the called function will do to the stack
			// So we can't track types accurately after this point
		}
	}

	bool SemanticValidator::isNumericType(StackValueType type) const {
		return type == StackValueType::INT || type == StackValueType::FLOAT;
	}

	const char* SemanticValidator::typeToString(StackValueType type) const {
		switch (type) {
		case StackValueType::INT:
			return "int";
		case StackValueType::FLOAT:
			return "float";
		case StackValueType::STRING:
			return "string";
		case StackValueType::PTR:
			return "ptr";
		case StackValueType::ANY:
			return "any";
		case StackValueType::UNKNOWN:
			return "unknown";
		case StackValueType::TAINTED:
			return "tainted";
		default:
			return "unknown";
		}
	}

} // namespace Qd

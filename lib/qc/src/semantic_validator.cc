#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_ctx.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_struct.h>
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
		// Allow int -> ptr implicit conversion (for null/0 pointers)
		if (actual == StackValueType::INT && expected == StackValueType::PTR) {
			return true;
		}
		return false;
	}

	// Check if a type string is a known struct name (local or imported)
	bool SemanticValidator::isStructTypeName(const std::string& typeStr) const {
		// Check local structs
		if (mDefinedStructs.find(typeStr) != mDefinedStructs.end()) {
			return true;
		}
		// Check imported module structs
		for (const auto& moduleEntry : mModuleStructs) {
			const auto& structs = moduleEntry.second;
			if (structs.find(typeStr) != structs.end()) {
				return true;
			}
		}
		return false;
	}

	// Validate that a type string is a valid type name
	// Returns true if valid, false otherwise
	bool SemanticValidator::isValidTypeName(const std::string& typeStr) const {
		// Primitive types
		if (typeStr == "i64" || typeStr == "f64" || typeStr == "str" || typeStr == "ptr" || typeStr == "any") {
			return true;
		}
		// Struct types
		return isStructTypeName(typeStr);
	}

	StackValueType SemanticValidator::getConstantType(const std::string& value) const {
		if (value.empty()) {
			return StackValueType::UNKNOWN;
		}

		// Check if it's a string literal (starts and ends with quotes)
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
			return StackValueType::STRING;
		}

		// Check if it contains a decimal point (float)
		if (value.find('.') != std::string::npos) {
			return StackValueType::FLOAT;
		}

		// Otherwise it's an integer
		return StackValueType::INT;
	}

	SemanticValidator::SemanticValidator()
		: mFilename(nullptr), mErrorCount(0), mWarningCount(0), mWerror(false), mIsModuleFile(false),
		  mStoreErrors(false), mWarningMinLine(0) {
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

		mErrorCount++;

		if (mStoreErrors) {
			ErrorInfo err;
			err.line = 0;
			err.column = 0;
			err.message = message;
			mStoredErrors.push_back(err);
		} else {
			// GCC/Clang style: quadc: filename: error: message
			std::cerr << Colors::bold() << "quadc: " << Colors::reset();
			if (mFilename) {
				std::cerr << Colors::bold() << mFilename << ":" << Colors::reset() << " ";
			}
			std::cerr << Colors::bold() << Colors::red() << "error:" << Colors::reset() << " ";
			std::cerr << Colors::bold() << message << Colors::reset() << std::endl;
		}
	}

	void SemanticValidator::reportErrorConditional(const IAstNode* node, const char* message, bool shouldReport) {
		if (!shouldReport) {
			return;
		}

		mErrorCount++;

		if (mStoreErrors) {
			ErrorInfo err;
			err.line = node ? node->line() : 0;
			err.column = node ? node->column() : 0;
			err.message = message;
			mStoredErrors.push_back(err);
		} else {
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
		}
	}

	void SemanticValidator::reportWarning(const IAstNode* node, const char* message) {
		// If werror is enabled, treat warnings as errors
		if (mWerror) {
			reportError(node, message);
			return;
		}

		// Suppress warnings for lines before the minimum line (for REPL incremental compilation)
		if (mWarningMinLine > 0 && node && static_cast<size_t>(node->line()) < mWarningMinLine) {
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

			// Check for duplicate function name
			if (mDefinedFunctions.find(func->name()) != mDefinedFunctions.end()) {
				std::string errorMsg = "Duplicate function definition: '" + func->name() + "'";
				reportError(func, errorMsg.c_str());
				return;
			}

			// Check for conflict with struct name
			if (mDefinedStructs.find(func->name()) != mDefinedStructs.end()) {
				std::string errorMsg = "Function '" + func->name() + "' conflicts with struct name";
				reportError(func, errorMsg.c_str());
				return;
			}

			// Check for conflict with constant name
			if (mDefinedConstants.find(func->name()) != mDefinedConstants.end()) {
				std::string errorMsg = "Function '" + func->name() + "' conflicts with constant name";
				reportError(func, errorMsg.c_str());
				return;
			}

			mDefinedFunctions.insert(func->name());
		}

		// If this is a constant declaration, add it to the symbol table
		if (node->type() == IAstNode::Type::CONSTANT_DECLARATION) {
			AstNodeConstant* constant = static_cast<AstNodeConstant*>(node);

			// Check for duplicate constant name
			if (mDefinedConstants.find(constant->name()) != mDefinedConstants.end()) {
				std::string errorMsg = "Duplicate constant definition: '" + constant->name() + "'";
				reportError(constant, errorMsg.c_str());
				return;
			}

			// Check for conflict with struct name
			if (mDefinedStructs.find(constant->name()) != mDefinedStructs.end()) {
				std::string errorMsg = "Constant '" + constant->name() + "' conflicts with struct name";
				reportError(constant, errorMsg.c_str());
				return;
			}

			// Check for conflict with function name
			if (mDefinedFunctions.find(constant->name()) != mDefinedFunctions.end()) {
				std::string errorMsg = "Constant '" + constant->name() + "' conflicts with function name";
				reportError(constant, errorMsg.c_str());
				return;
			}

			mDefinedConstants.insert(constant->name());
			mConstantValues[constant->name()] = constant->value();
		}

		// If this is a struct declaration, add it to the symbol table and collect field types
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);

			// Check for duplicate struct name
			if (mDefinedStructs.find(structDecl->name()) != mDefinedStructs.end()) {
				std::string errorMsg = "Duplicate struct definition: '" + structDecl->name() + "'";
				reportError(structDecl, errorMsg.c_str());
				return;
			}

			// Check for conflict with function name
			if (mDefinedFunctions.find(structDecl->name()) != mDefinedFunctions.end()) {
				std::string errorMsg = "Struct '" + structDecl->name() + "' conflicts with function name";
				reportError(structDecl, errorMsg.c_str());
				return;
			}

			// Check for conflict with constant name
			if (mDefinedConstants.find(structDecl->name()) != mDefinedConstants.end()) {
				std::string errorMsg = "Struct '" + structDecl->name() + "' conflicts with constant name";
				reportError(structDecl, errorMsg.c_str());
				return;
			}

			mDefinedStructs.insert(structDecl->name());
			mStructDeclarations[structDecl->name()] = structDecl;

			// Collect field types
			std::unordered_map<std::string, StackValueType> fieldTypes;
			std::unordered_set<std::string> seenFieldNames;
			for (size_t i = 0; i < structDecl->childCount(); i++) {
				IAstNode* child = structDecl->child(i);
				if (child && child->type() == IAstNode::Type::STRUCT_FIELD) {
					AstNodeStructField* field = static_cast<AstNodeStructField*>(child);

					// Check for duplicate field name
					if (seenFieldNames.find(field->name()) != seenFieldNames.end()) {
						std::string errorMsg =
								"Duplicate field name '" + field->name() + "' in struct '" + structDecl->name() + "'";
						reportError(field, errorMsg.c_str());
						return;
					}
					seenFieldNames.insert(field->name());

					StackValueType fieldType = StackValueType::UNKNOWN;
					const std::string& typeName = field->typeName();
					if (typeName == "f64") {
						fieldType = StackValueType::FLOAT;
					} else if (typeName == "i64") {
						fieldType = StackValueType::INT;
					} else if (typeName == "str") {
						fieldType = StackValueType::STRING;
					} else if (typeName == "ptr" || typeName.find('*') != std::string::npos) {
						fieldType = StackValueType::PTR;
					} else if (!typeName.empty() && std::isupper(typeName[0])) {
						// Struct type - treat as PTR and record the struct type name
						fieldType = StackValueType::PTR;
						mStructFieldStructTypes[structDecl->name()][field->name()] = typeName;
					}
					fieldTypes[field->name()] = fieldType;
				}
			}
			mStructFieldTypes[structDecl->name()] = fieldTypes;
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

				// Also register function signature for type checking
				FunctionSignature sig;

				// Process input parameters
				for (const auto* param : func->inputParameters) {
					std::string typeStr = param->typeString();
					if (typeStr == "i32" || typeStr == "i64" || typeStr == "u8" || typeStr == "u16" ||
						typeStr == "u32" || typeStr == "u64") {
						sig.consumes.push_back(StackValueType::INT);
					} else if (typeStr == "f32" || typeStr == "f64" || typeStr == "float") {
						sig.consumes.push_back(StackValueType::FLOAT);
					} else if (typeStr == "str") {
						sig.consumes.push_back(StackValueType::STRING);
					} else if (typeStr == "ptr" || typeStr == "bool" || isStructTypeName(typeStr)) {
						sig.consumes.push_back(StackValueType::PTR);
					} else {
						sig.consumes.push_back(StackValueType::ANY);
					}
				}

				// Process output parameters
				for (const auto* param : func->outputParameters) {
					std::string typeStr = param->typeString();
					if (typeStr == "i32" || typeStr == "i64" || typeStr == "u8" || typeStr == "u16" ||
						typeStr == "u32" || typeStr == "u64") {
						sig.produces.push_back(StackValueType::INT);
					} else if (typeStr == "f32" || typeStr == "f64" || typeStr == "float") {
						sig.produces.push_back(StackValueType::FLOAT);
					} else if (typeStr == "str") {
						sig.produces.push_back(StackValueType::STRING);
					} else if (typeStr == "ptr" || typeStr == "bool" || isStructTypeName(typeStr)) {
						sig.produces.push_back(StackValueType::PTR);
					} else {
						sig.produces.push_back(StackValueType::ANY);
					}
				}

				sig.throws = func->throws;
				mFunctionSignatures[qualifiedName] = sig;
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectDefinitions(node->child(i));
		}
	}

	void SemanticValidator::loadModuleDefinitions(
			const std::string& moduleName, const std::string& currentPackage, bool reportErrors) {
		// Check for circular dependency
		for (size_t i = 0; i < mModuleDependencyChain.size(); i++) {
			if (mModuleDependencyChain[i] == moduleName) {
				// Found circular dependency - build error message showing the cycle
				std::string errorMsg = "Circular module dependency detected: ";
				for (size_t j = i; j < mModuleDependencyChain.size(); j++) {
					errorMsg += mModuleDependencyChain[j];
					errorMsg += " -> ";
				}
				errorMsg += moduleName;
				reportError(errorMsg.c_str());
				return;
			}
		}

		// Check if we've already loaded this specific file (prevent duplicate loads)
		if (mLoadedModuleFiles.count(moduleName)) {
			return;
		}
		mLoadedModuleFiles.insert(moduleName);

		// Add to dependency chain
		mModuleDependencyChain.push_back(moduleName);

		// RAII-style cleanup: ensure we pop from dependency chain when function exits
		struct ChainGuard {
			std::vector<std::string>& chain;

			ChainGuard(std::vector<std::string>& c) : chain(c) {
			}

			~ChainGuard() {
				if (!chain.empty()) {
					chain.pop_back();
				}
			}
		} guard(mModuleDependencyChain);

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
			const char* quadratePath = std::getenv("QUADRATE_PATH");
			if (quadratePath) {
				packagesDir = quadratePath;
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
			const char* quadratePath = std::getenv("QUADRATE_PATH");
			if (quadratePath) {
				packagesDir = quadratePath;
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

			// Try 4: QUADRATE_LIBDIR environment variable (for development/dist)
			// Standard library modules are at $QUADRATE_LIBDIR/std<module>qd/qd/<module>/module.qd
			const char* quadrateLibDir = std::getenv("QUADRATE_LIBDIR");
			if (quadrateLibDir) {
				modulePath = std::string(quadrateLibDir) + "/std" + moduleName + "qd/qd/" + moduleName + "/module.qd";
				file.open(modulePath);
				if (file.good()) {
					// Store the module directory
					mModuleDirectories[moduleName] =
							std::string(quadrateLibDir) + "/std" + moduleName + "qd/qd/" + moduleName;
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					parseModuleAndCollectFunctions(moduleName, source);
					return;
				}
				file.close();
			}

			// Try 5: Standard library directories relative to current directory (for development)
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

			// Try 6: Standard library relative to executable (for installed binaries)
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

			// Try 7: $HOME/quadrate directory
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
		std::unordered_map<std::string, bool> moduleFunctions;
		collectModuleFunctions(moduleAstRoot, moduleFunctions);

		// Store the collected functions
		// If this module already has functions (from .qd file imports), merge with existing map
		if (mModuleFunctions.find(moduleName) != mModuleFunctions.end()) {
			// Merge: add new functions to existing map
			for (const auto& func : moduleFunctions) {
				mModuleFunctions[moduleName][func.first] = func.second;
			}
		} else {
			// Create new entry
			mModuleFunctions[moduleName] = moduleFunctions;
		}

		// Collect constant definitions from the module
		std::unordered_map<std::string, bool> moduleConstants;
		collectModuleConstants(moduleAstRoot, moduleConstants);

		// Store the collected constants
		if (mModuleConstants.find(moduleName) != mModuleConstants.end()) {
			// Merge: add new constants to existing map
			for (const auto& constant : moduleConstants) {
				mModuleConstants[moduleName][constant.first] = constant.second;
			}
		} else {
			// Create new entry
			mModuleConstants[moduleName] = moduleConstants;
		}

		// Also collect constant values
		collectModuleConstantValues(moduleAstRoot, moduleName);

		// Collect struct definitions from the module
		std::unordered_map<std::string, bool> moduleStructs;
		collectModuleStructs(moduleAstRoot, moduleStructs);

		// Store the collected structs
		if (mModuleStructs.find(moduleName) != mModuleStructs.end()) {
			// Merge: add new structs to existing map
			for (const auto& structEntry : moduleStructs) {
				mModuleStructs[moduleName][structEntry.first] = structEntry.second;
			}
		} else {
			// Create new entry
			mModuleStructs[moduleName] = moduleStructs;
		}

		// Also collect struct field types so field access validation works
		collectModuleStructFieldTypes(moduleAstRoot);

		// Analyze function signatures for module functions
		// We use a simplified analysis since we don't need iterative convergence for modules
		analyzeModuleFunctionSignatures(moduleAstRoot, moduleName);
	}

	void SemanticValidator::collectModuleFunctions(IAstNode* node, std::unordered_map<std::string, bool>& functions) {
		if (!node) {
			return;
		}

		// If this is a function declaration, add it with its visibility
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			functions[func->name()] = func->isPublic();
		}
		// If this is an import statement, add imported functions (always public for imported libs)
		else if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			AstNodeImport* import = static_cast<AstNodeImport*>(node);
			const auto& importedFuncs = import->functions();
			for (const auto* func : importedFuncs) {
				functions[func->name] = true; // Imported C functions are always public
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleFunctions(node->child(i), functions);
		}
	}

	void SemanticValidator::collectModuleConstants(IAstNode* node, std::unordered_map<std::string, bool>& constants) {
		if (!node) {
			return;
		}

		// If this is a constant declaration, add it with its visibility
		if (node->type() == IAstNode::Type::CONSTANT_DECLARATION) {
			AstNodeConstant* constNode = static_cast<AstNodeConstant*>(node);
			constants[constNode->name()] = constNode->isPublic();
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleConstants(node->child(i), constants);
		}
	}

	void SemanticValidator::collectModuleStructs(IAstNode* node, std::unordered_map<std::string, bool>& structs) {
		if (!node) {
			return;
		}

		// If this is a struct declaration, add it with its visibility
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structNode = static_cast<AstNodeStructDeclaration*>(node);
			structs[structNode->name()] = structNode->isPublic();
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleStructs(node->child(i), structs);
		}
	}

	void SemanticValidator::collectModuleStructFieldTypes(IAstNode* node) {
		if (!node) {
			return;
		}

		// If this is a struct declaration, collect its field types and store the declaration
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);
			mModuleStructDeclarations[structDecl->name()] = structDecl;
			std::unordered_map<std::string, StackValueType> fieldTypes;
			std::vector<std::string> fieldOrder;
			std::unordered_set<std::string> seenFieldNames;

			for (const auto* field : structDecl->fields()) {
				// Check for duplicate field names
				if (seenFieldNames.count(field->name())) {
					std::string errorMsg =
							"Duplicate field name '" + field->name() + "' in struct '" + structDecl->name() + "'";
					reportError(field, errorMsg.c_str());
					return;
				}
				seenFieldNames.insert(field->name());
				fieldOrder.push_back(field->name());

				StackValueType fieldType = StackValueType::UNKNOWN;
				const std::string& typeName = field->typeName();
				if (typeName == "f64") {
					fieldType = StackValueType::FLOAT;
				} else if (typeName == "i64") {
					fieldType = StackValueType::INT;
				} else if (typeName == "str") {
					fieldType = StackValueType::STRING;
				} else if (typeName == "ptr" || typeName.find('*') != std::string::npos) {
					fieldType = StackValueType::PTR;
				} else if (!typeName.empty() && std::isupper(typeName[0])) {
					// Struct type - treat as PTR and record the struct type name
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[structDecl->name()][field->name()] = typeName;
				}
				fieldTypes[field->name()] = fieldType;
			}
			mStructFieldTypes[structDecl->name()] = fieldTypes;
			mStructFieldOrder[structDecl->name()] = fieldOrder;
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleStructFieldTypes(node->child(i));
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

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}

				if (typeStr == "i64") {
					typeStack.push_back(StackValueType::INT);
				} else if (typeStr == "f64") {
					typeStack.push_back(StackValueType::FLOAT);
				} else if (typeStr == "str") {
					typeStack.push_back(StackValueType::STRING);
				} else if (typeStr == "ptr" || isStructTypeName(typeStr)) {
					typeStack.push_back(StackValueType::PTR);
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

				if (typeStr == "i64") {
					sig.consumes.push_back(StackValueType::INT);
				} else if (typeStr == "f64") {
					sig.consumes.push_back(StackValueType::FLOAT);
				} else if (typeStr == "str") {
					sig.consumes.push_back(StackValueType::STRING);
				} else if (typeStr == "ptr") {
					sig.consumes.push_back(StackValueType::PTR);
				} else if (isStructTypeName(typeStr)) {
					// Struct type - treat as PTR but track the struct type
					sig.consumes.push_back(StackValueType::PTR);
					sig.parameterStructTypes[i] = typeStr;
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

					// Validate type name
					if (!isValidTypeName(typeStr)) {
						reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
												   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
												   .c_str());
					}

					if (typeStr == "i64") {
						sig.consumes.push_back(StackValueType::INT);
					} else if (typeStr == "f64") {
						sig.consumes.push_back(StackValueType::FLOAT);
					} else if (typeStr == "str") {
						sig.consumes.push_back(StackValueType::STRING);
					} else if (typeStr == "ptr") {
						sig.consumes.push_back(StackValueType::PTR);
					} else if (isStructTypeName(typeStr)) {
						// Struct type - treat as PTR but track the struct type
						sig.consumes.push_back(StackValueType::PTR);
						sig.parameterStructTypes[i] = typeStr;
					} else {
						// Untyped or unknown - treat as ANY
						sig.consumes.push_back(StackValueType::ANY);
					}
				}

				// Build produces list from output parameters
				for (size_t i = 0; i < func->outputParameters.size(); i++) {
					AstNodeParameter* param = func->outputParameters[i];
					const std::string& typeStr = param->typeString();

					// Validate type name
					if (!isValidTypeName(typeStr)) {
						reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
												   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
												   .c_str());
					}

					if (typeStr == "i64") {
						sig.produces.push_back(StackValueType::INT);
					} else if (typeStr == "f64") {
						sig.produces.push_back(StackValueType::FLOAT);
					} else if (typeStr == "str") {
						sig.produces.push_back(StackValueType::STRING);
					} else if (typeStr == "ptr" || isStructTypeName(typeStr)) {
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
			if (mDefinedFunctions.find(name) != mDefinedFunctions.end()) {
				// Valid function
				return;
			}

			// Check if it's a defined constant
			if (mDefinedConstants.find(name) != mDefinedConstants.end()) {
				// Valid constant
				return;
			}

			// Check if it's a defined struct (for struct construction)
			if (mDefinedStructs.find(name) != mDefinedStructs.end()) {
				// Valid struct construction
				return;
			}

			// Check if it's a struct from an imported module
			for (const auto& moduleEntry : mModuleStructs) {
				const auto& structs = moduleEntry.second;
				if (structs.find(name) != structs.end() && structs.at(name)) {
					// Valid struct from imported module (must be public)
					return;
				}
			}

			// Not found - report error
			std::string errorMsg = "Undefined function '";
			errorMsg += name;
			errorMsg += "'";
			reportError(ident, errorMsg.c_str());
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
				auto constantIt = constants.find(functionName);
				if (constantIt != constants.end()) {
					// Check if the constant is public
					bool isPublic = constantIt->second;
					if (!isPublic) {
						std::string errorMsg = "Constant '";
						errorMsg += functionName;
						errorMsg += "' in module '";
						errorMsg += scopeName;
						errorMsg += "' is private and cannot be accessed from outside the module. Mark it as 'pub "
									"const' to export it.";
						reportError(scoped, errorMsg.c_str());
					}
					return;
				}
			}

			// Check if the function exists in the module
			auto moduleIt = mModuleFunctions.find(scopeName);
			if (moduleIt != mModuleFunctions.end()) {
				const auto& functions = moduleIt->second;
				auto funcIt = functions.find(functionName);
				if (funcIt == functions.end()) {
					// If not found as a function, check if it's a struct
					auto moduleStructsIt = mModuleStructs.find(scopeName);
					bool foundAsStruct = false;
					if (moduleStructsIt != mModuleStructs.end()) {
						const auto& structs = moduleStructsIt->second;
						auto structIt = structs.find(functionName);
						if (structIt != structs.end()) {
							foundAsStruct = true;
							// Check if the struct is public
							bool isPublic = structIt->second;
							if (!isPublic) {
								std::string errorMsg = "Struct '";
								errorMsg += functionName;
								errorMsg += "' in module '";
								errorMsg += scopeName;
								errorMsg += "' is private and cannot be accessed from outside the module. Mark it as "
											"'pub struct' to export it.";
								reportError(scoped, errorMsg.c_str());
							}
						}
					}

					if (!foundAsStruct) {
						std::string errorMsg = "Function, constant, or struct '";
						errorMsg += functionName;
						errorMsg += "' not found in module '";
						errorMsg += scopeName;
						errorMsg += "'";
						reportError(scoped, errorMsg.c_str());
					}
				} else {
					// Check if the function is public
					bool isPublic = funcIt->second;
					if (!isPublic) {
						std::string errorMsg = "Function '";
						errorMsg += functionName;
						errorMsg += "' in module '";
						errorMsg += scopeName;
						errorMsg += "' is private and cannot be accessed from outside the module. Mark it as 'pub fn' "
									"to export it.";
						reportError(scoped, errorMsg.c_str());
					}
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

		// When entering a function declaration, add parameters to local variables
		// so they can be used directly by name without -> binding
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			// Create a new scope for the function body with parameters pre-registered
			std::unordered_set<std::string> funcLocalVariables = localVariables;
			for (auto* paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				funcLocalVariables.insert(param->name());
			}
			// Process function body with parameters visible
			for (size_t i = 0; i < node->childCount(); i++) {
				validateReferencesInternal(node->child(i), childrenInsideForLoop, funcLocalVariables);
			}
			return;
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

			// Collect parameter names
			std::vector<std::string> paramNames;
			for (auto* paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				paramNames.push_back(param->name());
			}

			// Initialize type stack and parameter map with input parameters
			// Parameters are on the stack when function starts, and can be accessed by name
			std::unordered_map<std::string, StackValueType> paramTypes;
			for (auto* paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				std::string typeStr = param->typeString();

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}

				StackValueType paramType;
				if (typeStr == "i64") {
					paramType = StackValueType::INT;
				} else if (typeStr == "f64") {
					paramType = StackValueType::FLOAT;
				} else if (typeStr == "str") {
					paramType = StackValueType::STRING;
				} else if (typeStr == "ptr" || isStructTypeName(typeStr)) {
					paramType = StackValueType::PTR;
				} else {
					// Untyped or unknown - use ANY
					paramType = StackValueType::ANY;
				}

				typeStack.push_back(paramType);
				paramTypes[param->name()] = paramType;
			}

			// Analyze the function body in isolation (without resolving function calls)
			if (func->body()) {
				analyzeBlockInIsolation(func->body(), typeStack, paramTypes);
			}

			// Store the signature with input parameters as consumes
			FunctionSignature sig;

			// Build consumes list from input parameters
			for (size_t paramIdx = 0; paramIdx < func->inputParameters().size(); paramIdx++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[paramIdx]);
				std::string typeStr = param->typeString();

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}

				if (typeStr == "i64") {
					sig.consumes.push_back(StackValueType::INT);
				} else if (typeStr == "f64") {
					sig.consumes.push_back(StackValueType::FLOAT);
				} else if (typeStr == "str") {
					sig.consumes.push_back(StackValueType::STRING);
				} else if (typeStr == "ptr") {
					sig.consumes.push_back(StackValueType::PTR);
				} else if (isStructTypeName(typeStr)) {
					// Struct type - treat as PTR but track the struct type
					sig.consumes.push_back(StackValueType::PTR);
					sig.parameterStructTypes[paramIdx] = typeStr;
				} else {
					// Untyped or unknown - use ANY
					sig.consumes.push_back(StackValueType::ANY);
				}
			}

			// Validate declared output parameter types
			for (auto* paramNode : func->outputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				std::string typeStr = param->typeString();

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}
			}

			// Collect which fields are accessed on each parameter
			if (func->body()) {
				collectParameterFieldAccesses(func->body(), paramNames, sig.parameterFieldAccess);
			}

			// Infer struct types from field access patterns
			for (size_t paramIdx = 0; paramIdx < paramNames.size(); paramIdx++) {
				const std::string& paramName = paramNames[paramIdx];
				auto fieldAccessIt = sig.parameterFieldAccess.find(paramName);

				if (fieldAccessIt != sig.parameterFieldAccess.end()) {
					const auto& accessedFields = fieldAccessIt->second;

					// Try to find a unique struct that has all accessed fields
					std::string matchingStruct = findStructTypeByFields(accessedFields);
					if (!matchingStruct.empty()) {
						sig.parameterStructTypes[paramIdx] = matchingStruct;
					}
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

	void SemanticValidator::collectParameterFieldAccesses(IAstNode* node, const std::vector<std::string>& paramNames,
			std::unordered_map<std::string, std::unordered_map<std::string, StackValueType>>& fieldAccesses) {
		if (!node) {
			return;
		}

		// Track which local variables come from which parameters (via ->)
		std::unordered_map<std::string, std::string> localToParam; // local var name -> parameter name

		// Helper to recursively collect with local variable tracking
		std::function<void(IAstNode*)> collectRecursive = [&](IAstNode* n) {
			if (!n) {
				return;
			}

			// Track local variable bindings from parameters
			// Pattern: parameter values are on stack, then `-> localVar` pops them
			// We need to track the flow to know which local comes from which parameter
			// For simplicity, we'll track field accesses on ANY local variable or parameter
			// and attribute them all to the single PTR parameter (since most functions have one)

			if (n->type() == IAstNode::Type::FIELD_ACCESS) {
				AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(n);
				const std::string& varName = fieldAccess->varName();
				const std::string& fieldName = fieldAccess->fieldName();

				// Determine the type of this field by looking in all known structs
				StackValueType fieldType = StackValueType::UNKNOWN;
				for (const auto& structEntry : mStructFieldTypes) {
					const auto& fields = structEntry.second;
					auto it = fields.find(fieldName);
					if (it != fields.end()) {
						fieldType = it->second;
						break;
					}
				}

				// Check if this variable is directly a parameter
				bool isParam = std::find(paramNames.begin(), paramNames.end(), varName) != paramNames.end();

				// Check if this variable was bound from a parameter
				auto localIt = localToParam.find(varName);
				bool isFromParam = (localIt != localToParam.end());

				if (isParam) {
					fieldAccesses[varName][fieldName] = fieldType;
				} else if (isFromParam) {
					fieldAccesses[localIt->second][fieldName] = fieldType;
				} else {
					// This could be a local variable bound from a parameter
					// For now, attribute to first PTR parameter as a heuristic
					for (const auto& paramName : paramNames) {
						fieldAccesses[paramName][fieldName] = fieldType;
						break; // Only add to first parameter
					}
				}
			}

			// Track local variable bindings
			// Note: This is a simplified tracking - proper tracking would require data flow analysis
			if (n->type() == IAstNode::Type::LOCAL) {
				AstNodeLocal* local = static_cast<AstNodeLocal*>(n);
				// Assume this local is bound from a parameter (simplified heuristic)
				// In reality, we'd need to track the stack to know which value is being bound
				if (!paramNames.empty()) {
					localToParam[local->name()] = paramNames[0]; // Associate with first param
				}
			}

			// Recursively process children
			for (size_t i = 0; i < n->childCount(); i++) {
				collectRecursive(n->child(i));
			}
		};

		collectRecursive(node);
	}

	void SemanticValidator::analyzeBlockInIsolation(IAstNode* node, std::vector<StackValueType>& typeStack,
			const std::unordered_map<std::string, StackValueType>& initialLocalVars) {
		if (!node) {
			return;
		}

		// Dummy structTypeStack for signature analysis (not used, but required by API)
		std::vector<std::string> structTypeStack;

		// Track local variable types for accurate signature analysis
		// Start with any initial local variables (e.g., function parameters)
		std::unordered_map<std::string, StackValueType> localVarTypes = initialLocalVars;

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

			case IAstNode::Type::ARRAY_LITERAL: {
				// Array literal pushes a pointer (array reference) onto the stack
				typeStack.push_back(StackValueType::PTR);
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				// During signature analysis, don't report errors - just simulate the stack
				typeCheckInstructionInternal(child, instr->name().c_str(), typeStack, structTypeStack, false);
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

				// First check if it's a local variable reference
				auto localIt = localVarTypes.find(name);
				if (localIt != localVarTypes.end()) {
					// Push the local variable's type
					typeStack.push_back(localIt->second);
					// Push the struct type if this is a struct variable
					auto structTypeIt = mLocalVariableStructTypes.find(name);
					if (structTypeIt != mLocalVariableStructTypes.end()) {
						structTypeStack.push_back(structTypeIt->second);
					} else {
						structTypeStack.push_back("");
					}
					break;
				}

				auto sigIt = mFunctionSignatures.find(name);
				if (sigIt != mFunctionSignatures.end()) {
					// Apply the known signature: pop consumes, push produces
					const FunctionSignature& sig = sigIt->second;
					// Pop consumed types
					for (size_t j = 0; j < sig.consumes.size() && !typeStack.empty(); j++) {
						typeStack.pop_back();
					}
					// Push produced types
					for (const auto& type : sig.produces) {
						typeStack.push_back(type);
					}
					break;
				}

				// Check if it's a struct construction
				if (mDefinedStructs.find(name) != mDefinedStructs.end()) {
					// Struct construction produces a pointer
					auto structFieldIt = mStructFieldTypes.find(name);
					if (structFieldIt != mStructFieldTypes.end()) {
						size_t fieldCount = structFieldIt->second.size();
						// Pop field values from both stacks
						for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}
					}
					typeStack.push_back(StackValueType::PTR);
					structTypeStack.push_back(name); // Track the struct type
					break;
				}

				// Check if it's a struct from an imported module
				for (const auto& moduleEntry : mModuleStructs) {
					const auto& structs = moduleEntry.second;
					if (structs.find(name) != structs.end()) {
						auto structFieldIt = mStructFieldTypes.find(name);
						if (structFieldIt != mStructFieldTypes.end()) {
							size_t fieldCount = structFieldIt->second.size();
							for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
								typeStack.pop_back();
								if (!structTypeStack.empty()) {
									structTypeStack.pop_back();
								}
							}
						}
						typeStack.push_back(StackValueType::PTR);
						structTypeStack.push_back(name); // Track the struct type
						break;
					}
				}

				// Check if it's a constant
				auto constIt = mConstantValues.find(name);
				if (constIt != mConstantValues.end()) {
					// Push the constant's type onto the stack
					StackValueType constType = getConstantType(constIt->second);
					typeStack.push_back(constType);
				}
				// If signature not known yet, skip (will be resolved in next iteration)
				break;
			}

			case IAstNode::Type::FIELD_ACCESS: {
				// Field access pushes a value onto the stack
				// Try to find the field type by searching all known structs
				AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(child);
				const std::string& fieldName = fieldAccess->fieldName();

				StackValueType fieldType = StackValueType::UNKNOWN;
				// Search in local structs
				for (const auto& structEntry : mStructFieldTypes) {
					const auto& fields = structEntry.second;
					auto it = fields.find(fieldName);
					if (it != fields.end()) {
						fieldType = it->second;
						break;
					}
				}

				// If still unknown, use ANY as fallback
				if (fieldType == StackValueType::UNKNOWN) {
					fieldType = StackValueType::ANY;
				}

				typeStack.push_back(fieldType);
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
					// Apply the known signature: pop consumes, push produces
					const FunctionSignature& sig = sigIt->second;
					// Pop consumed types
					for (size_t j = 0; j < sig.consumes.size() && !typeStack.empty(); j++) {
						typeStack.pop_back();
					}
					// Push produced types
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

			case IAstNode::Type::LOCAL: {
				// Local variable binding: pop value from stack and store type
				AstNodeLocal* local = static_cast<AstNodeLocal*>(child);
				const std::string& varName = local->name();
				if (!typeStack.empty()) {
					StackValueType varType = typeStack.back();
					typeStack.pop_back();
					localVarTypes[varName] = varType;
				}
				break;
			}

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
			std::vector<std::string> structTypeStack;
			std::unordered_map<std::string, StackValueType> localVariables;

			// Clear local variable struct types for this function
			mLocalVariableStructTypes.clear();

			// Initialize type stack with input parameters
			// Input parameters are on the stack when the function starts
			// Also register them as local variables for direct access by name
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i]);
				const std::string& typeStr = param->typeString();

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}

				StackValueType paramType;
				std::string structType = "";

				if (typeStr == "i64") {
					paramType = StackValueType::INT;
				} else if (typeStr == "f64") {
					paramType = StackValueType::FLOAT;
				} else if (typeStr == "str") {
					paramType = StackValueType::STRING;
				} else if (typeStr == "ptr") {
					paramType = StackValueType::PTR;
				} else if (isStructTypeName(typeStr)) {
					// Struct type - treat as PTR but track the struct type
					paramType = StackValueType::PTR;
					structType = typeStr;
					mLocalVariableStructTypes[param->name()] = typeStr;
				} else {
					// Untyped or unknown - treat as ANY
					paramType = StackValueType::ANY;
				}

				typeStack.push_back(paramType);
				structTypeStack.push_back(structType);
				// Register parameter as local variable for direct access
				localVariables[param->name()] = paramType;
			}

			// Type check the function body
			if (func->body()) {
				typeCheckBlock(func->body(), typeStack, localVariables, structTypeStack);
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			typeCheckFunction(node->child(i));
		}
	}

	void SemanticValidator::typeCheckBlock(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
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
					structTypeStack.push_back("");
					break;
				case AstNodeLiteral::LiteralType::FLOAT:
					typeStack.push_back(StackValueType::FLOAT);
					structTypeStack.push_back("");
					break;
				case AstNodeLiteral::LiteralType::STRING:
					typeStack.push_back(StackValueType::STRING);
					structTypeStack.push_back("");
					break;
				}
				break;
			}

			case IAstNode::Type::ARRAY_LITERAL: {
				// Array literal pushes a pointer (array reference) onto the stack
				typeStack.push_back(StackValueType::PTR);
				structTypeStack.push_back("");
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				typeCheckInstruction(child, instr->name().c_str(), typeStack, structTypeStack);
				break;
			}

			case IAstNode::Type::BLOCK: {
				// Recursively check nested blocks
				typeCheckBlock(child, typeStack, localVariables, structTypeStack);
				break;
			}

			case IAstNode::Type::IF_STATEMENT: {
				// Check that stack has a condition value
				if (typeStack.empty()) {
					reportError(child, "Type error in 'if': Stack underflow (requires 1 condition value)");
					break;
				}
				// Pop the condition value
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}
				// Skip type checking if/else bodies - failable function patterns and
				// runtime stack manipulation make static tracking unreliable
				break;
			}

			case IAstNode::Type::DEFER_STATEMENT: {
				// Defer blocks must have zero net stack effect
				size_t stackSizeBefore = typeStack.size();

				// Type check the defer body
				for (size_t j = 0; j < child->childCount(); j++) {
					IAstNode* deferChild = child->child(j);
					if (deferChild && deferChild->type() == IAstNode::Type::BLOCK) {
						typeCheckBlock(deferChild, typeStack, localVariables, structTypeStack);
					}
				}

				int deferEffect = static_cast<int>(typeStack.size()) - static_cast<int>(stackSizeBefore);
				if (deferEffect != 0) {
					std::string errorMsg = "Stack effect error in 'defer': block must have zero net stack effect, but changes stack by ";
					errorMsg += std::to_string(deferEffect);
					reportError(child, errorMsg.c_str());
				}

				// Restore stack to before defer (defer shouldn't affect the surrounding stack)
				while (typeStack.size() > stackSizeBefore) {
					typeStack.pop_back();
					if (!structTypeStack.empty()) {
						structTypeStack.pop_back();
					}
				}
				break;
			}

			case IAstNode::Type::SWITCH_STATEMENT: {
				// Check that stack has a value to switch on
				if (typeStack.empty()) {
					reportError(child, "Type error in 'switch': Stack underflow (requires 1 value to match)");
					break;
				}
				// Pop the switch value
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}
				// Skip type checking switch case bodies - they may operate on runtime stack
				// values that can't be tracked statically (e.g., in interpreters)
				break;
			}

			case IAstNode::Type::FOR_STATEMENT:
			case IAstNode::Type::LOOP_STATEMENT: {
				// For now, skip loop type checking (break/continue complicate analysis)
				break;
			}

			case IAstNode::Type::CTX_STATEMENT: {
				// For now, skip ctx type checking since it's complex
				// (would need full stack effect analysis including clear, drop, etc.)
				// The runtime enforces the single-value constraint anyway
				// Just push a generic type to the parent stack
				typeStack.push_back(StackValueType::INT);
				structTypeStack.push_back("");
				break;
			}

			case IAstNode::Type::LOCAL: {
				// Handle local variable declaration: pop value from stack and store
				AstNodeLocal* local = static_cast<AstNodeLocal*>(child);
				const std::string& varName = local->name();

				// Check if variable name shadows a function
				if (mDefinedFunctions.find(varName) != mDefinedFunctions.end()) {
					std::string errorMsg = "Local variable '";
					errorMsg += varName;
					errorMsg += "' shadows function with same name";
					reportError(local, errorMsg.c_str());
					break;
				}

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

				// If it's a PTR type, also store which struct type it is (if any)
				if (varType == StackValueType::PTR && !structTypeStack.empty()) {
					std::string structType = structTypeStack.back();
					structTypeStack.pop_back();
					mLocalVariableStructTypes[varName] = structType;
				} else if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}
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
					// If it's a PTR, also push its struct type (if any)
					if (localIt->second == StackValueType::PTR) {
						auto structTypeIt = mLocalVariableStructTypes.find(name);
						if (structTypeIt != mLocalVariableStructTypes.end()) {
							structTypeStack.push_back(structTypeIt->second);
						} else {
							structTypeStack.push_back(""); // Unknown struct type
						}
					} else {
						structTypeStack.push_back("");
					}
					break;
				}

				// Check if it's a constant
				auto constIt = mConstantValues.find(name);
				if (constIt != mConstantValues.end()) {
					// Push the constant's type onto the stack
					StackValueType constType = getConstantType(constIt->second);
					typeStack.push_back(constType);
					structTypeStack.push_back("");
					break;
				}

				// Check if it's a struct construction
				if (mDefinedStructs.find(name) != mDefinedStructs.end()) {
					// Struct construction: consumes field values, produces pointer
					auto structDeclIt = mStructDeclarations.find(name);
					if (structDeclIt != mStructDeclarations.end()) {
						AstNodeStructDeclaration* structDecl = structDeclIt->second;
						const auto& fields = structDecl->fields();
						size_t fieldCount = fields.size();

						// Check if we have enough values on the stack
						if (typeStack.size() < fieldCount) {
							std::string errorMsg = "Type error in struct construction '";
							errorMsg += name;
							errorMsg += "': Stack underflow (requires ";
							errorMsg += std::to_string(fieldCount);
							errorMsg += " values, have ";
							errorMsg += std::to_string(typeStack.size());
							errorMsg += ")";
							reportError(ident, errorMsg.c_str());
						} else {
							// Validate each field type (fields are in declaration order)
							// Fields are consumed bottom-to-top from the stack
							for (size_t fi = 0; fi < fieldCount; fi++) {
								size_t stackIdx = typeStack.size() - fieldCount + fi;
								StackValueType actual = typeStack[stackIdx];
								std::string actualStructType =
										(stackIdx < structTypeStack.size()) ? structTypeStack[stackIdx] : "";
								const AstNodeStructField* field = fields[fi];
								const std::string& fieldName = field->name();

								// Get expected type from mStructFieldTypes
								auto structFieldTypesIt = mStructFieldTypes.find(name);
								if (structFieldTypesIt == mStructFieldTypes.end()) {
									continue;
								}
								auto fieldTypeIt = structFieldTypesIt->second.find(fieldName);
								if (fieldTypeIt == structFieldTypesIt->second.end()) {
									continue;
								}
								StackValueType expected = fieldTypeIt->second;

								// Check if this field expects a specific struct type
								std::string expectedStructType;
								auto structFieldStructTypesIt = mStructFieldStructTypes.find(name);
								if (structFieldStructTypesIt != mStructFieldStructTypes.end()) {
									auto fieldStructTypeIt = structFieldStructTypesIt->second.find(fieldName);
									if (fieldStructTypeIt != structFieldStructTypesIt->second.end()) {
										expectedStructType = fieldStructTypeIt->second;
									}
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
										std::string warnMsg = "Implicit cast in struct construction '";
										warnMsg += name;
										warnMsg += "': Field '";
										warnMsg += fieldName;
										warnMsg += "' expects ";
										warnMsg += stackValueTypeToString(expected);
										warnMsg += ", but got ";
										warnMsg += stackValueTypeToString(actual);
										reportWarning(ident, warnMsg.c_str());
									} else {
										// Type mismatch error
										std::string errorMsg = "Type error in struct construction '";
										errorMsg += name;
										errorMsg += "': Field '";
										errorMsg += fieldName;
										errorMsg += "' expects ";
										errorMsg += expectedStructType.empty() ? stackValueTypeToString(expected)
																			   : expectedStructType;
										errorMsg += ", but got ";
										errorMsg += actualStructType.empty() ? stackValueTypeToString(actual)
																			 : actualStructType;
										reportError(ident, errorMsg.c_str());
									}
								} else if (!expectedStructType.empty() && actualStructType != expectedStructType) {
									// Types match (both PTR) but struct types don't match
									std::string errorMsg = "Type error in struct construction '";
									errorMsg += name;
									errorMsg += "': Field '";
									errorMsg += fieldName;
									errorMsg += "' expects ";
									errorMsg += expectedStructType;
									errorMsg += ", but got ";
									errorMsg += actualStructType.empty() ? "ptr" : actualStructType;
									reportError(ident, errorMsg.c_str());
								}
							}
						}

						// Pop field values from stack
						for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}
					}

					// Struct pointers can now be used:
					// - Stored to variable with -> var
					// - Accessed with @field
					// - As arguments to other struct constructors
					// - Returned on stack
					// The code generator handles all these cases correctly.

					// Push pointer type for the constructed struct, along with its struct type
					typeStack.push_back(StackValueType::PTR);
					structTypeStack.push_back(name); // Track which struct type this is
					break;
				}

				// Check if it's a struct from an imported module
				for (const auto& moduleEntry : mModuleStructs) {
					const auto& structs = moduleEntry.second;
					if (structs.find(name) != structs.end() && structs.at(name)) {
						// Struct construction from module
						// Try to find struct declaration
						auto structDeclIt = mModuleStructDeclarations.find(name);
						if (structDeclIt != mModuleStructDeclarations.end()) {
							AstNodeStructDeclaration* structDecl = structDeclIt->second;
							const auto& fields = structDecl->fields();
							size_t fieldCount = fields.size();

							// Check if we have enough values on the stack
							if (typeStack.size() < fieldCount) {
								std::string errorMsg = "Type error in struct construction '";
								errorMsg += name;
								errorMsg += "': Stack underflow (requires ";
								errorMsg += std::to_string(fieldCount);
								errorMsg += " values, have ";
								errorMsg += std::to_string(typeStack.size());
								errorMsg += ")";
								reportError(ident, errorMsg.c_str());
							} else {
								// Validate each field type (fields are in declaration order)
								// Fields are consumed bottom-to-top from the stack
								for (size_t fi = 0; fi < fieldCount; fi++) {
									size_t stackIdx = typeStack.size() - fieldCount + fi;
									StackValueType actual = typeStack[stackIdx];
									const AstNodeStructField* field = fields[fi];
									const std::string& fieldName = field->name();

									// Get expected type from mStructFieldTypes
									auto structFieldTypesIt = mStructFieldTypes.find(name);
									if (structFieldTypesIt == mStructFieldTypes.end()) {
										continue;
									}
									auto fieldTypeIt = structFieldTypesIt->second.find(fieldName);
									if (fieldTypeIt == structFieldTypesIt->second.end()) {
										continue;
									}
									StackValueType expected = fieldTypeIt->second;

									// Skip check if actual type is UNKNOWN (can't determine type)
									if (actual == StackValueType::UNKNOWN) {
										continue;
									}

									// Check for type mismatch
									if (actual != expected) {
										// Check if implicit cast is allowed (int <-> float)
										if (isImplicitCastAllowed(actual, expected)) {
											// Warn about implicit cast
											std::string warnMsg = "Implicit cast in struct construction '";
											warnMsg += name;
											warnMsg += "': Field '";
											warnMsg += fieldName;
											warnMsg += "' expects ";
											warnMsg += stackValueTypeToString(expected);
											warnMsg += ", but got ";
											warnMsg += stackValueTypeToString(actual);
											reportWarning(ident, warnMsg.c_str());
										} else {
											// Type mismatch error
											std::string errorMsg = "Type error in struct construction '";
											errorMsg += name;
											errorMsg += "': Field '";
											errorMsg += fieldName;
											errorMsg += "' expects ";
											errorMsg += stackValueTypeToString(expected);
											errorMsg += ", but got ";
											errorMsg += stackValueTypeToString(actual);
											reportError(ident, errorMsg.c_str());
										}
									}
								}
							}

							// Pop field values from stack
							for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
								typeStack.pop_back();
								if (!structTypeStack.empty()) {
									structTypeStack.pop_back();
								}
							}
						}

						// Struct pointers can now be used flexibly - codegen handles all cases

						typeStack.push_back(StackValueType::PTR);
						structTypeStack.push_back(name); // Track which struct type this is
						break;
					}
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

					// Validate struct field requirements for PTR parameters
					if (!sig.parameterFieldAccess.empty()) {
						// Build a mapping from struct type name to required fields
						// by matching parameterStructTypes to parameterFieldAccess
						std::unordered_map<std::string, const std::unordered_map<std::string, StackValueType>*>
								structTypeToRequiredFields;
						for (const auto& paramField : sig.parameterFieldAccess) {
							// Find the struct type for this parameter by scanning parameterStructTypes
							for (const auto& pst : sig.parameterStructTypes) {
								// We need to match by name somehow - check if the parameter name matches
								// Since we don't have direct name->index, we'll use struct type matching
								const std::string& expectedStructType = pst.second;
								// If this struct type hasn't been assigned required fields yet, assign them
								if (structTypeToRequiredFields.find(expectedStructType) ==
										structTypeToRequiredFields.end()) {
									// Check if this param's field accesses match this struct type's fields
									auto structFieldIt = mStructFieldTypes.find(expectedStructType);
									if (structFieldIt != mStructFieldTypes.end()) {
										const auto& availableFields = structFieldIt->second;
										bool allFieldsMatch = true;
										for (const auto& reqField : paramField.second) {
											if (availableFields.find(reqField.first) == availableFields.end()) {
												allFieldsMatch = false;
												break;
											}
										}
										if (allFieldsMatch && !paramField.second.empty()) {
											structTypeToRequiredFields[expectedStructType] = &paramField.second;
										}
									}
								}
							}
						}

						// Validate each PTR parameter on the stack
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							if (sig.consumes[j] == StackValueType::PTR) {
								size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
								std::string actualStructType = "";
								if (stackIdx < structTypeStack.size()) {
									actualStructType = structTypeStack[stackIdx];
								}

								// Get expected struct type for this parameter index
								std::string expectedStructType = "";
								auto pstIt = sig.parameterStructTypes.find(j);
								if (pstIt != sig.parameterStructTypes.end()) {
									expectedStructType = pstIt->second;
								}

								// Validate struct type matches expectation
								if (!expectedStructType.empty() && !actualStructType.empty() &&
										actualStructType != expectedStructType) {
									std::string errorMsg = "Type error in function call '";
									errorMsg += name;
									errorMsg += "': Parameter ";
									errorMsg += std::to_string(j + 1);
									errorMsg += " expects struct '";
									errorMsg += expectedStructType;
									errorMsg += "', but got '";
									errorMsg += actualStructType;
									errorMsg += "'";
									reportError(ident, errorMsg.c_str());
								}

								// Validate field requirements for this specific struct type
								if (!actualStructType.empty()) {
									auto reqFieldsIt = structTypeToRequiredFields.find(actualStructType);
									if (reqFieldsIt != structTypeToRequiredFields.end()) {
										const auto& requiredFields = *reqFieldsIt->second;
										auto structFieldIt = mStructFieldTypes.find(actualStructType);
										if (structFieldIt != mStructFieldTypes.end()) {
											const auto& availableFields = structFieldIt->second;

											for (const auto& requiredFieldEntry : requiredFields) {
												const std::string& requiredField = requiredFieldEntry.first;
												StackValueType expectedType = requiredFieldEntry.second;

												auto fieldIt = availableFields.find(requiredField);
												if (fieldIt == availableFields.end()) {
													std::string errorMsg = "Type error in function call '";
													errorMsg += name;
													errorMsg += "': Struct '";
													errorMsg += actualStructType;
													errorMsg += "' is missing required field '";
													errorMsg += requiredField;
													errorMsg += "'";
													reportError(ident, errorMsg.c_str());
												} else {
													StackValueType actualType = fieldIt->second;
													if (actualType != expectedType &&
															expectedType != StackValueType::UNKNOWN &&
															actualType != StackValueType::UNKNOWN) {
														if (!isImplicitCastAllowed(actualType, expectedType)) {
															std::string errorMsg = "Type error in function call '";
															errorMsg += name;
															errorMsg += "': Field '";
															errorMsg += requiredField;
															errorMsg += "' in struct '";
															errorMsg += actualStructType;
															errorMsg += "' has type ";
															errorMsg += stackValueTypeToString(actualType);
															errorMsg += ", but function expects ";
															errorMsg += stackValueTypeToString(expectedType);
															reportError(ident, errorMsg.c_str());
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}

					// Save consumed struct types before popping (for pass-through tracking)
					std::vector<std::string> consumedStructTypes;
					if (sig.consumes.size() > 0 && structTypeStack.size() >= sig.consumes.size()) {
						// Save struct types in order (bottom to top of consumed portion)
						size_t startIdx = structTypeStack.size() - sig.consumes.size();
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							consumedStructTypes.push_back(structTypeStack[startIdx + j]);
						}
					}

					// Validate struct types match expected parameter types
					for (size_t paramIdx = 0; paramIdx < sig.consumes.size(); paramIdx++) {
						if (sig.consumes[paramIdx] != StackValueType::PTR) {
							continue;
						}

						// Check if this parameter has an expected struct type
						auto expectedTypeIt = sig.parameterStructTypes.find(paramIdx);
						if (expectedTypeIt == sig.parameterStructTypes.end()) {
							continue; // No specific struct type required
						}

						const std::string& expectedStruct = expectedTypeIt->second;

						// Get the actual struct type being passed
						// Parameters are in declaration order, which matches stack order (bottom to top)
						if (paramIdx < consumedStructTypes.size()) {
							const std::string& actualStruct = consumedStructTypes[paramIdx];

							if (!actualStruct.empty() && actualStruct != expectedStruct) {
								std::string errorMsg = "Type error in function '";
								errorMsg += name;
								errorMsg += "': Parameter ";
								errorMsg += std::to_string(paramIdx + 1);
								errorMsg += " expects struct type '";
								errorMsg += expectedStruct;
								errorMsg += "' but got '";
								errorMsg += actualStruct;
								errorMsg += "'";
								reportError(ident, errorMsg.c_str());
							}
						}
					}

					// Consume the parameters from the stack
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}

					// Helper lambda to determine struct type for a produced PTR value
					auto getProducedStructType = [&](size_t produceIdx, StackValueType producedType) -> std::string {
						if (producedType != StackValueType::PTR) {
							return ""; // Only PTR types can have struct types
						}
						// Conservative heuristic: if we consumed PTR parameters and are producing PTR values,
						// try to match them up in order (simple pass-through case)
						size_t consumedPtrCount = 0;
						size_t producedPtrCount = 0;
						for (size_t ci = 0; ci < sig.consumes.size(); ci++) {
							if (sig.consumes[ci] == StackValueType::PTR) {
								consumedPtrCount++;
							}
						}
						for (size_t pi = 0; pi < sig.produces.size(); pi++) {
							if (sig.produces[pi] == StackValueType::PTR) {
								if (pi == produceIdx) {
									// This is the PTR we're producing - find which PTR index it is
									break;
								}
								producedPtrCount++;
							}
						}
						// If we have at least as many consumed PTR params as produced PTR values,
						// try to match them up in order
						if (producedPtrCount < consumedPtrCount && producedPtrCount < consumedStructTypes.size()) {
							// Find the Nth consumed PTR parameter
							size_t ptrIdx = 0;
							for (size_t mi = 0; mi < sig.consumes.size() && mi < consumedStructTypes.size(); mi++) {
								if (sig.consumes[mi] == StackValueType::PTR) {
									if (ptrIdx == producedPtrCount) {
										return consumedStructTypes[mi];
									}
									ptrIdx++;
								}
							}
						}
						return ""; // Unknown struct type
					};

					// Apply the produces effect
					if (ident->checkError()) {
						// func? - immediately check error
						// Produces: value (untainted) + error_status (INT)
						for (size_t idx = 0; idx < sig.produces.size(); idx++) {
							const auto& type = sig.produces[idx];
							typeStack.push_back(type); // Push untainted value
							structTypeStack.push_back(getProducedStructType(idx, type));
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					} else if (sig.throws && !ident->abortOnError()) {
						// func without ! or ? - pushes result + error flag
						for (size_t idx = 0; idx < sig.produces.size(); idx++) {
							const auto& type = sig.produces[idx];
							typeStack.push_back(type);
							structTypeStack.push_back(getProducedStructType(idx, type));
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					} else {
						// Normal call or func!
						for (size_t idx = 0; idx < sig.produces.size(); idx++) {
							const auto& type = sig.produces[idx];
							typeStack.push_back(type);
							structTypeStack.push_back(getProducedStructType(idx, type));
						}
					}
				}
				// If it's not a user function, it must be a built-in (already validated in pass 2)
				// Built-ins are handled as Instructions, not Identifiers in the AST
				break;
			}

			case IAstNode::Type::FIELD_ACCESS: {
				// Field access: varName @fieldName - pushes the field value onto stack
				AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(child);
				const std::string& varName = fieldAccess->varName();
				const std::string& fieldName = fieldAccess->fieldName();

				// Check if varName is a struct type name (inline struct field access)
				// e.g., "100 200 IntPair @x" - IntPair is a struct type, not a variable
				// We distinguish inline construction from accessing an existing struct by checking:
				// - If structTypeStack.back() == varName, the struct is already constructed (no pop)
				// - Otherwise, the field values are on the stack waiting for construction (pop them)
				auto inlineStructIt = mStructFieldTypes.find(varName);
				if (inlineStructIt != mStructFieldTypes.end()) {
					const auto& fields = inlineStructIt->second;
					bool isExistingStruct =
							!structTypeStack.empty() && structTypeStack.back() == varName;

					if (!isExistingStruct) {
						// This is inline struct field access - pop struct field values from stacks
						size_t fieldCount = fields.size();
						for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}
					} else {
						// Accessing an existing struct - pop just the struct pointer
						typeStack.pop_back();
						structTypeStack.pop_back();
					}

					// Find the field type
					auto fieldIt = fields.find(fieldName);
					if (fieldIt != fields.end()) {
						StackValueType fieldType = fieldIt->second;
						typeStack.push_back(fieldType);

						// Check if field is a struct type
						auto fieldStructIt = mStructFieldStructTypes.find(varName);
						if (fieldStructIt != mStructFieldStructTypes.end()) {
							auto structNameIt = fieldStructIt->second.find(fieldName);
							if (structNameIt != fieldStructIt->second.end()) {
								structTypeStack.push_back(structNameIt->second);
							} else {
								structTypeStack.push_back("");
							}
						} else {
							structTypeStack.push_back("");
						}
					} else {
						std::string errorMsg = "Type error in field access '";
						errorMsg += varName;
						errorMsg += " @";
						errorMsg += fieldName;
						errorMsg += "': Struct '";
						errorMsg += varName;
						errorMsg += "' has no field named '";
						errorMsg += fieldName;
						errorMsg += "'";
						reportError(fieldAccess, errorMsg.c_str());
						typeStack.push_back(StackValueType::ANY);
						structTypeStack.push_back("");
					}
					break;
				}

				// Look up which struct type this variable holds
				std::string structType = "";
				auto structTypeIt = mLocalVariableStructTypes.find(varName);
				if (structTypeIt != mLocalVariableStructTypes.end()) {
					structType = structTypeIt->second;
				}

				// Validate the field exists on this struct type
				StackValueType fieldType = StackValueType::UNKNOWN;
				bool fieldFound = false;

				if (!structType.empty()) {
					// We know which struct type this is - validate against it
					auto structFieldIt = mStructFieldTypes.find(structType);
					if (structFieldIt != mStructFieldTypes.end()) {
						const auto& fields = structFieldIt->second;
						auto fieldIt = fields.find(fieldName);
						if (fieldIt != fields.end()) {
							fieldType = fieldIt->second;
							fieldFound = true;
						} else {
							// Field doesn't exist on this struct type!
							std::string errorMsg = "Type error in field access '";
							errorMsg += varName;
							errorMsg += " @";
							errorMsg += fieldName;
							errorMsg += "': Struct '";
							errorMsg += structType;
							errorMsg += "' has no field named '";
							errorMsg += fieldName;
							errorMsg += "'";
							reportError(fieldAccess, errorMsg.c_str());
							fieldType = StackValueType::ANY; // Continue with ANY to avoid cascading errors
						}
					}
				} else {
					// Variable is not a known struct type
					// Check if variable is definitely a scalar type (not a struct)
					auto localIt = localVariables.find(varName);
					if (localIt != localVariables.end() &&
							(localIt->second == StackValueType::INT || localIt->second == StackValueType::FLOAT ||
									localIt->second == StackValueType::STRING)) {
						// Variable is definitely a scalar type - this is an error
						std::string errorMsg = "Type error in field access '";
						errorMsg += varName;
						errorMsg += " @";
						errorMsg += fieldName;
						errorMsg += "': Variable '";
						errorMsg += varName;
						errorMsg += "' is not a struct type";
						reportError(fieldAccess, errorMsg.c_str());
						fieldType = StackValueType::ANY; // Continue with ANY to avoid cascading errors
					} else {
						// Variable is either a pointer or unknown - search all structs (for parameters/pointers)
						for (const auto& structEntry : mStructFieldTypes) {
							const auto& fields = structEntry.second;
							auto it = fields.find(fieldName);
							if (it != fields.end()) {
								fieldType = it->second;
								fieldFound = true;
								break;
							}
						}

						if (!fieldFound) {
							std::string errorMsg = "Type error in field access '";
							errorMsg += varName;
							errorMsg += " @";
							errorMsg += fieldName;
							errorMsg += "': Unknown variable or field";
							reportError(fieldAccess, errorMsg.c_str());
							fieldType = StackValueType::ANY;
						}
					}
				}

				// Push the field type onto the stack
				typeStack.push_back(fieldType);
				structTypeStack.push_back(""); // Field values are not struct pointers
				break;
			}

			case IAstNode::Type::SCOPED_IDENTIFIER: {
				// Handle module constants, structs, or function calls
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
						structTypeStack.push_back("");
						break;
					}
				}

				// Check if this is a struct construction
				auto moduleStructsIt = mModuleStructs.find(moduleName);
				if (moduleStructsIt != mModuleStructs.end()) {
					const auto& structs = moduleStructsIt->second;
					auto structIt = structs.find(functionName);
					if (structIt != structs.end() && structIt->second) {
						// This is a struct construction from a module
						// Use mStructFieldTypes and mStructFieldOrder since the AST node may be invalid
						auto structFieldTypesIt = mStructFieldTypes.find(functionName);
						auto structFieldOrderIt = mStructFieldOrder.find(functionName);

						if (structFieldTypesIt != mStructFieldTypes.end() &&
								structFieldOrderIt != mStructFieldOrder.end()) {
							const auto& fieldTypes = structFieldTypesIt->second;
							const auto& fieldOrder = structFieldOrderIt->second;
							size_t fieldCount = fieldOrder.size();

							// Check if we have enough values on the stack
							if (typeStack.size() < fieldCount) {
								std::string errorMsg = "Type error in struct construction '";
								errorMsg += qualifiedName;
								errorMsg += "': Stack underflow (requires ";
								errorMsg += std::to_string(fieldCount);
								errorMsg += " values, have ";
								errorMsg += std::to_string(typeStack.size());
								errorMsg += ")";
								reportError(scoped, errorMsg.c_str());
							} else {
								// Validate each field type (fields are in declaration order)
								// Fields are consumed bottom-to-top from the stack
								for (size_t fi = 0; fi < fieldOrder.size(); fi++) {
									size_t stackIdx = typeStack.size() - fieldOrder.size() + fi;
									StackValueType actual = typeStack[stackIdx];
									const std::string& fieldName = fieldOrder[fi];

									// Get expected type from fieldTypes
									auto fieldTypeIt = fieldTypes.find(fieldName);
									if (fieldTypeIt == fieldTypes.end()) {
										continue;
									}
									StackValueType expected = fieldTypeIt->second;

									// Skip check if actual type is UNKNOWN (can't determine type)
									if (actual == StackValueType::UNKNOWN) {
										continue;
									}

									// Check for type mismatch
									if (actual != expected) {
										// Check if implicit cast is allowed (int <-> float)
										if (isImplicitCastAllowed(actual, expected)) {
											// Warn about implicit cast
											std::string warnMsg = "Implicit cast in struct construction '";
											warnMsg += qualifiedName;
											warnMsg += "': Field '";
											warnMsg += fieldName;
											warnMsg += "' expects ";
											warnMsg += stackValueTypeToString(expected);
											warnMsg += ", but got ";
											warnMsg += stackValueTypeToString(actual);
											reportWarning(scoped, warnMsg.c_str());
										} else {
											// Type mismatch error
											std::string errorMsg = "Type error in struct construction '";
											errorMsg += qualifiedName;
											errorMsg += "': Field '";
											errorMsg += fieldName;
											errorMsg += "' expects ";
											errorMsg += stackValueTypeToString(expected);
											errorMsg += ", but got ";
											errorMsg += stackValueTypeToString(actual);
											reportError(scoped, errorMsg.c_str());
										}
									}
								}
							}

							// Pop field values from stack
							for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
								typeStack.pop_back();
								if (!structTypeStack.empty()) {
									structTypeStack.pop_back();
								}
							}
						}

						// Struct pointers can now be used flexibly - codegen handles all cases

						// Push pointer type for the constructed struct, with qualified name as type
						typeStack.push_back(StackValueType::PTR);
						structTypeStack.push_back(functionName); // Track the struct type (bare name)
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
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}

					// Apply the produces effect
					if (scoped->checkError()) {
						// func? - immediately check error
						// Produces: value (untainted) + error_status (INT)
						for (const auto& type : sig.produces) {
							typeStack.push_back(type);	   // Push untainted value
							structTypeStack.push_back(""); // TODO: Track struct types through function calls
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					} else if (sig.throws && !scoped->abortOnError()) {
						// func without ! or ? - pushes result + error flag
						for (const auto& type : sig.produces) {
							typeStack.push_back(type);
							structTypeStack.push_back(""); // TODO: Track struct types through function calls
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					} else {
						// Normal call or func!
						for (const auto& type : sig.produces) {
							typeStack.push_back(type);
							structTypeStack.push_back(""); // TODO: Track struct types through function calls
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

	void SemanticValidator::typeCheckInstruction(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		typeCheckInstructionInternal(node, name, typeStack, structTypeStack, true);
	}

	void SemanticValidator::typeCheckInstructionInternal(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack, bool reportErrors) {
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

		// error instruction: ( msg code -- ) sets error flag
		if (strcmp(name, "error") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(node, "Type error in 'error': Stack underflow (requires msg and code)", reportErrors);
				return;
			}
			// Pop msg and code
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
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
		// Comparison operations: eq, neq, lt, gt, lte, gte (consume 2, produce int/bool)
		else if (strcmp(name, "eq") == 0 || strcmp(name, "neq") == 0 || strcmp(name, "lt") == 0 ||
				 strcmp(name, "gt") == 0 || strcmp(name, "lte") == 0 || strcmp(name, "gte") == 0) {
			if (typeStack.size() < 2) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 2 values)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop both operands
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			// Push result (always int/bool)
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
		}
		// Logical operations: and, or (consume 2 bools, produce int/bool)
		else if (strcmp(name, "and") == 0 || strcmp(name, "or") == 0) {
			if (typeStack.size() < 2) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 2 values)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop both operands
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			// Push result (always int/bool)
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
		}
		// Logical negation: not (consume 1, produce int/bool)
		else if (strcmp(name, "not") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'not': Stack underflow (requires 1 value)", reportErrors);
				return;
			}
			// Pop operand, push result (type stays int)
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
		}
		// Negation: neg (preserve numeric type)
		else if (strcmp(name, "neg") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'neg': Stack underflow (requires 1 numeric value)", reportErrors);
				return;
			}
			// Type stays the same
		}
		// Modulo: mod (consume 2 ints, produce int)
		else if (strcmp(name, "mod") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(node, "Type error in 'mod': Stack underflow (requires 2 integer values)", reportErrors);
				return;
			}
			// Pop both operands
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			// Push result (always int)
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
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
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
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
			// Duplicate struct type as well
			if (!structTypeStack.empty()) {
				std::string topStruct = structTypeStack.back();
				structTypeStack.push_back(topStruct);
			}
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
			// Duplicate struct types as well
			if (structTypeStack.size() >= 2) {
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				std::string topStruct = structTypeStack.back();
				structTypeStack.push_back(secondStruct);
				structTypeStack.push_back(topStruct);
			}
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
			// Duplicate struct types as well
			if (structTypeStack.size() >= 2) {
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				std::string topStruct = structTypeStack.back();
				structTypeStack.push_back(secondStruct);
				structTypeStack.push_back(topStruct);
			}
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
			// Swap struct types as well
			if (structTypeStack.size() >= 3) {
				std::string thirdStruct = structTypeStack[structTypeStack.size() - 3];
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				std::string topStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.push_back(secondStruct);
				structTypeStack.push_back(thirdStruct);
				structTypeStack.push_back(topStruct);
			}
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
			// Copy struct type as well
			if (structTypeStack.size() >= 3) {
				std::string thirdStruct = structTypeStack[structTypeStack.size() - 3];
				structTypeStack.push_back(thirdStruct);
			}
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
			// Remove second struct type as well
			if (structTypeStack.size() >= 3) {
				std::string topStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.push_back(topStruct);
			}
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
			// Swap struct types as well
			if (structTypeStack.size() >= 2) {
				std::string aStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string bStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.push_back(aStruct);
				structTypeStack.push_back(bStruct);
			}
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
			// Copy struct type as well
			if (structTypeStack.size() >= 2) {
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				structTypeStack.push_back(secondStruct);
			}
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
			// Remove second struct type as well
			if (structTypeStack.size() >= 2) {
				std::string topStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.push_back(topStruct);
			}
		}
		// Stack operations: drop ( a -- )
		else if (strcmp(name, "drop") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'drop': Stack underflow (requires 1 value)", reportErrors);
				return;
			}
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
		}
		// Stack operations: drop2 ( a b -- )
		else if (strcmp(name, "drop2") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(node, "Type error in 'drop2': Stack underflow (requires 2 values)", reportErrors);
				return;
			}
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
		}
		// Stack operations: rot ( a b c -- b c a )
		else if (strcmp(name, "rot") == 0) {
			if (typeStack.size() < 3) {
				reportErrorConditional(node, "Type error in 'rot': Stack underflow (requires 3 values)", reportErrors);
				return;
			}
			StackValueType c = typeStack.back();
			typeStack.pop_back();
			StackValueType b = typeStack.back();
			typeStack.pop_back();
			StackValueType a = typeStack.back();
			typeStack.pop_back();
			typeStack.push_back(b);
			typeStack.push_back(c);
			typeStack.push_back(a);
			// Handle struct type stack
			if (structTypeStack.size() >= 3) {
				std::string cStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string bStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string aStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.push_back(bStruct);
				structTypeStack.push_back(cStruct);
				structTypeStack.push_back(aStruct);
			}
		}
		// Stack operations: tuck ( a b -- b a b )
		else if (strcmp(name, "tuck") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(node, "Type error in 'tuck': Stack underflow (requires 2 values)", reportErrors);
				return;
			}
			StackValueType b = typeStack.back();
			typeStack.pop_back();
			StackValueType a = typeStack.back();
			typeStack.pop_back();
			typeStack.push_back(b);
			typeStack.push_back(a);
			typeStack.push_back(b);
			// Handle struct type stack
			if (structTypeStack.size() >= 2) {
				std::string bStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string aStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.push_back(bStruct);
				structTypeStack.push_back(aStruct);
				structTypeStack.push_back(bStruct);
			}
		}
		// Stack operations: clear (empties the entire stack)
		else if (strcmp(name, "clear") == 0) {
			// Clear all elements from the type stack
			typeStack.clear();
			// Clear struct type stack as well
			structTypeStack.clear();
		}
		// Stack operations: depth (pushes the current stack depth as an integer)
		else if (strcmp(name, "depth") == 0) {
			// Push an int type onto the stack (depth is always an integer)
			typeStack.push_back(StackValueType::INT);
			// Push empty struct type (int is not a struct)
			structTypeStack.push_back("");
		}
		// call - invoke function pointer from stack
		else if (strcmp(name, "call") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'call': Stack underflow (requires 1 value)", reportErrors);
				return;
			}
			// Pop the function pointer - runtime will verify it's a pointer type
			typeStack.pop_back();
			// Pop struct type as well
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			// We don't know what the called function will do to the stack
			// So we can't track types accurately after this point
		}
		// free - deallocate memory pointed to by a pointer
		else if (strcmp(name, "free") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(
						node, "Type error in 'free': Stack underflow (requires 1 pointer)", reportErrors);
				return;
			}
			StackValueType top = typeStack.back();
			if (top != StackValueType::PTR) {
				std::string errorMsg = "Type error in 'free': Expected pointer type, got ";
				errorMsg += typeToString(top);
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop the pointer
			typeStack.pop_back();
			// Pop struct type as well
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
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

	StackValueType SemanticValidator::stringToStackValueType(const std::string& typeStr) {
		if (typeStr == "i64") {
			return StackValueType::INT;
		}
		if (typeStr == "f64") {
			return StackValueType::FLOAT;
		}
		if (typeStr == "str") {
			return StackValueType::STRING;
		}
		if (typeStr == "ptr" || typeStr.find('*') != std::string::npos) {
			return StackValueType::PTR;
		}
		return StackValueType::ANY;
	}

	std::string SemanticValidator::findStructTypeByFields(
			const std::unordered_map<std::string, StackValueType>& accessedFields) {
		if (accessedFields.empty()) {
			return "";
		}

		std::string matchingStruct;

		// Check all known struct definitions
		for (const auto& structPair : mStructFieldTypes) {
			const std::string& structName = structPair.first;
			const auto& structFields = structPair.second;

			bool allFieldsMatch = true;

			// Check if this struct has all the accessed fields (by name only, since types may be ambiguous)
			for (const auto& accessPair : accessedFields) {
				const std::string& fieldName = accessPair.first;

				// Find field in struct definition
				auto fieldIt = structFields.find(fieldName);

				if (fieldIt == structFields.end()) {
					// Field doesn't exist in this struct
					allFieldsMatch = false;
					break;
				}
			}

			if (allFieldsMatch) {
				if (matchingStruct.empty()) {
					matchingStruct = structName;
				} else {
					// Multiple structs match - ambiguous, return empty
					return "";
				}
			}
		}

		return matchingStruct;
	}

} // namespace Qd

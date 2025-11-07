#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_use.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <sstream>

namespace Qd {

	// List of built-in instructions (must match ast.cc)
	static const char* BUILTIN_INSTRUCTIONS[] = {"%", "*", "+", "-", ".", "/", "abs", "acos", "add", "and", "asin",
			"atan", "cb", "cbrt", "ceil", "call", "clear", "cos", "dec", "depth", "div", "drop", "drop2", "dup", "dup2",
			"eq", "error", "fac", "floor", "gt", "gte", "inc", "inv", "ln", "log10", "lshift", "lt", "lte", "max",
			"min", "mod", "mul", "neq", "neg", "nip", "not", "or", "over", "over2", "pick", "pow", "print", "prints",
			"printsv", "printv", "roll", "rot", "round", "rshift", "sin", "sq", "sqrt", "sub", "swap", "swap2", "tan",
			"tuck", "within", "xor"};

	SemanticValidator::SemanticValidator() : mFilename(nullptr), mErrorCount(0) {
	}

	bool SemanticValidator::isBuiltInInstruction(const char* name) const {
		static const size_t count = sizeof(BUILTIN_INSTRUCTIONS) / sizeof(BUILTIN_INSTRUCTIONS[0]);
		for (size_t i = 0; i < count; i++) {
			if (strcmp(name, BUILTIN_INSTRUCTIONS[i]) == 0) {
				return true;
			}
		}
		return false;
	}

	void SemanticValidator::reportError(const char* message) {
		reportErrorConditional(message, true);
	}

	void SemanticValidator::reportError(IAstNode* node, const char* message) {
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

	void SemanticValidator::reportErrorConditional(IAstNode* node, const char* message, bool shouldReport) {
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

	size_t SemanticValidator::validate(IAstNode* program, const char* filename) {
		mErrorCount = 0;
		mFilename = filename;
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
		bool signaturesChanged = true;
		int iteration = 0;
		const int maxIterations = 100; // Prevent infinite loops

		while (signaturesChanged && iteration < maxIterations) {
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
		if (iteration >= maxIterations) {
			std::cerr << Colors::bold() << Colors::magenta() << "Warning: " << Colors::reset()
					  << "Function signature analysis did not converge after " << maxIterations
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
			mImportedModules.insert(use->module());
			loadModuleDefinitions(use->module(), mCurrentPackage);
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

	void SemanticValidator::loadModuleDefinitions(const std::string& moduleName, const std::string& currentPackage) {
		// Check if we've already loaded this specific file (prevent duplicate loads)
		if (mLoadedModuleFiles.count(moduleName)) {
			return;
		}
		mLoadedModuleFiles.insert(moduleName);

		// Check if this is a direct file import (ends with .qd)
		bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

		std::string effectiveModuleName;
		if (isDirectFile) {
			// For .qd file imports, use the current package name
			effectiveModuleName = currentPackage;
		} else {
			effectiveModuleName = moduleName;
		}

		std::string modulePath;
		std::ifstream file;

		if (isDirectFile) {
			// Direct file import: look for helper.qd in the parent module's directory
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
				// Parse and add to current package namespace
				parseModuleAndCollectFunctions(currentPackage, source);
				return;
			}
			file.close();

			// If not found in same directory, try standard paths
			// Try 2: QUADRATE_ROOT
			const char* quadrateRoot = std::getenv("QUADRATE_ROOT");
			if (quadrateRoot) {
				modulePath = std::string(quadrateRoot) + "/" + moduleName;
				file.open(modulePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					parseModuleAndCollectFunctions(currentPackage, source);
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
					parseModuleAndCollectFunctions(currentPackage, source);
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

			// Try 2: QUADRATE_ROOT environment variable
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

			// Try 3: $HOME/quadrate directory
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

		// Module file doesn't exist anywhere - skip silently
		// This allows using 'use' statements before modules are installed
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
				loadModuleDefinitions(use->module(), moduleName);
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
				std::vector<StackValueType> typeStack;

				// Initialize type stack with input parameters
				for (size_t i = 0; i < func->inputParameters.size(); i++) {
					AstNodeParameter* param = func->inputParameters[i];
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

				// Add output parameters to type stack
				for (size_t i = 0; i < func->outputParameters.size(); i++) {
					AstNodeParameter* param = func->outputParameters[i];
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

				// Store the signature with qualified name: moduleName::functionName
				// This allows imported functions to be called with the module's namespace
				FunctionSignature sig;
				sig.produces = typeStack;
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
		if (!node) {
			return;
		}

		// Check if this is an identifier (function call)
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

			// Check if the function exists in the module
			auto moduleIt = mModuleFunctions.find(scopeName);
			if (moduleIt != mModuleFunctions.end()) {
				const auto& functions = moduleIt->second;
				if (functions.find(functionName) == functions.end()) {
					std::string errorMsg = "Function '";
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

		// Track when we enter a for loop or infinite loop
		bool childrenInsideForLoop = insideForLoop;
		if (node->type() == IAstNode::Type::FOR_STATEMENT || node->type() == IAstNode::Type::LOOP_STATEMENT) {
			childrenInsideForLoop = true;
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			validateReferences(node->child(i), childrenInsideForLoop);
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

			// Analyze the function body in isolation (without resolving function calls)
			if (func->body()) {
				analyzeBlockInIsolation(func->body(), typeStack);
			}

			// Store the signature - for now, assume functions consume nothing
			// and produce whatever is left on the stack
			FunctionSignature sig;
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
				typeCheckBlock(func->body(), typeStack);
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			typeCheckFunction(node->child(i));
		}
	}

	void SemanticValidator::typeCheckBlock(IAstNode* node, std::vector<StackValueType>& typeStack) {
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
				typeCheckBlock(child, typeStack);
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

			case IAstNode::Type::IDENTIFIER: {
				// Handle function calls - apply their stack effect
				AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(child);
				const std::string& name = ident->name();

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
												   "' must be immediately followed by 'if' to check for errors, or use '!' to abort on error";
							reportError(ident, errorMsg.c_str());
						}
					}

					// TODO: In the future, check if stack has enough values for sig.consumes
					// For now, we assume functions consume nothing

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
				// Handle module function calls - apply their stack effect
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);
				const std::string& moduleName = scoped->scope();
				const std::string& functionName = scoped->name();
				std::string qualifiedName = moduleName + "::" + functionName;

				// Look up the module function signature
				auto sigIt = mFunctionSignatures.find(qualifiedName);
				if (sigIt != mFunctionSignatures.end()) {
					const FunctionSignature& sig = sigIt->second;

					// Apply the produces effect
					for (const auto& type : sig.produces) {
						typeStack.push_back(type);
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

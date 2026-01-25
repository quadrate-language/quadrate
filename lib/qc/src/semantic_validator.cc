#include "instructions.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_anonymous_function.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_ctx.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_field_access.h>
#include <qc/ast_node_field_set.h>
#include <qc/ast_node_for.h>
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
#include <qc/ast_node_test.h>
#include <qc/ast_node_use.h>
#include <qc/ast_node_while.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <sstream>
#include <unordered_set>

namespace Qd {

#include "semantic_validator_internal.h"

	SemanticValidator::SemanticValidator()
		: mFilename(nullptr), mErrorCount(0), mWarningCount(0), mWerror(false), mIsModuleFile(false),
		  mStoreErrors(false), mWarningMinLine(0), mCurrentFunctionFallible(false), mInLoopBody(false),
		  mSource(nullptr) {
	}

	const std::unordered_map<std::string, StackValueType>* SemanticValidator::lookupStructFieldTypes(
			const std::string& typeName) const {
		// First try direct lookup (works for local structs and qualified names)
		auto it = mStructFieldTypes.find(typeName);
		if (it != mStructFieldTypes.end()) {
			return &it->second;
		}

		// If the type is unqualified (no ::), try finding it in imported modules
		if (typeName.find("::") == std::string::npos) {
			// Check if any module has this struct
			for (const auto& modulePair : mModuleStructs) {
				const std::string& moduleName = modulePair.first;
				const auto& moduleStructs = modulePair.second;
				if (moduleStructs.find(typeName) != moduleStructs.end()) {
					// Found it in a module, try qualified lookup
					std::string qualifiedName = moduleName + "::" + typeName;
					auto qualIt = mStructFieldTypes.find(qualifiedName);
					if (qualIt != mStructFieldTypes.end()) {
						return &qualIt->second;
					}
				}
			}
		}

		return nullptr;
	}

	// Check if a type string is a known struct name (local or imported)
	// Supports both unqualified names (Response) and qualified names (http::Response)

	bool SemanticValidator::isStructTypeName(const std::string& typeStr) const {
		// Extract base type if generic (e.g., Box<T> -> Box)
		std::string baseName = typeStr;
		size_t anglePos = typeStr.find('<');
		if (anglePos != std::string::npos) {
			baseName = typeStr.substr(0, anglePos);
		}

		// Check for qualified name (module::StructName)
		size_t colonPos = baseName.find("::");
		if (colonPos != std::string::npos) {
			std::string moduleName = baseName.substr(0, colonPos);
			std::string structName = baseName.substr(colonPos + 2);
			// Check if the module exists and has this struct
			auto moduleIt = mModuleStructs.find(moduleName);
			if (moduleIt != mModuleStructs.end()) {
				const auto& structs = moduleIt->second;
				if (structs.find(structName) != structs.end()) {
					return true;
				}
			}
			return false;
		}

		// Check local structs
		if (mDefinedStructs.find(baseName) != mDefinedStructs.end()) {
			return true;
		}
		// Check imported module structs (unqualified name)
		for (const auto& moduleEntry : mModuleStructs) {
			const auto& structs = moduleEntry.second;
			if (structs.find(baseName) != structs.end()) {
				return true;
			}
		}
		return false;
	}

	bool SemanticValidator::isValidTypeName(const std::string& typeStr) const {
		// Primitive types
		if (typeStr == "i64" || typeStr == "f64" || typeStr == "str" || typeStr == "ptr" || typeStr == "any") {
			return true;
		}
		// Type parameters (for generic functions)
		for (const auto& typeParam : mCurrentTypeParams) {
			if (typeStr == typeParam) {
				return true;
			}
		}

		// Check for generic types like Box<T> or Pair<T, U>
		size_t anglePos = typeStr.find('<');
		if (anglePos != std::string::npos) {
			// Extract base type
			std::string baseType = typeStr.substr(0, anglePos);
			// Base type must be a valid struct name
			if (!isStructTypeName(baseType)) {
				return false;
			}
			// Extract and validate type arguments
			size_t closePos = typeStr.rfind('>');
			if (closePos == std::string::npos || closePos <= anglePos) {
				return false;
			}
			std::string typeArgs = typeStr.substr(anglePos + 1, closePos - anglePos - 1);
			// Parse comma-separated type arguments
			size_t start = 0;
			while (start < typeArgs.size()) {
				size_t commaPos = typeArgs.find(',', start);
				std::string arg;
				if (commaPos == std::string::npos) {
					arg = typeArgs.substr(start);
					start = typeArgs.size();
				} else {
					arg = typeArgs.substr(start, commaPos - start);
					start = commaPos + 1;
				}
				// Trim whitespace
				while (!arg.empty() && arg.front() == ' ') {
					arg.erase(0, 1);
				}
				while (!arg.empty() && arg.back() == ' ') {
					arg.pop_back();
				}
				// Each type arg must be a valid type (recursive)
				if (!arg.empty() && !isValidTypeName(arg)) {
					return false;
				}
			}
			return true;
		}

		// Struct types
		return isStructTypeName(typeStr);
	}

	bool SemanticValidator::isCurrentTypeParam(const std::string& typeStr) const {
		// Check if it's an explicit type parameter in the current function
		for (const auto& typeParam : mCurrentTypeParams) {
			if (typeStr == typeParam) {
				return true;
			}
		}

		// Heuristic: if it's a single uppercase letter (or short uppercase name) and NOT a known struct,
		// treat it as a type parameter. This handles struct type params like T, U, V when they
		// appear in generic struct fields but the current function only uses a subset of them.
		if (!typeStr.empty() && typeStr.length() <= 2) {
			bool allUpper = true;
			for (char c : typeStr) {
				if (!std::isupper(static_cast<unsigned char>(c))) {
					allUpper = false;
					break;
				}
			}
			if (allUpper) {
				// It's a short uppercase identifier - check if it's NOT a known struct
				if (mDefinedStructs.find(typeStr) == mDefinedStructs.end()) {
					// Check module structs too
					bool isKnownStruct = false;
					for (const auto& moduleEntry : mModuleStructs) {
						if (moduleEntry.second.find(typeStr) != moduleEntry.second.end()) {
							isKnownStruct = true;
							break;
						}
					}
					if (!isKnownStruct) {
						return true; // Treat as type parameter
					}
				}
			}
		}

		return false;
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

	bool SemanticValidator::isBuiltInInstruction(const char* name) const {
		// Use the extended validator list which includes stdlib imports
		return Qd::isKnownInstruction(name);
	}

	void SemanticValidator::reportError(const char* message) {
		// Suppress errors inside loop bodies (we still analyze for method call marking)
		reportErrorConditional(message, !mInLoopBody);
	}

	void SemanticValidator::reportError(const IAstNode* node, const char* message) {
		// Suppress errors inside loop bodies (we still analyze for method call marking)
		reportErrorConditional(node, message, !mInLoopBody);
	}

	void SemanticValidator::reportErrorWithHint(const IAstNode* node, const char* message, const char* hint) {
		// Suppress errors inside loop bodies (we still analyze for method call marking)
		reportErrorConditionalWithHint(node, message, hint, !mInLoopBody);
	}

	void SemanticValidator::reportErrorConditional(const char* message, bool shouldReport) {
		reportErrorConditional(nullptr, message, shouldReport);
	}

	void SemanticValidator::reportErrorConditional(const IAstNode* node, const char* message, bool shouldReport) {
		if (!shouldReport) {
			return;
		}

		// Create a key for deduplication: line:column:message
		std::string errorKey;
		if (node) {
			errorKey = std::to_string(node->line()) + ":" + std::to_string(node->column()) + ":" + message;
		} else {
			errorKey = std::string("0:0:") + message;
		}

		// Skip if we've already reported this exact error
		if (mReportedErrors.find(errorKey) != mReportedErrors.end()) {
			return;
		}
		mReportedErrors.insert(errorKey);

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

			// Print source context if available
			if (mSource && node) {
				printSourceContext(node->line(), node->column());
			}
		}
	}

	void SemanticValidator::reportErrorConditionalWithHint(
			const IAstNode* node, const char* message, const char* hint, bool shouldReport) {
		if (!shouldReport) {
			return;
		}

		// Create a key for deduplication: line:column:message
		std::string errorKey;
		if (node) {
			errorKey = std::to_string(node->line()) + ":" + std::to_string(node->column()) + ":" + message;
		} else {
			errorKey = std::string("0:0:") + message;
		}

		// Skip if we've already reported this exact error
		if (mReportedErrors.find(errorKey) != mReportedErrors.end()) {
			return;
		}
		mReportedErrors.insert(errorKey);

		mErrorCount++;

		if (mStoreErrors) {
			ErrorInfo err;
			err.line = node ? node->line() : 0;
			err.column = node ? node->column() : 0;
			err.message = message;
			if (hint) {
				err.message += " (hint: ";
				err.message += hint;
				err.message += ")";
			}
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

			// Print source context if available
			if (mSource && node) {
				printSourceContext(node->line(), node->column());
			}

			// Print hint if provided
			if (hint) {
				std::cerr << Colors::cyan() << "  hint:" << Colors::reset() << " " << hint << std::endl;
			}
		}
	}

	void SemanticValidator::printSourceContext(size_t line, size_t column) {
		if (!mSource || line == 0) {
			return;
		}

		// Find the line in the source
		size_t currentLine = 1;
		const char* lineStart = mSource;
		const char* ptr = mSource;

		while (*ptr != '\0' && currentLine < line) {
			if (*ptr == '\n') {
				currentLine++;
				lineStart = ptr + 1;
			}
			ptr++;
		}

		// Find line end
		const char* lineEnd = lineStart;
		while (*lineEnd != '\0' && *lineEnd != '\n') {
			lineEnd++;
		}

		// Print line number gutter and source line
		std::cerr << Colors::cyan() << "    " << line << " | " << Colors::reset();
		std::cerr.write(lineStart, lineEnd - lineStart);
		std::cerr << std::endl;

		// Print caret pointer
		std::cerr << Colors::cyan() << "      | " << Colors::reset();
		for (size_t i = 1; i < column; i++) {
			std::cerr << " ";
		}
		std::cerr << Colors::green() << "^" << Colors::reset() << std::endl;
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

		// Print source context if available
		if (mSource && node) {
			printSourceContext(node->line(), node->column());
		}
		mWarningCount++;
	}

	size_t SemanticValidator::validate(IAstNode* program, const char* filename, bool isModuleFile, bool werror) {
		// Timing helper - only active when QUADC_TIMING is set
		static bool timing = std::getenv("QUADC_TIMING") != nullptr;
		auto timeLast = std::chrono::steady_clock::now();
		auto printTiming = [&](const char* label) {
			if (timing) {
				auto now = std::chrono::steady_clock::now();
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeLast).count();
				std::cerr << "[TIMING]   semantic/" << label << ": " << ms << "ms" << std::endl;
				timeLast = now;
			}
		};

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
		mModuleImportedFunctions.clear();
		mReportedErrors.clear();

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

		// Directory-based namespace: load sibling files as merged modules
		// This makes symbols from sibling files available without explicit imports
		//
		// Two-pass approach to handle dependencies between sibling files:
		// Pass 1: Parse all siblings and collect definitions (structs, functions)
		// Pass 2: Validate function signatures (requires all types to be known)
		static bool debugSibling = std::getenv("QUADC_DEBUG_SIBLING") != nullptr;
		if (debugSibling && !mSiblingFiles.empty()) {
			std::cerr << "[DEBUG SIBLING] Loading " << mSiblingFiles.size() << " sibling files (pass 1: collect definitions)" << std::endl;
		}

		// Pass 1: Parse all siblings and collect definitions first
		// Sort so files with likely struct definitions (shorter names) come first
		std::vector<std::string> sortedSiblings = mSiblingFiles;
		std::sort(sortedSiblings.begin(), sortedSiblings.end(), [](const std::string& a, const std::string& b) {
			// Prefer files named after types (shorter, common patterns)
			std::filesystem::path pa(a), pb(b);
			return pa.stem().string() < pb.stem().string();
		});

		std::vector<std::pair<std::string, IAstNode*>> siblingAsts;
		for (const auto& siblingFile : sortedSiblings) {
			std::filesystem::path p(siblingFile);
			std::string moduleName = p.stem().string();

			if (debugSibling) {
				std::cerr << "[DEBUG SIBLING]   Pass 1: " << siblingFile << " as " << moduleName << std::endl;
			}

			std::ifstream ifs(siblingFile);
			if (ifs) {
				std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

				// Parse and collect definitions only (no signature analysis yet)
				auto moduleAst = std::make_unique<Ast>();
				IAstNode* moduleAstRoot = moduleAst->generate(source.c_str(), false, siblingFile.c_str());
				if (!moduleAstRoot) {
					continue;
				}

				// Track this module as merged
				mMergedModules.insert(moduleName);

				// Process USE statements from sibling files to load imported modules
				// This ensures modules like 'math' are available when analyzing sibling code
				for (auto* child : moduleAstRoot->children()) {
					if (child && child->type() == IAstNode::Type::USE_STATEMENT) {
						AstNodeUse* use = static_cast<AstNodeUse*>(child);
						if (debugSibling) {
							std::cerr << "[DEBUG SIBLING]     Processing use: " << use->module() << std::endl;
						}
						// Load the module if not already loaded
						if (mImportedModules.find(use->module()) == mImportedModules.end()) {
							loadModuleDefinitions(use->module(), moduleName, false);
						}
					}
				}

				// Collect struct definitions first
				std::unordered_map<std::string, bool> moduleStructs;
				collectModuleStructs(moduleAstRoot, moduleStructs);
				if (mModuleStructs.find(moduleName) != mModuleStructs.end()) {
					for (const auto& s : moduleStructs) {
						mModuleStructs[moduleName][s.first] = s.second;
					}
				} else {
					mModuleStructs[moduleName] = moduleStructs;
				}
				for (const auto& s : moduleStructs) {
					mDefinedStructs.insert(s.first);
				}

				// Collect struct field types
				collectModuleStructFieldTypes(moduleAstRoot, moduleName, true);

				// Collect constants
				std::unordered_map<std::string, bool> moduleConstants;
				collectModuleConstants(moduleAstRoot, moduleConstants);
				if (mModuleConstants.find(moduleName) != mModuleConstants.end()) {
					for (const auto& c : moduleConstants) {
						mModuleConstants[moduleName][c.first] = c.second;
					}
				} else {
					mModuleConstants[moduleName] = moduleConstants;
				}
				for (const auto& c : moduleConstants) {
					mDefinedConstants.insert(c.first);
				}
				collectModuleConstantValues(moduleAstRoot, moduleName, true);

				// Collect function definitions
				std::unordered_map<std::string, bool> moduleFunctions;
				collectModuleFunctions(moduleAstRoot, moduleFunctions);
				if (mModuleFunctions.find(moduleName) != mModuleFunctions.end()) {
					for (const auto& f : moduleFunctions) {
						mModuleFunctions[moduleName][f.first] = f.second;
					}
				} else {
					mModuleFunctions[moduleName] = moduleFunctions;
				}
				for (const auto& f : moduleFunctions) {
					mDefinedFunctions.insert(f.first);
				}

				// Collect methods
				collectModuleMethods(moduleAstRoot, moduleName);

				// Cache the AST and source for pass 2 and type checking in pass 3c
				ParsedModuleAst cached;
				cached.ast = std::move(moduleAst);
				cached.root = moduleAstRoot;
				cached.source = source;
				cached.filePath = siblingFile;
				mParsedModuleAsts[siblingFile] = std::move(cached);
				// Add to parse order so type checking happens in Pass 3c
				mParsedModuleOrder.push_back(siblingFile);

				siblingAsts.push_back({moduleName, moduleAstRoot});
			}
		}

		if (debugSibling && !mSiblingFiles.empty()) {
			std::cerr << "[DEBUG SIBLING] After pass 1, mDefinedStructs: ";
			for (const auto& s : mDefinedStructs) {
				std::cerr << s << " ";
			}
			std::cerr << std::endl;
		}

		// Pass 2: Now analyze function signatures (all types are known)
		if (debugSibling && !siblingAsts.empty()) {
			std::cerr << "[DEBUG SIBLING] Pass 2: analyze function signatures" << std::endl;
		}
		for (const auto& [moduleName, moduleAstRoot] : siblingAsts) {
			analyzeModuleFunctionSignatures(moduleAstRoot, moduleName, true);
		}

		// Pass 1: Collect all function definitions
		collectDefinitions(program);
		printTiming("collectDefinitions");

		// Pass 2: Validate all references
		validateReferences(program);
		printTiming("validateReferences");

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
		printTiming("analyzeFunctionSignatures");

		// Pass 3b: Type check using function signatures
		typeCheckFunction(program);

		// Pass 3c: Type check merged module functions
		// Merged modules (local file imports that merge into main namespace) need
		// full type checking so that method calls get properly marked for the code generator.
		// We iterate in parsing order (stored in mParsedModuleOrder) which is dependency-aware:
		// modules that define structs are parsed before modules that use them.
		static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
		for (const auto& filePath : mParsedModuleOrder) {
			// Skip standard library modules (they're in share/quadrate or dist/share/quadrate)
			if (filePath.find("share/quadrate/") != std::string::npos) {
				continue;
			}

			// Extract module name from filepath for checking against mMergedModules
			// E.g., "/path/to/physics.qd" -> "physics"
			std::string moduleName;
			size_t lastSlash = filePath.find_last_of('/');
			size_t nameStart = (lastSlash != std::string::npos) ? lastSlash + 1 : 0;
			if (filePath.size() > nameStart + 3 &&
					filePath.substr(filePath.size() - 3) == ".qd") {
				moduleName = filePath.substr(nameStart, filePath.size() - nameStart - 3);
			}

			// Only type-check modules that are merged into main namespace
			if (!moduleName.empty() && mMergedModules.count(moduleName) > 0) {
				if (debug) {
					std::cerr << "[DEBUG] Type checking merged module: " << moduleName << " (" << filePath << ")"
							  << std::endl;
					// Dump relevant state for debugging
					std::cerr << "[DEBUG]   mDefinedStructs: ";
					for (const auto& s : mDefinedStructs) {
						std::cerr << s << " ";
					}
					std::cerr << std::endl;
					std::cerr << "[DEBUG]   mStructFieldTypes keys: ";
					for (const auto& p : mStructFieldTypes) {
						std::cerr << p.first << " ";
					}
					std::cerr << std::endl;
					std::cerr << "[DEBUG]   mStructFieldStructTypes keys: ";
					for (const auto& p : mStructFieldStructTypes) {
						std::cerr << p.first << "[";
						for (const auto& f : p.second) {
							std::cerr << f.first << ":" << f.second << " ";
						}
						std::cerr << "] ";
					}
					std::cerr << std::endl;
					std::cerr << "[DEBUG]   isStructTypeName(MyState): " << (isStructTypeName("MyState") ? "true" : "false") << std::endl;
					std::cerr << "[DEBUG]   isStructTypeName(math::Vec3): " << (isStructTypeName("math::Vec3") ? "true" : "false") << std::endl;
				}

				// Type check this merged module's AST
				auto astIt = mParsedModuleAsts.find(filePath);
				if (astIt != mParsedModuleAsts.end() && astIt->second.root) {
					typeCheckFunction(astIt->second.root);
				}
			}
		}
		printTiming("typeCheck");

		return mErrorCount;
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
		case StackValueType::TYPEVAR:
			return "typevar";
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
		// Check if this is a type parameter (for generic functions)
		for (const auto& typeParam : mCurrentTypeParams) {
			if (typeStr == typeParam) {
				return StackValueType::TYPEVAR;
			}
		}
		// Check if this is a struct type
		if (isStructTypeName(typeStr)) {
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

	std::string SemanticValidator::findMethodStructType(
			const std::string& concreteType, const std::string& methodName) const {
		// First try direct lookup
		auto it = mStructMethods.find(concreteType);
		if (it != mStructMethods.end() && it->second.count(methodName)) {
			return concreteType;
		}

		// Extract base type (e.g., "Box" from "Box<i64>" or "ct::Box<i64>")
		std::string baseType = concreteType;
		std::string modulePrefix;

		// Check for module prefix
		size_t colonPos = concreteType.find("::");
		size_t anglePos = concreteType.find('<');
		if (colonPos != std::string::npos && (anglePos == std::string::npos || colonPos < anglePos)) {
			modulePrefix = concreteType.substr(0, colonPos + 2);
			baseType = concreteType.substr(colonPos + 2);
		}

		// Remove generic args from base type
		anglePos = baseType.find('<');
		if (anglePos != std::string::npos) {
			baseType = baseType.substr(0, anglePos);
		}

		// Search for generic variants that have this method
		// Look for patterns like "BaseType<T>", "module::BaseType<T>", "BaseType<T, U>", etc.
		// Two-pass search: first look for exact prefix match, then fallback to any match
		std::string fallbackMatch;

		for (const auto& pair : mStructMethods) {
			const std::string& registeredType = pair.first;
			const auto& methods = pair.second;

			// Check if this type has the method
			if (methods.count(methodName) == 0) {
				continue;
			}

			// Check if registeredType matches our base type pattern
			std::string registeredBase = registeredType;
			std::string registeredPrefix;

			// Extract prefix from registered type
			size_t regColonPos = registeredType.find("::");
			size_t regAnglePos = registeredType.find('<');
			if (regColonPos != std::string::npos && (regAnglePos == std::string::npos || regColonPos < regAnglePos)) {
				registeredPrefix = registeredType.substr(0, regColonPos + 2);
				registeredBase = registeredType.substr(regColonPos + 2);
			}

			// Remove generic args from registered base
			regAnglePos = registeredBase.find('<');
			if (regAnglePos != std::string::npos) {
				registeredBase = registeredBase.substr(0, regAnglePos);
			}

			// Check if bases match
			if (registeredBase != baseType) {
				continue;
			}

			// Exact prefix match - return immediately
			if (!modulePrefix.empty() && modulePrefix == registeredPrefix) {
				return registeredType;
			}

			// Fallback: if we have a module prefix but registered doesn't (or vice versa)
			// Store as fallback but keep looking for exact match
			if (fallbackMatch.empty() &&
					(modulePrefix.empty() || registeredPrefix.empty() || modulePrefix == registeredPrefix)) {
				fallbackMatch = registeredType;
			}
		}

		return fallbackMatch;
	}

} // namespace Qd

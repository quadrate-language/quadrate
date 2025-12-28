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
#include <qc/instructions.h>
#include <qc/semantic_validator.h>
#include <sstream>
#include <unordered_set>

namespace Qd {

#include "semantic_validator_internal.h"

	SemanticValidator::SemanticValidator()
		: mFilename(nullptr), mErrorCount(0), mWarningCount(0), mWerror(false), mIsModuleFile(false),
		  mStoreErrors(false), mWarningMinLine(0), mCurrentFunctionFallible(false), mInLoopBody(false) {
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

	void SemanticValidator::reportErrorConditional(const char* message, bool shouldReport) {
		if (!shouldReport) {
			return;
		}

		// Create a key for deduplication
		std::string errorKey = std::string("0:0:") + message;

		// Skip if we've already reported this exact error
		if (mReportedErrors.find(errorKey) != mReportedErrors.end()) {
			return;
		}
		mReportedErrors.insert(errorKey);

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

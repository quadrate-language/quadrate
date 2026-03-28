#include "instructions.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_ctx.h>
#include <quadrate/qc/ast_node_defer.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/ast_node_while.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/semantic_validator.h>
#include <sstream>
#include <unordered_set>

namespace Qd {

#include "semantic_validator_internal.h"

	// Check if a type string is a known struct name (local or imported)
	// Supports both unqualified names (Response) and qualified names (http::Response)

	void SemanticValidator::collectDefinitions(IAstNode* node) {
		if (!node) {
			return;
		}

		// If this is a function declaration, add it to the symbol table
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);

			// Check for reserved keyword
			if (isReservedKeyword(func->name())) {
				std::string errorMsg =
						"'" + func->name() + "' is a reserved keyword and cannot be used as a function name";
				reportError(func, errorMsg.c_str());
				return;
			}

			// Check parameter names for reserved keywords
			for (const auto& paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				if (param->hasName() && isReservedKeyword(param->name())) {
					std::string errorMsg =
							"'" + param->name() + "' is a reserved keyword and cannot be used as a parameter name";
					reportError(param, errorMsg.c_str());
				}
			}
			for (const auto& paramNode : func->outputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				if (param->hasName() && isReservedKeyword(param->name())) {
					std::string errorMsg =
							"'" + param->name() + "' is a reserved keyword and cannot be used as a parameter name";
					reportError(param, errorMsg.c_str());
				}
			}

			// Handle struct methods (functions with receiver)
			if (func->hasReceiver()) {
				std::string structType = func->receiverType();
				std::string methodName = func->name();

				// Check that the receiver struct type exists
				if (mDefinedStructs.find(structType) == mDefinedStructs.end()) {
					std::string errorMsg = "Method receiver type '" + structType + "' is not a defined struct";
					reportError(func, errorMsg.c_str());
					return;
				}

				// Check for duplicate method
				if (mStructMethods.count(structType) && mStructMethods[structType].count(methodName)) {
					std::string errorMsg =
							"Duplicate method definition: '" + methodName + "' for struct '" + structType + "'";
					reportError(func, errorMsg.c_str());
					return;
				}

				// Register the method
				mStructMethods[structType][methodName] = func->isPublic();
				mStructMethodDecls[structType][methodName] = func;

				// Also register with mangled name for code generation lookup
				std::string mangledName = structType + "::" + methodName;
				mDefinedFunctions.insert(mangledName);
				return;
			}

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

			// Check for reserved keyword
			if (isReservedKeyword(constant->name())) {
				std::string errorMsg =
						"'" + constant->name() + "' is a reserved keyword and cannot be used as a constant name";
				reportError(constant, errorMsg.c_str());
				return;
			}

			// Check for duplicate constant name
			// Skip this check if the constant was pre-collected from main file (for sibling namespace support)
			if (mDefinedConstants.find(constant->name()) != mDefinedConstants.end() &&
					mPreCollectedConstants.find(constant->name()) == mPreCollectedConstants.end()) {
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

		// If this is an enum declaration, register variants as scoped constants
		if (node->type() == IAstNode::Type::ENUM_DECLARATION) {
			AstNodeEnumDeclaration* enumDecl = static_cast<AstNodeEnumDeclaration*>(node);

			if (isReservedKeyword(enumDecl->name())) {
				std::string errorMsg =
						"'" + enumDecl->name() + "' is a reserved keyword and cannot be used as an enum name";
				reportError(enumDecl, errorMsg.c_str());
				return;
			}

			if (mDefinedEnums.find(enumDecl->name()) != mDefinedEnums.end()) {
				std::string errorMsg = "Duplicate enum definition: '" + enumDecl->name() + "'";
				reportError(enumDecl, errorMsg.c_str());
				return;
			}

			mDefinedEnums.insert(enumDecl->name());
			for (const auto& variant : enumDecl->variants()) {
				std::string scopedName = enumDecl->name() + "::" + variant.name;
				mConstantValues[scopedName] = std::to_string(variant.value);
			}
		}

		// If this is a struct declaration, add it to the symbol table and collect field types
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);

			// Check for reserved keyword
			if (isReservedKeyword(structDecl->name())) {
				std::string errorMsg =
						"'" + structDecl->name() + "' is a reserved keyword and cannot be used as a struct name";
				reportError(structDecl, errorMsg.c_str());
				return;
			}

			// Check for duplicate struct name
			// Skip this check if the struct was pre-collected from main file (for sibling namespace support)
			if (mDefinedStructs.find(structDecl->name()) != mDefinedStructs.end() &&
					mPreCollectedStructs.find(structDecl->name()) == mPreCollectedStructs.end()) {
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
			for (auto* child : structDecl->children()) {
				if (child && child->type() == IAstNode::Type::STRUCT_FIELD) {
					AstNodeStructField* field = static_cast<AstNodeStructField*>(child);

					// Check for reserved keyword
					if (isReservedKeyword(field->name())) {
						std::string errorMsg =
								"'" + field->name() +
								"' is a reserved keyword and cannot be used as a field name in struct '" +
								structDecl->name() + "'";
						reportError(field, errorMsg.c_str());
						return;
					}

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
						// Unqualified struct type - treat as PTR and record the struct type name
						fieldType = StackValueType::PTR;
						mStructFieldStructTypes[structDecl->name()][field->name()] = typeName;
					} else if (typeName.find("::") != std::string::npos) {
						// Qualified struct type (e.g., vec2::Vec2)
						fieldType = StackValueType::PTR;
						mStructFieldStructTypes[structDecl->name()][field->name()] = typeName;
					}
					fieldTypes[field->name()] = fieldType;

					// Track fields with default values
					if (field->hasDefaultValue()) {
						mStructFieldsWithDefaults[structDecl->name()].insert(field->name());
					}
				}
			}
			mStructFieldTypes[structDecl->name()] = fieldTypes;
		}

		// If this is a test declaration, add it to the symbol table
		if (node->type() == IAstNode::Type::TEST_DECLARATION) {
			AstNodeTest* test = static_cast<AstNodeTest*>(node);

			// Check for reserved keyword
			if (isReservedKeyword(test->name())) {
				std::string errorMsg = "'" + test->name() + "' is a reserved keyword and cannot be used as a test name";
				reportError(test, errorMsg.c_str());
				return;
			}

			// Check for duplicate test name
			if (mDefinedTests.find(test->name()) != mDefinedTests.end()) {
				std::string errorMsg = "Duplicate test definition: '" + test->name() + "'";
				reportError(test, errorMsg.c_str());
				return;
			}

			mDefinedTests.insert(test->name());
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
			for (const auto& func : import->functions()) {
				std::string qualifiedName = import->namespaceName() + "::" + func->name;
				mImportedLibraryFunctions.insert(qualifiedName);
				// Also register unqualified name for use within the same module
				mImportedLibraryFunctions.insert(func->name);

				// Also register function signature for type checking
				FunctionSignature sig;

				// Process input parameters
				for (const auto& param : func->inputParameters) {
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
				size_t producesIdx = 0;
				for (const auto& param : func->outputParameters) {
					std::string typeStr = param->typeString();
					if (typeStr == "i32" || typeStr == "i64" || typeStr == "u8" || typeStr == "u16" ||
							typeStr == "u32" || typeStr == "u64") {
						sig.produces.push_back(StackValueType::INT);
					} else if (typeStr == "f32" || typeStr == "f64" || typeStr == "float") {
						sig.produces.push_back(StackValueType::FLOAT);
					} else if (typeStr == "str") {
						sig.produces.push_back(StackValueType::STRING);
					} else if (typeStr == "ptr" || typeStr == "bool") {
						sig.produces.push_back(StackValueType::PTR);
					} else if (isStructTypeName(typeStr)) {
						sig.produces.push_back(StackValueType::PTR);
						sig.producesStructTypes[producesIdx] = typeStr;
					} else {
						sig.produces.push_back(StackValueType::ANY);
					}
					producesIdx++;
				}

				sig.throws = func->throws;
				mFunctionSignatures[qualifiedName] = sig;
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectDefinitions(child);
		}
	}

	void SemanticValidator::validateReferences(IAstNode* node) {
		std::unordered_set<std::string> localVariables;
		std::unordered_set<std::string> iteratorNames;
		validateReferencesInternal(node, localVariables, iteratorNames);
	}

	void SemanticValidator::validateReferencesInternal(IAstNode* node, std::unordered_set<std::string>& localVariables,
			std::unordered_set<std::string>& iteratorNames) {
		if (!node) {
			return;
		}

		// Check if this is a local variable declaration
		if (node->type() == IAstNode::Type::LOCAL) {
			AstNodeLocal* local = static_cast<AstNodeLocal*>(node);
			// Insert all names (supports multiple assignment: -> a b c)
			for (const std::string& name : local->names()) {
				// Check for reserved keyword
				if (isReservedKeyword(name)) {
					reportError(local,
							("'" + name + "' is a reserved keyword and cannot be used as a variable name").c_str());
				}
				localVariables.insert(name);
			}
			return;
		}

		// Check if this is a break statement
		if (node->type() == IAstNode::Type::BREAK_STATEMENT) {
			if (iteratorNames.empty()) {
				reportErrorWithHint(node, "break statement not within loop or switch",
						"break can only be used inside 'loop', 'for', or 'switch' statements");
			}
			return;
		}

		// Check if this is a continue statement
		if (node->type() == IAstNode::Type::CONTINUE_STATEMENT) {
			if (iteratorNames.empty()) {
				reportErrorWithHint(node, "continue statement not within a loop",
						"continue can only be used inside 'loop' or 'for' statements");
			}
			return;
		}

		// Validate struct field types (check that types reference valid structs)
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);

			// Collect generic type parameters if this is a generic struct
			std::unordered_set<std::string> typeParams;
			if (structDecl->isGeneric()) {
				for (const auto& param : structDecl->typeParams()) {
					typeParams.insert(param);
				}
			}

			for (auto* child : structDecl->children()) {
				if (child && child->type() == IAstNode::Type::STRUCT_FIELD) {
					AstNodeStructField* field = static_cast<AstNodeStructField*>(child);
					const std::string& typeName = field->typeName();

					// Skip validation for generic type parameters (e.g., T in struct Box<T>)
					if (typeParams.find(typeName) != typeParams.end()) {
						continue;
					}

					// Check for qualified type names (e.g., vec2::Vec2)
					size_t colonPos = typeName.find("::");
					if (colonPos != std::string::npos) {
						std::string moduleName = typeName.substr(0, colonPos);
						std::string structTypeName = typeName.substr(colonPos + 2);

						// Check if the module is imported
						if (mImportedModules.find(moduleName) == mImportedModules.end()) {
							std::string errorMsg = "Module '" + moduleName + "' not imported for field type '" +
												   typeName + "'. Add 'use " + moduleName + "' to use this type";
							reportError(field, errorMsg.c_str());
						} else {
							// Check if the struct exists in the module
							auto moduleIt = mModuleStructs.find(moduleName);
							if (moduleIt == mModuleStructs.end() ||
									moduleIt->second.find(structTypeName) == moduleIt->second.end()) {
								std::string errorMsg =
										"Struct '" + structTypeName + "' not found in module '" + moduleName + "'";
								// Suggest a similar struct name from the module
								if (moduleIt != mModuleStructs.end()) {
									std::string suggestion = findSimilarNameInMap(structTypeName, moduleIt->second);
									if (!suggestion.empty()) {
										errorMsg += "; did you mean '" + suggestion + "'?";
									}
								}
								reportError(field, errorMsg.c_str());
							}
						}
					} else {
						// Unqualified type - check if it's a primitive or a known struct
						// Strip pointer prefix (*) if present
						std::string baseTypeName = typeName;
						if (!baseTypeName.empty() && baseTypeName[0] == '*') {
							baseTypeName = baseTypeName.substr(1);
						}

						// Primitive types: f64, i64, str, ptr, bool, u8, i8, u16, i16, u32, i32, u64
						static const std::unordered_set<std::string> primitiveTypes = {
								"f64", "i64", "str", "ptr", "bool", "u8", "i8", "u16", "i16", "u32", "i32", "u64"};

						if (primitiveTypes.find(baseTypeName) == primitiveTypes.end()) {
							// Not a primitive - check if it's a known struct
							// Check in local structs (mDefinedStructs)
							bool found = mDefinedStructs.find(baseTypeName) != mDefinedStructs.end();

							// Check in module structs (for sibling files)
							if (!found) {
								for (const auto& moduleEntry : mModuleStructs) {
									if (moduleEntry.second.find(baseTypeName) != moduleEntry.second.end()) {
										found = true;
										break;
									}
								}
							}

							if (!found) {
								std::string errorMsg =
										"Unknown type '" + typeName + "' in struct field '" + field->name() + "'";
								// Try to find similar names
								std::string suggestion = findSimilarName(baseTypeName, mDefinedStructs);
								if (!suggestion.empty()) {
									errorMsg += "; did you mean '" + suggestion + "'?";
								}
								reportError(field, errorMsg.c_str());
							}
						}
					}
				}
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

			// Check if it's a for loop iterator variable
			if (iteratorNames.count(name) > 0) {
				// Valid iterator reference
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

			// Check if it's an imported library function (unqualified, within same module)
			if (mImportedLibraryFunctions.find(name) != mImportedLibraryFunctions.end()) {
				// Valid imported library function
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

			// Check if it's a method name on any struct (type checking will verify the receiver)
			for (const auto& structEntry : mStructMethods) {
				if (structEntry.second.count(name)) {
					// Potentially valid method call - type checking will verify receiver type
					return;
				}
			}

			// Not found - report error with suggestion
			std::string errorMsg = "Undefined identifier '";
			errorMsg += name;
			errorMsg += "'";
			// Check local variables for similar names first
			std::string suggestion = findSimilarName(name, localVariables);
			if (suggestion.empty()) {
				suggestion = findSimilarFunctionName(name, mDefinedFunctions);
			}
			if (!suggestion.empty()) {
				errorMsg += "; did you mean '" + suggestion + "'?";
			} else {
				errorMsg += "; to declare a variable, use: value -> ";
				errorMsg += name;
			}
			reportError(ident, errorMsg.c_str());
		}

		// Check if this is a function pointer reference
		if (node->type() == IAstNode::Type::FUNCTION_POINTER_REFERENCE) {
			AstNodeFunctionPointerReference* funcPtr = static_cast<AstNodeFunctionPointerReference*>(node);
			const char* name = funcPtr->functionName().c_str();

			// Check if the referenced function is defined
			if (mDefinedFunctions.find(name) == mDefinedFunctions.end()) {
				// Not found - report error with suggestion
				std::string errorMsg = "Undefined function '";
				errorMsg += name;
				errorMsg += "' in function pointer reference";
				std::string suggestion = findSimilarFunctionName(name, mDefinedFunctions);
				if (!suggestion.empty()) {
					errorMsg += "; did you mean '" + suggestion + "'?";
				}
				reportError(funcPtr, errorMsg.c_str());
			}
		}

		// Check if this is a struct construction (StructName { field: value ... })
		if (node->type() == IAstNode::Type::STRUCT_CONSTRUCTION) {
			AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(node);
			const std::string& structName = construct->structName();

			// Special handling for anonymous error literal
			if (structName == "__error__") {
				// Validate field initializer expressions
				for (const auto& fieldInit : construct->fieldInits()) {
					for (const auto& valueNode : fieldInit.valueNodes) {
						validateReferencesInternal(valueNode.get(), localVariables, iteratorNames);
					}
				}
				return;
			}

			bool validStruct = false;

			// Check if struct name is qualified (e.g., "vec2::Vec2")
			size_t colonPos = structName.find("::");
			if (colonPos != std::string::npos) {
				// Qualified name - extract module and struct parts
				std::string moduleName = structName.substr(0, colonPos);
				std::string unqualifiedName = structName.substr(colonPos + 2);

				// Check if module exists and has this struct
				auto moduleIt = mModuleStructs.find(moduleName);
				if (moduleIt != mModuleStructs.end()) {
					const auto& structs = moduleIt->second;
					auto structIt = structs.find(unqualifiedName);
					if (structIt != structs.end()) {
						if (structIt->second) {
							// Struct is public - valid
							validStruct = true;
						} else {
							// Struct exists but is private - report visibility error
							std::string errorMsg = "Struct '";
							errorMsg += unqualifiedName;
							errorMsg += "' in module '";
							errorMsg += moduleName;
							errorMsg += "' is private and cannot be accessed from outside the module. Mark it as "
										"'pub struct' to export it.";
							reportError(construct, errorMsg.c_str());
							return;
						}
					}
				}
			} else {
				// Unqualified name - check if struct is defined locally
				if (mDefinedStructs.find(structName) != mDefinedStructs.end()) {
					validStruct = true;
				}

				// Check if struct is from an imported module
				if (!validStruct) {
					for (const auto& moduleEntry : mModuleStructs) {
						const auto& structs = moduleEntry.second;
						if (structs.find(structName) != structs.end() && structs.at(structName)) {
							validStruct = true;
							break;
						}
					}
				}
			}

			if (validStruct) {
				// Validate field initializer expressions
				for (const auto& fieldInit : construct->fieldInits()) {
					for (const auto& valueNode : fieldInit.valueNodes) {
						validateReferencesInternal(valueNode.get(), localVariables, iteratorNames);
					}
				}
				return;
			}

			// Not found - report error with suggestion
			std::string errorMsg = "Undefined struct '";
			errorMsg += structName;
			errorMsg += "'";
			std::string suggestion = findSimilarName(structName, mDefinedStructs);
			if (!suggestion.empty()) {
				errorMsg += "; did you mean '" + suggestion + "'?";
			}
			reportError(construct, errorMsg.c_str());
		}

		// Check if this is a scoped identifier (module function call like math::sqrt or std::printf)
		if (node->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
			AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(node);
			const std::string& scopeName = scoped->scope();
			const std::string& functionName = scoped->name();
			std::string qualifiedName = scopeName + "::" + functionName;

			// Check if this is an enum variant (e.g., TokenType::Int)
			if (mDefinedEnums.find(scopeName) != mDefinedEnums.end()) {
				if (mConstantValues.find(qualifiedName) != mConstantValues.end()) {
					return; // Valid enum variant
				}
				std::string errorMsg = "Unknown variant '";
				errorMsg += functionName;
				errorMsg += "' in enum '";
				errorMsg += scopeName;
				errorMsg += "'";
				reportError(scoped, errorMsg.c_str());
				return;
			}

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
						// Check if it's a public imported function
						auto moduleImportsIt = mModuleImportedFunctions.find(scopeName);
						bool foundAsImport = false;
						if (moduleImportsIt != mModuleImportedFunctions.end()) {
							const auto& imports = moduleImportsIt->second;
							auto importIt = imports.find(functionName);
							if (importIt != imports.end()) {
								foundAsImport = true;
								// Public imported function found - no error
							}
						}

						if (!foundAsImport) {
							// sb::append_any is a compiler-generated call from $"..." string interpolation
							// It's resolved at codegen time to sb::append or sb::append_int
							if (scopeName == "sb" && functionName == "append_any") {
								// Skip validation - handled by codegen
							} else {
								std::string errorMsg = "Function, constant, or struct '";
								errorMsg += functionName;
								errorMsg += "' not found in module '";
								errorMsg += scopeName;
								errorMsg += "'";
								// Try to suggest a similar name from the module
								auto moduleFuncsIt = mModuleFunctions.find(scopeName);
								if (moduleFuncsIt != mModuleFunctions.end()) {
									std::string suggestion = findSimilarNameInMap(functionName, moduleFuncsIt->second);
									if (!suggestion.empty()) {
										errorMsg += "; did you mean '" + suggestion + "'?";
									}
								}
								reportError(scoped, errorMsg.c_str());
							}
						}
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

		// When entering a for loop, add its iterator name to the set
		// Also create a new scope so variables defined inside don't leak out
		if (node->type() == IAstNode::Type::FOR_STATEMENT) {
			AstNodeForStatement* forStmt = static_cast<AstNodeForStatement*>(node);
			std::unordered_set<std::string> childIterators = iteratorNames;
			childIterators.insert(forStmt->iteratorName());
			std::unordered_set<std::string> forLocals = localVariables; // New scope for the for body
			for (auto* child : node->children()) {
				validateReferencesInternal(child, forLocals, childIterators);
			}
			return;
		}

		// When entering a while, loop or switch statement, use a placeholder iterator name for break/continue tracking
		// Also create a new scope so variables defined inside don't leak out
		if (node->type() == IAstNode::Type::WHILE_STATEMENT || node->type() == IAstNode::Type::LOOP_STATEMENT ||
				node->type() == IAstNode::Type::SWITCH_STATEMENT) {
			std::unordered_set<std::string> childIterators = iteratorNames;
			childIterators.insert("__loop__"); // Placeholder to indicate we're inside a loop/switch
			std::unordered_set<std::string> loopLocals = localVariables; // New scope for the loop body
			for (auto* child : node->children()) {
				validateReferencesInternal(child, loopLocals, childIterators);
			}
			return;
		}

		// When entering a function declaration, create a new scope for the function body
		// Parameters must be explicitly bound with -> before use
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			// Create a new scope for the function body
			std::unordered_set<std::string> funcLocalVariables = localVariables;
			std::unordered_set<std::string> funcIterators; // Empty - no iterators in function scope by default

			// For methods, the receiver is implicitly bound as a local variable
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			if (func->hasReceiver()) {
				funcLocalVariables.insert(func->receiverName());
			}

			// Named parameters are auto-bound as local variables
			// (only when ALL params are named; mixed stays on stack)
			bool allNamed = true;
			for (const auto& paramNode : func->inputParameters()) {
				if (!static_cast<AstNodeParameter*>(paramNode.get())->hasName()) {
					allNamed = false;
					break;
				}
			}
			if (allNamed) {
				for (const auto& paramNode : func->inputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
					if (param->hasName()) {
						funcLocalVariables.insert(param->name());
					}
				}
			}

			// Process function body
			for (auto* child : node->children()) {
				validateReferencesInternal(child, funcLocalVariables, funcIterators);
			}
			return;
		}

		// Block-scoped constructs: variables defined inside don't leak out
		// This includes if/else bodies, while bodies, blocks, etc.
		if (node->type() == IAstNode::Type::IF_STATEMENT) {
			AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(node);
			// Process then body with its own scope
			if (ifStmt->thenBody()) {
				std::unordered_set<std::string> thenLocals = localVariables;
				validateReferencesInternal(ifStmt->thenBody(), thenLocals, iteratorNames);
			}
			// Process else body with its own scope
			if (ifStmt->elseBody()) {
				std::unordered_set<std::string> elseLocals = localVariables;
				validateReferencesInternal(ifStmt->elseBody(), elseLocals, iteratorNames);
			}
			return;
		}

		if (node->type() == IAstNode::Type::BLOCK) {
			// Create a new scope for the block body
			std::unordered_set<std::string> blockLocals = localVariables;
			for (auto* child : node->children()) {
				validateReferencesInternal(child, blockLocals, iteratorNames);
			}
			return;
		}

		// Handle anonymous functions - detect captured variables from enclosing scope
		if (node->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
			AstNodeAnonymousFunction* anonFunc = static_cast<AstNodeAnonymousFunction*>(node);

			// Create a new empty scope for the anonymous function
			std::unordered_set<std::string> anonLocals;
			std::unordered_set<std::string> anonIterators;

			// Collect captured variables by walking the body
			collectCapturedVariables(anonFunc->body(), anonLocals, anonIterators, localVariables, anonFunc);

			// Now validate the body with captured variables added to the local scope
			// This ensures undefined references are properly reported
			std::unordered_set<std::string> validatedLocals;
			for (const auto& captured : anonFunc->capturedVariables()) {
				validatedLocals.insert(captured);
			}

			// Named parameters are auto-bound as local variables
			// (only when ALL params are named)
			bool allAnonNamed = true;
			for (const auto& paramNode : anonFunc->inputParameters()) {
				if (!static_cast<AstNodeParameter*>(paramNode.get())->hasName()) {
					allAnonNamed = false;
					break;
				}
			}
			if (allAnonNamed) {
				for (const auto& paramNode : anonFunc->inputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
					if (param->hasName()) {
						validatedLocals.insert(param->name());
					}
				}
			}

			// Note: anonLocals already contains variables defined with -> in the body
			validatedLocals.insert(anonLocals.begin(), anonLocals.end());

			// Validate the body
			validateReferencesInternal(anonFunc->body(), validatedLocals, anonIterators);

			return;
		}

		// Recursively process children
		for (auto* child : node->children()) {
			validateReferencesInternal(child, localVariables, iteratorNames);
		}
	}

	void SemanticValidator::collectCapturedVariables(IAstNode* node, std::unordered_set<std::string>& localVariables,
			std::unordered_set<std::string>& iteratorNames, const std::unordered_set<std::string>& outerScopeVariables,
			AstNodeAnonymousFunction* anonFunc) {
		if (!node) {
			return;
		}

		// Handle local variable definition (->)
		if (node->type() == IAstNode::Type::LOCAL) {
			AstNodeLocal* local = static_cast<AstNodeLocal*>(node);
			for (const auto& name : local->names()) {
				localVariables.insert(name);
			}
			return;
		}

		// Handle field access - check if the struct variable needs to be captured
		if (node->type() == IAstNode::Type::FIELD_ACCESS) {
			AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(node);
			const std::string& varName = fieldAccess->varName();

			// Skip if it's a local variable in the anonymous function
			if (localVariables.find(varName) != localVariables.end()) {
				return;
			}

			// Skip if it's an iterator
			if (iteratorNames.find(varName) != iteratorNames.end()) {
				return;
			}

			// Check if it's in the outer scope - this means it's a captured variable!
			if (outerScopeVariables.find(varName) != outerScopeVariables.end()) {
				// Add to captures (avoid duplicates)
				const auto& captures = anonFunc->capturedVariables();
				if (std::find(captures.begin(), captures.end(), varName) == captures.end()) {
					anonFunc->addCapturedVariable(varName);
				}
			}
			return;
		}

		// Handle field set - check if the struct variable needs to be captured
		if (node->type() == IAstNode::Type::FIELD_SET) {
			AstNodeFieldSet* fieldSet = static_cast<AstNodeFieldSet*>(node);
			const std::string& varName = fieldSet->varName();

			// Skip if it's a local variable in the anonymous function
			if (localVariables.find(varName) != localVariables.end()) {
				return;
			}

			// Skip if it's an iterator
			if (iteratorNames.find(varName) != iteratorNames.end()) {
				return;
			}

			// Check if it's in the outer scope - this means it's a captured variable!
			if (outerScopeVariables.find(varName) != outerScopeVariables.end()) {
				// Add to captures (avoid duplicates)
				const auto& captures = anonFunc->capturedVariables();
				if (std::find(captures.begin(), captures.end(), varName) == captures.end()) {
					anonFunc->addCapturedVariable(varName);
				}
			}
			return;
		}

		// Handle identifier references - this is where we detect captures
		if (node->type() == IAstNode::Type::IDENTIFIER) {
			AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
			const std::string& name = ident->name();

			// Skip if it's a local variable in the anonymous function
			if (localVariables.find(name) != localVariables.end()) {
				return;
			}

			// Skip if it's an iterator
			if (iteratorNames.find(name) != iteratorNames.end()) {
				return;
			}

			// Skip if it's a built-in instruction
			if (isBuiltInInstruction(name.c_str())) {
				return;
			}

			// Skip if it's a defined function
			if (mDefinedFunctions.find(name) != mDefinedFunctions.end()) {
				return;
			}

			// Skip if it's a constant
			if (mDefinedConstants.find(name) != mDefinedConstants.end()) {
				return;
			}

			// Skip if it's a struct constructor
			if (mDefinedStructs.find(name) != mDefinedStructs.end()) {
				return;
			}

			// Check if it's in the outer scope - this means it's a captured variable!
			if (outerScopeVariables.find(name) != outerScopeVariables.end()) {
				// Add to captures (avoid duplicates)
				const auto& captures = anonFunc->capturedVariables();
				if (std::find(captures.begin(), captures.end(), name) == captures.end()) {
					anonFunc->addCapturedVariable(name);
				}
				return;
			}

			// Otherwise it's an undefined reference - but that's reported by validateReferencesInternal
			return;
		}

		// Handle for loops - create new scope with iterator
		if (node->type() == IAstNode::Type::FOR_STATEMENT) {
			AstNodeForStatement* forStmt = static_cast<AstNodeForStatement*>(node);
			std::unordered_set<std::string> childIterators = iteratorNames;
			childIterators.insert(forStmt->iteratorName());
			std::unordered_set<std::string> forLocals = localVariables;
			for (auto* child : node->children()) {
				collectCapturedVariables(child, forLocals, childIterators, outerScopeVariables, anonFunc);
			}
			return;
		}

		// Handle while/loop/switch - create new scope
		if (node->type() == IAstNode::Type::WHILE_STATEMENT || node->type() == IAstNode::Type::LOOP_STATEMENT ||
				node->type() == IAstNode::Type::SWITCH_STATEMENT) {
			std::unordered_set<std::string> loopLocals = localVariables;
			for (auto* child : node->children()) {
				collectCapturedVariables(child, loopLocals, iteratorNames, outerScopeVariables, anonFunc);
			}
			return;
		}

		// Handle if statements - separate scopes for then/else
		if (node->type() == IAstNode::Type::IF_STATEMENT) {
			AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(node);
			if (ifStmt->thenBody()) {
				std::unordered_set<std::string> thenLocals = localVariables;
				collectCapturedVariables(ifStmt->thenBody(), thenLocals, iteratorNames, outerScopeVariables, anonFunc);
			}
			if (ifStmt->elseBody()) {
				std::unordered_set<std::string> elseLocals = localVariables;
				collectCapturedVariables(ifStmt->elseBody(), elseLocals, iteratorNames, outerScopeVariables, anonFunc);
			}
			return;
		}

		// Handle blocks - create new scope
		if (node->type() == IAstNode::Type::BLOCK) {
			std::unordered_set<std::string> blockLocals = localVariables;
			for (auto* child : node->children()) {
				collectCapturedVariables(child, blockLocals, iteratorNames, outerScopeVariables, anonFunc);
			}
			return;
		}

		// Handle nested anonymous functions - they form their own closure scope
		// For now, we don't support nested closures capturing from grandparent scopes
		if (node->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
			// Nested anonymous function - it can capture from our local scope
			// but for now we just validate it separately
			AstNodeAnonymousFunction* nestedAnon = static_cast<AstNodeAnonymousFunction*>(node);
			std::unordered_set<std::string> nestedLocals;
			std::unordered_set<std::string> nestedIterators;
			// The nested function can capture from the current scope + outer scope
			std::unordered_set<std::string> combinedOuter = outerScopeVariables;
			combinedOuter.insert(localVariables.begin(), localVariables.end());
			collectCapturedVariables(nestedAnon->body(), nestedLocals, nestedIterators, combinedOuter, nestedAnon);
			return;
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectCapturedVariables(child, localVariables, iteratorNames, outerScopeVariables, anonFunc);
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

			// Set current type parameters for generic functions
			mCurrentTypeParams = func->typeParams();

			// Collect parameter names and initialize type stack with input parameter types
			// The input parameters ARE on the runtime stack when the function starts,
			// but parameter names must be explicitly bound with -> before use
			std::vector<std::string> paramNames;
			for (const auto& paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				paramNames.push_back(param->name());

				// Validate type name
				std::string typeStr = param->typeString();
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}

				// Add parameter type to the type stack (values are on runtime stack)
				StackValueType paramType = stringToStackValueType(typeStr);
				typeStack.push_back(paramType);
			}

			// Analyze the function body in isolation (without resolving function calls)
			// Note: Parameters must be explicitly bound with -> before use, so we pass
			// an empty local variable map (parameter names are NOT pre-registered)
			std::unordered_map<std::string, StackValueType> emptyLocalVars;
			if (func->body()) {
				analyzeBlockInIsolation(func->body(), typeStack, emptyLocalVars);
			}

			// Store the signature with input parameters as consumes
			FunctionSignature sig;

			// Build consumes list from input parameters
			for (size_t paramIdx = 0; paramIdx < func->inputParameters().size(); paramIdx++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[paramIdx].get());
				std::string typeStr = param->typeString();

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}

				StackValueType paramType = stringToStackValueType(typeStr);
				sig.consumes.push_back(paramType);

				// Track struct types for PTR parameters
				if (paramType == StackValueType::PTR && isStructTypeName(typeStr)) {
					sig.parameterStructTypes[paramIdx] = typeStr;
				}
			}

			// Validate declared output parameter types
			for (const auto& paramNode : func->outputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
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

			// Build produces list from declared output parameters
			// Always use declared params — body analysis is unreliable for control flow
			size_t producesIdx = 0;
			for (const auto& paramNode : func->outputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				std::string typeStr = param->typeString();

				// Use stringToStackValueType to handle all types including type parameters
				StackValueType producedType = stringToStackValueType(typeStr);
				sig.produces.push_back(producedType);

				// Track struct types for PTR return values
				if (producedType == StackValueType::PTR && isStructTypeName(typeStr)) {
					sig.producesStructTypes[producesIdx] = typeStr;
				}
				producesIdx++;
			}
			sig.throws = func->throws();

			// For methods, use mangled name and insert receiver at beginning of consumes
			// Receiver is at stack top, validation code expects consumes[0] = receiver
			if (func->hasReceiver()) {
				// Insert receiver at front of consumes (index 0)
				sig.consumes.insert(sig.consumes.begin(), StackValueType::PTR);

				// Shift all existing parameterStructTypes indices by +1
				std::unordered_map<size_t, std::string> newParamStructTypes;
				for (const auto& entry : sig.parameterStructTypes) {
					newParamStructTypes[entry.first + 1] = entry.second;
				}
				newParamStructTypes[0] = func->receiverType();
				sig.parameterStructTypes = std::move(newParamStructTypes);

				// Use mangled name for methods
				std::string mangledName = func->receiverType() + "::" + func->name();
				mFunctionSignatures[mangledName] = sig;
			} else {
				mFunctionSignatures[func->name()] = sig;
			}

			// Clear type parameters after processing function
			mCurrentTypeParams.clear();
		}

		// Recursively process children
		for (auto* child : node->children()) {
			analyzeFunctionSignatures(child);
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
				// Assume locals are bound from parameters (simplified heuristic)
				// In reality, we'd need to track the stack to know which value is being bound
				// For multiple assignment (-> a b c), associate all with first param
				if (!paramNames.empty()) {
					for (const std::string& name : local->names()) {
						localToParam[name] = paramNames[0];
					}
				}
			}

			// Recursively process children
			for (auto* child : n->children()) {
				collectRecursive(child);
			}
		};

		collectRecursive(node);
	}

} // namespace Qd

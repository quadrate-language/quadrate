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
			for (auto* paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				if (isReservedKeyword(param->name())) {
					std::string errorMsg =
							"'" + param->name() + "' is a reserved keyword and cannot be used as a parameter name";
					reportError(param, errorMsg.c_str());
				}
			}
			for (auto* paramNode : func->outputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				if (isReservedKeyword(param->name())) {
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

			// Check for reserved keyword
			if (isReservedKeyword(structDecl->name())) {
				std::string errorMsg =
						"'" + structDecl->name() + "' is a reserved keyword and cannot be used as a struct name";
				reportError(structDecl, errorMsg.c_str());
				return;
			}

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
				size_t producesIdx = 0;
				for (const auto* param : func->outputParameters) {
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
		for (size_t i = 0; i < node->childCount(); i++) {
			collectDefinitions(node->child(i));
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
				reportError(node, "break statement not within loop or switch");
			}
			return;
		}

		// Check if this is a continue statement
		if (node->type() == IAstNode::Type::CONTINUE_STATEMENT) {
			if (iteratorNames.empty()) {
				reportError(node, "continue statement not within a loop");
			}
			return;
		}

		// Validate struct field types (check that qualified types reference valid modules/structs)
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);
			for (size_t i = 0; i < structDecl->childCount(); i++) {
				IAstNode* child = structDecl->child(i);
				if (child && child->type() == IAstNode::Type::STRUCT_FIELD) {
					AstNodeStructField* field = static_cast<AstNodeStructField*>(child);
					const std::string& typeName = field->typeName();

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

		// Check if this is a struct construction (StructName { field: value ... })
		if (node->type() == IAstNode::Type::STRUCT_CONSTRUCTION) {
			AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(node);
			const std::string& structName = construct->structName();

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
					if (structs.find(unqualifiedName) != structs.end() && structs.at(unqualifiedName)) {
						validStruct = true;
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
					for (IAstNode* valueNode : fieldInit.valueNodes) {
						validateReferencesInternal(valueNode, localVariables, iteratorNames);
					}
				}
				return;
			}

			// Not found - report error
			std::string errorMsg = "Undefined struct '";
			errorMsg += structName;
			errorMsg += "'";
			reportError(construct, errorMsg.c_str());
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
							std::string errorMsg = "Function, constant, or struct '";
							errorMsg += functionName;
							errorMsg += "' not found in module '";
							errorMsg += scopeName;
							errorMsg += "'";
							reportError(scoped, errorMsg.c_str());
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
			for (size_t i = 0; i < node->childCount(); i++) {
				validateReferencesInternal(node->child(i), forLocals, childIterators);
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
			for (size_t i = 0; i < node->childCount(); i++) {
				validateReferencesInternal(node->child(i), loopLocals, childIterators);
			}
			return;
		}

		// When entering a function declaration, create a new scope for the function body
		// Parameters must be explicitly bound with -> before use
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			// Create a new scope for the function body (parameters NOT pre-registered)
			std::unordered_set<std::string> funcLocalVariables = localVariables;
			std::unordered_set<std::string> funcIterators; // Empty - no iterators in function scope by default

			// For methods, the receiver is implicitly bound as a local variable
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			if (func->hasReceiver()) {
				funcLocalVariables.insert(func->receiverName());
			}

			// Process function body
			for (size_t i = 0; i < node->childCount(); i++) {
				validateReferencesInternal(node->child(i), funcLocalVariables, funcIterators);
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
			for (size_t i = 0; i < node->childCount(); i++) {
				validateReferencesInternal(node->child(i), blockLocals, iteratorNames);
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

			// Note: anonLocals already contains variables defined with -> in the body
			validatedLocals.insert(anonLocals.begin(), anonLocals.end());

			// Validate the body
			validateReferencesInternal(anonFunc->body(), validatedLocals, anonIterators);

			return;
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			validateReferencesInternal(node->child(i), localVariables, iteratorNames);
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
			for (size_t i = 0; i < node->childCount(); i++) {
				collectCapturedVariables(node->child(i), forLocals, childIterators, outerScopeVariables, anonFunc);
			}
			return;
		}

		// Handle while/loop/switch - create new scope
		if (node->type() == IAstNode::Type::WHILE_STATEMENT || node->type() == IAstNode::Type::LOOP_STATEMENT ||
				node->type() == IAstNode::Type::SWITCH_STATEMENT) {
			std::unordered_set<std::string> loopLocals = localVariables;
			for (size_t i = 0; i < node->childCount(); i++) {
				collectCapturedVariables(node->child(i), loopLocals, iteratorNames, outerScopeVariables, anonFunc);
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
			for (size_t i = 0; i < node->childCount(); i++) {
				collectCapturedVariables(node->child(i), blockLocals, iteratorNames, outerScopeVariables, anonFunc);
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
		for (size_t i = 0; i < node->childCount(); i++) {
			collectCapturedVariables(node->child(i), localVariables, iteratorNames, outerScopeVariables, anonFunc);
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

			// Collect parameter names and initialize type stack with input parameter types
			// The input parameters ARE on the runtime stack when the function starts,
			// but parameter names must be explicitly bound with -> before use
			std::vector<std::string> paramNames;
			for (auto* paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
				paramNames.push_back(param->name());

				// Validate type name
				std::string typeStr = param->typeString();
				if (!isValidTypeName(typeStr)) {
					reportError(param, ("Invalid type '" + typeStr + "' in parameter '" + param->name() +
											   "'. Valid types are: i64, f64, str, ptr, any, or a struct name")
											   .c_str());
				}

				// Add parameter type to the type stack (values are on runtime stack)
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
					paramType = StackValueType::ANY;
				}
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

			// Build produces list: prefer declared output parameters, fall back to body analysis
			// Using declared outputs is more reliable for functions with complex control flow
			if (!func->outputParameters().empty()) {
				size_t producesIdx = 0;
				for (auto* paramNode : func->outputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
					std::string typeStr = param->typeString();

					if (typeStr == "i64") {
						sig.produces.push_back(StackValueType::INT);
					} else if (typeStr == "f64") {
						sig.produces.push_back(StackValueType::FLOAT);
					} else if (typeStr == "str") {
						sig.produces.push_back(StackValueType::STRING);
					} else if (typeStr == "ptr") {
						sig.produces.push_back(StackValueType::PTR);
					} else if (isStructTypeName(typeStr)) {
						sig.produces.push_back(StackValueType::PTR);
						sig.producesStructTypes[producesIdx] = typeStr;
					} else {
						sig.produces.push_back(StackValueType::ANY);
					}
					producesIdx++;
				}
			} else {
				// No declared outputs - use body analysis result
				sig.produces = typeStack;
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
			for (size_t i = 0; i < n->childCount(); i++) {
				collectRecursive(n->child(i));
			}
		};

		collectRecursive(node);
	}

} // namespace Qd

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
#include <quadrate/qc/ast_node_array_literal.h>
#include <quadrate/qc/ast_node_as_cast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_ctx.h>
#include <quadrate/qc/ast_node_defer.h>
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
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/semantic_validator.h>
#include <sstream>
#include <unordered_set>

namespace Qd {

#include "semantic_validator_internal.h"

	// Helper: Convert StackValueType to Quadrate type name
	static const char* stackTypeToQdType(StackValueType type) {
		switch (type) {
		case StackValueType::INT:
			return "i64";
		case StackValueType::FLOAT:
			return "f64";
		case StackValueType::STRING:
			return "str";
		case StackValueType::PTR:
			return "ptr";
		default:
			return "any";
		}
	}

	// Helper: Build fn type string from FunctionSignature, e.g. "fn(i64 -- i64)"
	static std::string buildFnTypeString(const FunctionSignature& sig) {
		std::string s = "fn(";
		bool first = true;
		for (const auto& t : sig.consumes) {
			if (!first) {
				s += " ";
			}
			s += stackTypeToQdType(t);
			first = false;
		}
		if (!sig.consumes.empty()) {
			s += " ";
		}
		s += "--";
		first = true;
		for (const auto& t : sig.produces) {
			if (first) {
				s += " ";
			} else {
				s += " ";
			}
			s += stackTypeToQdType(t);
			first = false;
		}
		s += ")";
		return s;
	}

	// Helper: Collect all identifier references in an AST subtree (for unused param detection)
	static void collectIdentifierRefs(const IAstNode* node, std::unordered_set<std::string>& refs,
			const std::unordered_set<std::string>& paramNames) {
		if (!node) {
			return;
		}
		if (node->type() == IAstNode::Type::IDENTIFIER) {
			const auto* ident = static_cast<const AstNodeIdentifier*>(node);
			refs.insert(ident->name());
		}
		if (node->type() == IAstNode::Type::FIELD_ACCESS) {
			const auto* fieldAccess = static_cast<const AstNodeFieldAccess*>(node);
			refs.insert(fieldAccess->varName());
		}
		if (node->type() == IAstNode::Type::FIELD_SET) {
			const auto* fieldSet = static_cast<const AstNodeFieldSet*>(node);
			refs.insert(fieldSet->varName());
		}
		if (node->type() == IAstNode::Type::INSTRUCTION) {
			const auto* instr = static_cast<const AstNodeInstruction*>(node);
			if (paramNames.count(instr->name())) {
				refs.insert(instr->name());
			}
		}
		for (size_t i = 0; i < node->childCount(); i++) {
			collectIdentifierRefs(node->child(i), refs, paramNames);
		}
	}

	// Helper: Convert literal type to stack value type
	static StackValueType getLiteralStackType(AstNodeLiteral::LiteralType litType) {
		switch (litType) {
		case AstNodeLiteral::LiteralType::INTEGER:
			return StackValueType::INT;
		case AstNodeLiteral::LiteralType::BOOL:
			return StackValueType::INT;
		case AstNodeLiteral::LiteralType::FLOAT:
			return StackValueType::FLOAT;
		case AstNodeLiteral::LiteralType::STRING:
			return StackValueType::STRING;
		case AstNodeLiteral::LiteralType::NULL_PTR:
			return StackValueType::PTR;
		default:
			return StackValueType::INT;
		}
	}

	// Helper: Push produces types from a function signature onto the type stacks
	static void pushProducesTypes(const FunctionSignature& sig, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack) {
		for (size_t idx = 0; idx < sig.produces.size(); idx++) {
			typeStack.push_back(sig.produces[idx]);
			auto structIt = sig.producesStructTypes.find(idx);
			structTypeStack.push_back(structIt != sig.producesStructTypes.end() ? structIt->second : "");
		}
	}

	// Helper: Check if a block ends with a diverging instruction (like `panic`)
	// Diverging instructions never return, so stack effects don't need to balance
	static bool blockEndsDiverging(IAstNode* block) {
		if (!block || block->childCount() == 0) {
			return false;
		}
		IAstNode* lastChild = block->child(block->childCount() - 1);
		if (!lastChild) {
			return false;
		}
		// Check if the last instruction is `panic`
		if (lastChild->type() == IAstNode::Type::INSTRUCTION) {
			AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(lastChild);
			return instr->name() == "panic";
		}
		// Check if the block ends with `return`
		if (lastChild->type() == IAstNode::Type::RETURN_STATEMENT) {
			return true;
		}
		// Recursively check nested blocks (e.g., if the last child is another block)
		if (lastChild->type() == IAstNode::Type::BLOCK) {
			return blockEndsDiverging(lastChild);
		}
		return false;
	}

	// Check if a type string is a known struct name (local or imported)
	// Supports both unqualified names (Response) and qualified names (http::Response)

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

		// Process each child in the block (using index-based loop for peek-ahead access)
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}

			switch (child->type()) {
			case IAstNode::Type::LITERAL: {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(child);
				typeStack.push_back(getLiteralStackType(lit->literalType()));
				break;
			}

			case IAstNode::Type::ARRAY_LITERAL: {
				// Array literal pushes a pointer (array reference) onto the stack
				typeStack.push_back(StackValueType::PTR);
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				const std::string& instrName = instr->name();

				// Check if instruction name shadows a local variable - if so, treat as variable reference
				auto localIt = localVarTypes.find(instrName);
				if (localIt != localVarTypes.end()) {
					// Push the local variable's type onto the stack
					typeStack.push_back(localIt->second);
					break;
				}

				// During signature analysis, don't report errors - just simulate the stack
				typeCheckInstructionInternal(child, instrName.c_str(), typeStack, structTypeStack, false);
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
					const auto* structFields = lookupStructFieldTypes(name);
					if (structFields != nullptr) {
						size_t fieldCount = structFields->size();
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
					const std::string& moduleName = moduleEntry.first;
					const auto& structs = moduleEntry.second;
					if (structs.find(name) != structs.end()) {
						std::string qualifiedName = moduleName + "::" + name;
						const auto* structFields = lookupStructFieldTypes(qualifiedName);
						if (structFields != nullptr) {
							size_t fieldCount = structFields->size();
							for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
								typeStack.pop_back();
								if (!structTypeStack.empty()) {
									structTypeStack.pop_back();
								}
							}
						}
						typeStack.push_back(StackValueType::PTR);
						structTypeStack.push_back(qualifiedName); // Track the qualified struct type
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
				// Check if it's a module-level mutable `var` — push its
				// declared type. Lives on the runtime stack.
				auto gvIt = mGlobalVarTypes.find(name);
				if (gvIt != mGlobalVarTypes.end()) {
					typeStack.push_back(stringToStackValueType(gvIt->second));
				}
				// If signature not known yet, skip (will be resolved in next iteration)
				break;
			}

			case IAstNode::Type::FIELD_ACCESS: {
				// Field access pushes a value onto the stack
				// Detailed validation is done in the main type checking pass (case at line ~3266)
				// Here we just handle the stack effect for signature analysis
				AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(child);
				const std::string& fieldName = fieldAccess->fieldName();

				StackValueType fieldType = StackValueType::UNKNOWN;
				// Search in all known structs to determine the field type
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
			case IAstNode::Type::FIELD_SET: {
				AstNodeFieldSet* fs = static_cast<AstNodeFieldSet*>(child);
				if (fs->varName().empty()) {
					if (fs->noReturn()) {
						// >>field! : pop value, pop struct, push nothing (net: -2)
						if (typeStack.size() >= 2) {
							typeStack.pop_back(); // pop value
							typeStack.pop_back(); // pop struct
						} else if (!typeStack.empty()) {
							typeStack.pop_back();
						}
					} else {
						// >>field : pop value, struct stays (net: -1)
						if (typeStack.size() >= 2) {
							typeStack.pop_back(); // pop value
						} else if (!typeStack.empty()) {
							typeStack.pop_back();
						}
					}
				} else {
					if (!typeStack.empty()) {
						typeStack.pop_back();
					}
				}
				break;
			}
			case IAstNode::Type::SCOPED_IDENTIFIER: {
				// Apply module function signature if known
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);

				// In pass 1 (signature analysis), don't resolve sb::append_any —
				// just apply the same stack effect as sb::append (ptr, any -> ptr)
				if (scoped->scope() == "sb" && scoped->name() == "append_any") {
					if (!typeStack.empty()) {
						typeStack.pop_back(); // pop value
					}
					if (!typeStack.empty()) {
						typeStack.pop_back(); // pop sb ptr
					}
					typeStack.push_back(StackValueType::PTR); // push result sb ptr
					break;
				}

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
				} else {
					// Try as a method call — search module's struct methods
					bool foundMethod = false;
					for (const auto& structEntry : mStructMethods) {
						const std::string& structType = structEntry.first;
						// Check if struct belongs to this module
						if (structType.find(moduleName + "::") != 0 &&
								!mStructMethods.count(moduleName + "::" + structType)) {
							// Also check unqualified struct names from this module
							bool belongsToModule = false;
							for (const auto& methodEntry : structEntry.second) {
								std::string methodKey = structType + "::" + methodEntry.first;
								if (mFunctionSignatures.count(methodKey)) {
									belongsToModule = true;
									break;
								}
							}
							if (!belongsToModule) {
								continue;
							}
						}
						if (structEntry.second.count(functionName)) {
							std::string methodKey = structType + "::" + functionName;
							auto methodSigIt = mFunctionSignatures.find(methodKey);
							if (methodSigIt != mFunctionSignatures.end()) {
								const FunctionSignature& sig = methodSigIt->second;
								for (size_t j = 0; j < sig.consumes.size() && !typeStack.empty(); j++) {
									typeStack.pop_back();
								}
								for (const auto& type : sig.produces) {
									typeStack.push_back(type);
								}
								foundMethod = true;
								break;
							}
						}
					}
					// If not found, skip (will be resolved in next iteration)
					(void)foundMethod;
				}
				break;
			}

			case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
				// Function pointer references push a pointer type onto the stack
				typeStack.push_back(StackValueType::PTR);
				break;

			case IAstNode::Type::ANONYMOUS_FUNCTION:
				// Anonymous functions push a function pointer onto the stack
				// The function body will be validated separately during code generation
				typeStack.push_back(StackValueType::PTR);
				break;

			case IAstNode::Type::STRUCT_CONSTRUCTION: {
				// Struct construction with named fields: StructName { field: expr ... }
				// Field expressions are self-contained, so struct construction just pushes PTR
				AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(child);
				const std::string& structName = construct->structName();
				typeStack.push_back(StackValueType::PTR);

				// Check if struct is from a module - use qualified name for method resolution
				std::string qualifiedStructName = structName;
				if (structName.find("::") == std::string::npos) {
					// Unqualified name - check if it's from a module
					for (const auto& moduleEntry : mModuleStructs) {
						const auto& structs = moduleEntry.second;
						if (structs.find(structName) != structs.end() && structs.at(structName)) {
							qualifiedStructName = moduleEntry.first + "::" + structName;
							break;
						}
					}
				}
				structTypeStack.push_back(qualifiedStructName);
				break;
			}

			case IAstNode::Type::LOCAL: {
				// Local variable binding: pop value(s) from stack and store type(s)
				// Supports multiple assignment: -> a b c
				// Special case: -> _ discards the value (like drop)
				AstNodeLocal* local = static_cast<AstNodeLocal*>(child);
				for (const std::string& varName : local->names()) {
					if (!typeStack.empty()) {
						StackValueType varType = typeStack.back();
						typeStack.pop_back();
						// Don't store _ as a variable - it's a discard
						if (varName != "_") {
							localVarTypes[varName] = varType;
						}
					}
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

			// Set current type parameters for generic functions
			mCurrentTypeParams = func->typeParams();

			// Clear local variable struct types for this function
			mLocalVariableStructTypes.clear();

			// Initialize type stack with input parameters
			// Named parameters are auto-bound as local variables at function entry,
			// but only when ALL params are named (mixed named/unnamed all stay on stack)
			bool allParamsNamed = true;
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				if (!static_cast<AstNodeParameter*>(func->inputParameters()[i].get())->hasName()) {
					allParamsNamed = false;
					break;
				}
			}
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i].get());
				const std::string& typeStr = param->typeString();

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					std::string errorMsg = "Invalid type '" + typeStr + "'";
					if (param->hasName()) {
						errorMsg += " in parameter '" + param->name() + "'";
					}
					errorMsg += ". Valid types are: i64, f64, str, ptr, any, []T, fn(...), or a struct name";
					reportError(param, errorMsg.c_str());
				}

				// Resolve type aliases
				std::string resolvedType = typeStr;
				auto tcAliasIt = mTypeAliases.find(typeStr);
				if (tcAliasIt != mTypeAliases.end()) {
					resolvedType = tcAliasIt->second;
				}

				StackValueType paramType = stringToStackValueType(resolvedType);
				std::string structType = "";

				// Track struct/array/fn types for PTR parameters
				if (paramType == StackValueType::PTR &&
						(isStructTypeName(resolvedType) ||
								(resolvedType.size() > 2 && resolvedType[0] == '[' && resolvedType[1] == ']') ||
								(resolvedType.size() > 3 && resolvedType.substr(0, 3) == "fn("))) {
					structType = resolvedType;
				}

				if (allParamsNamed && param->hasName()) {
					// Named parameter: auto-bound as a local variable
					localVariables[param->name()] = paramType;
					if (!structType.empty()) {
						mLocalVariableStructTypes[param->name()] = structType;
					}
				} else {
					// Unnamed parameter: stays on the stack
					typeStack.push_back(paramType);
					structTypeStack.push_back(structType);
				}
			}

			// Track whether current function is fallible (for panic validation)
			mCurrentFunctionFallible = func->throws();
			mCurrentFunctionOutputCount = func->outputParameters().size();
			mHasUnpredictableStack = false;

			// For methods, register the receiver as a local variable (it's implicitly bound)
			if (func->hasReceiver()) {
				const std::string& receiverName = func->receiverName();
				const std::string& receiverType = func->receiverType();
				localVariables[receiverName] = StackValueType::PTR;
				mLocalVariableStructTypes[receiverName] = receiverType;
			}

			// Type check the function body
			if (func->body()) {
				typeCheckBlock(func->body(), typeStack, localVariables, structTypeStack);
			}

			// Warn about unused named parameters
			if (func->body()) {
				// Collect named param names
				std::unordered_set<std::string> paramNames;
				for (size_t i = 0; i < func->inputParameters().size(); i++) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i].get());
					if (param->hasName() && param->name()[0] != '_') {
						paramNames.insert(param->name());
					}
				}
				if (!paramNames.empty()) {
					std::unordered_set<std::string> refs;
					collectIdentifierRefs(func->body(), refs, paramNames);
					for (size_t i = 0; i < func->inputParameters().size(); i++) {
						AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i].get());
						if (param->hasName() && param->name()[0] != '_' && refs.find(param->name()) == refs.end()) {
							reportWarning(param, ("Unused parameter '" + param->name() + "'").c_str());
						}
					}
				}
			}

			// Validate that the type stack matches declared output parameters
			// Skip if the function body diverges (e.g., ends with panic or return)
			// Skip if stack effects are unpredictable (e.g., 'read' instruction or unhandled instructions)
			if (func->body() && !blockEndsDiverging(func->body()) && !mHasUnpredictableStack) {
				size_t expectedOutputs = func->outputParameters().size();
				size_t actualOutputs = typeStack.size();

				if (actualOutputs != expectedOutputs) {
					std::string errorMsg = "Function '";
					errorMsg += func->name();
					errorMsg += "' declares ";
					errorMsg += std::to_string(expectedOutputs);
					errorMsg += " output(s) but body leaves ";
					errorMsg += std::to_string(actualOutputs);
					errorMsg += " value(s) on the stack";
					reportError(func, errorMsg.c_str());
				} else if (actualOutputs == expectedOutputs) {
					// Check that types match
					for (size_t i = 0; i < expectedOutputs; i++) {
						AstNodeParameter* outParam = static_cast<AstNodeParameter*>(func->outputParameters()[i].get());
						StackValueType expectedType = stringToStackValueType(outParam->typeString());
						StackValueType actualType = typeStack[i];

						if (expectedType == StackValueType::ANY || expectedType == StackValueType::UNKNOWN ||
								expectedType == StackValueType::TYPEVAR) {
							continue;
						}
						if (actualType == StackValueType::UNKNOWN || actualType == StackValueType::ANY ||
								actualType == StackValueType::TYPEVAR) {
							continue;
						}

						if (actualType != expectedType) {
							std::string errorMsg = "Function '";
							errorMsg += func->name();
							errorMsg += "' output ";
							errorMsg += std::to_string(i + 1);
							errorMsg += " ('";
							errorMsg += outParam->name();
							errorMsg += "') expects ";
							errorMsg += stackValueTypeToString(expectedType);
							errorMsg += " but got ";
							errorMsg += stackValueTypeToString(actualType);
							reportError(func, errorMsg.c_str());
						} else if (actualType == StackValueType::PTR) {
							// Both are PTR — check array/struct subtype
							std::string expectedStructType = outParam->typeString();
							std::string actualStructType = (i < structTypeStack.size()) ? structTypeStack[i] : "";
							bool expectedIsArray = expectedStructType.size() > 2 && expectedStructType[0] == '[' &&
												   expectedStructType[1] == ']';
							bool actualIsArray = actualStructType.size() > 2 && actualStructType[0] == '[' &&
												 actualStructType[1] == ']';
							if (expectedIsArray && actualIsArray && expectedStructType != actualStructType) {
								std::string errorMsg = "Function '";
								errorMsg += func->name();
								errorMsg += "' output '";
								errorMsg += outParam->name();
								errorMsg += "' expects type '";
								errorMsg += expectedStructType;
								errorMsg += "' but got '";
								errorMsg += actualStructType;
								errorMsg += "'";
								reportError(func, errorMsg.c_str());
							}
						}
					}
				}
			}

			// Reset function state
			mCurrentFunctionFallible = false;
			mCurrentFunctionOutputCount = 0;

			// Clear type parameters after type checking
			mCurrentTypeParams.clear();
		}

		// Type check test declarations
		if (node->type() == IAstNode::Type::TEST_DECLARATION) {
			AstNodeTest* test = static_cast<AstNodeTest*>(node);
			std::vector<StackValueType> typeStack;
			std::vector<std::string> structTypeStack;
			std::unordered_map<std::string, StackValueType> localVariables;

			// Clear local variable struct types for this test
			mLocalVariableStructTypes.clear();

			// Tests have no parameters - start with empty stack

			// Type check the test body
			if (test->body()) {
				typeCheckBlock(test->body(), typeStack, localVariables, structTypeStack);
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			typeCheckFunction(child);
		}
	}

	void SemanticValidator::typeCheckTest(IAstNode* node) {
		// Tests are handled in typeCheckFunction along with functions
		// This method exists for potential future specialized test validation
		typeCheckFunction(node);
	}

	void SemanticValidator::typeCheckBlock(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
		if (!node) {
			return;
		}

		// Process each child in the block (using index-based loop for peek-ahead access)
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}

			switch (child->type()) {
			case IAstNode::Type::LITERAL: {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(child);
				typeStack.push_back(getLiteralStackType(lit->literalType()));
				structTypeStack.push_back("");
				break;
			}

			case IAstNode::Type::ARRAY_LITERAL: {
				// Array literal pushes a pointer (array reference) onto the stack
				typeStack.push_back(StackValueType::PTR);
				// Infer array element type from first element
				AstNodeArrayLiteral* arrLit = static_cast<AstNodeArrayLiteral*>(child);
				if (arrLit->elements().empty()) {
					structTypeStack.push_back("[]any");
				} else {
					IAstNode* firstElem = arrLit->elements()[0].get();
					if (firstElem->type() == IAstNode::Type::LITERAL) {
						auto* lit = static_cast<AstNodeLiteral*>(firstElem);
						switch (lit->literalType()) {
						case AstNodeLiteral::LiteralType::INTEGER:
						case AstNodeLiteral::LiteralType::BOOL:
							structTypeStack.push_back("[]i64");
							break;
						case AstNodeLiteral::LiteralType::FLOAT:
							structTypeStack.push_back("[]f64");
							break;
						case AstNodeLiteral::LiteralType::STRING:
							structTypeStack.push_back("[]str");
							break;
						default:
							structTypeStack.push_back("[]any");
							break;
						}
					} else {
						structTypeStack.push_back("[]any");
					}
				}
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				const std::string& instrName = instr->name();

				// Check if instruction name shadows a local variable - if so, treat as variable reference
				auto localIt = localVariables.find(instrName);
				if (localIt != localVariables.end()) {
					// Push the local variable's type onto the stack (variable shadowing)
					typeStack.push_back(localIt->second);
					if (localIt->second == StackValueType::PTR) {
						auto structTypeIt = mLocalVariableStructTypes.find(instrName);
						if (structTypeIt != mLocalVariableStructTypes.end()) {
							structTypeStack.push_back(structTypeIt->second);
						} else {
							structTypeStack.push_back("");
						}
					} else {
						structTypeStack.push_back("");
					}
					break;
				}

				// Check if this is a method call on a struct
				// Methods require: receiver pushed first, then parameters on top
				// e.g., for method set(idx, elem): v idx elem set!
				std::string receiverStructType;
				std::string registeredStructType;

				// First pass: find any struct on the stack that has this method
				// to determine the method signature and expected parameter count
				size_t searchLimit = std::min(structTypeStack.size(), typeStack.size());
				for (size_t idx = searchLimit; idx > 0; idx--) {
					const std::string& structType = structTypeStack[idx - 1];
					if (!structType.empty()) {
						std::string potentialRegisteredType = findMethodStructType(structType, instrName);
						if (!potentialRegisteredType.empty()) {
							registeredStructType = potentialRegisteredType;
							break;
						}
					}
				}

				if (!registeredStructType.empty()) {
					// This is a method call - look up the signature with mangled name
					std::string mangledName = registeredStructType + "::" + instrName;
					auto methodSigIt = mFunctionSignatures.find(mangledName);
					if (methodSigIt != mFunctionSignatures.end()) {
						const FunctionSignature& sig = methodSigIt->second;

						// Calculate expected receiver position based on parameter count
						// With receiver-first: receiver should be at position additionalParams from top
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;

						// Second pass: check if there's a valid receiver at the expected position
						// Expected receiver index in stack (0-indexed from bottom)
						// For additionalParams=1 and stack [receiver, param], receiver is at index 0
						if (typeStack.size() <= additionalParams) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += instrName;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(instr, errorMsg.c_str());
							break;
						}

						size_t expectedReceiverIdx = typeStack.size() - 1 - additionalParams;

						// Verify the struct at expected position has this method
						bool foundValidReceiver = false;
						if (expectedReceiverIdx < structTypeStack.size()) {
							const std::string& structAtExpectedPos = structTypeStack[expectedReceiverIdx];
							std::string actualRegisteredType = findMethodStructType(structAtExpectedPos, instrName);
							if (!actualRegisteredType.empty()) {
								// Valid receiver-first call
								receiverStructType = structAtExpectedPos;
								registeredStructType = actualRegisteredType;
								foundValidReceiver = true;
							}
						}

						if (!foundValidReceiver) {
							std::string errorMsg = "Method call '";
							errorMsg += instrName;
							errorMsg += "': receiver must be pushed first, then ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " parameter(s) on top (e.g., 'receiver";
							for (size_t p = 0; p < additionalParams; p++) {
								errorMsg += " param";
							}
							errorMsg += " ";
							errorMsg += instrName;
							errorMsg += "')";
							reportError(instr, errorMsg.c_str());
							break;
						}

						// receiverPositionFromTop equals additionalParams by construction
						// (we verified the receiver is at the expected position above)
						size_t receiverPositionFromTop = additionalParams;

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values
						pushProducesTypes(sig, typeStack, structTypeStack);

						// Mark instruction as a method call for code generation
						// Use registeredStructType (the generic type) for proper function name lookup
						instr->setIsMethodCall(true);
						instr->setReceiverType(registeredStructType);
						instr->setMethodInputParamCount(additionalParams);
						// Use the receiver position calculated earlier (before stack modifications)
						instr->setMethodReceiverPositionFromTop(receiverPositionFromTop);
						break;
					}
				}

				// Fall back to builtin instruction handling
				typeCheckInstruction(child, instrName.c_str(), typeStack, structTypeStack);
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
					reportErrorWithHint(child, "Type error in 'if': Stack underflow (requires 1 condition value)",
							"push a condition before 'if', e.g. 'x 0 > if { ... }'");
					break;
				}
				// Pop the condition value
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}

				// Analyze branches to track stack effects
				AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(child);
				IAstNode* thenBody = ifStmt->thenBody();
				IAstNode* elseBody = ifStmt->elseBody();

				if (thenBody && elseBody) {
					// Analyze then branch
					std::vector<StackValueType> thenStack = typeStack;
					std::unordered_map<std::string, StackValueType> thenVars = localVariables;
					std::vector<std::string> thenStructStack = structTypeStack;
					typeCheckBlock(thenBody, thenStack, thenVars, thenStructStack);

					// Analyze else branch (same starting stack - both branches receive
					// result values from fallible calls, though error values are undefined)
					std::vector<StackValueType> elseStack = typeStack;
					std::unordered_map<std::string, StackValueType> elseVars = localVariables;
					std::vector<std::string> elseStructStack = structTypeStack;
					typeCheckBlock(elseBody, elseStack, elseVars, elseStructStack);

					int thenEffect = static_cast<int>(thenStack.size()) - static_cast<int>(typeStack.size());
					int elseEffect = static_cast<int>(elseStack.size()) - static_cast<int>(typeStack.size());

					// Check if either branch diverges (e.g., ends with `panic`)
					// Diverging branches never return, so stack effects don't need to balance
					bool thenDiverges = blockEndsDiverging(thenBody);
					bool elseDiverges = blockEndsDiverging(elseBody);

					// Both branches should have the same stack effect (balanced).
					// Stack effect mismatch is an error because it leaves the stack in an
					// unpredictable state, making subsequent code incorrect.
					// Exception: if one branch diverges, it never returns, so no mismatch.
					if (thenDiverges && elseDiverges) {
						// Both branches diverge (return/panic) — no effect on parent stack
					} else if (thenEffect != elseEffect && !thenDiverges && !elseDiverges) {
						// Effects disagree. This is legitimate and expected after a fallible
						// call: the success arm receives the call's result value while the
						// failure arm does not. Anywhere else it means the two arms leave the
						// stack at different depths, so whatever follows reads a value whose
						// identity depends on which arm ran -- and codegen silently truncates
						// to the shallower arm, discarding the excess.
						// Resolve the sibling immediately before this `if`. A fallible call there
						// is the legitimate source of an arm mismatch.
						bool afterFallibleCall = false;
						if (i > 0) {
							IAstNode* prev = node->child(i - 1);
							std::string callee;
							if (prev && prev->type() == IAstNode::Type::IDENTIFIER) {
								callee = static_cast<AstNodeIdentifier*>(prev)->name();
							} else if (prev && prev->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
								auto* sc = static_cast<AstNodeScopedIdentifier*>(prev);
								callee = sc->scope() + "::" + sc->name();
							}
							if (!callee.empty()) {
								auto sigIt = mFunctionSignatures.find(callee);
								afterFallibleCall = sigIt != mFunctionSignatures.end() && sigIt->second.throws;
							}
						}

						// Only report when the stack model is trustworthy. mHasUnpredictableStack
						// means something in this function -- a variadic like fmt::printf, an
						// imported C function, flag::parse -- consumed an amount the signature
						// does not describe, so the arm effects are computed from a model already
						// known to be wrong. The function-level arity check and the defer-effect
						// check gate on this for the same reason; this one did not, and it made
						// every module-qualified method call on a receiver read as +1. Both
						// `wc.qd` sites were that: `f flag::destroy` is net zero at runtime.
						if (!afterFallibleCall && !mHasUnpredictableStack) {
							std::string msg =
									"if/else branches leave different numbers of values on the stack (then: " +
									std::to_string(thenEffect) + ", else: " + std::to_string(elseEffect) +
									"); both arms must leave the same number, since whatever follows "
									"reads a value whose identity would otherwise depend on which arm ran";
							reportError(child, msg.c_str());
						}
						// Use the if (success) branch as authoritative.
						typeStack = thenStack;
						structTypeStack = thenStructStack;
					} else if (thenDiverges && !elseDiverges) {
						// Only else branch returns, apply its effects
						typeStack = elseStack;
						structTypeStack = elseStructStack;
					} else if (elseDiverges && !thenDiverges) {
						// Only then branch returns, apply its effects
						typeStack = thenStack;
						structTypeStack = thenStructStack;
					} else if (thenEffect > 0) {
						// Both branches have the same positive effect, apply it
						for (size_t k = typeStack.size(); k < thenStack.size(); k++) {
							typeStack.push_back(thenStack[k]);
							if (k < thenStructStack.size()) {
								structTypeStack.push_back(thenStructStack[k]);
							} else {
								structTypeStack.push_back("");
							}
						}
					} else if (thenEffect < 0) {
						// Both branches consume values, set to final state
						typeStack = thenStack;
						structTypeStack = thenStructStack;
					}
				} else if (thenBody) {
					// Only then branch - analyze with a copy since branch might not execute
					// (common pattern: fallible_func if { -> var ... })
					std::vector<StackValueType> thenStack = typeStack;
					std::unordered_map<std::string, StackValueType> thenVars = localVariables;
					std::vector<std::string> thenStructStack = structTypeStack;
					typeCheckBlock(thenBody, thenStack, thenVars, thenStructStack);
					// Don't apply stack effects - the branch might not run
				}
				break;
			}

			case IAstNode::Type::DEFER_STATEMENT: {
				// Defer blocks must have zero net stack effect
				size_t stackSizeBefore = typeStack.size();

				// Type check the defer body
				for (auto* deferChild : child->children()) {
					if (deferChild && deferChild->type() == IAstNode::Type::BLOCK) {
						typeCheckBlock(deferChild, typeStack, localVariables, structTypeStack);
					}
				}

				int deferEffect = static_cast<int>(typeStack.size()) - static_cast<int>(stackSizeBefore);
				if (deferEffect != 0 && !mHasUnpredictableStack) {
					std::string errorMsg = "Stack effect error in 'defer': block must have zero net stack effect, but "
										   "changes stack by ";
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
					reportErrorWithHint(child, "Type error in 'switch': Stack underflow (requires 1 value to match)",
							"push a value before 'switch', e.g. 'x switch { case 1 { ... } }'");
					break;
				}
				// Pop the switch value
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}

				// Analyze all case branches to track stack effects
				AstNodeSwitchStatement* switchStmt = static_cast<AstNodeSwitchStatement*>(child);
				const auto& cases = switchStmt->cases();
				bool hasDefault = false;
				bool allSameEffect = true;
				int commonEffect = 0;
				bool firstCase = true;
				std::vector<StackValueType> firstCaseStack;
				std::vector<std::string> firstCaseStructStack;
				// Track Ok branch separately for fallible switch patterns
				std::vector<StackValueType> okCaseStack;
				std::vector<std::string> okCaseStructStack;
				bool hasOkCase = false;
				bool allDiverge = true;

				for (const auto* caseNode : cases) {
					if (caseNode->isDefault()) {
						hasDefault = true;
					}
					IAstNode* caseBody = caseNode->body();
					if (caseBody) {
						// Analyze case body with a copy of current stack
						std::vector<StackValueType> caseStack = typeStack;
						std::unordered_map<std::string, StackValueType> caseVars = localVariables;
						std::vector<std::string> caseStructStack = structTypeStack;
						typeCheckBlock(caseBody, caseStack, caseVars, caseStructStack);

						int caseEffect = static_cast<int>(caseStack.size()) - static_cast<int>(typeStack.size());

						// Track if this branch diverges (return/panic)
						if (!blockEndsDiverging(caseBody)) {
							allDiverge = false;
						}

						// Detect Ok case for fallible switch patterns.
						// Ok is parsed as literal integer 1 (true).
						if (!caseNode->isDefault() && caseNode->value()) {
							if (caseNode->value()->type() == IAstNode::Type::LITERAL) {
								AstNodeLiteral* caseLit = static_cast<AstNodeLiteral*>(caseNode->value());
								bool isOk = (caseLit->literalType() == AstNodeLiteral::LiteralType::INTEGER &&
													caseLit->value() == "1") ||
											(caseLit->literalType() == AstNodeLiteral::LiteralType::BOOL &&
													(caseLit->value() == "Ok" || caseLit->value() == "true"));
								if (isOk) {
									okCaseStack = caseStack;
									okCaseStructStack = caseStructStack;
									hasOkCase = true;
								}
							}
						}

						if (firstCase) {
							commonEffect = caseEffect;
							firstCaseStack = caseStack;
							firstCaseStructStack = caseStructStack;
							firstCase = false;
						} else if (caseEffect != commonEffect) {
							allSameEffect = false;
						}
					}
				}

				// Apply stack effects from switch branches
				if (hasDefault && !firstCase) {
					if (allDiverge) {
						// All branches diverge (return/panic) — no effect on parent stack
					} else if (allSameEffect && commonEffect != 0) {
						// All branches have the same effect — apply it
						typeStack = firstCaseStack;
						structTypeStack = firstCaseStructStack;
					} else if (!allSameEffect && hasOkCase) {
						// Fallible switch: branches disagree because the Ok branch gets
						// the result value and error branches don't.
						// Use Ok branch's state as authoritative.
						typeStack = okCaseStack;
						structTypeStack = okCaseStructStack;
					}
				}
				break;
			}

			case IAstNode::Type::FOR_STATEMENT: {
				// For loops: pop 3 values (start, end, step), type check body with iterator variable
				AstNodeForStatement* forStmt = static_cast<AstNodeForStatement*>(child);

				// Pop start, end, step from type stack
				for (int popIdx = 0; popIdx < 3; popIdx++) {
					if (!typeStack.empty()) {
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}
				}

				// Type check body with a copy of the state (loop body effects are complex with break/continue)
				std::vector<StackValueType> loopStack = typeStack;
				std::unordered_map<std::string, StackValueType> loopVars = localVariables;
				std::vector<std::string> loopStructStack = structTypeStack;

				// Add iterator variable as INT
				const std::string& iterName = forStmt->iteratorName();
				loopVars[iterName] = StackValueType::INT;

				// Type check the body with error suppression (marks method calls without reporting type errors)
				// This is necessary because loop bodies can have complex stack effects that are hard to analyze
				bool wasInLoopBody = mInLoopBody;
				mInLoopBody = true;
				if (forStmt->body()) {
					typeCheckBlock(forStmt->body(), loopStack, loopVars, loopStructStack);
				}
				mInLoopBody = wasInLoopBody;

				// Don't modify parent stack - loops don't have consistent stack effects
				break;
			}

			case IAstNode::Type::LOOP_STATEMENT: {
				// Infinite loops with break/continue have unpredictable stack effects
				mHasUnpredictableStack = true;
				std::vector<StackValueType> loopStack = typeStack;
				std::unordered_map<std::string, StackValueType> loopVars = localVariables;
				std::vector<std::string> loopStructStack = structTypeStack;

				bool wasInLoopBody = mInLoopBody;
				mInLoopBody = true;
				AstNodeLoopStatement* loopStmt = static_cast<AstNodeLoopStatement*>(child);
				if (loopStmt->body()) {
					typeCheckBlock(loopStmt->body(), loopStack, loopVars, loopStructStack);
				}
				mInLoopBody = wasInLoopBody;

				// Don't modify parent stack
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
				// Supports multiple assignment: -> a b c pops 3 values
				// Special case: -> _ discards the value (like drop)
				AstNodeLocal* local = static_cast<AstNodeLocal*>(child);
				const std::vector<std::string>& varNames = local->names();

				// Process each variable name (in order: first name gets top of stack)
				for (const std::string& varName : varNames) {
					// Special case: _ is discard (drop), not a variable
					if (varName == "_") {
						// Check if stack is empty
						if (typeStack.empty()) {
							reportError(local, "Type error in discard '-> _': Stack underflow (no value to discard)");
							continue;
						}
						// Pop the value type from the stack but don't store it
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
						continue;
					}

					// Check if variable name shadows a function
					if (mDefinedFunctions.find(varName) != mDefinedFunctions.end()) {
						std::string errorMsg = "Local variable '";
						errorMsg += varName;
						errorMsg += "' shadows function with same name";
						reportError(local, errorMsg.c_str());
						continue;
					}

					// Check if stack is empty
					if (typeStack.empty()) {
						std::string errorMsg = "Type error in local variable '";
						errorMsg += varName;
						errorMsg += "': Stack underflow (no value to store)";
						reportError(local, errorMsg.c_str());
						continue;
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
						static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
						if (debug) {
							std::cerr << "[DEBUG TC] Store -> " << varName << " with struct type: " << structType
									  << std::endl;
						}

						// If there's a pending function signature, store it with this variable
						if (mPendingFnSignature.has_value()) {
							mLocalVariableFnSignatures[varName] = mPendingFnSignature.value();
							mPendingFnSignature.reset();
						}
					} else if (!structTypeStack.empty()) {
						structTypeStack.pop_back();
					}
				}
				break;
			}

			case IAstNode::Type::STRUCT_CONSTRUCTION: {
				// Handle struct construction with named fields: StructName { field: expr ... }
				AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(child);
				const std::string& name = construct->structName();
				const auto& fieldInits = construct->fieldInits();

				// Get struct declaration - try local first, then module structs
				// For module structs, we need to look up with qualified key
				AstNodeStructDeclaration* structDecl = nullptr;
				std::string lookupKey = name; // Key for mStructFieldTypes lookup
				auto structDeclIt = mStructDeclarations.find(name);
				if (structDeclIt != mStructDeclarations.end()) {
					structDecl = structDeclIt->second;
				} else {
					// Try qualified lookup first (for qualified names like "vec2::Vec2")
					auto moduleDeclIt = mModuleStructDeclarations.find(name);
					if (moduleDeclIt != mModuleStructDeclarations.end()) {
						structDecl = moduleDeclIt->second;
						lookupKey = name;
					} else if (name.find("::") == std::string::npos) {
						// Unqualified name - search in modules
						for (const auto& modulePair : mModuleStructs) {
							const std::string& moduleName = modulePair.first;
							if (modulePair.second.find(name) != modulePair.second.end()) {
								std::string qualifiedName = moduleName + "::" + name;
								auto qualIt = mModuleStructDeclarations.find(qualifiedName);
								if (qualIt != mModuleStructDeclarations.end()) {
									structDecl = qualIt->second;
									lookupKey = qualifiedName;
									break;
								}
							}
						}
					}
				}

				// Special handling for anonymous error literal: error { code = X message = Y }
				// This pushes message then code onto the stack (for panic to consume)
				if (name == "__error__") {
					// Validate only 'code' and 'message' fields are allowed
					bool hasCode = false;
					bool hasMessage = false;
					for (const auto& fieldInit : fieldInits) {
						const std::string& fieldName = fieldInit.fieldName;
						if (fieldName == "code") {
							hasCode = true;
						} else if (fieldName == "message") {
							hasMessage = true;
						} else {
							std::string errorMsg = "Unknown field '";
							errorMsg += fieldName;
							errorMsg += "' in error literal; only 'code' and 'message' are allowed";
							reportError(construct, errorMsg.c_str());
						}
					}
					if (!hasCode) {
						reportError(construct, "Error literal requires 'code' field");
					}
					if (!hasMessage) {
						reportError(construct, "Error literal requires 'message' field");
					}
					// Pushes message (str) then code (int) onto stack
					typeStack.push_back(StackValueType::STRING);
					structTypeStack.push_back("");
					typeStack.push_back(StackValueType::INT);
					structTypeStack.push_back("");
					break;
				}

				if (structDecl) {
					std::unordered_set<std::string> providedFields;

					// Process each field initializer
					for (const auto& fieldInit : fieldInits) {
						const std::string& fieldName = fieldInit.fieldName;
						providedFields.insert(fieldName);

						// Check if field exists in struct using mStructFieldTypes
						// (don't use structDecl->fields() as it may have been freed for imported modules)
						bool fieldExists = false;
						StackValueType expectedType = StackValueType::UNKNOWN;
						std::string expectedStructType;

						const auto* structFieldTypes = lookupStructFieldTypes(lookupKey);
						if (structFieldTypes != nullptr) {
							auto fieldTypeIt = structFieldTypes->find(fieldName);
							if (fieldTypeIt != structFieldTypes->end()) {
								fieldExists = true;
								expectedType = fieldTypeIt->second;
							}
						}

						// Also check mStructFieldStructTypes for PTR field types
						auto structFieldStructTypesIt = mStructFieldStructTypes.find(lookupKey);
						if (structFieldStructTypesIt != mStructFieldStructTypes.end()) {
							auto fieldStructTypeIt = structFieldStructTypesIt->second.find(fieldName);
							if (fieldStructTypeIt != structFieldStructTypesIt->second.end()) {
								expectedStructType = fieldStructTypeIt->second;
							}
						}

						if (!fieldExists) {
							std::string errorMsg = "Unknown field '";
							errorMsg += fieldName;
							errorMsg += "' in struct '";
							errorMsg += name;
							errorMsg += "'";
							// Try to suggest a similar field name
							if (structFieldTypes != nullptr) {
								std::string suggestion = findSimilarNameInMap(fieldName, *structFieldTypes);
								if (!suggestion.empty()) {
									errorMsg += "; did you mean '";
									errorMsg += suggestion;
									errorMsg += "'?";
								}
							}
							reportError(construct, errorMsg.c_str());
							continue;
						}

						// Process the field's expression nodes to determine the type it produces
						// We create a temporary type stack to track what the expression produces
						std::vector<StackValueType> exprTypeStack;
						std::vector<std::string> exprStructTypeStack;

						for (const auto& exprNodePtr : fieldInit.valueNodes) {
							IAstNode* exprNode = exprNodePtr.get();
							// Recursively type-check the expression node
							// For simplicity, we simulate basic type inference here
							switch (exprNode->type()) {
							case IAstNode::Type::LITERAL: {
								AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(exprNode);
								exprTypeStack.push_back(getLiteralStackType(lit->literalType()));
								exprStructTypeStack.push_back("");
								break;
							}
							case IAstNode::Type::IDENTIFIER: {
								AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(exprNode);
								auto localIt = localVariables.find(ident->name());
								if (localIt != localVariables.end()) {
									exprTypeStack.push_back(localIt->second);
									if (localIt->second == StackValueType::PTR) {
										auto structTypeIt = mLocalVariableStructTypes.find(ident->name());
										if (structTypeIt != mLocalVariableStructTypes.end()) {
											exprStructTypeStack.push_back(structTypeIt->second);
										} else {
											exprStructTypeStack.push_back("");
										}
									} else {
										exprStructTypeStack.push_back("");
									}
								} else {
									exprTypeStack.push_back(StackValueType::UNKNOWN);
									exprStructTypeStack.push_back("");
								}
								break;
							}
							case IAstNode::Type::STRUCT_CONSTRUCTION: {
								// Nested struct construction
								AstNodeStructConstruction* nested = static_cast<AstNodeStructConstruction*>(exprNode);
								exprTypeStack.push_back(StackValueType::PTR);
								// Qualify struct name if from module
								std::string nestedStructName = nested->structName();
								if (nestedStructName.find("::") == std::string::npos) {
									for (const auto& moduleEntry : mModuleStructs) {
										const auto& structs = moduleEntry.second;
										if (structs.find(nestedStructName) != structs.end() &&
												structs.at(nestedStructName)) {
											nestedStructName = moduleEntry.first + "::" + nestedStructName;
											break;
										}
									}
								}
								exprStructTypeStack.push_back(nestedStructName);
								break;
							}
							case IAstNode::Type::ARRAY_LITERAL: {
								// Array literal pushes a pointer with element type info
								AstNodeArrayLiteral* arrLit = static_cast<AstNodeArrayLiteral*>(exprNode);
								exprTypeStack.push_back(StackValueType::PTR);
								if (arrLit->elements().empty()) {
									exprStructTypeStack.push_back("[]any");
								} else {
									IAstNode* firstElem = arrLit->elements()[0].get();
									if (firstElem->type() == IAstNode::Type::LITERAL) {
										auto* lit = static_cast<AstNodeLiteral*>(firstElem);
										switch (lit->literalType()) {
										case AstNodeLiteral::LiteralType::INTEGER:
										case AstNodeLiteral::LiteralType::BOOL:
											exprStructTypeStack.push_back("[]i64");
											break;
										case AstNodeLiteral::LiteralType::FLOAT:
											exprStructTypeStack.push_back("[]f64");
											break;
										case AstNodeLiteral::LiteralType::STRING:
											exprStructTypeStack.push_back("[]str");
											break;
										default:
											exprStructTypeStack.push_back("[]any");
											break;
										}
									} else {
										exprStructTypeStack.push_back("[]any");
									}
								}
								break;
							}
							case IAstNode::Type::INSTRUCTION: {
								// Instructions modify the stack - simplified handling
								AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(exprNode);
								const std::string& instrName = instr->name();
								// Handle common arithmetic operations
								if (instrName == "+" || instrName == "-" || instrName == "*" || instrName == "/" ||
										instrName == "add" || instrName == "sub" || instrName == "mul" ||
										instrName == "div") {
									if (exprTypeStack.size() >= 2) {
										exprTypeStack.pop_back();
										exprStructTypeStack.pop_back();
									}
								}
								break;
							}
							default:
								// For other node types, assume they push one value
								exprTypeStack.push_back(StackValueType::UNKNOWN);
								exprStructTypeStack.push_back("");
								break;
							}
						}

						// After processing expression, check the resulting type
						if (!exprTypeStack.empty()) {
							StackValueType actualType = exprTypeStack.back();
							std::string actualStructType =
									exprStructTypeStack.empty() ? "" : exprStructTypeStack.back();

							// Skip type checking if expected type is a type parameter (for generics)
							bool isGenericField = !expectedStructType.empty() && isCurrentTypeParam(expectedStructType);

							if (!isGenericField && actualType != StackValueType::UNKNOWN &&
									expectedType != StackValueType::UNKNOWN) {
								if (actualType != expectedType) {
									if (isImplicitCastAllowed(actualType, expectedType)) {
										// Only warn for casts that should be warned about
										if (shouldWarnImplicitCast(actualType, expectedType)) {
											std::string warnMsg = "Implicit cast in struct construction '";
											warnMsg += name;
											warnMsg += "': Field '";
											warnMsg += fieldName;
											warnMsg += "' expects ";
											warnMsg += stackValueTypeToString(expectedType);
											warnMsg += ", but got ";
											warnMsg += stackValueTypeToString(actualType);
											reportWarning(construct, warnMsg.c_str());
										}
									} else {
										std::string errorMsg = "Type error in struct construction '";
										errorMsg += name;
										errorMsg += "': Field '";
										errorMsg += fieldName;
										errorMsg += "' expects ";
										errorMsg += expectedStructType.empty() ? stackValueTypeToString(expectedType)
																			   : expectedStructType;
										errorMsg += ", but got ";
										errorMsg += actualStructType.empty() ? stackValueTypeToString(actualType)
																			 : actualStructType;
										reportError(construct, errorMsg.c_str());
									}
								} else if (!expectedStructType.empty() && actualStructType != expectedStructType) {
									std::string errorMsg = "Type error in struct construction '";
									errorMsg += name;
									errorMsg += "': Field '";
									errorMsg += fieldName;
									errorMsg += "' expects ";
									errorMsg += expectedStructType;
									errorMsg += ", but got ";
									errorMsg += actualStructType.empty() ? "ptr" : actualStructType;
									reportError(construct, errorMsg.c_str());
								}
							}
						}
					}

					// Check for missing fields using mStructFieldTypes (safe for imported modules)
					const auto* structFieldTypesForMissing = lookupStructFieldTypes(lookupKey);
					if (structFieldTypesForMissing != nullptr) {
						// Get fields with defaults for this struct
						const std::unordered_set<std::string>* fieldsWithDefaults = nullptr;
						auto defaultsIt = mStructFieldsWithDefaults.find(lookupKey);
						if (defaultsIt != mStructFieldsWithDefaults.end()) {
							fieldsWithDefaults = &defaultsIt->second;
						}

						for (const auto& fieldEntry : *structFieldTypesForMissing) {
							if (providedFields.find(fieldEntry.first) == providedFields.end()) {
								// Check if field has a default value
								bool hasDefault =
										fieldsWithDefaults != nullptr &&
										fieldsWithDefaults->find(fieldEntry.first) != fieldsWithDefaults->end();
								if (!hasDefault) {
									std::string errorMsg = "Missing field '";
									errorMsg += fieldEntry.first;
									errorMsg += "' in struct construction '";
									errorMsg += name;
									errorMsg += "'";
									reportError(construct, errorMsg.c_str());
								}
							}
						}
					}
				}

				// Push pointer type for the constructed struct
				typeStack.push_back(StackValueType::PTR);
				// Use qualified name for method resolution (already computed in lookupKey for module structs)
				if (lookupKey.find("::") != std::string::npos) {
					structTypeStack.push_back(lookupKey);
				} else {
					// Check if struct is from a module - use qualified name
					std::string qualifiedStructName = name;
					for (const auto& moduleEntry : mModuleStructs) {
						const auto& structs = moduleEntry.second;
						if (structs.find(name) != structs.end() && structs.at(name)) {
							qualifiedStructName = moduleEntry.first + "::" + name;
							break;
						}
					}
					structTypeStack.push_back(qualifiedStructName);
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
							static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
							if (debug) {
								std::cerr << "[DEBUG TC] Local var '" << name
										  << "' pushed struct type: " << structTypeIt->second << std::endl;
							}
						} else {
							structTypeStack.push_back(""); // Unknown struct type
							static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
							if (debug) {
								std::cerr << "[DEBUG TC] Local var '" << name
										  << "' has no struct type in mLocalVariableStructTypes" << std::endl;
							}
						}

						// If the variable has a known function signature, set it as pending
						auto fnSigIt = mLocalVariableFnSignatures.find(name);
						if (fnSigIt != mLocalVariableFnSignatures.end()) {
							mPendingFnSignature = fnSigIt->second;
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

				// Module-level `var`. Pushes its declared type on the stack.
				// If the declared type is a struct name, carry the struct type
				// forward so a following `<<field` access resolves to the
				// right layout.
				auto gvIt2 = mGlobalVarTypes.find(name);
				if (gvIt2 != mGlobalVarTypes.end()) {
					const std::string& gvType = gvIt2->second;
					typeStack.push_back(stringToStackValueType(gvType));
					structTypeStack.push_back(isStructTypeName(gvType) ? gvType : "");
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
								const AstNodeStructField* field = fields[fi].get();
								const std::string& fieldName = field->name();

								// Get expected type from mStructFieldTypes
								const auto* structFieldTypesPtr = lookupStructFieldTypes(name);
								if (structFieldTypesPtr == nullptr) {
									continue;
								}
								auto fieldTypeIt = structFieldTypesPtr->find(fieldName);
								if (fieldTypeIt == structFieldTypesPtr->end()) {
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

								// Skip check if expected type is a type parameter (for generics)
								bool isGenericField =
										!expectedStructType.empty() && isCurrentTypeParam(expectedStructType);
								if (isGenericField) {
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
										// Only warn for casts that should be warned about
										if (shouldWarnImplicitCast(actual, expected)) {
											std::string warnMsg = "Implicit cast in struct construction '";
											warnMsg += name;
											warnMsg += "': Field '";
											warnMsg += fieldName;
											warnMsg += "' expects ";
											warnMsg += stackValueTypeToString(expected);
											warnMsg += ", but got ";
											warnMsg += stackValueTypeToString(actual);
											reportWarning(ident, warnMsg.c_str());
										}
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
					const std::string& moduleName = moduleEntry.first;
					const auto& structs = moduleEntry.second;
					if (structs.find(name) != structs.end() && structs.at(name)) {
						// Struct construction from module - use qualified name for lookups
						std::string qualifiedStructName = moduleName + "::" + name;
						// Try to find struct declaration
						auto structDeclIt = mModuleStructDeclarations.find(qualifiedStructName);
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
									const AstNodeStructField* field = fields[fi].get();
									const std::string& fieldName = field->name();

									// Get expected type from mStructFieldTypes
									const auto* structFieldTypesPtr = lookupStructFieldTypes(qualifiedStructName);
									if (structFieldTypesPtr == nullptr) {
										continue;
									}
									auto fieldTypeIt = structFieldTypesPtr->find(fieldName);
									if (fieldTypeIt == structFieldTypesPtr->end()) {
										continue;
									}
									StackValueType expected = fieldTypeIt->second;

									// Check if field expects a struct type (including type parameters)
									std::string expectedStructType;
									auto structFieldStructTypesIt = mStructFieldStructTypes.find(qualifiedStructName);
									if (structFieldStructTypesIt != mStructFieldStructTypes.end()) {
										auto fieldStructTypeIt = structFieldStructTypesIt->second.find(fieldName);
										if (fieldStructTypeIt != structFieldStructTypesIt->second.end()) {
											expectedStructType = fieldStructTypeIt->second;
										}
									}

									// Skip check if expected type is a type parameter (for generics)
									bool isGenericField =
											!expectedStructType.empty() && isCurrentTypeParam(expectedStructType);
									if (isGenericField) {
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
											// Only warn for casts that should be warned about
											if (shouldWarnImplicitCast(actual, expected)) {
												std::string warnMsg = "Implicit cast in struct construction '";
												warnMsg += name;
												warnMsg += "': Field '";
												warnMsg += fieldName;
												warnMsg += "' expects ";
												warnMsg += stackValueTypeToString(expected);
												warnMsg += ", but got ";
												warnMsg += stackValueTypeToString(actual);
												reportWarning(ident, warnMsg.c_str());
											}
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
						structTypeStack.push_back(qualifiedStructName); // Track the qualified struct type
						break;
					}
				}

				// Check if this is a method call on a struct
				// Methods require: receiver pushed first, then parameters on top
				// e.g., for method set(idx, elem): v idx elem set!
				std::string receiverStructType;
				std::string registeredStructType;
				size_t receiverStackIdx = 0;

				{
					static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
					if (debug) {
						std::cerr << "[DEBUG TC] Checking for method call '" << name << "' - structTypeStack: [";
						for (size_t k = 0; k < structTypeStack.size(); k++) {
							if (k > 0) {
								std::cerr << ", ";
							}
							std::cerr << "'" << structTypeStack[k] << "'";
						}
						std::cerr << "]" << std::endl;
					}
				}

				// First pass: find any struct on the stack that has this method
				// to determine the method signature and expected parameter count
				size_t searchLimit = std::min(structTypeStack.size(), typeStack.size());
				for (size_t idx = searchLimit; idx > 0; idx--) {
					const std::string& structType = structTypeStack[idx - 1];
					if (!structType.empty()) {
						std::string potentialRegisteredType = findMethodStructType(structType, name);
						if (!potentialRegisteredType.empty()) {
							registeredStructType = potentialRegisteredType;
							break;
						}
					}
				}
				if (!registeredStructType.empty()) {
					// This is a method call - look up the signature with mangled name
					std::string mangledName = registeredStructType + "::" + name;
					auto methodSigIt = mFunctionSignatures.find(mangledName);
					if (methodSigIt != mFunctionSignatures.end()) {
						const FunctionSignature& sig = methodSigIt->second;

						// Validate '!' and '?' usage
						if ((ident->abortOnError() || ident->propagateOnError()) && !sig.throws) {
							std::string op = ident->abortOnError() ? "!" : "?";
							std::string errorMsg = "Cannot use '" + op + "' operator on method '" + name +
												   "' which is not marked as fallible";
							reportError(ident, errorMsg.c_str());
						}

						// Check '?' requires caller to be fallible
						if (ident->propagateOnError() && !mCurrentFunctionFallible) {
							std::string errorMsg =
									"Cannot use '?' operator on method '" + name +
									"': enclosing function must be fallible (add '!' to function signature)";
							reportError(ident, errorMsg.c_str());
						}

						// Check fallible methods without ! or ? must be followed by 'if' or 'switch'
						if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
							IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
							if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
													 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
								std::string errorMsg =
										"Fallible method '" + name +
										"' must be immediately followed by 'if' or 'switch' to check for "
										"errors, or use '!' to abort on error, or '?' to propagate";
								reportError(ident, errorMsg.c_str());
							}
						}

						// Calculate expected receiver position based on parameter count
						// With receiver-first: receiver should be at position additionalParams from top
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;

						// Second pass: check if there's a valid receiver at the expected position
						if (typeStack.size() <= additionalParams) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += name;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(ident, errorMsg.c_str());
							break;
						}

						size_t expectedReceiverIdx = typeStack.size() - 1 - additionalParams;

						// Verify the struct at expected position has this method
						bool foundValidReceiver = false;
						if (expectedReceiverIdx < structTypeStack.size()) {
							const std::string& structAtExpectedPos = structTypeStack[expectedReceiverIdx];
							std::string actualRegisteredType = findMethodStructType(structAtExpectedPos, name);
							if (!actualRegisteredType.empty()) {
								// Valid receiver-first call
								receiverStructType = structAtExpectedPos;
								registeredStructType = actualRegisteredType;
								receiverStackIdx = expectedReceiverIdx;
								foundValidReceiver = true;
							}
						}

						if (!foundValidReceiver) {
							std::string errorMsg = "Method call '";
							errorMsg += name;
							errorMsg += "': receiver must be pushed first, then ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " parameter(s) on top (e.g., 'receiver";
							for (size_t p = 0; p < additionalParams; p++) {
								errorMsg += " param";
							}
							errorMsg += " ";
							errorMsg += name;
							errorMsg += "')";
							reportError(ident, errorMsg.c_str());
							break;
						}

						// receiverPositionFromTop is now always additionalParams (by construction)
						size_t receiverPositionFromTop = additionalParams;

						if (typeStack.size() < sig.consumes.size()) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += name;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(ident, errorMsg.c_str());
							break;
						}

						// Validate parameter types (skip receiver at index 0)
						// Stack order: [receiver, param1, param2, ..., paramN] with receiver at bottom
						// Signature: [receiver, param1, param2, ..., paramN]
						// For sig index j (1 to N), stack index is: receiverIdx + j
						for (size_t j = 1; j < sig.consumes.size(); j++) {
							size_t stackIdx = receiverStackIdx + j;
							// Bounds check before accessing typeStack
							if (stackIdx >= typeStack.size()) {
								break;
							}
							StackValueType expected = sig.consumes[j];
							StackValueType actual = typeStack[stackIdx];

							if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN) {
								continue;
							}
							if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY) {
								continue;
							}

							if (actual != expected) {
								if (isImplicitCastAllowed(actual, expected)) {
									std::string warnMsg = "Implicit cast in method call '";
									warnMsg += name;
									warnMsg += "': Parameter ";
									warnMsg += std::to_string(j);
									warnMsg += " expects ";
									warnMsg += stackValueTypeToString(expected);
									warnMsg += ", but got ";
									warnMsg += stackValueTypeToString(actual);
									reportWarning(ident, warnMsg.c_str());
								} else {
									std::string errorMsg = "Type error in method call '";
									errorMsg += name;
									errorMsg += "': Parameter ";
									errorMsg += std::to_string(j);
									errorMsg += " expects ";
									errorMsg += stackValueTypeToString(expected);
									errorMsg += ", but got ";
									errorMsg += stackValueTypeToString(actual);
									reportError(ident, errorMsg.c_str());
								}
							}
						}

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values
						if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
							pushProducesTypes(sig, typeStack, structTypeStack);
							typeStack.push_back(StackValueType::INT); // Error status
							structTypeStack.push_back("");
						} else {
							pushProducesTypes(sig, typeStack, structTypeStack);
						}

						// Mark identifier as a method call for code generation
						// Use registeredStructType (the generic type) for proper function name lookup
						{
							static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
							if (debug) {
								std::cerr << "[DEBUG TC] Marking '" << name << "' as method call on "
										  << registeredStructType << std::endl;
							}
						}
						ident->setIsMethodCall(true);
						ident->setReceiverType(registeredStructType);
						ident->setMethodInputParamCount(additionalParams);
						// Use the receiver position calculated earlier (before stack modifications)
						ident->setMethodReceiverPositionFromTop(receiverPositionFromTop);
						break;
					}
				}

				// Check if this is a user-defined function
				auto sigIt = mFunctionSignatures.find(name);
				if (sigIt != mFunctionSignatures.end()) {
					const FunctionSignature& sig = sigIt->second;

					// Validate '!' and '?' usage: only allowed on fallible functions (marked with '!')
					if ((ident->abortOnError() || ident->propagateOnError()) && !sig.throws) {
						std::string op = ident->abortOnError() ? "!" : "?";
						std::string errorMsg = "Cannot use '" + op + "' operator on function '" + name +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(ident, errorMsg.c_str());
					}

					// Check '?' requires caller to be fallible
					if (ident->propagateOnError() && !mCurrentFunctionFallible) {
						std::string errorMsg = "Cannot use '?' operator on function '" + name +
											   "': enclosing function must be fallible (add '!' to function signature)";
						reportError(ident, errorMsg.c_str());
					}

					// Check fallible functions without ! or ? must be followed by 'if' or 'switch'
					if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
						IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
						if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
												 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
							std::string errorMsg = "Fallible function '" + name +
												   "' must be immediately followed by 'if' or 'switch' to check for "
												   "errors, or use '!' to abort on error, or '?' to propagate";
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

						// Skip check if expected type is ANY, UNKNOWN, or TYPEVAR (generic type parameter)
						// TYPEVAR accepts any concrete type at the call site
						if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN ||
								expected == StackValueType::TYPEVAR) {
							continue;
						}

						// Skip check if actual type is UNKNOWN, ANY, or TYPEVAR (can't determine type at compile time)
						// TYPEVAR occurs when a generic type parameter (like T) is passed to a typed function
						if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY ||
								actual == StackValueType::TYPEVAR) {
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
									const auto* structFieldPtr = lookupStructFieldTypes(expectedStructType);
									if (structFieldPtr != nullptr) {
										const auto& availableFields = *structFieldPtr;
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
										const auto* structFieldPtr = lookupStructFieldTypes(actualStructType);
										if (structFieldPtr != nullptr) {
											const auto& availableFields = *structFieldPtr;

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

							if (!actualStruct.empty() &&
									!structTypesMatch(actualStruct, expectedStruct, mMergedModules)) {
								std::string errorMsg = "Type error in function '";
								errorMsg += name;
								errorMsg += "': Parameter ";
								errorMsg += std::to_string(paramIdx + 1);
								errorMsg += " expects type '";
								errorMsg += expectedStruct;
								errorMsg += "' but got '";
								errorMsg += actualStruct;
								errorMsg += "'";
								reportError(ident, errorMsg.c_str());
							} else if (actualStruct.empty() && expectedStruct.size() > 3 &&
									   expectedStruct.substr(0, 3) == "fn(") {
								// Untyped ptr passed where typed fn(...) expected
								std::string warnMsg = "Untyped 'ptr' passed to function '";
								warnMsg += name;
								warnMsg += "': Parameter ";
								warnMsg += std::to_string(paramIdx + 1);
								warnMsg += " expects '";
								warnMsg += expectedStruct;
								warnMsg += "'. Consider adding a type annotation";
								reportWarning(ident, warnMsg.c_str());
							}
						}
					}

					// Check if any parameter is PTR (function pointer) before consuming
					bool consumesPtrParam = false;
					for (const auto& paramType : sig.consumes) {
						if (paramType == StackValueType::PTR) {
							consumesPtrParam = true;
							break;
						}
					}

					// For generic functions: unify type variables with concrete types
					// Track the first concrete type unified with TYPEVAR (simple approach)
					StackValueType unifiedTypeVarType = StackValueType::UNKNOWN;
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						if (sig.consumes[j] == StackValueType::TYPEVAR) {
							size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
							if (stackIdx < typeStack.size()) {
								unifiedTypeVarType = typeStack[stackIdx];
								break; // Use first TYPEVAR's unified type
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

					// Reset pending function signature only if we consumed a PTR parameter -
					// this ensures signatures from input function pointers don't leak to
					// returned function pointers, but preserves signatures for functions
					// that return closures without taking function pointer parameters
					if (consumesPtrParam) {
						mPendingFnSignature.reset();
					}

					// Helper lambda to determine struct type for a produced PTR value
					auto getProducedStructType = [&](size_t produceIdx, StackValueType producedType) -> std::string {
						if (producedType != StackValueType::PTR) {
							return ""; // Only PTR types can have struct types
						}
						// First check if the signature explicitly declares the return struct type
						auto structIt = sig.producesStructTypes.find(produceIdx);
						if (structIt != sig.producesStructTypes.end()) {
							return structIt->second;
						}
						// Fallback: Conservative heuristic: if we consumed PTR parameters and are producing PTR values,
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

					// Helper to resolve TYPEVAR to unified concrete type
					auto resolveTypeVar = [&](StackValueType type) -> StackValueType {
						if (type == StackValueType::TYPEVAR && unifiedTypeVarType != StackValueType::UNKNOWN) {
							return unifiedTypeVarType;
						}
						return type;
					};

					// Apply the produces effect
					if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
						// func without ! or ? - pushes result + error flag
						for (size_t idx = 0; idx < sig.produces.size(); idx++) {
							StackValueType type = resolveTypeVar(sig.produces[idx]);
							typeStack.push_back(type);
							structTypeStack.push_back(getProducedStructType(idx, type));
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					} else {
						// Normal call or func!
						for (size_t idx = 0; idx < sig.produces.size(); idx++) {
							StackValueType type = resolveTypeVar(sig.produces[idx]);
							typeStack.push_back(type);
							structTypeStack.push_back(getProducedStructType(idx, type));
						}
					}
				}
				// If it's not a user function, it could be unresolved — stack effects unknown
				mHasUnpredictableStack = true;
				break;
			}

			case IAstNode::Type::FIELD_ACCESS: {
				// Field access: varName @fieldName - pushes the field value onto stack
				AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(child);
				const std::string& varName = fieldAccess->varName();
				const std::string& fieldName = fieldAccess->fieldName();

				// Stack-based field access (empty varName): pops struct from stack, pushes field
				// This occurs after 'as' casts: "c as Type @field"
				if (varName.empty()) {
					// Pop the struct value from the stack
					if (!typeStack.empty()) {
						// Get struct type from the stack to look up field type
						std::string structType = "";
						if (!structTypeStack.empty()) {
							structType = structTypeStack.back();
						}
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}

						// Look up field type if we know the struct type
						StackValueType fieldType = StackValueType::UNKNOWN;
						std::string fieldStructType = "";
						if (!structType.empty()) {
							const auto* structFieldPtr = lookupStructFieldTypes(structType);
							if (structFieldPtr != nullptr) {
								auto fieldIt = structFieldPtr->find(fieldName);
								if (fieldIt != structFieldPtr->end()) {
									fieldType = fieldIt->second;
									// Check if field is a struct type
									auto fstIt = mStructFieldStructTypes.find(structType);
									if (fstIt != mStructFieldStructTypes.end()) {
										auto nameIt = fstIt->second.find(fieldName);
										if (nameIt != fstIt->second.end()) {
											fieldStructType = nameIt->second;
										}
									}
								}
							}
						}
						if (fieldType == StackValueType::UNKNOWN) {
							// Unknown struct type — search all structs for this field
							for (const auto& structEntry : mStructFieldTypes) {
								auto it = structEntry.second.find(fieldName);
								if (it != structEntry.second.end()) {
									fieldType = it->second;
									break;
								}
							}
						}
						typeStack.push_back(fieldType);
						structTypeStack.push_back(fieldStructType);
					} else {
						typeStack.push_back(StackValueType::UNKNOWN);
						structTypeStack.push_back("");
					}
					break;
				}

				// Special handling for global error access: error @code or error @message
				if (varName == "__global_error__") {
					if (fieldName == "code") {
						typeStack.push_back(StackValueType::INT);
						structTypeStack.push_back("");
					} else if (fieldName == "message") {
						typeStack.push_back(StackValueType::STRING);
						structTypeStack.push_back("");
					} else {
						std::string errorMsg = "Unknown field '";
						errorMsg += fieldName;
						errorMsg += "' in error; only 'code' and 'message' are available";
						reportError(fieldAccess, errorMsg.c_str());
						typeStack.push_back(StackValueType::ANY);
						structTypeStack.push_back("");
					}
					break;
				}

				// Check if varName is a struct type name (inline struct field access)
				// e.g., "100 200 IntPair @x" - IntPair is a struct type, not a variable
				// We distinguish inline construction from accessing an existing struct by checking:
				// - If structTypeStack.back() == varName, the struct is already constructed (no pop)
				// - Otherwise, the field values are on the stack waiting for construction (pop them)
				const auto* inlineStructFields = lookupStructFieldTypes(varName);
				if (inlineStructFields != nullptr) {
					const auto& fields = *inlineStructFields;
					bool isExistingStruct = !structTypeStack.empty() && structTypeStack.back() == varName;

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
						errorMsg += " <<";
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
					const auto* structFieldPtr = lookupStructFieldTypes(structType);
					if (structFieldPtr != nullptr) {
						const auto& fields = *structFieldPtr;
						auto fieldIt = fields.find(fieldName);
						if (fieldIt != fields.end()) {
							fieldType = fieldIt->second;
							fieldFound = true;
						} else {
							// Field doesn't exist on this struct type!
							std::string errorMsg = "Type error in field access '";
							errorMsg += varName;
							errorMsg += " <<";
							errorMsg += fieldName;
							errorMsg += "': Struct '";
							errorMsg += structType;
							errorMsg += "' has no field named '";
							errorMsg += fieldName;
							errorMsg += "'";
							// Suggest a similar field name
							std::string suggestion = findSimilarNameInMap(fieldName, fields);
							if (!suggestion.empty()) {
								errorMsg += "; did you mean '";
								errorMsg += suggestion;
								errorMsg += "'?";
							}
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
						errorMsg += " <<";
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
							errorMsg += " <<";
							errorMsg += fieldName;
							errorMsg += "': Unknown variable or field";
							// Try to suggest a similar variable name first
							std::string varSuggestion = findSimilarNameInMap(varName, localVariables);
							if (!varSuggestion.empty()) {
								errorMsg += "; did you mean variable '";
								errorMsg += varSuggestion;
								errorMsg += "'?";
							} else {
								// Try to find a similar field name across all structs
								std::unordered_set<std::string> allFields;
								for (const auto& structEntry : mStructFieldTypes) {
									for (const auto& field : structEntry.second) {
										allFields.insert(field.first);
									}
								}
								std::string fieldSuggestion = findSimilarName(fieldName, allFields);
								if (!fieldSuggestion.empty()) {
									errorMsg += "; did you mean field '";
									errorMsg += fieldSuggestion;
									errorMsg += "'?";
								}
							}
							reportError(fieldAccess, errorMsg.c_str());
							fieldType = StackValueType::ANY;
						}
					}
				}

				// Push the field type onto the stack
				typeStack.push_back(fieldType);

				// If this field is a struct-typed field, push its struct type for chaining
				std::string fieldStructType = "";
				if (fieldType == StackValueType::PTR && !structType.empty()) {
					auto fieldStructIt = mStructFieldStructTypes.find(structType);
					if (fieldStructIt != mStructFieldStructTypes.end()) {
						auto structNameIt = fieldStructIt->second.find(fieldName);
						if (structNameIt != fieldStructIt->second.end()) {
							fieldStructType = structNameIt->second;
						}
					}
				}
				structTypeStack.push_back(fieldStructType);
				static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
				if (debug) {
					std::cerr << "[DEBUG TC] Field access " << varName << " <<" << fieldName
							  << " -> struct type: " << fieldStructType << " (var struct type was: " << structType
							  << ")" << std::endl;
				}
				break;
			}

			case IAstNode::Type::FIELD_SET: {
				// Field set: <<field (stack-based)
				AstNodeFieldSet* fieldSet = static_cast<AstNodeFieldSet*>(child);
				const std::string& varName = fieldSet->varName();
				const std::string& fieldName = fieldSet->fieldName();

				// Look up which struct type this variable holds
				std::string structType = "";
				if (varName.empty()) {
					// Stack-based field set: struct type from struct type stack
					if (!structTypeStack.empty()) {
						// Value is on top, struct below — peek at second from top
						if (structTypeStack.size() >= 2) {
							structType = structTypeStack[structTypeStack.size() - 2];
						}
					}
				} else {
					auto structTypeIt = mLocalVariableStructTypes.find(varName);
					if (structTypeIt != mLocalVariableStructTypes.end()) {
						structType = structTypeIt->second;
					}
				}

				// Validate the field exists on this struct type
				bool fieldFound = false;

				if (!structType.empty()) {
					// We know which struct type this is - validate against it
					const auto* structFieldPtr = lookupStructFieldTypes(structType);
					if (structFieldPtr != nullptr) {
						const auto& fields = *structFieldPtr;
						auto fieldIt = fields.find(fieldName);
						if (fieldIt != fields.end()) {
							fieldFound = true;
						} else {
							// Field doesn't exist on this struct type!
							std::string errorMsg = "Field '";
							errorMsg += fieldName;
							errorMsg += "' does not exist in struct '";
							errorMsg += structType;
							errorMsg += "'";
							// Suggest a similar field name
							std::string suggestion = findSimilarNameInMap(fieldName, fields);
							if (!suggestion.empty()) {
								errorMsg += "; did you mean '";
								errorMsg += suggestion;
								errorMsg += "'?";
							}
							reportError(fieldSet, errorMsg.c_str());
						}
					}
				} else {
					// Variable is not a known struct type
					auto localIt = localVariables.find(varName);
					if (localIt != localVariables.end() &&
							(localIt->second == StackValueType::INT || localIt->second == StackValueType::FLOAT ||
									localIt->second == StackValueType::STRING)) {
						// Variable is definitely a scalar type - this is an error
						std::string errorMsg = "Variable '";
						errorMsg += varName;
						errorMsg += "' is not a struct type";
						reportError(fieldSet, errorMsg.c_str());
					} else {
						// Variable is either a pointer or unknown - search all structs
						for (const auto& structEntry : mStructFieldTypes) {
							const auto& fields = structEntry.second;
							auto it = fields.find(fieldName);
							if (it != fields.end()) {
								fieldFound = true;
								break;
							}
						}

						if (!fieldFound) {
							std::string errorMsg = "Unknown variable '";
							errorMsg += varName;
							errorMsg += "' or field '";
							errorMsg += fieldName;
							errorMsg += "'";
							// Try to suggest a similar variable name first
							std::string varSuggestion = findSimilarNameInMap(varName, localVariables);
							if (!varSuggestion.empty()) {
								errorMsg += "; did you mean variable '";
								errorMsg += varSuggestion;
								errorMsg += "'?";
							} else {
								// Try to find a similar field name across all structs
								std::unordered_set<std::string> allFields;
								for (const auto& structEntry : mStructFieldTypes) {
									for (const auto& field : structEntry.second) {
										allFields.insert(field.first);
									}
								}
								std::string fieldSuggestion = findSimilarName(fieldName, allFields);
								if (!fieldSuggestion.empty()) {
									errorMsg += "; did you mean field '";
									errorMsg += fieldSuggestion;
									errorMsg += "'?";
								}
							}
							reportError(fieldSet, errorMsg.c_str());
						}
					}
				}

				// Pop the value being assigned from the stack
				if (varName.empty()) {
					if (!typeStack.empty()) {
						typeStack.pop_back(); // pop value
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}
					// >>field! also pops the struct
					if (fieldSet->noReturn()) {
						if (!typeStack.empty()) {
							typeStack.pop_back(); // pop struct
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}
					}
				} else {
					if (!typeStack.empty()) {
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}
				}
				break;
			}

			case IAstNode::Type::SCOPED_IDENTIFIER: {
				// Handle module constants, structs, or function calls
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);

				// Resolve sb::append_any at type-check time based on the type stack
				// This is generated by $"..." string interpolation
				// Use mResolvedInterpolations to only resolve each node once
				if (scoped->scope() == "sb" && scoped->name() == "append_any" &&
						mResolvedInterpolations.find(scoped) == mResolvedInterpolations.end()) {
					mResolvedInterpolations.insert(scoped);
					if (!typeStack.empty() && typeStack.back() == StackValueType::STRING) {
						scoped->setName("append");
					} else {
						scoped->setName("append_int");
					}
				}

				const std::string& moduleName = scoped->scope();
				const std::string& functionName = scoped->name();
				std::string qualifiedName = moduleName + "::" + functionName;

				// Imported C functions and variadic stdlib functions may have signatures
				// that don't reflect all consumed values. Mark stack as unpredictable.
				if (mImportedLibraryFunctions.find(qualifiedName) != mImportedLibraryFunctions.end() ||
						qualifiedName == "fmt::printf" || qualifiedName == "fmt::sprintf" ||
						qualifiedName == "flag::parse") {
					mHasUnpredictableStack = true;
				}

				// Check if this is a local enum variant (e.g., Color::Red)
				if (mDefinedEnums.find(moduleName) != mDefinedEnums.end()) {
					if (mConstantValues.find(qualifiedName) != mConstantValues.end()) {
						typeStack.push_back(StackValueType::INT);
						structTypeStack.push_back("");
						break;
					}
				}

				// Also check local constant values directly (for enum variants in module files)
				if (mConstantValues.find(qualifiedName) != mConstantValues.end()) {
					typeStack.push_back(StackValueType::INT);
					structTypeStack.push_back("");
					break;
				}

				// Self-qualified module constant. `time.qd` declares
				// `import "libtime.a" as "time"` and reads its own `pub const Millisecond` as
				// `time::Millisecond`; the constant is registered under the bare name, so the
				// qualified lookup above misses it. Guarded on the scope being an FFI namespace
				// so this cannot swallow a genuinely unresolved `othermodule::Name`. Without it
				// the reference pushed nothing and the following `/` reported a spurious
				// "Stack underflow (requires 2 numeric values)".
				if (mImportedLibraries.find(moduleName) != mImportedLibraries.end()) {
					auto selfConstIt = mConstantValues.find(functionName);
					if (selfConstIt != mConstantValues.end()) {
						const std::string& value = selfConstIt->second;
						if (!value.empty() && value[0] == '"') {
							typeStack.push_back(StackValueType::STRING);
						} else if (value.find('.') != std::string::npos) {
							typeStack.push_back(StackValueType::FLOAT);
						} else {
							typeStack.push_back(StackValueType::INT);
						}
						structTypeStack.push_back("");
						break;
					}
				}

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
						// Use qualified name for lookup since module structs are stored with qualified keys
						const auto* structFieldTypesPtr = lookupStructFieldTypes(qualifiedName);
						auto structFieldOrderIt = mStructFieldOrder.find(qualifiedName);

						if (structFieldTypesPtr != nullptr && structFieldOrderIt != mStructFieldOrder.end()) {
							const auto& fieldTypes = *structFieldTypesPtr;
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
											// Only warn for casts that should be warned about
											if (shouldWarnImplicitCast(actual, expected)) {
												std::string warnMsg = "Implicit cast in struct construction '";
												warnMsg += qualifiedName;
												warnMsg += "': Field '";
												warnMsg += fieldName;
												warnMsg += "' expects ";
												warnMsg += stackValueTypeToString(expected);
												warnMsg += ", but got ";
												warnMsg += stackValueTypeToString(actual);
												reportWarning(scoped, warnMsg.c_str());
											}
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

				// Check if this is an explicit method call (StructType::method)
				// The scope could be a struct type name
				if ((mDefinedStructs.count(moduleName) || mStructMethods.count(moduleName)) &&
						mStructMethods.count(moduleName) && mStructMethods.at(moduleName).count(functionName)) {
					// Explicit method call - check that stack top matches the receiver type
					std::string receiverStructType;
					if (!typeStack.empty() && typeStack.back() == StackValueType::PTR && !structTypeStack.empty()) {
						receiverStructType = structTypeStack.back();
					}

					// Verify receiver type matches the explicit type (or is a subtype in the future)
					if (!receiverStructType.empty() && receiverStructType != moduleName) {
						std::string errorMsg = "Type error: Explicit method call '";
						errorMsg += qualifiedName;
						errorMsg += "' expects receiver of type '";
						errorMsg += moduleName;
						errorMsg += "' but stack top is '";
						errorMsg += receiverStructType;
						errorMsg += "'";
						reportError(scoped, errorMsg.c_str());
					}

					// Look up method signature
					auto methodSigIt = mFunctionSignatures.find(qualifiedName);
					if (methodSigIt != mFunctionSignatures.end()) {
						const FunctionSignature& sig = methodSigIt->second;

						// Validate '!' and '?' usage
						if ((scoped->abortOnError() || scoped->propagateOnError()) && !sig.throws) {
							std::string op = scoped->abortOnError() ? "!" : "?";
							std::string errorMsg = "Cannot use '" + op + "' operator on method '" + functionName +
												   "' which is not marked as fallible";
							reportError(scoped, errorMsg.c_str());
						}

						// Check '?' requires caller to be fallible
						if (scoped->propagateOnError() && !mCurrentFunctionFallible) {
							std::string errorMsg =
									"Cannot use '?' operator on method '" + functionName +
									"': enclosing function must be fallible (add '!' to function signature)";
							reportError(scoped, errorMsg.c_str());
						}

						// Check fallible methods without ! or ? must be followed by 'if' or 'switch'
						if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
							IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
							if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
													 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
								std::string errorMsg =
										"Fallible method '" + functionName +
										"' must be immediately followed by 'if' or 'switch' to check for "
										"errors, or use '!' to abort on error, or '?' to propagate";
								reportError(scoped, errorMsg.c_str());
							}
						}

						// Check if stack has enough values (receiver + params)
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;
						if (typeStack.size() < 1 + additionalParams) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += functionName;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(scoped, errorMsg.c_str());
							break;
						}

						// Validate parameter types (skip receiver at index 0)
						// Stack order: [param1, param2, ..., paramN, receiver]
						// Signature: [receiver, param1, param2, ..., paramN]
						// For sig index j (1 to N), stack index is: size - consumes.size() + (j-1)
						for (size_t j = 1; j < sig.consumes.size(); j++) {
							size_t stackIdx = typeStack.size() - sig.consumes.size() + (j - 1);
							StackValueType expected = sig.consumes[j];
							StackValueType actual = typeStack[stackIdx];

							if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN) {
								continue;
							}
							if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY) {
								continue;
							}

							if (actual != expected) {
								if (isImplicitCastAllowed(actual, expected)) {
									std::string warnMsg = "Implicit cast in method call '";
									warnMsg += functionName;
									warnMsg += "': Parameter ";
									warnMsg += std::to_string(j);
									warnMsg += " expects ";
									warnMsg += stackValueTypeToString(expected);
									warnMsg += ", but got ";
									warnMsg += stackValueTypeToString(actual);
									reportWarning(scoped, warnMsg.c_str());
								} else {
									std::string errorMsg = "Type error in method call '";
									errorMsg += functionName;
									errorMsg += "': Parameter ";
									errorMsg += std::to_string(j);
									errorMsg += " expects ";
									errorMsg += stackValueTypeToString(expected);
									errorMsg += ", but got ";
									errorMsg += stackValueTypeToString(actual);
									reportError(scoped, errorMsg.c_str());
								}
							}
						}

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values
						if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
							pushProducesTypes(sig, typeStack, structTypeStack);
							typeStack.push_back(StackValueType::INT);
							structTypeStack.push_back("");
						} else {
							pushProducesTypes(sig, typeStack, structTypeStack);
						}

						// Mark as method call for code generation
						scoped->setIsMethodCall(true);
						scoped->setMethodInputParamCount(additionalParams);
						// For explicit method calls, receiver is always on top of stack
						scoped->setMethodReceiverPositionFromTop(0);
						break;
					}
				}

				// Check if this is a module method call (e.g., sb::append on a StringBuilder)
				// Search the type stack for a struct from this module at the expected depth
				if (!typeStack.empty() && !structTypeStack.empty()) {
					// Try to find a method with this name on any struct from this module
					// by searching the structTypeStack from top down
					bool foundModuleMethod = false;
					for (size_t searchDepth = 0; searchDepth < typeStack.size() && !foundModuleMethod; searchDepth++) {
						size_t stackIdx = typeStack.size() - 1 - searchDepth;
						if (typeStack[stackIdx] != StackValueType::PTR) {
							continue;
						}
						if (stackIdx >= structTypeStack.size()) {
							continue;
						}
						std::string receiverStructType = structTypeStack[stackIdx];
						if (receiverStructType.empty()) {
							continue;
						}

						// Check if receiver struct belongs to this module
						bool isModuleStruct = receiverStructType.find(moduleName + "::") == 0;
						if (!isModuleStruct) {
							std::string qualified = moduleName + "::" + receiverStructType;
							if (mStructMethods.count(qualified) || mStructMethods.count(receiverStructType)) {
								isModuleStruct = true;
							}
						}
						if (!isModuleStruct) {
							continue;
						}

						// Find the registered struct type for method lookup
						std::string registeredStructType = findMethodStructType(receiverStructType, functionName);
						if (registeredStructType.empty() && receiverStructType.find("::") == std::string::npos) {
							registeredStructType =
									findMethodStructType(moduleName + "::" + receiverStructType, functionName);
						}
						if (registeredStructType.empty()) {
							continue;
						}

						// Found a method — verify parameter count matches receiver position
						std::string methodQualifiedName = registeredStructType + "::" + functionName;
						auto methodSigIt = mFunctionSignatures.find(methodQualifiedName);
						if (methodSigIt == mFunctionSignatures.end()) {
							continue;
						}
						const FunctionSignature& sig = methodSigIt->second;
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;

						// Receiver should be at exactly additionalParams from top
						if (searchDepth != additionalParams) {
							continue;
						}

						// Validate '!' and '?' usage
						if ((scoped->abortOnError() || scoped->propagateOnError()) && !sig.throws) {
							std::string op = scoped->abortOnError() ? "!" : "?";
							std::string errorMsg = "Cannot use '" + op + "' operator on method '" + functionName +
												   "' which is not marked as fallible";
							reportError(scoped, errorMsg.c_str());
						}

						// Check '?' requires caller to be fallible
						if (scoped->propagateOnError() && !mCurrentFunctionFallible) {
							std::string errorMsg =
									"Cannot use '?' operator on method '" + functionName +
									"': enclosing function must be fallible (add '!' to function signature)";
							reportError(scoped, errorMsg.c_str());
						}

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values
						pushProducesTypes(sig, typeStack, structTypeStack);

						// Mark as method call for code generation
						scoped->setIsMethodCall(true);
						scoped->setReceiverType(registeredStructType);
						scoped->setMethodInputParamCount(additionalParams);
						scoped->setMethodReceiverPositionFromTop(additionalParams);
						foundModuleMethod = true;
						break;
					}
					if (foundModuleMethod) {
						break;
					}
				}

				// Look up the module function signature
				auto sigIt = mFunctionSignatures.find(qualifiedName);
				if (sigIt != mFunctionSignatures.end()) {
					const FunctionSignature& sig = sigIt->second;

					// Validate '!' and '?' usage: only allowed on fallible functions (marked with '!')
					if ((scoped->abortOnError() || scoped->propagateOnError()) && !sig.throws) {
						std::string op = scoped->abortOnError() ? "!" : "?";
						std::string errorMsg = "Cannot use '" + op + "' operator on function '" + qualifiedName +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(scoped, errorMsg.c_str());
					}

					// Check '?' requires caller to be fallible
					if (scoped->propagateOnError() && !mCurrentFunctionFallible) {
						std::string errorMsg = "Cannot use '?' operator on function '" + qualifiedName +
											   "': enclosing function must be fallible (add '!' to function signature)";
						reportError(scoped, errorMsg.c_str());
					}

					// Check fallible functions without ! or ? must be followed by 'if' or 'switch'
					if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
						IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
						if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
												 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
							std::string errorMsg = "Fallible function '" + qualifiedName +
												   "' must be immediately followed by 'if' or 'switch' to check for "
												   "errors, or use '!' to abort on error, or '?' to propagate";
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

						// Skip check if expected type is ANY, UNKNOWN, or TYPEVAR (generic type parameter)
						// TYPEVAR accepts any concrete type at the call site
						if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN ||
								expected == StackValueType::TYPEVAR) {
							continue;
						}

						// Skip check if actual type is UNKNOWN, ANY, or TYPEVAR (can't determine type at compile time)
						// TYPEVAR occurs when a generic type parameter (like T) is passed to a typed function
						if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY ||
								actual == StackValueType::TYPEVAR) {
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

					// Check if any parameter is PTR (function pointer) before consuming
					bool consumesPtrParam = false;
					for (const auto& paramType : sig.consumes) {
						if (paramType == StackValueType::PTR) {
							consumesPtrParam = true;
							break;
						}
					}

					// For generic functions: unify type variables with concrete types
					// Track the first concrete type unified with TYPEVAR (simple approach)
					StackValueType unifiedTypeVarType = StackValueType::UNKNOWN;
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						if (sig.consumes[j] == StackValueType::TYPEVAR) {
							size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
							if (stackIdx < typeStack.size()) {
								unifiedTypeVarType = typeStack[stackIdx];
								break; // Use first TYPEVAR's unified type
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

					// Reset pending function signature only if we consumed a PTR parameter -
					// this ensures signatures from input function pointers don't leak to
					// returned function pointers, but preserves signatures for functions
					// that return closures without taking function pointer parameters
					if (consumesPtrParam) {
						mPendingFnSignature.reset();
					}

					// Helper to resolve TYPEVAR to unified concrete type
					auto resolveTypeVar = [&](StackValueType type) -> StackValueType {
						if (type == StackValueType::TYPEVAR && unifiedTypeVarType != StackValueType::UNKNOWN) {
							return unifiedTypeVarType;
						}
						return type;
					};

					// Apply the produces effect
					if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
						// func without ! or ? - pushes result + error flag
						for (size_t idx = 0; idx < sig.produces.size(); idx++) {
							StackValueType type = resolveTypeVar(sig.produces[idx]);
							typeStack.push_back(type);
							auto structIt = sig.producesStructTypes.find(idx);
							structTypeStack.push_back(
									structIt != sig.producesStructTypes.end() ? structIt->second : "");
						}
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					} else {
						// Normal call or func!
						for (size_t idx = 0; idx < sig.produces.size(); idx++) {
							StackValueType type = resolveTypeVar(sig.produces[idx]);
							typeStack.push_back(type);
							auto structIt = sig.producesStructTypes.find(idx);
							structTypeStack.push_back(
									structIt != sig.producesStructTypes.end() ? structIt->second : "");
						}
					}
				}
				// If signature not found, module wasn't loaded or analyzed
				// Stack effects are unknown — skip strict output validation
				mHasUnpredictableStack = true;
				break;
			}

			case IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
				// Function pointer references push a pointer type onto the stack
				AstNodeFunctionPointerReference* fnRef = static_cast<AstNodeFunctionPointerReference*>(child);
				typeStack.push_back(StackValueType::PTR);
				// Look up the function's signature and build fn type string
				std::string fnTypeStr = "";
				auto sigIt = mFunctionSignatures.find(fnRef->functionName());
				if (sigIt != mFunctionSignatures.end()) {
					mPendingFnSignature = sigIt->second;
					fnTypeStr = buildFnTypeString(sigIt->second);
				}
				structTypeStack.push_back(fnTypeStr);
				break;
			}

			case IAstNode::Type::ANONYMOUS_FUNCTION: {
				// Anonymous functions push a function pointer onto the stack
				AstNodeAnonymousFunction* anonFunc = static_cast<AstNodeAnonymousFunction*>(child);

				// Extract the function signature from input/output parameters
				FunctionSignature sig;
				for (const auto& paramNode : anonFunc->inputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
					sig.consumes.push_back(stringToStackValueType(param->typeString()));
				}
				for (const auto& paramNode : anonFunc->outputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
					sig.produces.push_back(stringToStackValueType(param->typeString()));
				}
				std::string fnTypeStr = buildFnTypeString(sig);

				// Store as pending signature for subsequent LOCAL or call
				mPendingFnSignature = sig;

				typeStack.push_back(StackValueType::PTR);
				structTypeStack.push_back(fnTypeStr);
				break;
			}

			case IAstNode::Type::RETURN_STATEMENT: {
				// Validate that the stack has the expected number of return values
				if (mCurrentFunctionOutputCount > 0 && typeStack.size() < mCurrentFunctionOutputCount) {
					std::string errorMsg = "'return' in function expecting ";
					errorMsg += std::to_string(mCurrentFunctionOutputCount);
					errorMsg += " return value";
					if (mCurrentFunctionOutputCount > 1) {
						errorMsg += "s";
					}
					errorMsg += ", but stack has ";
					errorMsg += std::to_string(typeStack.size());
					reportError(child, errorMsg.c_str());
				}
				// Stop processing this block — code after return is unreachable
				return;
			}

			case IAstNode::Type::AS_CAST: {
				// Type narrowing cast: updates struct type on top of stack
				// No push/pop — just changes the type annotation
				AstNodeAsCast* asCast = static_cast<AstNodeAsCast*>(child);
				if (!structTypeStack.empty()) {
					structTypeStack.back() = asCast->typeName();
				}
				break;
			}

			default:
				// Other node types don't affect the type stack
				break;
			}
		}
	}

	// typeCheckInstruction and typeCheckInstructionInternal are in semantic_validator_instructions.cc

} // namespace Qd

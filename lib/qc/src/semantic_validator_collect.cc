#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_conditional_break.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_defer.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_global_var.h>
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
#include <quadrate/qc/ast_node_type_alias.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/numeric_literal.h>
#include <quadrate/qc/semantic_validator.h>
#include <unordered_set>

namespace Qd {

#include "semantic_validator_internal.h"

	// Check if a type string is a known struct name (local or imported)
	// Supports both unqualified names (Response) and qualified names (http::Response)

	bool SemanticValidator::checkDeclName(IAstNode* node, const std::string& name, const std::string& usedAs) {
		if (!isReservedKeyword(name)) {
			return true;
		}
		reportError(node, ("'" + name + "' is a reserved keyword and cannot be used as " + usedAs).c_str());
		return false;
	}

	void SemanticValidator::collectDefinitions(IAstNode* node) {
		if (!node) {
			return;
		}

		bool walkChildren = true;
		switch (node->type()) {
		case IAstNode::Type::FUNCTION_DECLARATION:
			walkChildren = collectFunctionDefinition(static_cast<AstNodeFunctionDeclaration*>(node));
			break;
		case IAstNode::Type::CONSTANT_DECLARATION:
			walkChildren = collectConstantDefinition(static_cast<AstNodeConstant*>(node));
			break;
		case IAstNode::Type::GLOBAL_VAR_DECLARATION:
			walkChildren = collectGlobalVarDefinition(static_cast<AstNodeGlobalVar*>(node));
			break;
		case IAstNode::Type::ENUM_DECLARATION:
			walkChildren = collectEnumDefinition(static_cast<AstNodeEnumDeclaration*>(node));
			break;
		case IAstNode::Type::TYPE_ALIAS_DECLARATION:
			walkChildren = collectTypeAliasDefinition(static_cast<AstNodeTypeAlias*>(node));
			break;
		case IAstNode::Type::STRUCT_DECLARATION:
			walkChildren = collectStructDefinition(static_cast<AstNodeStructDeclaration*>(node));
			break;
		case IAstNode::Type::TEST_DECLARATION:
			walkChildren = collectTestDefinition(static_cast<AstNodeTest*>(node));
			break;
		case IAstNode::Type::USE_STATEMENT:
			collectUseStatement(static_cast<AstNodeUse*>(node));
			break;
		case IAstNode::Type::IMPORT_STATEMENT:
			collectImportStatement(static_cast<AstNodeImport*>(node));
			break;
		default:
			break;
		}
		if (!walkChildren) {
			return;
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectDefinitions(child);
		}
	}

	// A function declaration goes into the symbol table
	bool SemanticValidator::collectFunctionDefinition(AstNodeFunctionDeclaration* func) {
		if (!checkDeclName(func, func->name(), "a function name")) {
			return false;
		}
		// A call by this name would always run the builtin, and the function never. A method
		// is dispatched on its receiver, and a module's function is called qualified,
		// `mem::free`, so either may share a builtin's name.
		if (!mIsModuleFile && !func->hasReceiver() && isBuiltInInstruction(func->name().c_str())) {
			reportError(func, ("'" + func->name() +
									  "' is a builtin, and a function cannot take its name: a call to it would run "
									  "the builtin")
									  .c_str());
			return false;
		}

		// Check parameter names for reserved keywords
		for (const auto& paramNode : func->inputParameters()) {
			AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
			if (param->hasName()) {
				checkDeclName(param, param->name(), "a parameter name");
			}
		}
		for (const auto& paramNode : func->outputParameters()) {
			AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
			if (param->hasName()) {
				checkDeclName(param, param->name(), "a parameter name");
			}
		}

		// Whether an input is bound to a local is carried by `def`, rather than `fn`, not by
		// whether someone wrote a name. With it every input must be named, since each is
		// bound; without it the values stay on the stack and the names are documentation,
		// so naming some and not others is merely partial documentation and is allowed. A
		// receiver is bound either way.
		if (!func->isStack()) {
			if (func->inputParameters().empty()) {
				std::string errorMsg =
						"'def " + func->name() + "' has no inputs to bind; write 'fn " + func->name() + "'";
				reportError(func, errorMsg.c_str());
			}
			for (const auto& paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				if (!param->hasName()) {
					std::string errorMsg = "Unnamed input parameter '" + param->typeString() + "' in 'def " +
										   func->name() +
										   "', which binds every input: name it ('value:" + param->typeString() +
										   "'), or write 'fn " + func->name() +
										   "' to leave the arguments "
										   "on the stack for the body to work on";
					reportError(param, errorMsg.c_str());
				}
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
				return false;
			}

			// Check for duplicate method
			if (mStructMethods.count(structType) && mStructMethods[structType].count(methodName)) {
				std::string errorMsg =
						"Duplicate method definition: '" + methodName + "' for struct '" + structType + "'";
				reportError(func, errorMsg.c_str());
				return false;
			}

			// Register the method
			mStructMethods[structType][methodName] = func->isPublic();
			mStructMethodDecls[structType][methodName] = func;

			// Also register with mangled name for code generation lookup
			std::string mangledName = structType + "::" + methodName;
			mDefinedFunctions.insert(mangledName);
			return false;
		}

		// Check for duplicate function name
		if (mDefinedFunctions.find(func->name()) != mDefinedFunctions.end()) {
			std::string errorMsg = "Duplicate function definition: '" + func->name() + "'";
			reportError(func, errorMsg.c_str());
			return false;
		}

		// Check for conflict with struct name
		if (mDefinedStructs.find(func->name()) != mDefinedStructs.end()) {
			std::string errorMsg = "Function '" + func->name() + "' conflicts with struct name";
			reportError(func, errorMsg.c_str());
			return false;
		}

		// Check for conflict with constant name
		if (mDefinedConstants.find(func->name()) != mDefinedConstants.end()) {
			std::string errorMsg = "Function '" + func->name() + "' conflicts with constant name";
			reportError(func, errorMsg.c_str());
			return false;
		}

		mDefinedFunctions.insert(func->name());
		return true;
	}

	// A constant declaration goes into the symbol table
	bool SemanticValidator::collectConstantDefinition(AstNodeConstant* constant) {
		if (!checkDeclName(constant, constant->name(), "a constant name")) {
			return false;
		}

		// Check for duplicate constant name
		// Skip this check if the constant was pre-collected from main file (for sibling namespace support),
		// or if the name so far only came from a merged file import - this file declaring it too is a
		// shadow, not a redefinition. The name is dropped from that set here so a second declaration
		// in this file still reports.
		if (mMergedImportedConstants.erase(constant->name()) == 0 &&
				mDefinedConstants.find(constant->name()) != mDefinedConstants.end() &&
				mPreCollectedConstants.find(constant->name()) == mPreCollectedConstants.end()) {
			std::string errorMsg = "Duplicate constant definition: '" + constant->name() + "'";
			reportError(constant, errorMsg.c_str());
			return false;
		}

		// Check for conflict with struct name
		if (mDefinedStructs.find(constant->name()) != mDefinedStructs.end()) {
			std::string errorMsg = "Constant '" + constant->name() + "' conflicts with struct name";
			reportError(constant, errorMsg.c_str());
			return false;
		}

		// Check for conflict with function name
		if (mDefinedFunctions.find(constant->name()) != mDefinedFunctions.end()) {
			std::string errorMsg = "Constant '" + constant->name() + "' conflicts with function name";
			reportError(constant, errorMsg.c_str());
			return false;
		}

		// A const is stored as written and every tier re-reads that text, so a literal
		// that will not read has to be caught here: downstream it turns into a silent
		// zero (or, before this check existed, an "Invalid integer constant" from the
		// code generator with no line number on it).
		const std::string literalProblem = numericLiteralProblem(constant->value());
		if (!literalProblem.empty()) {
			reportError(constant, literalProblem.c_str());
		}

		mDefinedConstants.insert(constant->name());
		mConstantValues[constant->name()] = constant->value();
		return true;
	}

	// Module-level mutable `var`. Collected here so identifier resolution
	// can accept both reads (`counter`) and writes (`-> counter`) without
	// reporting "Undefined identifier".
	bool SemanticValidator::collectGlobalVarDefinition(AstNodeGlobalVar* varNode) {
		if (!checkDeclName(varNode, varNode->name(), "a var name")) {
			return false;
		}
		// Skip if already collected (the validator may run its collect pass
		// twice on the same module — once pre-scan, once proper).
		if (mDefinedGlobalVars.count(varNode->name())) {
			return false;
		}
		if (mDefinedConstants.count(varNode->name()) || mDefinedFunctions.count(varNode->name()) ||
				mDefinedStructs.count(varNode->name())) {
			std::string errorMsg = "'" + varNode->name() + "' conflicts with an existing declaration";
			reportError(varNode, errorMsg.c_str());
			return false;
		}
		const std::string& typeName = varNode->typeName();
		const std::string sizedGlobal = sizedIntTypeThroughAliases(typeName);
		if (!sizedGlobal.empty()) {
			reportError(varNode, sizedTypeMisuseMessage(typeName, sizedGlobal, "a module global").c_str());
			return false;
		}
		if (!isValidTypeName(typeName)) {
			std::string errorMsg = "Unknown type '" + typeName + "' for var";
			reportError(varNode, errorMsg.c_str());
			return false;
		}
		const std::string initializerProblem = numericLiteralProblem(varNode->value());
		if (!initializerProblem.empty()) {
			reportError(varNode, initializerProblem.c_str());
		}
		mDefinedGlobalVars.insert(varNode->name());
		mGlobalVarTypes[varNode->name()] = typeName;
		return true;
	}

	// An enum declaration registers its variants as scoped constants
	bool SemanticValidator::collectEnumDefinition(AstNodeEnumDeclaration* enumDecl) {
		if (!checkDeclName(enumDecl, enumDecl->name(), "an enum name")) {
			return false;
		}

		if (mDefinedEnums.find(enumDecl->name()) != mDefinedEnums.end()) {
			std::string errorMsg = "Duplicate enum definition: '" + enumDecl->name() + "'";
			reportError(enumDecl, errorMsg.c_str());
			return false;
		}

		mDefinedEnums.insert(enumDecl->name());
		std::vector<std::string>& variantNames = mEnumVariants[enumDecl->name()];
		variantNames.clear();
		for (const auto& variant : enumDecl->variants()) {
			std::string scopedName = enumDecl->name() + "::" + variant.name;
			mConstantValues[scopedName] = std::to_string(variant.value);
			variantNames.push_back(variant.name);
		}
		return true;
	}

	bool SemanticValidator::collectTypeAliasDefinition(AstNodeTypeAlias* typeAlias) {
		if (!checkDeclName(typeAlias, typeAlias->name(), "a type alias")) {
			return false;
		}
		mTypeAliases[typeAlias->name()] = typeAlias->targetType();
		return true;
	}

	// A struct declaration goes into the symbol table along with its field types
	bool SemanticValidator::collectStructDefinition(AstNodeStructDeclaration* structDecl) {
		if (!checkDeclName(structDecl, structDecl->name(), "a struct name")) {
			return false;
		}

		// Check for duplicate struct name
		// Skip this check if the struct was pre-collected from main file (for sibling namespace support)
		if (mDefinedStructs.find(structDecl->name()) != mDefinedStructs.end() &&
				mPreCollectedStructs.find(structDecl->name()) == mPreCollectedStructs.end()) {
			std::string errorMsg = "Duplicate struct definition: '" + structDecl->name() + "'";
			reportError(structDecl, errorMsg.c_str());
			return false;
		}

		// Check for conflict with function name
		if (mDefinedFunctions.find(structDecl->name()) != mDefinedFunctions.end()) {
			std::string errorMsg = "Struct '" + structDecl->name() + "' conflicts with function name";
			reportError(structDecl, errorMsg.c_str());
			return false;
		}

		// Check for conflict with constant name
		if (mDefinedConstants.find(structDecl->name()) != mDefinedConstants.end()) {
			std::string errorMsg = "Struct '" + structDecl->name() + "' conflicts with constant name";
			reportError(structDecl, errorMsg.c_str());
			return false;
		}

		mDefinedStructs.insert(structDecl->name());
		mStructDeclarations[structDecl->name()] = structDecl;

		// Collect field types
		std::unordered_map<std::string, StackValueType> fieldTypes;
		std::unordered_set<std::string> seenFieldNames;
		for (auto* child : structDecl->children()) {
			if (child && child->type() == IAstNode::Type::STRUCT_FIELD) {
				AstNodeStructField* field = static_cast<AstNodeStructField*>(child);

				if (!checkDeclName(field, field->name(), "a field name in struct '" + structDecl->name() + "'")) {
					return false;
				}

				// Check for duplicate field name
				if (seenFieldNames.find(field->name()) != seenFieldNames.end()) {
					std::string errorMsg =
							"Duplicate field name '" + field->name() + "' in struct '" + structDecl->name() + "'";
					reportError(field, errorMsg.c_str());
					return false;
				}
				seenFieldNames.insert(field->name());

				StackValueType fieldType = StackValueType::UNKNOWN;
				const std::string& typeName = field->typeName();
				// Resolve type aliases
				const std::string resolvedTypeName = resolveTypeAliasChain(typeName);

				if (resolvedTypeName == "f64") {
					fieldType = StackValueType::FLOAT;
				} else if (resolvedTypeName == "d128") {
					fieldType = StackValueType::D128;
				} else if (resolvedTypeName == "i64" || resolvedTypeName == "i32" || resolvedTypeName == "i16" ||
						   resolvedTypeName == "i8" || resolvedTypeName == "u64" || resolvedTypeName == "u32" ||
						   resolvedTypeName == "u16" || resolvedTypeName == "u8") {
					// All sized integer fields widen to INT on the stack —
					// sized-int semantics live in memory, not on the stack.
					fieldType = StackValueType::INT;
				} else if (resolvedTypeName == "str") {
					fieldType = StackValueType::STRING;
				} else if (resolvedTypeName.size() > 1 && resolvedTypeName[0] == '*') {
					// `next:*Node` is a pointer to a struct: a pointer on the stack, but a
					// pointer to something with a name. Dropping the name here is what left
					// a method call on `n <<next` unresolvable.
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[structDecl->name()][field->name()] = resolvedTypeName.substr(1);
					mStructPointerFields[structDecl->name()].insert(field->name());
				} else if (resolvedTypeName == "ptr" || resolvedTypeName.find('*') != std::string::npos) {
					// A plain `ptr` field used to record no type at all, which left the field
					// unchecked: `struct C { data:ptr }` accepted `C { data = [1 2 3] }`, and the
					// array then reached every raw-buffer function through `c <<data`. Recording
					// it makes the field say what it is, so the same rule a declared `ptr`
					// parameter gets applies here.
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[structDecl->name()][field->name()] = "ptr";
				} else if (resolvedTypeName.size() > 2 && resolvedTypeName[0] == '[' && resolvedTypeName[1] == ']') {
					// Array type: []T - treat as PTR and track the array type
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[structDecl->name()][field->name()] = resolvedTypeName;
				} else if (resolvedTypeName.size() > 3 && resolvedTypeName.substr(0, 3) == "fn(") {
					// Function pointer type: fn(i64 -- i64) - treat as PTR
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[structDecl->name()][field->name()] = resolvedTypeName;
				} else if (!resolvedTypeName.empty() && std::isupper(resolvedTypeName[0])) {
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
		return true;
	}

	// A test declaration goes into the symbol table
	bool SemanticValidator::collectTestDefinition(AstNodeTest* test) {
		if (!checkDeclName(test, test->name(), "a test name")) {
			return false;
		}

		// Check for duplicate test name
		if (mDefinedTests.find(test->name()) != mDefinedTests.end()) {
			std::string errorMsg = "Duplicate test definition: '" + test->name() + "'";
			reportError(test, errorMsg.c_str());
			return false;
		}

		mDefinedTests.insert(test->name());
		return true;
	}

	// A use statement adds the module to the imported modules and loads its definitions
	void SemanticValidator::collectUseStatement(AstNodeUse* use) {
		std::string moduleName = use->module();

		// Freestanding mode: only `bits`, `limits`, and the safe
		// subset of `mem` (alloc/realloc/free are excluded at link
		// time when libmem-freestanding.a is used) are usable.
		// Everything else allocates or calls libc. User .qd file
		// imports (containing `/` or `.qd`) are allowed since the
		// user controls what those files contain.
		if (mFreestandingMode) {
			bool isUserImport = moduleName.find('/') != std::string::npos ||
								(moduleName.size() > 3 && moduleName.substr(moduleName.size() - 3) == ".qd");
			if (!isUserImport && moduleName != "bits" && moduleName != "limits" && moduleName != "mem" &&
					moduleName != "sys") {
				std::string err = "module '" + moduleName +
								  "' is not available in --freestanding mode "
								  "(allocates or depends on libc)";
				reportError(use, err.c_str());
			}
		}

		mImportedModules.insert(moduleName);

		// For file imports, also register the derived namespace.
		// This allows "use "calculator.qd"" to work with "calculator::function".
		// A module file being validated on its own needs this too: `use "../ffi/sdl.qd"`
		// is what puts `sdl::` in scope, and the main-file pass already registers exactly
		// these two names for the same file (see the scopedImports block in pass 3c).
		// Leaving it out here made every qualified call in a module that imports an FFI
		// wrapper by path fail with "Module 'sdl' not imported" in that pass alone.
		std::string packageName = getPackageFromModuleName(moduleName);
		if (packageName != moduleName) {
			mImportedModules.insert(packageName);
		}

		// Only report errors for missing modules if this is the main entry point (not a module file)
		loadModuleDefinitions(moduleName, mCurrentPackage, !mIsModuleFile);
	}

	// An import statement registers the library functions it declares
	void SemanticValidator::collectImportStatement(AstNodeImport* import) {
		mImportedLibraries[import->namespaceName()] = import->library();
		// Register all imported functions as namespace::function
		for (const auto& func : import->functions()) {
			std::string qualifiedName = import->namespaceName() + "::" + func->name;
			mImportedLibraryFunctions.insert(qualifiedName);
			// Also register unqualified name for use within the same module
			mImportedLibraryFunctions.insert(func->name);

			// Also register function signature for type checking
			FunctionSignature sig;

			for (const auto& param : func->inputParameters) {
				addSignatureInput(sig, param->typeString(), "");
			}
			for (const auto& param : func->outputParameters) {
				addSignatureOutput(sig, param->typeString(), "");
			}

			sig.throws = func->throws;
			mFunctionSignatures[qualifiedName] = sig;
		}
	}

	void SemanticValidator::markBodyWithRemovedBuiltin(const IAstNode* node) {
		// Walk out to the enclosing function or test. Everything from the removed
		// name onwards is simulated against a stack that is missing its effect, so
		// the whole body's stack reasoning is unusable, not just the one line.
		for (const IAstNode* n = node; n != nullptr; n = n->parent()) {
			if (n->type() == IAstNode::Type::FUNCTION_DECLARATION || n->type() == IAstNode::Type::TEST_DECLARATION) {
				mBodiesWithRemovedBuiltins.insert(n);
				return;
			}
		}
	}

	void SemanticValidator::validateReferences(IAstNode* node) {
		std::unordered_set<std::string> localVariables;
		std::unordered_set<std::string> iteratorNames;
		validateReferencesInternal(node, localVariables, iteratorNames);
	}

	// A lambda's named parameters, which are its own and never a capture of the same name.
	// Taken for one, the body read the outer variable in place of its argument.
	static std::unordered_set<std::string> lambdaParameterNames(AstNodeAnonymousFunction* anonFunc) {
		std::unordered_set<std::string> names;
		for (const auto& paramNode : anonFunc->inputParameters()) {
			const auto* param = static_cast<const AstNodeParameter*>(paramNode.get());
			if (param->hasName()) {
				names.insert(param->name());
			}
		}
		return names;
	}

	// The statements that open a scope of their own: `for`, `loop`, `switch`, and an `if`'s
	// branches and a block. Each body is visited with a copy of the enclosing locals, so a name
	// bound inside does not leak out, and a `for` adds its iterator. With loopMarkers, a `loop`
	// or `switch` also adds a placeholder iterator, which is how `break` and the like tell
	// whether they sit in a loop. Returns false, having visited nothing, for any other node.
	// visit(child, locals, iterators) walks one child in the scope it is given.
	template <typename Visit>
	static bool walkScoped(IAstNode* node, const std::unordered_set<std::string>& locals,
			std::unordered_set<std::string>& iterators, bool loopMarkers, Visit&& visit) {
		switch (node->type()) {
		case IAstNode::Type::FOR_STATEMENT: {
			std::unordered_set<std::string> childIterators = iterators;
			childIterators.insert(static_cast<AstNodeForStatement*>(node)->iteratorName());
			std::unordered_set<std::string> forLocals = locals;
			for (auto* child : node->children()) {
				visit(child, forLocals, childIterators);
			}
			return true;
		}
		case IAstNode::Type::LOOP_STATEMENT:
		case IAstNode::Type::SWITCH_STATEMENT: {
			std::unordered_set<std::string> loopLocals = locals;
			if (!loopMarkers) {
				for (auto* child : node->children()) {
					visit(child, loopLocals, iterators);
				}
				return true;
			}
			// The two are told apart because `while` and `until` need a loop, not just a switch
			std::unordered_set<std::string> childIterators = iterators;
			childIterators.insert(node->type() == IAstNode::Type::LOOP_STATEMENT ? "__loop__" : "__switch__");
			for (auto* child : node->children()) {
				visit(child, loopLocals, childIterators);
			}
			return true;
		}
		case IAstNode::Type::IF_STATEMENT: {
			AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(node);
			if (ifStmt->thenBody()) {
				std::unordered_set<std::string> thenLocals = locals;
				visit(ifStmt->thenBody(), thenLocals, iterators);
			}
			if (ifStmt->elseBody()) {
				std::unordered_set<std::string> elseLocals = locals;
				visit(ifStmt->elseBody(), elseLocals, iterators);
			}
			return true;
		}
		case IAstNode::Type::BLOCK: {
			std::unordered_set<std::string> blockLocals = locals;
			for (auto* child : node->children()) {
				visit(child, blockLocals, iterators);
			}
			return true;
		}
		default:
			return false;
		}
	}

	void SemanticValidator::validateReferencesInternal(IAstNode* node, std::unordered_set<std::string>& localVariables,
			std::unordered_set<std::string>& iteratorNames) {
		if (!node) {
			return;
		}

		switch (node->type()) {
		case IAstNode::Type::LOCAL:
			// Insert all names (supports multiple assignment: -> a b c)
			for (const std::string& name : static_cast<AstNodeLocal*>(node)->names()) {
				checkDeclName(node, name, "a variable name");
				localVariables.insert(name);
			}
			return;
		case IAstNode::Type::BREAK_STATEMENT:
		case IAstNode::Type::CONTINUE_STATEMENT:
		case IAstNode::Type::CONDITIONAL_BREAK:
			checkLoopControlRef(node, iteratorNames);
			return;
		case IAstNode::Type::STRUCT_DECLARATION:
			checkStructFieldTypes(static_cast<AstNodeStructDeclaration*>(node));
			return;
		case IAstNode::Type::SWITCH_STATEMENT:
			// Its body is walked as a scope below
			checkSwitchCases(static_cast<AstNodeSwitchStatement*>(node));
			break;
		case IAstNode::Type::IDENTIFIER:
			if (checkIdentifierRef(static_cast<AstNodeIdentifier*>(node), localVariables, iteratorNames)) {
				return;
			}
			break;
		case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
			checkFunctionPointerRef(static_cast<AstNodeFunctionPointerReference*>(node));
			break;
		case IAstNode::Type::STRUCT_CONSTRUCTION:
			if (checkStructConstructionRef(
						static_cast<AstNodeStructConstruction*>(node), localVariables, iteratorNames)) {
				return;
			}
			break;
		case IAstNode::Type::SCOPED_IDENTIFIER:
			checkScopedRef(static_cast<AstNodeScopedIdentifier*>(node));
			break;
		case IAstNode::Type::FUNCTION_DECLARATION:
			validateFunctionRefs(static_cast<AstNodeFunctionDeclaration*>(node), localVariables);
			return;
		case IAstNode::Type::ANONYMOUS_FUNCTION:
			validateAnonymousFunctionRefs(static_cast<AstNodeAnonymousFunction*>(node), localVariables, iteratorNames);
			return;
		default:
			break;
		}

		auto visit = [this](IAstNode* child, std::unordered_set<std::string>& locals,
							 std::unordered_set<std::string>& iterators) {
			validateReferencesInternal(child, locals, iterators);
		};
		if (walkScoped(node, localVariables, iteratorNames, true, visit)) {
			return;
		}

		// Recursively process children
		for (auto* child : node->children()) {
			validateReferencesInternal(child, localVariables, iteratorNames);
		}
	}

	// `break`, `continue`, `while` and `until` act on the innermost loop. A `switch` is not one
	// -- a `break` in it leaves the loop around it -- so a switch with no loop around it is not
	// enough. Accepting it there compiled to nothing: the jump had no loop to go to.
	void SemanticValidator::checkLoopControlRef(IAstNode* node, const std::unordered_set<std::string>& iteratorNames) {
		for (const auto& name : iteratorNames) {
			if (name != "__switch__") {
				return;
			}
		}
		std::string keyword = node->type() == IAstNode::Type::BREAK_STATEMENT ? "break"
							  : node->type() == IAstNode::Type::CONTINUE_STATEMENT
									  ? "continue"
									  : static_cast<AstNodeConditionalBreak*>(node)->keyword();
		const char* example = node->type() == IAstNode::Type::BREAK_STATEMENT	   ? "loop { done if { break } }"
							  : node->type() == IAstNode::Type::CONTINUE_STATEMENT ? "loop { skip if { continue } }"
							  : node->type() == IAstNode::Type::CONDITIONAL_BREAK &&
											  static_cast<AstNodeConditionalBreak*>(node)->breakWhen()
									  ? "loop { done until }"
									  : "loop { i n < while }";
		reportErrorWithHint(node, ("'" + keyword + "' not within a loop").c_str(),
				("'" + keyword + "' acts on the innermost 'loop' or 'for'; a 'switch' is not a loop, e.g. '" + example +
						"'")
						.c_str());
	}

	// Struct field types have to name valid types
	void SemanticValidator::checkStructFieldTypes(AstNodeStructDeclaration* structDecl) {
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

				// Check for qualified type names (e.g., vec2::Vec2). The brackets come off
				// first: `[]vec2::Vec2` names the same type `vec2::Vec2` does, and looking
				// for the "::" before peeling them read the module name as "[]vec2" and told
				// the author to `use []vec2`.
				std::string namedType = typeName;
				while (namedType.size() > 2 && namedType[0] == '[' && namedType[1] == ']') {
					namedType = namedType.substr(2);
				}
				size_t colonPos = namedType.find("::");
				if (colonPos != std::string::npos) {
					std::string moduleName = namedType.substr(0, colonPos);
					std::string structTypeName = namedType.substr(colonPos + 2);

					// Check if the module is imported
					if (mImportedModules.find(moduleName) == mImportedModules.end()) {
						std::string errorMsg = "Module '" + moduleName + "' not imported for field type '" + typeName +
											   "'. Add 'use " + moduleName + "' to use this type";
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

					// Type aliases - valid
					if (mTypeAliases.find(baseTypeName) != mTypeAliases.end() ||
							!builtinTypeAliasTarget(baseTypeName).empty()) {
						continue;
					}

					// Function pointer types: fn(i64 -- i64) - valid
					if (baseTypeName.size() > 3 && baseTypeName.substr(0, 3) == "fn(") {
						continue;
					}

					// Array types: []T - validate the element type
					if (baseTypeName.size() > 2 && baseTypeName[0] == '[' && baseTypeName[1] == ']') {
						std::string elemType = baseTypeName.substr(2);
						if (!isValidTypeName(elemType)) {
							std::string errorMsg =
									"Unknown element type '" + elemType + "' in array field '" + field->name() + "'";
							reportError(field, errorMsg.c_str());
						}
						continue;
					}

					// A field cannot be `any`: it holds one value of one type, which is what a
					// generic struct's type parameter is for
					if (!isBuiltinTypeName(baseTypeName) || baseTypeName == "any") {
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
	}

	// A switch needs at least one case and no case value twice
	void SemanticValidator::checkSwitchCases(AstNodeSwitchStatement* switchStmt) {
		if (switchStmt->cases().empty()) {
			reportError(switchStmt, "Switch statement must have at least one case");
		}

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

	// An identifier is a function call or a local variable reference. Returns true when it
	// resolves to something, or reports what is wrong with it.
	bool SemanticValidator::checkIdentifierRef(AstNodeIdentifier* ident,
			const std::unordered_set<std::string>& localVariables,
			const std::unordered_set<std::string>& iteratorNames) {
		const char* name = ident->name().c_str();

		// A for loop iterator, a local variable or a built-in instruction
		if (iteratorNames.count(name) > 0 || localVariables.find(name) != localVariables.end() ||
				isBuiltInInstruction(name)) {
			return true;
		}

		// An imported library function (unqualified, within same module), a defined function or
		// a defined constant
		if (mImportedLibraryFunctions.find(name) != mImportedLibraryFunctions.end() ||
				mDefinedFunctions.find(name) != mDefinedFunctions.end() ||
				mDefinedConstants.find(name) != mDefinedConstants.end()) {
			return true;
		}

		// Inside a module file, a function pulled in by an intra-module `use "other.qd"` is
		// referenced without a prefix — it shares the module's namespace. Those land in
		// mModuleFunctions under the current package rather than in mDefinedFunctions, so
		// resolve against them too. Guarded on mIsModuleFile: in a main file an unqualified
		// name must not silently reach into an imported module.
		if (mIsModuleFile) {
			auto pkg = mModuleFunctions.find(mCurrentPackage);
			if (pkg != mModuleFunctions.end() && pkg->second.count(name) > 0) {
				return true;
			}
		}

		// A module-level mutable `var`, or a defined struct (for struct construction)
		if (mDefinedGlobalVars.find(name) != mDefinedGlobalVars.end() ||
				mDefinedStructs.find(name) != mDefinedStructs.end()) {
			return true;
		}

		// A struct from an imported module (must be public)
		for (const auto& moduleEntry : mModuleStructs) {
			const auto& structs = moduleEntry.second;
			if (structs.find(name) != structs.end() && structs.at(name)) {
				return true;
			}
		}

		// A method name on any struct (type checking will verify the receiver)
		for (const auto& structEntry : mStructMethods) {
			if (structEntry.second.count(name)) {
				return true;
			}
		}

		// Removed builtins report what happened rather than "undefined identifier",
		// which would otherwise suggest a typo or a missing variable declaration.
		if (const RemovedInstruction* removed = findRemovedInstruction(name)) {
			reportError(ident, removedInstructionMessage(*removed).c_str());
			markBodyWithRemovedBuiltin(ident);
			return true;
		}

		// A removed keyword is not a typo either - it is a name that used to mean
		// something. Say what it meant before the fuzzy matcher guesses.
		if (const char* removedNote = removedKeywordMessage(name)) {
			reportError(ident, removedNote);
			return true;
		}

		// An input of a function that leaves its inputs on the stack, named in its body
		if (auto unbound = mUnboundParameters.find(name); unbound != mUnboundParameters.end()) {
			reportErrorWithHint(ident,
					("'" + std::string(name) + "' is an input of " + unbound->second +
							", which leaves its inputs on the stack rather than binding them")
							.c_str(),
					"write 'def' instead of 'fn' to bind the inputs to their names");
			mUndefinedNodes.insert(ident);
			return true;
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
		mUndefinedNodes.insert(ident);
		return false;
	}

	// A function pointer reference has to name a defined function
	void SemanticValidator::checkFunctionPointerRef(AstNodeFunctionPointerReference* funcPtr) {
		const char* name = funcPtr->functionName().c_str();
		if (mDefinedFunctions.find(name) != mDefinedFunctions.end()) {
			return;
		}

		// A removed builtin is not a typo - say so before the fuzzy matcher
		// gets a chance to point at an unrelated op with a different effect.
		if (const RemovedInstruction* removed = findRemovedInstruction(name)) {
			reportError(funcPtr, removedInstructionMessage(*removed).c_str());
			markBodyWithRemovedBuiltin(funcPtr);
			return;
		}

		// Not found - report error with suggestion
		std::string errorMsg = "Undefined function '";
		errorMsg += name;
		errorMsg += "' in function pointer reference";
		std::string suggestion = findSimilarFunctionName(name, mDefinedFunctions);
		if (!suggestion.empty()) {
			errorMsg += "; did you mean '" + suggestion + "'?";
		}
		reportError(funcPtr, errorMsg.c_str());
		mUndefinedNodes.insert(funcPtr);
	}

	// A struct construction (StructName { field: value ... }) has to name a struct it can see.
	// Returns true when it is handled, its field initializers included.
	bool SemanticValidator::checkStructConstructionRef(AstNodeStructConstruction* construct,
			std::unordered_set<std::string>& localVariables, std::unordered_set<std::string>& iteratorNames) {
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
				auto structIt = structs.find(unqualifiedName);
				if (structIt != structs.end()) {
					if (!structIt->second) {
						// Struct exists but is private - report visibility error
						std::string errorMsg = "Struct '";
						errorMsg += unqualifiedName;
						errorMsg += "' in module '";
						errorMsg += moduleName;
						errorMsg += "' is private and cannot be accessed from outside the module. Mark it as "
									"'pub struct' to export it.";
						reportError(construct, errorMsg.c_str());
						return true;
					}
					validStruct = true;
				}
			}
		} else {
			// Unqualified name - defined locally, or a public struct from an imported module
			validStruct = mDefinedStructs.find(structName) != mDefinedStructs.end();
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
			return true;
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
		return false;
	}

	// A scoped identifier: an enum variant (TokenType::Int), a library function (std::printf)
	// or a module's function, constant or struct (math::sqrt)
	void SemanticValidator::checkScopedRef(AstNodeScopedIdentifier* scoped) {
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
			return;
		}

		// Check if this is an imported library namespace (even if function not declared)
		if (mImportedLibraries.find(scopeName) != mImportedLibraries.end()) {
			// A module may qualify a reference with its own name, and that name can also be
			// the namespace of an `import` block in the same file: `math.qd` declares
			// `import "libmath.a" as "math"` and calls its own `pub fn abs` as `math::abs`;
			// `time.qd` declares `import "libtime.a" as "time"` and reads its own
			// `pub const Millisecond` as `time::Millisecond`. Resolve against the module's
			// own definitions before rejecting. Only fires when a name is both an FFI
			// namespace and a locally defined function or constant, which is exactly this
			// case. Getting this wrong also cascades: an unresolved `time::Millisecond` left
			// nothing on the stack, so the following `/` reported a spurious underflow.
			if (mDefinedFunctions.find(functionName) != mDefinedFunctions.end() ||
					mDefinedConstants.find(functionName) != mDefinedConstants.end()) {
				return;
			}
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
			mUndefinedNodes.insert(scoped);
			return;
		}

		// Check if this is a constant (constants take precedence over functions)
		auto constIt = mModuleConstants.find(scopeName);
		if (constIt != mModuleConstants.end()) {
			const auto& constants = constIt->second;
			auto constantIt = constants.find(functionName);
			if (constantIt != constants.end()) {
				// Check if the constant is public
				if (!constantIt->second) {
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

		// If the module is not in mModuleFunctions, loadModuleDefinitions failed, and that was
		// likely already reported
		auto moduleIt = mModuleFunctions.find(scopeName);
		if (moduleIt == mModuleFunctions.end()) {
			return;
		}

		// Check if the function exists in the module
		const auto& functions = moduleIt->second;
		auto funcIt = functions.find(functionName);
		if (funcIt != functions.end()) {
			// Check if the function is public
			if (!funcIt->second) {
				std::string errorMsg = "Function '";
				errorMsg += functionName;
				errorMsg += "' in module '";
				errorMsg += scopeName;
				errorMsg += "' is private and cannot be accessed from outside the module. Mark it as 'pub fn' "
							"to export it.";
				reportError(scoped, errorMsg.c_str());
			}
			return;
		}

		// If not found as a function, check if it's a struct
		auto moduleStructsIt = mModuleStructs.find(scopeName);
		if (moduleStructsIt != mModuleStructs.end()) {
			const auto& structs = moduleStructsIt->second;
			auto structIt = structs.find(functionName);
			if (structIt != structs.end()) {
				// Check if the struct is public
				if (!structIt->second) {
					std::string errorMsg = "Struct '";
					errorMsg += functionName;
					errorMsg += "' in module '";
					errorMsg += scopeName;
					errorMsg += "' is private and cannot be accessed from outside the module. Mark it as "
								"'pub struct' to export it.";
					reportError(scoped, errorMsg.c_str());
				}
				return;
			}
		}

		// A public imported function
		auto moduleImportsIt = mModuleImportedFunctions.find(scopeName);
		if (moduleImportsIt != mModuleImportedFunctions.end() &&
				moduleImportsIt->second.find(functionName) != moduleImportsIt->second.end()) {
			return;
		}

		// sb::append_any is a compiler-generated call from $"..." string interpolation
		// It's resolved at codegen time to sb::append or sb::append_int
		if (scopeName == "sb" && functionName == "append_any") {
			return;
		}

		std::string errorMsg = "Function, constant, or struct '";
		errorMsg += functionName;
		errorMsg += "' not found in module '";
		errorMsg += scopeName;
		errorMsg += "'";
		// Try to suggest a similar name from the module
		std::string suggestion = findSimilarNameInMap(functionName, functions);
		if (!suggestion.empty()) {
			errorMsg += "; did you mean '" + suggestion + "'?";
		}
		reportError(scoped, errorMsg.c_str());
		mUndefinedNodes.insert(scoped);
	}

	// A function declaration's body is a new scope. Parameters must be explicitly bound with
	// -> before use
	void SemanticValidator::validateFunctionRefs(
			AstNodeFunctionDeclaration* func, const std::unordered_set<std::string>& localVariables) {
		std::unordered_set<std::string> funcLocalVariables = localVariables;
		std::unordered_set<std::string> funcIterators; // Empty - no iterators in function scope by default

		// For methods, the receiver is implicitly bound as a local variable
		if (func->hasReceiver()) {
			funcLocalVariables.insert(func->receiverName());
		}

		// Input parameters are bound as local variables when the function is `def`.
		// Otherwise the names are documentation and are not in scope, which is what leaves
		// the body free to bind a local of the same name with `->`.
		auto savedUnbound = std::move(mUnboundParameters);
		mUnboundParameters.clear();
		for (const auto& paramNode : func->inputParameters()) {
			AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
			if (!param->hasName()) {
				continue;
			}
			if (func->isStack()) {
				mUnboundParameters[param->name()] = "'" + func->name() + "'";
			} else {
				funcLocalVariables.insert(param->name());
			}
		}

		// Process function body
		for (auto* child : func->children()) {
			validateReferencesInternal(child, funcLocalVariables, funcIterators);
		}
		mUnboundParameters = std::move(savedUnbound);
	}

	// An anonymous function's body is a new scope, holding its parameters and the variables
	// it captures from the enclosing scope
	void SemanticValidator::validateAnonymousFunctionRefs(AstNodeAnonymousFunction* anonFunc,
			const std::unordered_set<std::string>& localVariables,
			const std::unordered_set<std::string>& iteratorNames) {
		// A new scope for the anonymous function, holding its own parameters
		std::unordered_set<std::string> anonLocals = lambdaParameterNames(anonFunc);
		std::unordered_set<std::string> anonIterators;

		// A `for` iterator is in scope for the body but is tracked in iteratorNames rather than
		// in localVariables, so passing localVariables alone as the outer scope meant a closure
		// referring to the iterator captured nothing and the name came back undefined. The
		// placeholder loop/switch marker is not a real binding and is left out.
		std::unordered_set<std::string> outerScope = localVariables;
		for (const auto& iterName : iteratorNames) {
			if (iterName != "__loop__" && iterName != "__switch__") {
				outerScope.insert(iterName);
			}
		}

		// Collect captured variables by walking the body
		collectCapturedVariables(anonFunc->body(), anonLocals, anonIterators, outerScope, anonFunc);

		// Now validate the body with captured variables added to the local scope
		// This ensures undefined references are properly reported
		std::unordered_set<std::string> validatedLocals;
		for (const auto& captured : anonFunc->capturedVariables()) {
			validatedLocals.insert(captured);
		}

		// Named parameters are bound as local variables in a `def` -- the same rule
		// a named function follows.
		auto savedUnbound = std::move(mUnboundParameters);
		mUnboundParameters.clear();
		for (const auto& paramNode : anonFunc->inputParameters()) {
			AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
			if (!param->hasName()) {
				continue;
			}
			if (anonFunc->isStack()) {
				mUnboundParameters[param->name()] = "this anonymous function";
			} else {
				validatedLocals.insert(param->name());
			}
		}

		// Note: anonLocals already contains variables defined with -> in the body
		validatedLocals.insert(anonLocals.begin(), anonLocals.end());

		// Validate the body
		validateReferencesInternal(anonFunc->body(), validatedLocals, anonIterators);
		mUnboundParameters = std::move(savedUnbound);
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

		// Handle identifier references - this is where we detect captures
		if (node->type() == IAstNode::Type::IDENTIFIER) {
			AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
			const std::string& name = ident->name();

			// Skip a local variable in the anonymous function, an iterator, a built-in
			// instruction, a defined function, a constant and a struct constructor
			if (localVariables.find(name) != localVariables.end() || iteratorNames.find(name) != iteratorNames.end() ||
					isBuiltInInstruction(name.c_str()) || mDefinedFunctions.find(name) != mDefinedFunctions.end() ||
					mDefinedConstants.find(name) != mDefinedConstants.end() ||
					mDefinedStructs.find(name) != mDefinedStructs.end()) {
				return;
			}

			// Check if it's in the outer scope - this means it's a captured variable!
			if (outerScopeVariables.find(name) != outerScopeVariables.end()) {
				// Add to captures (avoid duplicates)
				const auto& captures = anonFunc->capturedVariables();
				if (std::find(captures.begin(), captures.end(), name) == captures.end()) {
					anonFunc->addCapturedVariable(name);
				}
			}

			// Otherwise it's an undefined reference - but that's reported by validateReferencesInternal
			return;
		}

		// Scoped statements: for (with its iterator), loop, switch, if and blocks
		auto visit = [&](IAstNode* child, std::unordered_set<std::string>& locals,
							 std::unordered_set<std::string>& iterators) {
			collectCapturedVariables(child, locals, iterators, outerScopeVariables, anonFunc);
		};
		if (walkScoped(node, localVariables, iteratorNames, false, visit)) {
			return;
		}

		// A nested anonymous function captures from this one's scope and from further out. What
		// it takes from further out, this one has to capture too, to have it to hand on: the
		// nested closure's environment is built here, from what this function can see.
		if (node->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
			AstNodeAnonymousFunction* nestedAnon = static_cast<AstNodeAnonymousFunction*>(node);
			std::unordered_set<std::string> nestedLocals = lambdaParameterNames(nestedAnon);
			std::unordered_set<std::string> nestedIterators;
			std::unordered_set<std::string> combinedOuter = outerScopeVariables;
			combinedOuter.insert(localVariables.begin(), localVariables.end());
			combinedOuter.insert(iteratorNames.begin(), iteratorNames.end());
			collectCapturedVariables(nestedAnon->body(), nestedLocals, nestedIterators, combinedOuter, nestedAnon);
			for (const std::string& name : nestedAnon->capturedVariables()) {
				const auto& captures = anonFunc->capturedVariables();
				if (!localVariables.count(name) && !iteratorNames.count(name) && outerScopeVariables.count(name) &&
						std::find(captures.begin(), captures.end(), name) == captures.end()) {
					anonFunc->addCapturedVariable(name);
				}
			}
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

		// Each function's signature, from what it declares
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);

			// Set current type parameters for generic functions
			mCurrentTypeParams = func->typeParams();

			// The parameters' names, for the field accesses below, and their types checked
			std::vector<std::string> paramNames;
			for (const auto& paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				paramNames.push_back(param->name());

				// Validate type name
				std::string typeStr = param->typeString();
				const std::string sizedParam = sizedIntTypeThroughAliases(typeStr);
				if (!sizedParam.empty()) {
					reportError(param, sizedTypeMisuseMessage(typeStr, sizedParam, "a parameter").c_str());
				}
				if (!isValidTypeName(typeStr)) {
					reportError(param, invalidTypeMessage(typeStr, parameterSuffix(param)).c_str());
				}
			}

			// Store the signature with input parameters as consumes
			FunctionSignature sig;

			// Build consumes list from input parameters
			for (size_t paramIdx = 0; paramIdx < func->inputParameters().size(); paramIdx++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[paramIdx].get());
				std::string typeStr = param->typeString();

				// Validate type name
				const std::string sizedConsumed = sizedIntTypeThroughAliases(typeStr);
				if (!sizedConsumed.empty()) {
					reportError(param, sizedTypeMisuseMessage(typeStr, sizedConsumed, "a parameter").c_str());
				}
				if (!isValidTypeName(typeStr)) {
					reportError(param, invalidTypeMessage(typeStr, parameterSuffix(param)).c_str());
				}

				addSignatureInput(sig, typeStr, "");
			}

			// Validate declared output parameter types
			for (const auto& paramNode : func->outputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				std::string typeStr = param->typeString();

				// Validate type name
				const std::string sizedOutput = sizedIntTypeThroughAliases(typeStr);
				if (!sizedOutput.empty()) {
					reportError(param, sizedTypeMisuseMessage(typeStr, sizedOutput, "a return value").c_str());
				}
				if (!isValidTypeName(typeStr)) {
					reportError(param, invalidTypeMessage(typeStr, parameterSuffix(param)).c_str());
				}
			}

			// Collect which fields are accessed on each parameter
			if (func->body()) {
				collectParameterFieldAccesses(func->body(), paramNames, sig.parameterFieldAccess);
			}

			// Infer struct types from field access patterns, for parameters that did not
			// declare one. A declared type always wins: the inference reads every field
			// mentioned in the body, including fields of *other* structs reached through a
			// call, so it happily concludes that `fn f(re:Nfa …)` whose body does
			// `re idx state_at <<kind` takes a State -- the struct that has `kind`. That
			// overwrote the declared Nfa and every call site was then rejected for passing
			// the type the signature actually asks for.
			for (size_t paramIdx = 0; paramIdx < paramNames.size(); paramIdx++) {
				if (sig.parameterStructTypes.find(paramIdx) != sig.parameterStructTypes.end()) {
					continue;
				}
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
			for (const auto& paramNode : func->outputParameters()) {
				addSignatureOutput(sig, static_cast<AstNodeParameter*>(paramNode.get())->typeString(), "");
			}
			sig.throws = func->throws();
			sig.typeParams = mCurrentTypeParams;
			sig.stackVariables = func->stackVariables();
			sig.inputStackVariable = func->inputStackVariable();
			sig.outputStackVariable = func->outputStackVariable();
			for (const auto& tp : func->receiverTypeParams()) {
				if (std::find(sig.typeParams.begin(), sig.typeParams.end(), tp) == sig.typeParams.end()) {
					sig.typeParams.push_back(tp);
				}
			}

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
				std::unordered_map<size_t, std::string> newParamTypeNames;
				for (const auto& entry : sig.parameterTypeNames) {
					newParamTypeNames[entry.first + 1] = entry.second;
				}
				newParamTypeNames[0] = func->receiverType();
				sig.parameterTypeNames = std::move(newParamTypeNames);

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
		// Records `<<field` reads whose operand is a parameter (or a local bound from one). The
		// operand is the identifier node just before the field access in the same block; a read
		// through anything else (a call, a chained access) says nothing about a parameter.
		auto recordAccess = [&](const std::string& varName, const std::string& fieldName) {
			StackValueType fieldType = StackValueType::UNKNOWN;
			for (const auto& structEntry : mStructFieldTypes) {
				auto it = structEntry.second.find(fieldName);
				if (it != structEntry.second.end()) {
					fieldType = it->second;
					break;
				}
			}
			if (std::find(paramNames.begin(), paramNames.end(), varName) != paramNames.end()) {
				fieldAccesses[varName][fieldName] = fieldType;
				return;
			}
			auto localIt = localToParam.find(varName);
			if (localIt != localToParam.end()) {
				fieldAccesses[localIt->second][fieldName] = fieldType;
				return;
			}
			// Unknown provenance: attribute to the first parameter, as before (a heuristic).
			if (!paramNames.empty()) {
				fieldAccesses[paramNames[0]][fieldName] = fieldType;
			}
		};

		std::function<void(IAstNode*)> collectRecursive = [&](IAstNode* n) {
			if (!n) {
				return;
			}
			for (size_t i = 0; i < n->childCount(); i++) {
				IAstNode* c = n->child(i);
				if (!c) {
					continue;
				}
				if (c->type() == IAstNode::Type::FIELD_ACCESS && i > 0) {
					IAstNode* prev = n->child(i - 1);
					if (prev && prev->type() == IAstNode::Type::IDENTIFIER) {
						recordAccess(static_cast<AstNodeIdentifier*>(prev)->name(),
								static_cast<AstNodeFieldAccess*>(c)->fieldName());
					}
				} else if (c->type() == IAstNode::Type::LOCAL) {
					// Simplified tracking: locals are assumed to be bound from the first parameter.
					AstNodeLocal* local = static_cast<AstNodeLocal*>(c);
					if (!paramNames.empty()) {
						for (const std::string& name : local->names()) {
							localToParam[name] = paramNames[0];
						}
					}
				}
				collectRecursive(c);
			}
		};

		collectRecursive(node);
	}

} // namespace Qd

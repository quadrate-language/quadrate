#include <algorithm>
#include <chrono>
#include <climits>
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
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/module_resolver.h>
#include <quadrate/qc/numeric_literal.h>
#include <quadrate/qc/semantic_validator.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>

namespace Qd {

#include "semantic_validator_internal.h"

	// Check if a type string is a known struct name (local or imported)
	// Supports both unqualified names (Response) and qualified names (http::Response)

	// Every source file of a module directory, under one namespace: the files quadc compiles
	void SemanticValidator::loadModuleDirectory(const std::string& moduleDir, const std::string& moduleName) {
		mModuleDirectories[moduleName] = moduleDir;
		for (const auto& qdFile : moduleFiles(moduleDir)) {
			std::ifstream file(qdFile);
			if (file.good()) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				parseModuleAndCollectFunctions(moduleName, buffer.str(), qdFile);
			}
		}
	}

	void SemanticValidator::loadModuleDefinitions(const std::string& moduleName, const std::string& currentPackage,
			bool reportErrors, const std::string& importerDirectory) {
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
		// Track if this is a local file import that should merge into main namespace
		bool mergeIntoMain = false;
		if (isDirectFile) {
			// For .qd file imports from top-level files, derive package name from filename
			// For intra-module imports (inside a module's USE statements), use the current package name
			// We detect intra-module imports by checking:
			//   - mIsModuleFile: we're compiling a module.qd directly
			//   - mModuleDependencyChain not empty: we're processing nested imports from within a module
			//     (the chain contains module names, not just directory-derived package names)
			// Note: currentPackage alone is not sufficient because it's also set from directory names
			// The dependency chain already includes the current module (added by guard above),
			// so we check if size > 1 to see if there's a parent module context
			// quadc re-validates every imported .qd outside the main file's directory on its
			// own, with isModuleFile set. A `use` written directly in that file is the same
			// statement the main-file pass reads, so it has to mean the same thing: the file
			// gets a namespace from its own name and its definitions come into scope
			// unqualified. Treating it as an intra-module import instead collapsed it into the
			// enclosing directory's package, so `use "../ffi/sdl.qd"` left `sdl::Init` with no
			// known stack effect and every call to it reported a bogus underflow — in that pass
			// only. Nested imports (chain > 1) keep the old intra-module behaviour.
			bool isRootOfModulePass = mIsModuleFile && mModuleDependencyChain.size() == 1;
			bool isIntraModuleImport = (mIsModuleFile || mModuleDependencyChain.size() > 1) && !isRootOfModulePass;
			if (!isIntraModuleImport) {
				effectiveModuleName = getPackageFromModuleName(moduleName);
				mergeIntoMain = true; // Mark for merging into main namespace
			} else {
				// Check if the importing module was itself merged - if so, propagate merge status
				// This allows transitive .qd file imports from merged modules to also be merged
				if (mMergedModules.count(currentPackage) > 0) {
					effectiveModuleName = getPackageFromModuleName(moduleName);
					mergeIntoMain = true; // Transitive merge
				} else {
					// Regular intra-module import - use the current package name
					effectiveModuleName = currentPackage;
				}
			}
		} else {
			effectiveModuleName = moduleName;
		}

		Qd::ModuleSearch search;
		search.sourceDir = mSourceDirectory;
		search.includePaths = mIncludePaths;
		search.versionPins = mModuleVersionPins;

		if (isDirectFile) {
			// A file is found beside the file that wrote the `use` first, then beside the module
			// that file belongs to. The importer's own directory has to come first: a file
			// reached as `use "qd/r_sky.qd"` from a main file one level up writes
			// `use "tables.qd"` meaning its sibling, not a `tables.qd` next to main.
			const std::string parentDir =
					mModuleDirectories.count(currentPackage) ? mModuleDirectories[currentPackage] : mSourceDirectory;
			search.sourceDir = importerDirectory.empty() ? parentDir : importerDirectory;
			search.fileSearchDirs = {parentDir};
			const std::string path = findModuleFile(moduleName, search);
			if (!path.empty()) {
				std::ifstream file(path);
				std::stringstream buffer;
				buffer << file.rdbuf();
				parseModuleAndCollectFunctions(effectiveModuleName, buffer.str(), path, mergeIntoMain);
				return;
			}
		} else {
			const Qd::ModuleLocation location = findModule(moduleName, search);
			if (!location.ambiguous.empty()) {
				if (reportErrors) {
					const std::string packages = packagesDirectory();
					std::string errorMsg = "Ambiguous module '" + moduleName +
										   "': multiple installed packages claim this namespace.\n"
										   "Use full path import to disambiguate:\n";
					for (const auto& claim : location.ambiguous) {
						std::string relative = std::filesystem::path(claim).lexically_relative(packages).string();
						errorMsg += "  use " + relative.substr(0, relative.find('@')) + "\n";
					}
					reportError(errorMsg.c_str());
				}
				return;
			}
			if (!location.directory.empty()) {
				// A full-path import's words go under the namespace its qd.json declares
				if (location.namespaceName != moduleName) {
					mImportedModules.insert(location.namespaceName);
				}
				loadModuleDirectory(location.directory, location.namespaceName);
				return;
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
				// A directory of that name with no .qd files in it is found but not a module
				bool foundDirectoryWithoutModuleFile = false;
				std::string directoryPath;
				std::vector<std::string> candidates = {mSourceDirectory + "/" + moduleName};
				for (const auto& root : moduleRoots()) {
					candidates.push_back(root + "/" + moduleName);
				}
				for (const auto& candidate : candidates) {
					std::error_code ec;
					if (std::filesystem::is_directory(candidate, ec)) {
						foundDirectoryWithoutModuleFile = true;
						directoryPath = candidate;
						break;
					}
				}

				std::string errorMsg;
				if (foundDirectoryWithoutModuleFile) {
					errorMsg = "Module '";
					errorMsg += moduleName;
					errorMsg += "' directory found at '";
					errorMsg += directoryPath;
					errorMsg += "', but it does not contain any .qd files.\n";
					errorMsg += "Module directories must contain at least one .qd file.\n";
					errorMsg += "Either create '";
					errorMsg += directoryPath;
					errorMsg += "/";
					errorMsg += moduleName;
					errorMsg += ".qd' or use a direct file import like: use \"";
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

	void SemanticValidator::parseModuleAndCollectFunctions(
			const std::string& moduleName, const std::string& source, const std::string& filePath, bool mergeIntoMain) {
		// Skip if already cached (prevents double-parsing of sibling files)
		if (!filePath.empty() && mParsedModuleAsts.find(filePath) != mParsedModuleAsts.end()) {
			return;
		}

		// Timing helper - only active when QUADC_TIMING is set
		static bool timing = std::getenv("QUADC_TIMING") != nullptr;
		auto timeLast = std::chrono::steady_clock::now();
		auto printTiming = [&](const char* label) {
			if (timing) {
				auto now = std::chrono::steady_clock::now();
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeLast).count();
				std::cerr << "[TIMING]     parseModule/" << label << ": " << ms << "ms (" << moduleName << ")"
						  << std::endl;
				timeLast = now;
			}
		};

		// Parse the module file to AST - use unique_ptr so we can cache it
		auto moduleAst = std::make_unique<Ast>();
		IAstNode* moduleAstRoot = moduleAst->generate(source.c_str(), false, filePath.c_str());
		printTiming("astGenerate");
		if (!moduleAstRoot) {
			// Failed to parse - skip silently
			return;
		}

		// Cache the parsed AST for reuse by the module loop
		if (!filePath.empty()) {
			ParsedModuleAst cached;
			cached.ast = std::move(moduleAst);
			cached.root = moduleAstRoot;
			cached.source = source;
			cached.filePath = filePath;
			mParsedModuleAsts[filePath] = std::move(cached);
			// Track parsing order for dependency-aware type checking
			mParsedModuleOrder.push_back(filePath);
			// Update moduleAst to point to the cached version (we moved from it)
			moduleAst.reset();
		}

		// Track this module as merged BEFORE processing nested imports
		// This allows nested .qd file imports to check if their importer was merged
		if (mergeIntoMain) {
			mMergedModules.insert(moduleName);
		}

		// Process USE statements in the module first (to load .qd file imports)
		// We need to recursively collect definitions, including from imported .qd files
		std::string moduleDirectory;
		if (!filePath.empty()) {
			moduleDirectory = std::filesystem::path(filePath).parent_path().string();
		}
		for (auto* child : moduleAstRoot->children()) {
			if (child && child->type() == IAstNode::Type::USE_STATEMENT) {
				AstNodeUse* use = static_cast<AstNodeUse*>(child);
				// Recursively load this module/file with the current module as context
				// Pass false for reportErrors since this is a nested import
				loadModuleDefinitions(use->module(), moduleName, false, moduleDirectory);
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

		// For local file imports that merge into main namespace, also register in direct lookup maps
		// This allows unqualified access to definitions from included files
		if (mergeIntoMain) {
			for (const auto& func : moduleFunctions) {
				mDefinedFunctions.insert(func.first);
			}
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

		// For local file imports that merge into main namespace, also register constants in direct lookup maps
		if (mergeIntoMain) {
			for (const auto& constant : moduleConstants) {
				mDefinedConstants.insert(constant.first);
				mMergedImportedConstants.insert(constant.first);
			}
			// Also register module enums in mDefinedEnums for type checking
			for (size_t i = 0; i < moduleAstRoot->childCount(); i++) {
				IAstNode* child = moduleAstRoot->child(i);
				if (child && child->type() == IAstNode::Type::ENUM_DECLARATION) {
					AstNodeEnumDeclaration* enumNode = static_cast<AstNodeEnumDeclaration*>(child);
					mDefinedEnums.insert(enumNode->name());
				}
			}
			// Collect module-level `var` declarations so reads from the
			// importing file don't trigger "undefined identifier" errors.
			for (size_t i = 0; i < moduleAstRoot->childCount(); i++) {
				IAstNode* child = moduleAstRoot->child(i);
				if (child && child->type() == IAstNode::Type::GLOBAL_VAR_DECLARATION) {
					auto* varNode = static_cast<AstNodeGlobalVar*>(child);
					mDefinedGlobalVars.insert(varNode->name());
					mGlobalVarTypes[varNode->name()] = varNode->typeName();
				}
			}
		}

		// Also collect constant values
		collectModuleConstantValues(moduleAstRoot, moduleName, mergeIntoMain);

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

		// For local file imports that merge into main namespace, also register structs in direct lookup maps
		if (mergeIntoMain) {
			for (const auto& structEntry : moduleStructs) {
				mDefinedStructs.insert(structEntry.first);
			}
			// Track this module as merged for struct type equivalence checking
			mMergedModules.insert(moduleName);
		}

		// Also collect struct field types so field access validation works
		collectModuleStructFieldTypes(moduleAstRoot, moduleName, mergeIntoMain);

		// Collect struct methods from the module
		collectModuleMethods(moduleAstRoot, moduleName);

		// Collect public imported functions from the module
		std::unordered_map<std::string, ImportedFunctionInfo> moduleImports;
		collectModuleImportedFunctions(moduleAstRoot, moduleName, moduleImports);

		// ... and reject the module's own bodies calling them without the module name.
		checkModuleUnqualifiedImportCalls(moduleAstRoot, moduleName, moduleImports);

		// Store the collected imported functions
		if (mModuleImportedFunctions.find(moduleName) != mModuleImportedFunctions.end()) {
			// Merge: add new imported functions to existing map
			for (const auto& imp : moduleImports) {
				mModuleImportedFunctions[moduleName][imp.first] = imp.second;
			}
		} else {
			// Create new entry
			mModuleImportedFunctions[moduleName] = moduleImports;
		}

		// Analyze function signatures for module functions
		// We use a simplified analysis since we don't need iterative convergence for modules
		analyzeModuleFunctionSignatures(moduleAstRoot, moduleName, mergeIntoMain);
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
			for (const auto& func : importedFuncs) {
				functions[func->name] = func->isPublic;
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectModuleFunctions(child, functions);
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

		// If this is an enum declaration, add variants as scoped constants
		if (node->type() == IAstNode::Type::ENUM_DECLARATION) {
			AstNodeEnumDeclaration* enumNode = static_cast<AstNodeEnumDeclaration*>(node);
			for (const auto& variant : enumNode->variants()) {
				std::string scopedName = enumNode->name() + "::" + variant.name;
				constants[scopedName] = enumNode->isPublic();
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectModuleConstants(child, constants);
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
		for (auto* child : node->children()) {
			collectModuleStructs(child, structs);
		}
	}

	void SemanticValidator::collectModuleMethods(IAstNode* node, const std::string& moduleName) {
		if (!node) {
			return;
		}

		// If this is a function declaration with a receiver, it's a struct method
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			if (func->hasReceiver()) {
				std::string structType = func->receiverType();
				std::string methodName = func->name();
				bool isPublic = func->isPublic();

				// Register in mStructMethods with qualified struct type
				// so method resolution works: "moduleName::StructType" -> methodName -> isPublic
				std::string qualifiedStructType = moduleName + "::" + structType;
				mStructMethods[qualifiedStructType][methodName] = isPublic;
				mStructMethodDecls[qualifiedStructType][methodName] = func;

				// Also register with unqualified struct type for intra-module method calls
				// This allows a module to call its own methods when struct type is not yet qualified
				mStructMethods[structType][methodName] = isPublic;
				mStructMethodDecls[structType][methodName] = func;
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectModuleMethods(child, moduleName);
		}
	}

	void SemanticValidator::collectModuleStructFieldTypes(
			IAstNode* node, const std::string& moduleName, bool mergeIntoMain) {
		if (!node) {
			return;
		}

		// If this is a struct declaration, collect its field types and store the declaration
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);
			// Use qualified key for module structs (e.g., "vec2::Vec2")
			std::string qualifiedName = moduleName + "::" + structDecl->name();
			std::string unqualifiedName = structDecl->name();
			mModuleStructDeclarations[qualifiedName] = structDecl;
			// For local file imports that merge into main namespace, also register with unqualified name
			if (mergeIntoMain) {
				mModuleStructDeclarations[unqualifiedName] = structDecl;
				// Also register in mStructDeclarations for direct unqualified lookup
				mStructDeclarations[unqualifiedName] = structDecl;
			}
			std::unordered_map<std::string, StackValueType> fieldTypes;
			std::vector<std::string> fieldOrder;
			std::unordered_set<std::string> seenFieldNames;

			for (const auto& field : structDecl->fields()) {
				// Check for duplicate field names
				if (seenFieldNames.count(field->name())) {
					std::string errorMsg =
							"Duplicate field name '" + field->name() + "' in struct '" + structDecl->name() + "'";
					reportError(field.get(), errorMsg.c_str());
					return;
				}
				seenFieldNames.insert(field->name());
				fieldOrder.push_back(field->name());

				StackValueType fieldType = StackValueType::UNKNOWN;
				const std::string& typeName = field->typeName();
				if (typeName == "f64") {
					fieldType = StackValueType::FLOAT;
				} else if (typeName == "d128") {
					fieldType = StackValueType::D128;
				} else if (typeName == "i64" || typeName == "i32" || typeName == "i16" || typeName == "i8" ||
						   typeName == "u64" || typeName == "u32" || typeName == "u16" || typeName == "u8") {
					fieldType = StackValueType::INT;
				} else if (typeName == "str") {
					fieldType = StackValueType::STRING;
				} else if (typeName.size() > 1 && typeName[0] == '*') {
					// A pointer to a struct still names the struct -- see the same case in
					// semantic_validator_collect.cc.
					fieldType = StackValueType::PTR;
					std::string pointeeType = typeName.substr(1);
					if (pointeeType.find("::") == std::string::npos && !moduleName.empty() && moduleName != "main" &&
							!mergeIntoMain) {
						pointeeType = moduleName + "::" + pointeeType;
					}
					mStructFieldStructTypes[qualifiedName][field->name()] = pointeeType;
					mStructPointerFields[qualifiedName].insert(field->name());
					if (mergeIntoMain) {
						mStructFieldStructTypes[unqualifiedName][field->name()] = pointeeType;
						mStructPointerFields[unqualifiedName].insert(field->name());
					}
				} else if (typeName == "ptr" || typeName.find('*') != std::string::npos) {
					// Recorded rather than left blank -- see the same case in
					// semantic_validator_collect.cc.
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[qualifiedName][field->name()] = "ptr";
					if (mergeIntoMain) {
						mStructFieldStructTypes[unqualifiedName][field->name()] = "ptr";
					}
				} else if (typeName.size() > 2 && typeName[0] == '[' && typeName[1] == ']') {
					// Array type: []T - treat as PTR and track the array type
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[qualifiedName][field->name()] = typeName;
					if (mergeIntoMain) {
						mStructFieldStructTypes[unqualifiedName][field->name()] = typeName;
					}
				} else if (typeName.size() > 3 && typeName.substr(0, 3) == "fn(") {
					// Function pointer type: fn(i64 -- i64) - treat as PTR
					fieldType = StackValueType::PTR;
					mStructFieldStructTypes[qualifiedName][field->name()] = typeName;
					if (mergeIntoMain) {
						mStructFieldStructTypes[unqualifiedName][field->name()] = typeName;
					}
				} else if (looksLikeStructType(typeName)) {
					// Struct type - treat as PTR and record the struct type name
					fieldType = StackValueType::PTR;
					// If the type is unqualified (no ::), qualify it with the current module name
					// This ensures "ControlPoint" in module "spline" becomes "spline::ControlPoint"
					// so it matches when compared against fully qualified types
					// BUT: don't qualify if merging into main (sibling files share main namespace),
					// and not if the "type" is one of the struct's own type parameters -- `U` in
					// `struct Pair<T, U>` is not a struct in this module, and qualifying it to
					// `ct::U` stopped it being recognised as a parameter at all: constructing a
					// `Pair<i64, str>` was rejected with "Field 'second' expects ct::U, but got
					// string". A struct declared in the program rather than a module never went
					// through this path, which is why the same declaration worked there.
					const auto& structTypeParams = structDecl->typeParams();
					bool isOwnTypeParam = std::find(structTypeParams.begin(), structTypeParams.end(), typeName) !=
										  structTypeParams.end();
					std::string qualifiedTypeName = typeName;
					if (!isOwnTypeParam && typeName.find("::") == std::string::npos && !moduleName.empty() &&
							moduleName != "main" && !mergeIntoMain) {
						qualifiedTypeName = moduleName + "::" + typeName;
					}
					mStructFieldStructTypes[qualifiedName][field->name()] = qualifiedTypeName;
					// For local file imports that merge into main namespace, also register with unqualified name
					if (mergeIntoMain) {
						mStructFieldStructTypes[unqualifiedName][field->name()] = qualifiedTypeName;
					}
				}
				fieldTypes[field->name()] = fieldType;

				// Track fields with default values
				if (field->hasDefaultValue()) {
					mStructFieldsWithDefaults[qualifiedName].insert(field->name());
					// For local file imports that merge into main namespace, also register with unqualified name
					if (mergeIntoMain) {
						mStructFieldsWithDefaults[unqualifiedName].insert(field->name());
					}
				}
			}
			mStructFieldTypes[qualifiedName] = fieldTypes;
			mStructFieldOrder[qualifiedName] = fieldOrder;
			// For local file imports that merge into main namespace, also register with unqualified name
			if (mergeIntoMain) {
				mStructFieldTypes[unqualifiedName] = fieldTypes;
				mStructFieldOrder[unqualifiedName] = fieldOrder;
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectModuleStructFieldTypes(child, moduleName, mergeIntoMain);
		}
	}

	void SemanticValidator::collectModuleImportedFunctions(IAstNode* node, const std::string& moduleName,
			std::unordered_map<std::string, ImportedFunctionInfo>& imports) {
		if (!node) {
			return;
		}

		// If this is an import statement, collect public imported functions
		if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			AstNodeImport* import = static_cast<AstNodeImport*>(node);
			const std::string& library = import->library();
			const std::string& importNamespace = import->namespaceName();

			for (const auto& func : import->functions()) {
				if (func->isPublic) {
					ImportedFunctionInfo info;
					info.library = library;
					info.importNamespace = importNamespace;
					info.cFunctionName = func->name;
					info.throws = func->throws;
					imports[func->name] = info;
				}
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectModuleImportedFunctions(child, moduleName, imports);
		}
	}

	// Reports a bare reference to one of this module's own imported functions.
	//
	// Inside the module that declares the import, `c is_digit` reads as the natural spelling,
	// but it does not resolve to the import. Nothing modelled its stack effect, so the call
	// left an extra value behind: `c is_digit if { ... }` in unicode.qd consumed that instead
	// of the result and always took the true branch, and digit_value answered 17 for 'A'
	// rather than -1. No diagnostic, wrong data.
	//
	// The module's own bodies are not run through typeCheckBlock when a program imports it,
	// which is why this is a walk of its own rather than a case in the type checker. It is a
	// purely syntactic match: only a name declared in this module's import block counts, and
	// the qualified form is a SCOPED_IDENTIFIER, so it never matches here.
	void SemanticValidator::checkModuleUnqualifiedImportCalls(IAstNode* node, const std::string& moduleName,
			const std::unordered_map<std::string, ImportedFunctionInfo>& imports) {
		if (!node || imports.empty()) {
			return;
		}

		// An import block declares the names; it does not call them.
		if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			return;
		}

		if (node->type() == IAstNode::Type::IDENTIFIER || node->type() == IAstNode::Type::INSTRUCTION) {
			const std::string& name = (node->type() == IAstNode::Type::IDENTIFIER)
											  ? static_cast<AstNodeIdentifier*>(node)->name()
											  : static_cast<AstNodeInstruction*>(node)->name();
			if (imports.find(name) != imports.end()) {
				// Reported without a node: the position belongs to the module's source, but the
				// reporter names the file being compiled, so anchoring it here would point the
				// caret at an unrelated line of the importing program. The module and line go in
				// the text instead.
				std::string err = "in module '";
				err += moduleName;
				err += "' (line ";
				err += std::to_string(node->line());
				err += "): '";
				err += name;
				err += "' is imported by that module and must be called as '";
				err += moduleName;
				err += "::";
				err += name;
				err += "'. An unqualified call does not resolve to the import, and leaves the stack wrong "
					   "rather than failing";
				reportError(err.c_str());
			}
		}

		for (auto* child : node->children()) {
			checkModuleUnqualifiedImportCalls(child, moduleName, imports);
		}
	}

	void SemanticValidator::collectModuleConstantValues(
			IAstNode* node, const std::string& moduleName, bool mergeIntoMain) {
		if (!node) {
			return;
		}

		// If this is a constant declaration, store its value
		if (node->type() == IAstNode::Type::CONSTANT_DECLARATION) {
			AstNodeConstant* constNode = static_cast<AstNodeConstant*>(node);
			std::string qualifiedName = moduleName + "::" + constNode->name();
			mModuleConstantValues[qualifiedName] = constNode->value();
			// For local file imports that merge into main namespace, also register with unqualified name
			if (mergeIntoMain) {
				mModuleConstantValues[constNode->name()] = constNode->value();
				// Also register in mConstantValues for direct unqualified lookup
				mConstantValues[constNode->name()] = constNode->value();
			}
		}

		// If this is an enum declaration, store variant values
		if (node->type() == IAstNode::Type::ENUM_DECLARATION) {
			AstNodeEnumDeclaration* enumNode = static_cast<AstNodeEnumDeclaration*>(node);
			// A case label inside the module spells the enum bare; one outside spells it
			// module-qualified. Record both so the exhaustiveness check finds it either way.
			std::vector<std::string>& bare = mEnumVariants[enumNode->name()];
			std::vector<std::string>& qualified = mEnumVariants[moduleName + "::" + enumNode->name()];
			bare.clear();
			qualified.clear();
			for (const auto& variant : enumNode->variants()) {
				std::string scopedName = enumNode->name() + "::" + variant.name;
				std::string qualifiedName = moduleName + "::" + scopedName;
				mModuleConstantValues[qualifiedName] = std::to_string(variant.value);
				bare.push_back(variant.name);
				qualified.push_back(variant.name);
				if (mergeIntoMain) {
					mModuleConstantValues[scopedName] = std::to_string(variant.value);
					mConstantValues[scopedName] = std::to_string(variant.value);
				}
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectModuleConstantValues(child, moduleName, mergeIntoMain);
		}
	}

	void SemanticValidator::analyzeModuleFunctionSignatures(
			IAstNode* node, const std::string& moduleName, bool mergeIntoMain) {
		if (!node) {
			return;
		}

		// Each function's signature, from what it declares
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);

			// Set current type params for generic functions
			mCurrentTypeParams = func->typeParams();
			// Also include receiver type params if this is a method on a generic struct
			if (func->hasReceiverTypeParams()) {
				for (const auto& tp : func->receiverTypeParams()) {
					// Only add if not already in type params
					if (std::find(mCurrentTypeParams.begin(), mCurrentTypeParams.end(), tp) ==
							mCurrentTypeParams.end()) {
						mCurrentTypeParams.push_back(tp);
					}
				}
			}

			for (const auto& paramNode : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				if (!isValidTypeName(param->typeString())) {
					reportError(param, invalidTypeMessage(param->typeString(), parameterSuffix(param)).c_str());
				}
			}

			// Store the signature with qualified name: moduleName::functionName
			FunctionSignature sig;
			// An unqualified struct name in a module's signature is that module's struct, unless
			// the module is merged into the program, where the names stay as the program sees them
			const std::string qualifyIn = (moduleName == "main" || mergeIntoMain) ? "" : moduleName;

			// Build consumes list from input parameters
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i].get());
				addSignatureInput(sig, param->typeString(), qualifyIn);
			}

			// Build produces list: prefer declared output parameters, fall back to body analysis
			// Using declared outputs is more reliable for functions with complex control flow
			if (!func->outputParameters().empty()) {
				for (size_t i = 0; i < func->outputParameters().size(); i++) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(func->outputParameters()[i].get());
					addSignatureOutput(sig, param->typeString(), qualifyIn);
				}
			}
			// A function that declares no outputs produces nothing. The residual of the isolated
			// body analysis used to be substituted here, and for any body with a loop, `switch`,
			// `if` or an unmodelled call that residual was the function's own *inputs* -- so
			// `bytes::fill` was registered as producing four values.
			sig.throws = func->throws();
			sig.typeParams = mCurrentTypeParams;

			// For methods, include receiver as implicit first parameter and use mangled name
			if (func->hasReceiver()) {
				// Insert receiver as first consumed parameter
				sig.consumes.insert(sig.consumes.begin(), StackValueType::PTR);
				// Track receiver struct type (shift other indices)
				std::unordered_map<size_t, std::string> newParamStructTypes;
				for (const auto& entry : sig.parameterStructTypes) {
					newParamStructTypes[entry.first + 1] = entry.second;
				}
				std::string receiverType = func->receiverType();
				if (receiverType.find("::") == std::string::npos) {
					// For "main" namespace or merged modules, keep unqualified
					if (moduleName == "main" || mergeIntoMain) {
						newParamStructTypes[0] = receiverType;
					} else {
						newParamStructTypes[0] = moduleName + "::" + receiverType;
					}
				} else {
					newParamStructTypes[0] = receiverType;
				}
				sig.parameterStructTypes = newParamStructTypes;
				std::unordered_map<size_t, std::string> newParamTypeNames;
				for (const auto& entry : sig.parameterTypeNames) {
					newParamTypeNames[entry.first + 1] = entry.second;
				}
				newParamTypeNames[0] = func->receiverType();
				sig.parameterTypeNames = std::move(newParamTypeNames);

				// Use mangled name: moduleName::StructType::methodName
				std::string qualifiedName = moduleName + "::" + func->receiverType() + "::" + func->name();
				mFunctionSignatures[qualifiedName] = sig;

				// Also register with unqualified struct type for intra-module method calls
				std::string unqualifiedName = func->receiverType() + "::" + func->name();
				if (unqualifiedName != qualifiedName) {
					mFunctionSignatures[unqualifiedName] = sig;
				}
			} else {
				std::string qualifiedName = moduleName + "::" + func->name();
				mFunctionSignatures[qualifiedName] = sig;

				// Also register with unqualified name for intra-module function calls
				// This allows functions within the same module to call each other
				// without qualifying the name (e.g., 'is_unreserved' instead of 'uri::is_unreserved')
				mFunctionSignatures[func->name()] = sig;
			}

			// Clear type params after processing function
			mCurrentTypeParams.clear();
		}
		// Analyze imported functions and register them with the module's namespace
		else if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			AstNodeImport* import = static_cast<AstNodeImport*>(node);
			const auto& importedFuncs = import->functions();

			for (const auto& func : importedFuncs) {
				FunctionSignature sig;

				// Build consumes list from input parameters
				for (size_t i = 0; i < func->inputParameters.size(); i++) {
					AstNodeParameter* param = func->inputParameters[i].get();
					const std::string& typeStr = param->typeString();

					// Validate type name
					if (!isValidTypeName(typeStr)) {
						reportError(param, invalidTypeMessage(typeStr, parameterSuffix(param)).c_str());
					}

					addSignatureInput(sig, typeStr, mergeIntoMain ? "" : moduleName);
				}

				// Build produces list from output parameters
				for (size_t i = 0; i < func->outputParameters.size(); i++) {
					AstNodeParameter* param = func->outputParameters[i].get();
					const std::string& typeStr = param->typeString();

					// Validate type name
					if (!isValidTypeName(typeStr)) {
						reportError(param, invalidTypeMessage(typeStr, parameterSuffix(param)).c_str());
					}

					addSignatureOutput(sig, typeStr, mergeIntoMain ? "" : moduleName);
				}

				// Store the signature with qualified name: moduleName::functionName
				// This allows imported functions to be called with the module's namespace
				sig.throws = func->throws;
				std::string qualifiedName = moduleName + "::" + func->name;
				mFunctionSignatures[qualifiedName] = sig;
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			analyzeModuleFunctionSignatures(child, moduleName, mergeIntoMain);
		}
	}

} // namespace Qd

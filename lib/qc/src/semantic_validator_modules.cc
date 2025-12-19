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

			// Try 2: Include paths from -I flags
			for (const auto& includePath : mIncludePaths) {
				std::string expandedPath = expandTilde(includePath);

				// Check if the include path IS the module directory (contains module.qd directly)
				// and qd.json name matches the module name we're looking for
				std::string directModulePath = expandedPath + "/module.qd";
				if (std::filesystem::exists(directModulePath)) {
					// Check qd.json for module name
					std::string manifestPath = expandedPath + "/qd.json";
					if (std::filesystem::exists(manifestPath)) {
						std::ifstream manifestFile(manifestPath);
						if (manifestFile.is_open()) {
							// Simple JSON name extraction - look for "name": "value"
							std::stringstream manifestBuffer;
							manifestBuffer << manifestFile.rdbuf();
							std::string manifestContent = manifestBuffer.str();
							manifestFile.close();

							// Find "name" key
							size_t nameKeyPos = manifestContent.find("\"name\"");
							if (nameKeyPos != std::string::npos) {
								// Find colon after key
								size_t colonPos = manifestContent.find(':', nameKeyPos + 6);
								if (colonPos != std::string::npos) {
									// Find opening quote of value
									size_t valueStart = manifestContent.find('"', colonPos + 1);
									if (valueStart != std::string::npos) {
										// Find closing quote
										size_t valueEnd = manifestContent.find('"', valueStart + 1);
										if (valueEnd != std::string::npos) {
											std::string nameValue =
													manifestContent.substr(valueStart + 1, valueEnd - valueStart - 1);
											if (nameValue == moduleName) {
												mModuleDirectories[moduleName] = expandedPath;
												file.open(directModulePath);
												if (file.good()) {
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
									}
								}
							}
						}
					}
				}

				// Check for module as subdirectory of include path
				std::string includedPath = expandedPath + "/" + moduleName + "/module.qd";
				file.open(includedPath);
				if (file.good()) {
					mModuleDirectories[moduleName] = expandedPath + "/" + moduleName;
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string source = buffer.str();
					file.close();
					parseModuleAndCollectFunctions(moduleName, source);
					return;
				}
				file.close();
			}

			// Try 3: Third-party packages directory (installed via quadpm)
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
			std::string stdLibPath = "lib/qd" + moduleName + "/qd/" + moduleName + "/module.qd";
			file.open(stdLibPath);
			if (file.good()) {
				// Store the module directory
				mModuleDirectories[moduleName] = "lib/qd" + moduleName + "/qd/" + moduleName;
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
		collectModuleStructFieldTypes(moduleAstRoot, moduleName);

		// Collect public imported functions from the module
		std::unordered_map<std::string, ImportedFunctionInfo> moduleImports;
		collectModuleImportedFunctions(moduleAstRoot, moduleName, moduleImports);

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
				functions[func->name] = func->isPublic;
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

	void SemanticValidator::collectModuleStructFieldTypes(IAstNode* node, const std::string& moduleName) {
		if (!node) {
			return;
		}

		// If this is a struct declaration, collect its field types and store the declaration
		if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
			AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);
			// Use qualified key for module structs (e.g., "vec2::Vec2")
			std::string qualifiedName = moduleName + "::" + structDecl->name();
			mModuleStructDeclarations[qualifiedName] = structDecl;
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
					mStructFieldStructTypes[qualifiedName][field->name()] = typeName;
				}
				fieldTypes[field->name()] = fieldType;
			}
			mStructFieldTypes[qualifiedName] = fieldTypes;
			mStructFieldOrder[qualifiedName] = fieldOrder;
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleStructFieldTypes(node->child(i), moduleName);
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

			for (const auto* func : import->functions()) {
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
		for (size_t i = 0; i < node->childCount(); i++) {
			collectModuleImportedFunctions(node->child(i), moduleName, imports);
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
					// Qualify struct type with module name if not already qualified
					if (typeStr.find("::") == std::string::npos) {
						sig.parameterStructTypes[i] = moduleName + "::" + typeStr;
					} else {
						sig.parameterStructTypes[i] = typeStr;
					}
				} else {
					// Untyped or unknown - use ANY
					sig.consumes.push_back(StackValueType::ANY);
				}
			}

			// Build produces list: prefer declared output parameters, fall back to body analysis
			// Using declared outputs is more reliable for functions with complex control flow
			if (!func->outputParameters().empty()) {
				for (size_t i = 0; i < func->outputParameters().size(); i++) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(func->outputParameters()[i]);
					const std::string& typeStr = param->typeString();

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
						// Qualify struct type with module name if not already qualified
						if (typeStr.find("::") == std::string::npos) {
							sig.producesStructTypes[i] = moduleName + "::" + typeStr;
						} else {
							sig.producesStructTypes[i] = typeStr;
						}
					} else {
						sig.produces.push_back(StackValueType::ANY);
					}
				}
			} else {
				// No declared outputs - use body analysis result
				sig.produces = typeStack;
			}
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
						// Qualify struct type with module name if not already qualified
						if (typeStr.find("::") == std::string::npos) {
							sig.parameterStructTypes[i] = moduleName + "::" + typeStr;
						} else {
							sig.parameterStructTypes[i] = typeStr;
						}
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
					} else if (typeStr == "ptr") {
						sig.produces.push_back(StackValueType::PTR);
					} else if (isStructTypeName(typeStr)) {
						sig.produces.push_back(StackValueType::PTR);
						// Qualify struct type with module name if not already qualified
						if (typeStr.find("::") == std::string::npos) {
							sig.producesStructTypes[i] = moduleName + "::" + typeStr;
						} else {
							sig.producesStructTypes[i] = typeStr;
						}
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

} // namespace Qd

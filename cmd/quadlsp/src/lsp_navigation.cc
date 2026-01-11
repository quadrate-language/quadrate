// Navigation handlers for QuadrateLSP
// Split from main.cc for maintainability

#include "lsp_impl.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <qc/ast.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_field_access.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_struct.h>
#include <sstream>

void QuadrateLSP::findIdentifiersInNode(
		Qd::IAstNode* node, const std::string& targetName, std::vector<Qd::IAstNode*>& results) {
	if (!node) {
		return;
	}

	// Check if this node is a function declaration matching our target
	if (node->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
		Qd::AstNodeFunctionDeclaration* funcDecl = static_cast<Qd::AstNodeFunctionDeclaration*>(node);
		if (funcDecl->name() == targetName) {
			results.push_back(node);
		}
	}
	// Check if this node is a local variable declaration (-> varname)
	else if (node->type() == Qd::IAstNode::Type::LOCAL) {
		Qd::AstNodeLocal* local = static_cast<Qd::AstNodeLocal*>(node);
		// Check all names in the local (supports -> a b c syntax)
		for (const std::string& name : local->names()) {
			if (name == targetName) {
				results.push_back(node);
				break;
			}
		}
	}
	// Check if this node is an identifier matching our target
	else if (node->type() == Qd::IAstNode::Type::IDENTIFIER) {
		Qd::AstNodeIdentifier* ident = static_cast<Qd::AstNodeIdentifier*>(node);
		if (ident->name() == targetName) {
			results.push_back(node);
		}
	} else if (node->type() == Qd::IAstNode::Type::SCOPED_IDENTIFIER) {
		Qd::AstNodeScopedIdentifier* scoped = static_cast<Qd::AstNodeScopedIdentifier*>(node);
		std::string fullName = scoped->scope() + "::" + scoped->name();
		if (fullName == targetName || scoped->name() == targetName) {
			results.push_back(node);
		}
	}

	// Recursively search children
	for (size_t i = 0; i < node->childCount(); i++) {
		findIdentifiersInNode(node->child(i), targetName, results);
	}
}

Qd::AstNodeLocal* QuadrateLSP::findLocalDeclaration(
		Qd::IAstNode* startNode, const std::string& varName, size_t requestLine) {
	if (!startNode) {
		return nullptr;
	}

	// Walk up to find the containing function
	Qd::IAstNode* current = startNode;
	Qd::IAstNode* functionNode = nullptr;

	while (current) {
		if (current->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			functionNode = current;
			break;
		}
		current = current->parent();
	}

	if (!functionNode) {
		return nullptr;
	}

	// Recursively search for local declarations in the function
	// We need to find declarations that appear before the request line
	std::vector<Qd::AstNodeLocal*> candidates;
	std::function<void(Qd::IAstNode*)> searchLocals = [&](Qd::IAstNode* node) {
		if (!node) {
			return;
		}

		if (node->type() == Qd::IAstNode::Type::LOCAL) {
			Qd::AstNodeLocal* localNode = static_cast<Qd::AstNodeLocal*>(node);
			// Check all names (supports multiple assignment: -> a b c)
			const auto& names = localNode->names();
			for (const auto& name : names) {
				if (name == varName) {
					// Only consider declarations that appear before the request line
					size_t declLine = (localNode->line() > 0) ? localNode->line() - 1 : 0;
					if (declLine <= requestLine) {
						candidates.push_back(localNode);
					}
					break; // Found match, no need to check more names
				}
			}
		}

		// Recursively search children
		for (size_t i = 0; i < node->childCount(); i++) {
			searchLocals(node->child(i));
		}
	};

	searchLocals(functionNode);

	// Return the last declaration before the request line (closest scope)
	if (!candidates.empty()) {
		return candidates.back();
	}

	return nullptr;
}

json_t* QuadrateLSP::findDefinitionInModule(
		const std::string& modulePath, const std::string& symbolName, const std::string& symbolType) {
	// Read the module file
	std::ifstream file(modulePath);
	if (!file.good()) {
		return json_null();
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string moduleText = buffer.str();

	// Parse the module file
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

	if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
		return json_null();
	}

	// Search for the definition
	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);

		if (symbolType == "function" && child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
			if (funcNode->name() == symbolName) {
				// Found the function definition
				json_t* location = json_object();
				std::string moduleUri = "file://" + modulePath;
				json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

				json_t* range = json_object();
				json_t* start = json_object();
				size_t lspLine = (funcNode->line() > 0) ? funcNode->line() - 1 : 0;
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(start, "character", json_integer(0));
				json_object_set_new(range, "start", start);

				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(funcNode->name().length())));
				json_object_set_new(range, "end", end);

				json_object_set_new(location, "range", range);
				return location;
			}
		} else if (symbolType == "constant" && child && child->type() == Qd::IAstNode::Type::CONSTANT_DECLARATION) {
			Qd::AstNodeConstant* constNode = static_cast<Qd::AstNodeConstant*>(child);
			if (constNode->name() == symbolName) {
				// Found the constant definition
				json_t* location = json_object();
				std::string moduleUri = "file://" + modulePath;
				json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

				json_t* range = json_object();
				json_t* start = json_object();
				size_t lspLine = (constNode->line() > 0) ? constNode->line() - 1 : 0;
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(start, "character", json_integer(0));
				json_object_set_new(range, "start", start);

				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(constNode->name().length())));
				json_object_set_new(range, "end", end);

				json_object_set_new(location, "range", range);
				return location;
			}
		} else if (symbolType == "struct" && child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
			Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);
			if (structNode->name() == symbolName) {
				// Found the struct definition
				json_t* location = json_object();
				std::string moduleUri = "file://" + modulePath;
				json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

				json_t* range = json_object();
				json_t* start = json_object();
				size_t lspLine = (structNode->line() > 0) ? structNode->line() - 1 : 0;
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(start, "character", json_integer(0));
				json_object_set_new(range, "start", start);

				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(
						end, "character", json_integer(static_cast<json_int_t>(structNode->name().length())));
				json_object_set_new(range, "end", end);

				json_object_set_new(location, "range", range);
				return location;
			}
		} else if (symbolType == "function" && child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
			// Check for imported functions (like those in stdlib modules)
			Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
			const auto& importedFuncs = importNode->functions();
			for (const auto* importedFunc : importedFuncs) {
				if (importedFunc->name == symbolName) {
					// Found the imported function declaration
					json_t* location = json_object();
					std::string moduleUri = "file://" + modulePath;
					json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (importedFunc->line > 0) ? importedFunc->line - 1 : 0;
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(0));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(importedFunc->name.length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(location, "range", range);
					return location;
				}
			}
		}
	}

	return json_null();
}

std::string QuadrateLSP::findStructTypeOfVariable(Qd::IAstNode* root, const std::string& varName, size_t requestLine) {
	// Find the function containing the requested line
	Qd::AstNodeFunctionDeclaration* functionNode = nullptr;
	std::function<void(Qd::IAstNode*)> searchFunction = [&](Qd::IAstNode* node) {
		if (!node) {
			return;
		}

		if (node->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(node);
			// Check if the requested line is within this function
			size_t funcStartLine = (funcNode->line() > 0) ? funcNode->line() - 1 : 0;
			// Simple heuristic: if the function starts before or at the request line, it might contain it
			if (funcStartLine <= requestLine) {
				functionNode = funcNode;
			}
		}

		for (size_t i = 0; i < node->childCount(); i++) {
			searchFunction(node->child(i));
		}
	};

	searchFunction(root);

	if (!functionNode) {
		return "";
	}

	// Find the local declaration and look for a preceding struct constructor
	// Pattern: ... StructName -> varName  OR  ... module::StructName -> varName
	std::string structType;
	std::function<void(Qd::IAstNode*)> searchConstructor = [&](Qd::IAstNode* node) {
		if (!node || !structType.empty()) {
			return;
		}

		// If this is a block, iterate through children sequentially
		if (node->type() == Qd::IAstNode::Type::BLOCK) {
			for (size_t i = 0; i < node->childCount(); i++) {
				Qd::IAstNode* child = node->child(i);
				if (!child) {
					continue;
				}

				// Look for LOCAL node with matching name
				if (child->type() == Qd::IAstNode::Type::LOCAL) {
					Qd::AstNodeLocal* localNode = static_cast<Qd::AstNodeLocal*>(child);
					// Check all names (supports multiple assignment: -> a b c)
					const auto& localNames = localNode->names();
					bool hasMatch = false;
					for (const auto& name : localNames) {
						if (name == varName) {
							hasMatch = true;
							break;
						}
					}
					if (hasMatch) {
						// Found the declaration - now look backwards for struct constructor
						// Check previous sibling
						if (i > 0) {
							Qd::IAstNode* prevSibling = node->child(i - 1);
							if (prevSibling) {
								// Check if it's a scoped identifier (module::Struct)
								if (prevSibling->type() == Qd::IAstNode::Type::SCOPED_IDENTIFIER) {
									Qd::AstNodeScopedIdentifier* scopedNode =
											static_cast<Qd::AstNodeScopedIdentifier*>(prevSibling);
									structType = scopedNode->scope() + "::" + scopedNode->name();
									return;
								}
								// Check if it's a plain identifier (Struct)
								else if (prevSibling->type() == Qd::IAstNode::Type::IDENTIFIER) {
									Qd::AstNodeIdentifier* identNode =
											static_cast<Qd::AstNodeIdentifier*>(prevSibling);
									structType = identNode->name();
									return;
								}
							}
						}
						return;
					}
				}
			}
		}

		// Recursively search children
		for (size_t i = 0; i < node->childCount(); i++) {
			searchConstructor(node->child(i));
		}
	};

	searchConstructor(functionNode);
	return structType;
}

json_t* QuadrateLSP::handleFieldAccessDefinition(
		Qd::IAstNode* root, const std::string& uri, size_t line, bool cursorOnVariable) {
	// Find the field access node at the target line
	Qd::AstNodeFieldAccess* fieldAccessNode = nullptr;
	std::function<void(Qd::IAstNode*)> searchFieldAccess = [&](Qd::IAstNode* node) {
		if (!node) {
			return;
		}

		if (node->type() == Qd::IAstNode::Type::FIELD_ACCESS) {
			Qd::AstNodeFieldAccess* faNode = static_cast<Qd::AstNodeFieldAccess*>(node);
			// Check if this field access is on the target line
			size_t nodeLine = (faNode->line() > 0) ? faNode->line() - 1 : 0;
			if (nodeLine == line) {
				fieldAccessNode = faNode;
				return;
			}
		}

		// Recursively search children
		for (size_t i = 0; i < node->childCount(); i++) {
			searchFieldAccess(node->child(i));
			if (fieldAccessNode) {
				return;
			}
		}
	};

	searchFieldAccess(root);

	if (!fieldAccessNode) {
		return json_null();
	}

	if (cursorOnVariable) {
		// User clicked on the variable name - find the local variable declaration
		std::string varName = fieldAccessNode->varName();
		Qd::AstNodeLocal* localNode = findLocalDeclaration(fieldAccessNode, varName, line);

		if (localNode) {
			// Found the local variable declaration
			json_t* location = json_object();
			json_object_set_new(location, "uri", json_string(uri.c_str()));

			json_t* range = json_object();
			json_t* start = json_object();
			size_t lspLine = (localNode->line() > 0) ? localNode->line() - 1 : 0;
			json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
			json_object_set_new(start, "character", json_integer(0));
			json_object_set_new(range, "start", start);

			json_t* end = json_object();
			json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
			json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(localNode->name().length())));
			json_object_set_new(range, "end", end);

			json_object_set_new(location, "range", range);
			return location;
		}
	} else {
		// User clicked on the field name - find the struct field definition
		std::string varName = fieldAccessNode->varName();
		std::string fieldName = fieldAccessNode->fieldName();

		// Find the struct type of this variable
		std::string structType = findStructTypeOfVariable(root, varName, line);
		if (structType.empty()) {
			return json_null();
		}

		// Now find the struct declaration
		// First check if it's a scoped struct (module::StructName)
		size_t colonPos = structType.find("::");
		if (colonPos != std::string::npos) {
			// This is a module struct
			std::string moduleName = structType.substr(0, colonPos);
			std::string structName = structType.substr(colonPos + 2);

			// Get source directory from the current document URI
			std::string sourceDir;
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				size_t lastSlash = filePath.find_last_of('/');
				if (lastSlash != std::string::npos) {
					sourceDir = filePath.substr(0, lastSlash);
				}
			}

			// Resolve module path
			std::string modulePath = resolveModulePath(moduleName, sourceDir);
			if (!modulePath.empty()) {
				// Parse the module file
				std::ifstream moduleFile(modulePath);
				if (moduleFile.good()) {
					std::stringstream buffer;
					buffer << moduleFile.rdbuf();
					std::string moduleContent = buffer.str();

					Qd::Ast moduleAst;
					Qd::IAstNode* moduleRoot = moduleAst.generate(moduleContent.c_str(), false, nullptr);

					if (moduleRoot && !moduleAst.hasErrors()) {
						// Find the struct in the module
						for (size_t i = 0; i < moduleRoot->childCount(); i++) {
							Qd::IAstNode* child = moduleRoot->child(i);
							if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
								Qd::AstNodeStructDeclaration* structNode =
										static_cast<Qd::AstNodeStructDeclaration*>(child);
								if (structNode->name() == structName) {
									// Found the struct - now find the field
									const auto& fields = structNode->fields();
									for (const auto* field : fields) {
										if (field->name() == fieldName) {
											// Found the field!
											json_t* location = json_object();
											std::string moduleUri = "file://" + modulePath;
											json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

											json_t* range = json_object();
											json_t* start_json = json_object();
											size_t lspLine = (field->line() > 0) ? field->line() - 1 : 0;
											json_object_set_new(start_json, "line",
													json_integer(static_cast<json_int_t>(lspLine)));
											json_object_set_new(start_json, "character", json_integer(0));
											json_object_set_new(range, "start", start_json);

											json_t* end = json_object();
											json_object_set_new(
													end, "line", json_integer(static_cast<json_int_t>(lspLine)));
											json_object_set_new(end, "character",
													json_integer(static_cast<json_int_t>(field->name().length())));
											json_object_set_new(range, "end", end);

											json_object_set_new(location, "range", range);
											return location;
										}
									}
								}
							}
						}
					}
				}
			}
		} else {
			// This is a local struct in the same file
			for (size_t i = 0; i < root->childCount(); i++) {
				Qd::IAstNode* child = root->child(i);
				if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
					Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);
					if (structNode->name() == structType) {
						// Found the struct - now find the field
						const auto& fields = structNode->fields();
						for (const auto* field : fields) {
							if (field->name() == fieldName) {
								// Found the field!
								json_t* location = json_object();
								json_object_set_new(location, "uri", json_string(uri.c_str()));

								json_t* range = json_object();
								json_t* start_json = json_object();
								size_t lspLine = (field->line() > 0) ? field->line() - 1 : 0;
								json_object_set_new(
										start_json, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(start_json, "character", json_integer(0));
								json_object_set_new(range, "start", start_json);

								json_t* end = json_object();
								json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(end, "character",
										json_integer(static_cast<json_int_t>(field->name().length())));
								json_object_set_new(range, "end", end);

								json_object_set_new(location, "range", range);
								return location;
							}
						}
					}
				}
			}
		}
	}

	return json_null();
}

void QuadrateLSP::handleDefinition(const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	} else {
		// Try to read from disk
		if (uri.substr(0, 7) == "file://") {
			std::string filePath = uri.substr(7);
			std::ifstream file(filePath);
			if (file.good()) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				documentText = buffer.str();
			}
		}
	}

	json_t* result = json_null();

	if (!documentText.empty()) {
		// First check if we're on an import statement line (use <module>)
		// This is done before AST parsing to work even with parse errors
		std::vector<std::string> docLines;
		{
			std::istringstream lineStream(documentText);
			std::string docLine;
			while (std::getline(lineStream, docLine)) {
				docLines.push_back(docLine);
			}
		}
		if (line < docLines.size()) {
			const std::string& targetLine = docLines[line];
			// Check if line starts with "use "
			size_t usePos = targetLine.find("use ");
			if (usePos != std::string::npos && usePos < 10) { // Allow some leading whitespace
				// Extract module name after "use "
				size_t moduleStart = usePos + 4;
				while (moduleStart < targetLine.length() && isspace(targetLine[moduleStart])) {
					moduleStart++;
				}
				size_t moduleEnd = moduleStart;
				while (moduleEnd < targetLine.length() &&
						(isalnum(targetLine[moduleEnd]) || targetLine[moduleEnd] == '_')) {
					moduleEnd++;
				}
				// Only navigate if cursor is on the module name, not on "use" keyword
				if (moduleEnd > moduleStart && character >= moduleStart && character < moduleEnd) {
					std::string moduleName = targetLine.substr(moduleStart, moduleEnd - moduleStart);
					std::string sourceDir = std::filesystem::path(uri.substr(7)).parent_path().string();
					std::string modulePath = resolveModulePath(moduleName, sourceDir);

					if (!modulePath.empty()) {
						json_t* location = json_object();
						json_object_set_new(location, "uri", json_string(("file://" + modulePath).c_str()));

						json_t* range = json_object();
						json_t* start = json_object();
						json_object_set_new(start, "line", json_integer(0));
						json_object_set_new(start, "character", json_integer(0));
						json_object_set_new(range, "start", start);

						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(0));
						json_object_set_new(end, "character", json_integer(0));
						json_object_set_new(range, "end", end);

						json_object_set_new(location, "range", range);
						result = location;

						json_object_set_new(response, "result", result);
						sendMessage(response);
						json_decref(response);
						return;
					}
				}
			}
		}

		std::string word = getWordAtPosition(documentText, line, character);

		if (!word.empty()) {
			// Parse the document
			Qd::Ast ast;
			Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

			if (root && !ast.hasErrors() && root->type() == Qd::IAstNode::Type::PROGRAM) {
				// Check if we're in a field access expression (v@x)
				// Find the character at the cursor position to see if @ is nearby
				std::vector<std::string> lines;
				std::istringstream stream(documentText);
				std::string currentLine;
				while (std::getline(stream, currentLine)) {
					lines.push_back(currentLine);
				}

				if (line < lines.size()) {
					const std::string& targetLine = lines[line];
					// Look for @ before or after the cursor
					bool inFieldAccess = false;
					bool cursorOnVariable = false;

					// Search for @ near the cursor
					for (size_t i = 0; i < targetLine.length(); i++) {
						if (targetLine[i] == '@') {
							// Check if cursor is near this @
							if (i >= character) {
								// @ is at or after cursor - cursor might be on variable
								if (i - character <= word.length()) {
									inFieldAccess = true;
									cursorOnVariable = true;
									break;
								}
							} else {
								// @ is before cursor - cursor might be on field name
								if (character - i <= word.length() + 1) {
									inFieldAccess = true;
									cursorOnVariable = false;
									break;
								}
							}
						}
					}

					if (inFieldAccess) {
						// Find the field access node at this location
						result = handleFieldAccessDefinition(root, uri, line, cursorOnVariable);
						if (!json_is_null(result)) {
							json_object_set_new(response, "result", result);
							sendMessage(response);
							json_decref(response);
							return;
						}
					}
				}

				// Search for function or struct declaration matching the word
				for (size_t i = 0; i < root->childCount(); i++) {
					Qd::IAstNode* child = root->child(i);

					if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
						Qd::AstNodeFunctionDeclaration* funcNode =
								static_cast<Qd::AstNodeFunctionDeclaration*>(child);

						if (funcNode->name() == word) {
							// Found the definition
							json_t* location = json_object();
							json_object_set_new(location, "uri", json_string(uri.c_str()));

							json_t* range = json_object();
							json_t* start = json_object();
							size_t lspLine = (funcNode->line() > 0) ? funcNode->line() - 1 : 0;
							json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(start, "character", json_integer(0));
							json_object_set_new(range, "start", start);

							json_t* end = json_object();
							json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(end, "character",
									json_integer(static_cast<json_int_t>(funcNode->name().length())));
							json_object_set_new(range, "end", end);

							json_object_set_new(location, "range", range);
							result = location;
							break;
						}
					} else if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
						Qd::AstNodeStructDeclaration* structNode =
								static_cast<Qd::AstNodeStructDeclaration*>(child);

						if (structNode->name() == word) {
							// Found the struct definition
							json_t* location = json_object();
							json_object_set_new(location, "uri", json_string(uri.c_str()));

							json_t* range = json_object();
							json_t* start = json_object();
							size_t lspLine = (structNode->line() > 0) ? structNode->line() - 1 : 0;
							json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(start, "character", json_integer(0));
							json_object_set_new(range, "start", start);

							json_t* end = json_object();
							json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(end, "character",
									json_integer(static_cast<json_int_t>(structNode->name().length())));
							json_object_set_new(range, "end", end);

							json_object_set_new(location, "range", range);
							result = location;
							break;
						}
					} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
						// Check imported functions
						Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
						std::string namespaceName = importNode->namespaceName();

						const auto& importedFuncs = importNode->functions();
						for (const auto* importedFunc : importedFuncs) {
							std::string fullName = namespaceName + "::" + importedFunc->name;
							if (fullName == word || importedFunc->name == word) {
								// Found the imported function declaration
								json_t* location = json_object();
								json_object_set_new(location, "uri", json_string(uri.c_str()));

								json_t* range = json_object();
								json_t* start = json_object();
								size_t lspLine = (importedFunc->line > 0) ? importedFunc->line - 1 : 0;
								json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(start, "character", json_integer(0));
								json_object_set_new(range, "start", start);

								json_t* end = json_object();
								json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(end, "character",
										json_integer(static_cast<json_int_t>(importedFunc->name.length())));
								json_object_set_new(range, "end", end);

								json_object_set_new(location, "range", range);
								result = location;
								break;
							}
						}
						if (!json_is_null(result)) {
							break;
						}
					}
				}

				// If no function or import found, try searching for local variable declarations
				if (json_is_null(result)) {
					// Find the identifier node at the cursor position
					std::vector<Qd::IAstNode*> identifiers;
					findIdentifiersInNode(root, word, identifiers);

					// Find the identifier at the requested line
					Qd::IAstNode* targetIdentifier = nullptr;
					for (auto* node : identifiers) {
						size_t nodeLine = (node->line() > 0) ? node->line() - 1 : 0;
						if (nodeLine == line && node->type() == Qd::IAstNode::Type::IDENTIFIER) {
							targetIdentifier = node;
							break;
						}
					}

					if (targetIdentifier) {
						// Search for the local variable declaration
						Qd::AstNodeLocal* localDecl = findLocalDeclaration(targetIdentifier, word, line);

						if (localDecl) {
							// Found the local variable declaration
							json_t* location = json_object();
							json_object_set_new(location, "uri", json_string(uri.c_str()));

							json_t* range = json_object();
							json_t* start = json_object();
							size_t lspLine = (localDecl->line() > 0) ? localDecl->line() - 1 : 0;
							json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(start, "character", json_integer(0));
							json_object_set_new(range, "start", start);

							json_t* end = json_object();
							json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(end, "character",
									json_integer(static_cast<json_int_t>(localDecl->name().length())));
							json_object_set_new(range, "end", end);

							json_object_set_new(location, "range", range);
							result = location;
						}
					}
				}

				// If still not found, try scoped identifiers (module::symbol)
				if (json_is_null(result) && word.find("::") != std::string::npos) {
					// Extract module name and symbol name
					size_t colonPos = word.find("::");
					std::string moduleName = word.substr(0, colonPos);
					std::string symbolName = word.substr(colonPos + 2);

					// Get source directory from URI
					std::string filePath = uri.substr(7); // Remove "file://"
					std::string sourceDir = std::filesystem::path(filePath).parent_path().string();

					// Resolve module path
					std::string modulePath = resolveModulePath(moduleName, sourceDir);

					if (!modulePath.empty()) {
						// Try to find the symbol as a function first, then as a constant, then as a struct
						result = findDefinitionInModule(modulePath, symbolName, "function");
						if (json_is_null(result)) {
							result = findDefinitionInModule(modulePath, symbolName, "constant");
						}
						if (json_is_null(result)) {
							result = findDefinitionInModule(modulePath, symbolName, "struct");
						}
					}
				}
			}
		}
	}

	json_object_set_new(response, "result", result);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleFoldingRange(const std::string& id, const std::string& uri) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	} else {
		if (uri.substr(0, 7) == "file://") {
			std::string filePath = uri.substr(7);
			std::ifstream file(filePath);
			if (file.good()) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				documentText = buffer.str();
			}
		}
	}

	json_t* ranges = json_array();

	if (!documentText.empty()) {
		// Parse the document
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

		if (root && !ast.hasErrors() && root->type() == Qd::IAstNode::Type::PROGRAM) {
			// Helper lambda to recursively find folding ranges
			std::function<void(Qd::IAstNode*)> findFoldingRanges = [&](Qd::IAstNode* node) {
				if (!node) {
					return;
				}

				size_t startLine = 0;
				size_t endLine = 0;
				std::string kind;

				// Check for foldable constructs
				if (node->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
					Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(node);
					startLine = funcNode->line() > 0 ? funcNode->line() - 1 : 0;

					// Find the end line by looking at the last instruction
					if (funcNode->childCount() > 0) {
						Qd::IAstNode* lastChild = funcNode->child(funcNode->childCount() - 1);
						if (lastChild) {
							endLine = lastChild->line() > 0 ? lastChild->line() - 1 : startLine;
						}
					}
					// If no children, just fold the declaration line
					if (endLine <= startLine) {
						endLine = startLine;
					}
					kind = "region";
				} else if (node->type() == Qd::IAstNode::Type::IF_STATEMENT) {
					startLine = node->line() > 0 ? node->line() - 1 : 0;
					// Find end by traversing children
					std::function<size_t(Qd::IAstNode*)> findMaxLine = [&](Qd::IAstNode* n) -> size_t {
						if (!n) {
							return 0;
						}
						size_t maxLine = n->line() > 0 ? n->line() - 1 : 0;
						for (size_t i = 0; i < n->childCount(); i++) {
							size_t childMax = findMaxLine(n->child(i));
							if (childMax > maxLine) {
								maxLine = childMax;
							}
						}
						return maxLine;
					};
					endLine = findMaxLine(node);
					kind = "region";
				} else if (node->type() == Qd::IAstNode::Type::WHILE_STATEMENT ||
						   node->type() == Qd::IAstNode::Type::FOR_STATEMENT ||
						   node->type() == Qd::IAstNode::Type::LOOP_STATEMENT) {
					startLine = node->line() > 0 ? node->line() - 1 : 0;
					std::function<size_t(Qd::IAstNode*)> findMaxLine = [&](Qd::IAstNode* n) -> size_t {
						if (!n) {
							return 0;
						}
						size_t maxLine = n->line() > 0 ? n->line() - 1 : 0;
						for (size_t i = 0; i < n->childCount(); i++) {
							size_t childMax = findMaxLine(n->child(i));
							if (childMax > maxLine) {
								maxLine = childMax;
							}
						}
						return maxLine;
					};
					endLine = findMaxLine(node);
					kind = "region";
				} else if (node->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
					// Import blocks can be folded
					startLine = node->line() > 0 ? node->line() - 1 : 0;
					// For imports, we'd need to find the closing brace
					// For now, just don't fold single-line imports
					kind = "imports";
				} else if (node->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
					startLine = node->line() > 0 ? node->line() - 1 : 0;
					std::function<size_t(Qd::IAstNode*)> findMaxLine = [&](Qd::IAstNode* n) -> size_t {
						if (!n) {
							return 0;
						}
						size_t maxLine = n->line() > 0 ? n->line() - 1 : 0;
						for (size_t i = 0; i < n->childCount(); i++) {
							size_t childMax = findMaxLine(n->child(i));
							if (childMax > maxLine) {
								maxLine = childMax;
							}
						}
						return maxLine;
					};
					endLine = findMaxLine(node);
					kind = "region";
				}

				// Add folding range if we found a valid one
				if (!kind.empty() && endLine > startLine) {
					json_t* range = json_object();
					json_object_set_new(range, "startLine", json_integer(static_cast<json_int_t>(startLine)));
					json_object_set_new(range, "endLine", json_integer(static_cast<json_int_t>(endLine)));
					json_object_set_new(range, "kind", json_string(kind.c_str()));
					json_array_append_new(ranges, range);
				}

				// Recurse into children
				for (size_t i = 0; i < node->childCount(); i++) {
					findFoldingRanges(node->child(i));
				}
			};

			// Process all top-level nodes
			for (size_t i = 0; i < root->childCount(); i++) {
				findFoldingRanges(root->child(i));
			}
		}
	}

	json_object_set_new(response, "result", ranges);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleDocumentHighlight(const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	} else {
		if (uri.substr(0, 7) == "file://") {
			std::string filePath = uri.substr(7);
			std::ifstream file(filePath);
			if (file.good()) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				documentText = buffer.str();
			}
		}
	}

	json_t* highlights = json_array();

	if (!documentText.empty()) {
		std::string word = getWordAtPosition(documentText, line, character);

		if (!word.empty()) {
			// Parse the document
			Qd::Ast ast;
			Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

			if (root && !ast.hasErrors()) {
				// Find all references to this identifier
				std::vector<Qd::IAstNode*> references;
				findIdentifiersInNode(root, word, references);

				for (Qd::IAstNode* ref : references) {
					json_t* highlight = json_object();

					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (ref->line() > 0) ? ref->line() - 1 : 0;
					size_t lspCol = (ref->column() > 0) ? ref->column() - 1 : 0;

					// For function declarations, find the actual column of the function name
					if (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
						std::istringstream lineStream(documentText);
						std::string lineText;
						for (size_t i = 0; i <= lspLine; i++) {
							if (!std::getline(lineStream, lineText)) {
								lineText.clear();
								break;
							}
						}
						size_t fnPos = lineText.find("fn ");
						if (fnPos != std::string::npos) {
							lspCol = fnPos + 3;
						}
					}

					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspCol)));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(lspCol + word.length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(highlight, "range", range);

					// DocumentHighlightKind: 1 = Text, 2 = Read, 3 = Write
					// For function declarations, use Write; for references, use Read
					int kind = (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) ? 3 : 2;
					json_object_set_new(highlight, "kind", json_integer(kind));

					json_array_append_new(highlights, highlight);
				}
			}
		}
	}

	json_object_set_new(response, "result", highlights);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleReferences(const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	} else {
		// Try to read from disk
		if (uri.substr(0, 7) == "file://") {
			std::string filePath = uri.substr(7);
			std::ifstream file(filePath);
			if (file.good()) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				documentText = buffer.str();
			}
		}
	}

	json_t* locations = json_array();

	if (!documentText.empty()) {
		std::string word = getWordAtPosition(documentText, line, character);

		if (!word.empty()) {
			// Parse the document
			Qd::Ast ast;
			Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

			if (root && !ast.hasErrors()) {
				// Find all references to this identifier
				std::vector<Qd::IAstNode*> references;
				findIdentifiersInNode(root, word, references);

				for (Qd::IAstNode* ref : references) {
					json_t* location = json_object();
					json_object_set_new(location, "uri", json_string(uri.c_str()));

					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (ref->line() > 0) ? ref->line() - 1 : 0;
					size_t lspCol = (ref->column() > 0) ? ref->column() - 1 : 0;

					// For function declarations, find the actual column of the function name
					if (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
						std::istringstream lineStream(documentText);
						std::string lineText;
						for (size_t i = 0; i <= lspLine; i++) {
							if (!std::getline(lineStream, lineText)) {
								lineText.clear();
								break;
							}
						}
						size_t fnPos = lineText.find("fn ");
						if (fnPos != std::string::npos) {
							lspCol = fnPos + 3;
						}
					}

					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspCol)));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(lspCol + word.length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(location, "range", range);
					json_array_append_new(locations, location);
				}
			}
		}
	}

	json_object_set_new(response, "result", locations);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleRename(
		const std::string& id, const std::string& uri, size_t line, size_t character, const std::string& newName) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	} else {
		// Try to read from disk
		if (uri.substr(0, 7) == "file://") {
			std::string filePath = uri.substr(7);
			std::ifstream file(filePath);
			if (file.good()) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				documentText = buffer.str();
			}
		}
	}

	json_t* workspaceEdit = json_object();
	json_t* changes = json_object();

	if (!documentText.empty()) {
		std::string word = getWordAtPosition(documentText, line, character);

		if (!word.empty()) {
			// Parse the document
			Qd::Ast ast;
			Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

			if (root && !ast.hasErrors()) {
				// Find all references to this identifier
				std::vector<Qd::IAstNode*> references;
				findIdentifiersInNode(root, word, references);

				json_t* edits = json_array();

				for (Qd::IAstNode* ref : references) {
					json_t* edit = json_object();

					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (ref->line() > 0) ? ref->line() - 1 : 0;
					size_t lspCol = (ref->column() > 0) ? ref->column() - 1 : 0;

					// For function declarations, find the actual column of the function name
					if (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
						std::istringstream lineStream(documentText);
						std::string lineText;
						for (size_t i = 0; i <= lspLine; i++) {
							if (!std::getline(lineStream, lineText)) {
								lineText.clear();
								break;
							}
						}
						size_t fnPos = lineText.find("fn ");
						if (fnPos != std::string::npos) {
							lspCol = fnPos + 3;
						}
					}
					// For local variable declarations
					else if (ref->type() == Qd::IAstNode::Type::LOCAL) {
						std::istringstream lineStream(documentText);
						std::string lineText;
						for (size_t i = 0; i <= lspLine; i++) {
							if (!std::getline(lineStream, lineText)) {
								lineText.clear();
								break;
							}
						}
						size_t arrowPos = lineText.find("-> ");
						if (arrowPos != std::string::npos) {
							// Find the variable name position after the arrow
							size_t varPos = lineText.find(word, arrowPos + 3);
							if (varPos != std::string::npos) {
								lspCol = varPos;
							}
						}
					}

					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspCol)));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(lspCol + word.length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(edit, "range", range);
					json_object_set_new(edit, "newText", json_string(newName.c_str()));

					json_array_append_new(edits, edit);
				}

				json_object_set_new(changes, uri.c_str(), edits);
			}
		}
	}

	json_object_set_new(workspaceEdit, "changes", changes);
	json_object_set_new(response, "result", workspaceEdit);

	sendMessage(response);
	json_decref(response);
}

// Navigation handlers for QuadrateLSP
// Split from main.cc for maintainability

#include "lsp_impl.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_struct_construction.h>
#include <quadrate/qc/ast_node_struct_declaration.h>
#include <quadrate/qc/ast_node_struct_field.h>
#include <quadrate/qc/ast_node_use.h>
#include <sstream>

static bool isIdentifierChar(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static size_t offsetOfPosition(const std::string& text, size_t line, size_t column) {
	size_t lineStart = 0;
	for (size_t i = 0; i < line; i++) {
		size_t newline = text.find('\n', lineStart);
		if (newline == std::string::npos) {
			return text.size();
		}
		lineStart = newline + 1;
	}
	return std::min(lineStart + column, text.size());
}

static void positionOfOffset(const std::string& text, size_t offset, size_t& line, size_t& column) {
	line = 0;
	size_t lineStart = 0;
	for (size_t i = 0; i < offset && i < text.size(); i++) {
		if (text[i] == '\n') {
			line++;
			lineStart = i + 1;
		}
	}
	column = offset - lineStart;
}

static size_t lineEndOfOffset(const std::string& text, size_t offset) {
	size_t end = text.find('\n', offset);
	return end == std::string::npos ? text.size() : end;
}

static bool nameAtOffset(const std::string& text, size_t offset, const std::string& name) {
	if (name.empty() || offset + name.size() > text.size() || text.compare(offset, name.size(), name) != 0) {
		return false;
	}
	bool startOk = offset == 0 || !isIdentifierChar(text[offset - 1]);
	bool endOk = offset + name.size() >= text.size() || !isIdentifierChar(text[offset + name.size()]);
	return startOk && endOk;
}

static size_t findNameForward(const std::string& text, const std::string& name, size_t from, size_t limit) {
	size_t pos = text.find(name, from);
	while (pos != std::string::npos && pos + name.size() <= limit) {
		if (nameAtOffset(text, pos, name)) {
			return pos;
		}
		pos = text.find(name, pos + 1);
	}
	return std::string::npos;
}

static size_t findNameBeforeColon(const std::string& text, size_t typeOffset, const std::string& name) {
	size_t pos = typeOffset;
	while (pos > 0 && (text[pos - 1] == ' ' || text[pos - 1] == '\t')) {
		pos--;
	}
	if (pos > 0 && text[pos - 1] == ':') {
		pos--;
	}
	while (pos > 0 && (text[pos - 1] == ' ' || text[pos - 1] == '\t')) {
		pos--;
	}
	if (pos >= name.size() && nameAtOffset(text, pos - name.size(), name)) {
		return pos - name.size();
	}
	return std::string::npos;
}

static size_t findNameOnLine(const std::string& text, size_t offset, const std::string& name) {
	if (nameAtOffset(text, offset, name)) {
		return offset;
	}
	size_t lineStart = text.rfind('\n', offset == 0 ? 0 : offset - 1);
	lineStart = (lineStart == std::string::npos || offset == 0) ? 0 : lineStart + 1;
	size_t lineEnd = lineEndOfOffset(text, offset);
	size_t found = findNameForward(text, name, offset, lineEnd);
	if (found == std::string::npos) {
		found = findNameForward(text, name, lineStart, lineEnd);
	}
	return found;
}

// Byte offsets of every place `node` spells `name`, from the node's recorded
// position. Declarations of parameters and struct fields are recorded at the
// type, so their names are found just before the colon.
static std::vector<std::pair<size_t, bool>> nodeNameOffsets(
		const std::string& text, Qd::IAstNode* node, const std::string& name) {
	std::vector<std::pair<size_t, bool>> offsets;
	if (!node || node->line() == 0) {
		return offsets;
	}
	size_t base = offsetOfPosition(text, node->line() - 1, node->column() > 0 ? node->column() - 1 : 0);
	auto add = [&](size_t offset, bool declaration) {
		if (offset != std::string::npos) {
			offsets.push_back({offset, declaration});
		}
	};

	switch (node->type()) {
	case Qd::IAstNode::Type::FUNCTION_DECLARATION:
	case Qd::IAstNode::Type::STRUCT_DECLARATION:
	case Qd::IAstNode::Type::ENUM_DECLARATION:
	case Qd::IAstNode::Type::CONSTANT_DECLARATION:
		add(findNameOnLine(text, base, name), true);
		break;
	case Qd::IAstNode::Type::LOCAL:
		add(findNameForward(text, name, base, lineEndOfOffset(text, base)), true);
		break;
	case Qd::IAstNode::Type::VARIABLE_DECLARATION: {
		Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(node);
		if (param->name() == name) {
			add(findNameBeforeColon(text, base, name), true);
		}
		if (param->typeString() == name) {
			add(findNameOnLine(text, base, name), false);
		}
		break;
	}
	case Qd::IAstNode::Type::STRUCT_FIELD: {
		Qd::AstNodeStructField* field = static_cast<Qd::AstNodeStructField*>(node);
		if (field->name() == name) {
			add(findNameBeforeColon(text, base, name), true);
		}
		if (field->typeName() == name) {
			add(findNameOnLine(text, base, name), false);
		}
		break;
	}
	case Qd::IAstNode::Type::STRUCT_CONSTRUCTION: {
		Qd::AstNodeStructConstruction* construction = static_cast<Qd::AstNodeStructConstruction*>(node);
		if (construction->structName() == name) {
			add(findNameOnLine(text, base, name), false);
		}
		size_t searchFrom = base + construction->structName().size();
		for (const auto& fieldInit : construction->fieldInits()) {
			if (fieldInit.fieldName != name) {
				continue;
			}
			size_t pos = findNameForward(text, name, searchFrom, text.size());
			while (pos != std::string::npos) {
				size_t after = pos + name.size();
				while (after < text.size() && (text[after] == ' ' || text[after] == '\t')) {
					after++;
				}
				if (after < text.size() && (text[after] == '=' || text[after] == ':')) {
					break;
				}
				pos = findNameForward(text, name, pos + 1, text.size());
			}
			add(pos, false);
			if (pos != std::string::npos) {
				searchFrom = pos + name.size();
			}
		}
		break;
	}
	case Qd::IAstNode::Type::SCOPED_IDENTIFIER: {
		if (name.find("::") != std::string::npos) {
			add(findNameOnLine(text, base, name), false);
		} else {
			size_t separator = text.find("::", base);
			if (separator != std::string::npos && separator < lineEndOfOffset(text, base) &&
					nameAtOffset(text, separator + 2, name)) {
				add(separator + 2, false);
			}
		}
		break;
	}
	default:
		add(findNameOnLine(text, base, name), false);
		break;
	}
	return offsets;
}

std::vector<NameOccurrence> QuadrateLSP::nameOccurrences(
		const std::string& text, const std::vector<Qd::IAstNode*>& nodes, const std::string& name) {
	std::map<size_t, bool> byOffset;
	for (Qd::IAstNode* node : nodes) {
		for (const auto& [offset, declaration] : nodeNameOffsets(text, node, name)) {
			byOffset[offset] = byOffset[offset] || declaration;
		}
	}
	std::vector<NameOccurrence> occurrences;
	for (const auto& [offset, declaration] : byOffset) {
		NameOccurrence occurrence;
		positionOfOffset(text, offset, occurrence.line, occurrence.column);
		occurrence.length = name.size();
		occurrence.declaration = declaration;
		occurrences.push_back(occurrence);
	}
	return occurrences;
}

std::vector<NameOccurrence> QuadrateLSP::findNameOccurrences(const std::string& text, Qd::IAstNode* root,
		const std::string& word, size_t line, size_t character, bool* isLocal) {
	Qd::AstNodeFunctionDeclaration* containingFunc = findContainingFunction(root, line + 1, character + 1);
	bool local = containingFunc && isLocalVariableOrParameter(containingFunc, word);
	if (isLocal) {
		*isLocal = local;
	}
	std::vector<Qd::IAstNode*> references;
	findIdentifiersInNode(local ? containingFunc : root, word, references, local);
	return nameOccurrences(text, references, word);
}

static json_t* makeLocationAt(const std::string& uri, size_t line, size_t column, size_t length) {
	json_t* start = json_object();
	json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(line)));
	json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(column)));
	json_t* end = json_object();
	json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(line)));
	json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(column + length)));
	json_t* range = json_object();
	json_object_set_new(range, "start", start);
	json_object_set_new(range, "end", end);
	json_t* location = json_object();
	json_object_set_new(location, "uri", json_string(uri.c_str()));
	json_object_set_new(location, "range", range);
	return location;
}

json_t* QuadrateLSP::makeNameLocation(
		const std::string& uri, const std::string& text, Qd::IAstNode* node, const std::string& name) {
	size_t line = (node && node->line() > 0) ? node->line() - 1 : 0;
	size_t column = 0;
	for (const auto& [offset, declaration] : nodeNameOffsets(text, node, name)) {
		if (declaration) {
			positionOfOffset(text, offset, line, column);
			break;
		}
	}
	return makeLocationAt(uri, line, column, name.size());
}

json_t* QuadrateLSP::makeNameLocationAt(
		const std::string& uri, const std::string& text, size_t line, size_t column, const std::string& name) {
	size_t lspLine = line > 0 ? line - 1 : 0;
	size_t lspColumn = 0;
	size_t found = findNameOnLine(text, offsetOfPosition(text, lspLine, column > 0 ? column - 1 : 0), name);
	if (found != std::string::npos) {
		positionOfOffset(text, found, lspLine, lspColumn);
	}
	return makeLocationAt(uri, lspLine, lspColumn, name.size());
}

void QuadrateLSP::findIdentifiersInNode(Qd::IAstNode* node, const std::string& targetName,
		std::vector<Qd::IAstNode*>& results, bool includeParameters) {
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
	// Check if this node is a struct declaration
	else if (node->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
		Qd::AstNodeStructDeclaration* structDecl = static_cast<Qd::AstNodeStructDeclaration*>(node);
		if (structDecl->name() == targetName) {
			results.push_back(node);
		}
	}
	// Check if this node is a struct field
	else if (node->type() == Qd::IAstNode::Type::STRUCT_FIELD) {
		Qd::AstNodeStructField* field = static_cast<Qd::AstNodeStructField*>(node);
		if (field->name() == targetName) {
			results.push_back(node);
		}
		// Also check if the field's type matches (for struct type renaming)
		if (field->typeName() == targetName) {
			results.push_back(node);
		}
	}
	// Check if this node is a struct construction (e.g., Point { x = 1 })
	else if (node->type() == Qd::IAstNode::Type::STRUCT_CONSTRUCTION) {
		Qd::AstNodeStructConstruction* construction = static_cast<Qd::AstNodeStructConstruction*>(node);
		if (construction->structName() == targetName) {
			results.push_back(node);
		}
		// Also check field initializers for field name rename
		for (const auto& fieldInit : construction->fieldInits()) {
			if (fieldInit.fieldName == targetName) {
				results.push_back(node); // We'll handle field init positions specially
			}
		}
	}
	// Check if this node is a field access (e.g., p <<x)
	else if (node->type() == Qd::IAstNode::Type::FIELD_ACCESS) {
		Qd::AstNodeFieldAccess* access = static_cast<Qd::AstNodeFieldAccess*>(node);
		if (access->fieldName() == targetName) {
			results.push_back(node);
		}
	}
	// Check if this node is a parameter/variable declaration with matching type
	else if (node->type() == Qd::IAstNode::Type::VARIABLE_DECLARATION) {
		Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(node);
		if (param->typeString() == targetName || (includeParameters && param->name() == targetName)) {
			results.push_back(node);
		}
	}

	// Recursively search children
	for (size_t i = 0; i < node->childCount(); i++) {
		findIdentifiersInNode(node->child(i), targetName, results, includeParameters);
	}
}

// Find the function declaration that contains the given line/column position
Qd::AstNodeFunctionDeclaration* QuadrateLSP::findContainingFunction(Qd::IAstNode* root, size_t line, size_t column) {
	Qd::AstNodeFunctionDeclaration* best = nullptr;
	std::function<void(Qd::IAstNode*)> search = [&](Qd::IAstNode* node) {
		if (!node) {
			return;
		}
		if (node->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			Qd::AstNodeFunctionDeclaration* funcDecl = static_cast<Qd::AstNodeFunctionDeclaration*>(node);
			bool startsBefore = funcDecl->line() < line || (funcDecl->line() == line && funcDecl->column() <= column);
			bool later = !best || funcDecl->line() > best->line() ||
						 (funcDecl->line() == best->line() && funcDecl->column() > best->column());
			if (funcDecl->body() && startsBefore && later) {
				best = funcDecl;
			}
		}
		for (size_t i = 0; i < node->childCount(); i++) {
			search(node->child(i));
		}
	};
	search(root);
	return best;
}

// Check if a name is a local variable or parameter within a function
bool QuadrateLSP::isLocalVariableOrParameter(Qd::IAstNode* funcNode, const std::string& name) {
	if (!funcNode || funcNode->type() != Qd::IAstNode::Type::FUNCTION_DECLARATION) {
		return false;
	}

	Qd::AstNodeFunctionDeclaration* func = static_cast<Qd::AstNodeFunctionDeclaration*>(funcNode);

	// Check input parameters
	for (const auto& param : func->inputParameters()) {
		if (param->type() == Qd::IAstNode::Type::VARIABLE_DECLARATION) {
			const Qd::AstNodeParameter* p = static_cast<const Qd::AstNodeParameter*>(param.get());
			if (p->name() == name) {
				return true;
			}
		}
	}

	// Check output parameters
	for (const auto& param : func->outputParameters()) {
		if (param->type() == Qd::IAstNode::Type::VARIABLE_DECLARATION) {
			const Qd::AstNodeParameter* p = static_cast<const Qd::AstNodeParameter*>(param.get());
			if (p->name() == name) {
				return true;
			}
		}
	}

	// Check for local variable declarations (-> varname) within the function body
	std::function<bool(Qd::IAstNode*)> searchLocals = [&](Qd::IAstNode* node) -> bool {
		if (!node) {
			return false;
		}

		if (node->type() == Qd::IAstNode::Type::LOCAL) {
			Qd::AstNodeLocal* local = static_cast<Qd::AstNodeLocal*>(node);
			for (const std::string& localName : local->names()) {
				if (localName == name) {
					return true;
				}
			}
		}

		for (size_t i = 0; i < node->childCount(); i++) {
			if (searchLocals(node->child(i))) {
				return true;
			}
		}
		return false;
	};

	return searchLocals(func->body());
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
	// Collect all .qd files to search
	std::vector<std::string> filesToSearch;
	filesToSearch.push_back(modulePath);

	// If modulePath is a file, also search sibling files in the same directory
	try {
		std::filesystem::path p(modulePath);
		if (std::filesystem::is_regular_file(p)) {
			std::filesystem::path dir = p.parent_path();
			for (const auto& entry : std::filesystem::directory_iterator(dir)) {
				if (entry.is_regular_file()) {
					std::string filename = entry.path().filename().string();
					if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".qd") {
						if (entry.path().string() != modulePath) {
							// Exclude test files
							if (filename.size() <= 8 || filename.substr(filename.size() - 8) != "_test.qd") {
								filesToSearch.push_back(entry.path().string());
							}
						}
					}
				}
			}
		}
	} catch (...) {
		// Ignore filesystem errors
	}

	// Search all files for the symbol
	for (const auto& searchPath : filesToSearch) {
		std::ifstream file(searchPath);
		if (!file.good()) {
			continue;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string moduleText = buffer.str();

		// Parse the module file
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

		if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
			continue;
		}

		// Search for the definition in this file
		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* child = root->child(i);

			if (symbolType == "function" && child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
				if (funcNode->name() == symbolName) {
					// Found the function definition
					json_t* location =
							makeNameLocation(lspPathToUri(searchPath), moduleText, funcNode, funcNode->name());
					return location;
				}
			} else if (symbolType == "constant" && child && child->type() == Qd::IAstNode::Type::CONSTANT_DECLARATION) {
				Qd::AstNodeConstant* constNode = static_cast<Qd::AstNodeConstant*>(child);
				if (constNode->name() == symbolName) {
					// Found the constant definition
					json_t* location =
							makeNameLocation(lspPathToUri(searchPath), moduleText, constNode, constNode->name());
					return location;
				}
			} else if (symbolType == "struct" && child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
				Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);
				if (structNode->name() == symbolName) {
					// Found the struct definition
					json_t* location =
							makeNameLocation(lspPathToUri(searchPath), moduleText, structNode, structNode->name());
					return location;
				}
			} else if (symbolType == "constant" && child && child->type() == Qd::IAstNode::Type::ENUM_DECLARATION) {
				Qd::AstNodeEnumDeclaration* enumNode = static_cast<Qd::AstNodeEnumDeclaration*>(child);
				// Check if symbolName is EnumName::Variant or just EnumName
				std::string enumPrefix = enumNode->name() + "::";
				if (symbolName == enumNode->name() || symbolName.substr(0, enumPrefix.size()) == enumPrefix) {
					json_t* location =
							makeNameLocation(lspPathToUri(searchPath), moduleText, enumNode, enumNode->name());
					return location;
				}
			} else if (symbolType == "function" && child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
				// Check for imported functions (like those in stdlib modules)
				Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
				const auto& importedFuncs = importNode->functions();
				for (const auto& importedFunc : importedFuncs) {
					if (importedFunc->name == symbolName) {
						// Found the imported function declaration
						json_t* location = makeNameLocationAt(lspPathToUri(searchPath), moduleText, importedFunc->line,
								importedFunc->column, importedFunc->name);
						return location;
					}
				}
			}
		}
	} // End of filesToSearch loop

	return json_null();
}

json_t* QuadrateLSP::findMethodInModule(const std::string& modulePath, const std::string& methodName) {
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

	// Search for method definitions (functions with a receiver)
	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);

		if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);

			// Check if this is a method (has a receiver) with the matching name
			if (funcNode->hasReceiver() && funcNode->name() == methodName) {
				// Found the method definition
				json_t* location = makeNameLocation(lspPathToUri(modulePath), moduleText, funcNode, funcNode->name());
				return location;
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

	// First, check if the variable is a function parameter
	for (const auto& paramNode : functionNode->inputParameters()) {
		if (paramNode) {
			Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(paramNode.get());
			if (param->name() == varName) {
				std::string paramType = param->typeString();
				if (!paramType.empty()) {
					return paramType;
				}
			}
		}
	}

	// Also check method receiver if this is a method
	if (functionNode->hasReceiver() && functionNode->receiverName() == varName) {
		std::string receiverType = functionNode->receiverType();
		if (!receiverType.empty()) {
			return receiverType;
		}
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
								// Check if it's a struct construction (e.g., math::Vec3{x: 1.0, y: 2.0})
								if (prevSibling->type() == Qd::IAstNode::Type::STRUCT_CONSTRUCTION) {
									Qd::AstNodeStructConstruction* constNode =
											static_cast<Qd::AstNodeStructConstruction*>(prevSibling);
									structType = constNode->structName();
									return;
								}
								// Check if it's a scoped identifier (module::Struct)
								else if (prevSibling->type() == Qd::IAstNode::Type::SCOPED_IDENTIFIER) {
									Qd::AstNodeScopedIdentifier* scopedNode =
											static_cast<Qd::AstNodeScopedIdentifier*>(prevSibling);
									structType = scopedNode->scope() + "::" + scopedNode->name();
									return;
								}
								// Check if it's a plain identifier (Struct)
								else if (prevSibling->type() == Qd::IAstNode::Type::IDENTIFIER) {
									Qd::AstNodeIdentifier* identNode = static_cast<Qd::AstNodeIdentifier*>(prevSibling);
									structType = identNode->name();
									return;
								}
							}
						}
						// Continue searching other declarations
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

// The variable a `<<field`/`>>field` operates on, when the node just before it in its block
// is an identifier. The field node itself carries only the field name.
static std::string precedingIdentifierName(Qd::IAstNode* node) {
	Qd::IAstNode* parent = node ? node->parent() : nullptr;
	if (!parent) {
		return "";
	}
	for (size_t i = 1; i < parent->childCount(); i++) {
		if (parent->child(i) == node) {
			Qd::IAstNode* prev = parent->child(i - 1);
			if (prev && prev->type() == Qd::IAstNode::Type::IDENTIFIER) {
				return static_cast<Qd::AstNodeIdentifier*>(prev)->name();
			}
			return "";
		}
	}
	return "";
}

json_t* QuadrateLSP::handleFieldAccessDefinition(
		Qd::IAstNode* root, const std::string& uri, size_t line, bool cursorOnVariable) {
	// Find the field access or field set node at the target line
	// Both <<field (FIELD_ACCESS) and >>field (FIELD_SET) should navigate to field definition
	std::string foundVarName;
	std::string foundFieldName;
	Qd::IAstNode* foundNode = nullptr;

	std::function<void(Qd::IAstNode*)> searchFieldNode = [&](Qd::IAstNode* node) {
		if (!node || foundNode) {
			return;
		}

		if (node->type() == Qd::IAstNode::Type::FIELD_ACCESS) {
			Qd::AstNodeFieldAccess* faNode = static_cast<Qd::AstNodeFieldAccess*>(node);
			size_t nodeLine = (faNode->line() > 0) ? faNode->line() - 1 : 0;
			if (nodeLine == line) {
				foundVarName = precedingIdentifierName(node);
				foundFieldName = faNode->fieldName();
				foundNode = node;
				return;
			}
		} else if (node->type() == Qd::IAstNode::Type::FIELD_SET) {
			Qd::AstNodeFieldSet* fsNode = static_cast<Qd::AstNodeFieldSet*>(node);
			size_t nodeLine = (fsNode->line() > 0) ? fsNode->line() - 1 : 0;
			if (nodeLine == line) {
				foundVarName = precedingIdentifierName(node);
				foundFieldName = fsNode->fieldName();
				foundNode = node;
				return;
			}
		}

		// Recursively search children
		for (size_t i = 0; i < node->childCount(); i++) {
			searchFieldNode(node->child(i));
			if (foundNode) {
				return;
			}
		}
	};

	searchFieldNode(root);

	if (!foundNode) {
		return json_null();
	}

	if (cursorOnVariable) {
		// User clicked on the variable name - find the local variable declaration
		Qd::AstNodeLocal* localNode = findLocalDeclaration(foundNode, foundVarName, line);

		if (localNode) {
			// Found the local variable declaration
			json_t* location = makeNameLocation(uri, getDocumentText(uri), localNode, foundVarName);
			return location;
		}
	} else {
		// User clicked on the field name - find the struct field definition
		// Find the struct type of this variable
		std::string structType = findStructTypeOfVariable(root, foundVarName, line);
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
			std::string filePath = lspUriToPath(uri);
			if (!filePath.empty()) {
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
									for (const auto& field : fields) {
										if (field->name() == foundFieldName) {
											// Found the field!
											json_t* location = makeNameLocation(lspPathToUri(modulePath), moduleContent,
													field.get(), field->name());
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
			// This is a local struct - search in current file first, then same directory
			// (directory-based namespace)

			// Helper lambda to search for struct field in an AST
			auto searchStructField = [&](Qd::IAstNode* searchRoot, const std::string& targetUri) -> json_t* {
				for (size_t i = 0; i < searchRoot->childCount(); i++) {
					Qd::IAstNode* child = searchRoot->child(i);
					if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
						Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);
						if (structNode->name() == structType) {
							// Found the struct - now find the field
							const auto& fields = structNode->fields();
							for (const auto& field : fields) {
								if (field->name() == foundFieldName) {
									// Found the field!
									json_t* location = makeNameLocation(
											targetUri, getDocumentText(targetUri), field.get(), field->name());
									return location;
								}
							}
						}
					}
				}
				return nullptr;
			};

			// First, search in current file
			json_t* result = searchStructField(root, uri);
			if (result) {
				return result;
			}

			// If not found, search other .qd files in the same directory
			// (directory-based namespace)
			std::string sourceDir;
			std::string filePath = lspUriToPath(uri);
			if (!filePath.empty()) {
				size_t lastSlash = filePath.find_last_of('/');
				if (lastSlash != std::string::npos) {
					sourceDir = filePath.substr(0, lastSlash);
				}
			}

			if (!sourceDir.empty()) {
				try {
					for (const auto& entry : std::filesystem::directory_iterator(sourceDir)) {
						if (entry.path().extension() == ".qd" &&
								entry.path().string() != lspUriToPath(uri)) { // Skip current file
							// Parse the file
							std::ifstream file(entry.path());
							if (file.good()) {
								std::stringstream buffer;
								buffer << file.rdbuf();
								std::string fileContent = buffer.str();

								Qd::Ast fileAst;
								Qd::IAstNode* fileRoot = fileAst.generate(fileContent.c_str(), false, nullptr);

								if (fileRoot && !fileAst.hasErrors()) {
									std::string fileUri = lspPathToUri(entry.path().string());
									result = searchStructField(fileRoot, fileUri);
									if (result) {
										return result;
									}
								}
							}
						}
					}
				} catch (...) {
					// Ignore filesystem errors
				}
			}
		}
	}

	return json_null();
}

void QuadrateLSP::handleDefinition(const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", makeResponseId(id));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	} else {
		// Try to read from disk
		std::string filePath = lspUriToPath(uri);
		if (!filePath.empty()) {
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
					std::string sourceDir = std::filesystem::path(lspUriToPath(uri)).parent_path().string();
					std::string modulePath = resolveModulePath(moduleName, sourceDir);

					if (!modulePath.empty()) {
						json_t* location = json_object();
						json_object_set_new(location, "uri", json_string(lspPathToUri(modulePath).c_str()));

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

			// Note: We proceed even with AST errors for some lookups (like imported module methods)
			// because imports may still be parseable even if other parts have errors
			if (root && root->type() == Qd::IAstNode::Type::PROGRAM) {
				// Check if we're in a field access expression (v <<x)
				// Find the character at the cursor position to see if @ is nearby
				std::vector<std::string> lines;
				std::istringstream stream(documentText);
				std::string currentLine;
				while (std::getline(stream, currentLine)) {
					lines.push_back(currentLine);
				}

				if (line < lines.size()) {
					const std::string& targetLine = lines[line];
					// Look for @ or . (field access/set) before or after the cursor
					bool inFieldAccess = false;
					bool cursorOnVariable = false;

					// Search for <<field (read) or >>field (write) near the cursor
					for (size_t i = 0; i + 1 < targetLine.length(); i++) {
						if ((targetLine[i] == '<' && targetLine[i + 1] == '<') ||
								(targetLine[i] == '>' && targetLine[i + 1] == '>')) {
							// Check if followed by an identifier (field name)
							if (i + 2 < targetLine.length() &&
									(std::isalpha(targetLine[i + 2]) || targetLine[i + 2] == '_')) {
								// Check if cursor is near this field operator
								if (i >= character) {
									if (i - character <= word.length()) {
										inFieldAccess = true;
										cursorOnVariable = true;
										break;
									}
								} else {
									if (character - i <= word.length() + 2) {
										inFieldAccess = true;
										cursorOnVariable = false;
										break;
									}
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
						Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);

						if (funcNode->name() == word) {
							// Found the definition
							json_t* location = makeNameLocation(uri, documentText, funcNode, funcNode->name());
							result = location;
							break;
						}
					} else if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
						Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);

						if (structNode->name() == word) {
							// Found the struct definition
							json_t* location = makeNameLocation(uri, documentText, structNode, structNode->name());
							result = location;
							break;
						}
					} else if (child && child->type() == Qd::IAstNode::Type::ENUM_DECLARATION) {
						Qd::AstNodeEnumDeclaration* enumNode = static_cast<Qd::AstNodeEnumDeclaration*>(child);
						if (enumNode->name() == word) {
							json_t* location = makeNameLocation(uri, documentText, enumNode, enumNode->name());
							result = location;
							break;
						}
					} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
						// Check imported functions
						Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
						std::string namespaceName = importNode->namespaceName();

						const auto& importedFuncs = importNode->functions();
						for (const auto& importedFunc : importedFuncs) {
							std::string fullName = namespaceName + "::" + importedFunc->name;
							if (fullName == word || importedFunc->name == word) {
								// Found the imported function declaration
								json_t* location = makeNameLocationAt(uri, documentText, importedFunc->line,
										importedFunc->column, importedFunc->name);
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
							json_t* location = makeNameLocation(uri, documentText, localDecl, word);
							result = location;
						} else if (Qd::AstNodeFunctionDeclaration* func =
										   findContainingFunction(root, line + 1, character + 1)) {
							for (const auto* params : {&func->inputParameters(), &func->outputParameters()}) {
								for (const auto& param : *params) {
									if (json_is_null(result) &&
											param->type() == Qd::IAstNode::Type::VARIABLE_DECLARATION &&
											static_cast<Qd::AstNodeParameter*>(param.get())->name() == word) {
										result = makeNameLocation(uri, documentText, param.get(), word);
									}
								}
							}
						}
					}
				}

				// If still not found, search imported modules for methods
				if (json_is_null(result) && word.find("::") == std::string::npos) {
					// Collect imported modules from USE_STATEMENT nodes (use <module>)
					std::vector<std::string> importedModules;
					for (size_t i = 0; i < root->childCount(); i++) {
						Qd::IAstNode* child = root->child(i);
						if (child && child->type() == Qd::IAstNode::Type::USE_STATEMENT) {
							Qd::AstNodeUse* useNode = static_cast<Qd::AstNodeUse*>(child);
							importedModules.push_back(useNode->module());
						}
					}

					// Get source directory from URI
					std::string filePath = lspUriToPath(uri);
					std::string sourceDir = std::filesystem::path(filePath).parent_path().string();

					// Search each imported module for a method with this name
					for (const auto& moduleName : importedModules) {
						std::string modulePath = resolveModulePath(moduleName, sourceDir);
						if (!modulePath.empty()) {
							// Look for method definitions in the module
							result = findMethodInModule(modulePath, word);
							if (!json_is_null(result)) {
								break;
							}
						}
					}

					// If still not found, search for struct definitions in imported modules
					if (json_is_null(result)) {
						for (const auto& moduleName : importedModules) {
							std::string modulePath = resolveModulePath(moduleName, sourceDir);
							if (!modulePath.empty()) {
								result = findDefinitionInModule(modulePath, word, "struct");
								if (!json_is_null(result)) {
									break;
								}
							}
						}
					}

					// If still not found, search sibling files for struct definitions
					if (json_is_null(result)) {
						std::vector<std::string> siblings = getSiblingQdFiles(filePath);
						for (const auto& siblingPath : siblings) {
							result = findDefinitionInModule(siblingPath, word, "struct");
							if (!json_is_null(result)) {
								break;
							}
							// Also search for functions in siblings
							result = findDefinitionInModule(siblingPath, word, "function");
							if (!json_is_null(result)) {
								break;
							}
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
					std::string filePath = lspUriToPath(uri);
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
	json_object_set_new(response, "id", makeResponseId(id));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	} else {
		std::string filePath = lspUriToPath(uri);
		if (!filePath.empty()) {
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
				} else if (node->type() == Qd::IAstNode::Type::FOR_STATEMENT ||
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

void QuadrateLSP::handleDocumentHighlight(
		const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", makeResponseId(id));

	std::string documentText = getDocumentText(uri);
	json_t* highlights = json_array();

	std::string word = documentText.empty() ? "" : getWordAtPosition(documentText, line, character);
	if (!word.empty()) {
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

		if (root && !ast.hasErrors()) {
			for (const NameOccurrence& occurrence : findNameOccurrences(documentText, root, word, line, character)) {
				json_t* location = makeLocationAt(uri, occurrence.line, occurrence.column, occurrence.length);
				json_t* highlight = json_object();
				json_object_set(highlight, "range", json_object_get(location, "range"));
				// DocumentHighlightKind: 1 = Text, 2 = Read, 3 = Write
				json_object_set_new(highlight, "kind", json_integer(occurrence.declaration ? 3 : 2));
				json_decref(location);
				json_array_append_new(highlights, highlight);
			}
		}
	}

	json_object_set_new(response, "result", highlights);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleReferences(
		const std::string& id, const std::string& uri, size_t line, size_t character, bool includeDeclaration) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", makeResponseId(id));

	std::string documentText = getDocumentText(uri);
	json_t* locations = json_array();

	std::string word = documentText.empty() ? "" : getWordAtPosition(documentText, line, character);
	if (!word.empty()) {
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

		if (root && !ast.hasErrors()) {
			for (const NameOccurrence& occurrence : findNameOccurrences(documentText, root, word, line, character)) {
				if (occurrence.declaration && !includeDeclaration) {
					continue;
				}
				json_array_append_new(
						locations, makeLocationAt(uri, occurrence.line, occurrence.column, occurrence.length));
			}
		}
	}

	json_object_set_new(response, "result", locations);
	sendMessage(response);
	json_decref(response);
}

static bool isValidIdentifier(const std::string& name) {
	if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) {
		return false;
	}
	return std::all_of(name.begin(), name.end(), isIdentifierChar);
}

static bool occurrenceContains(const NameOccurrence& occurrence, size_t line, size_t character) {
	return occurrence.line == line && occurrence.column <= character &&
		   character <= occurrence.column + occurrence.length;
}

bool QuadrateLSP::findRenameTarget(const std::string& documentText, size_t line, size_t character, std::string& word,
		std::vector<NameOccurrence>& occurrences, bool& isLocal, std::string& error) {
	word = documentText.empty() ? "" : getWordAtPosition(documentText, line, character);
	if (word.empty()) {
		error = "No symbol to rename at this position";
		return false;
	}
	if (word.find("::") != std::string::npos) {
		error = "Cannot rename a module-qualified name";
		return false;
	}
	if (lspIsReservedName(word)) {
		error = "Cannot rename built-in '" + word + "'";
		return false;
	}

	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);
	if (!root || ast.hasErrors()) {
		error = "Cannot rename while the document has errors";
		return false;
	}

	occurrences = findNameOccurrences(documentText, root, word, line, character, &isLocal);
	bool atCursor = std::any_of(occurrences.begin(), occurrences.end(),
			[&](const NameOccurrence& occurrence) { return occurrenceContains(occurrence, line, character); });
	if (!atCursor) {
		error = "No renameable symbol at this position";
		return false;
	}
	return true;
}

void QuadrateLSP::handlePrepareRename(const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", makeResponseId(id));

	std::string documentText = getDocumentText(uri);
	std::string word;
	std::vector<NameOccurrence> occurrences;
	bool isLocal = false;
	std::string error;
	json_t* result = json_null();

	if (findRenameTarget(documentText, line, character, word, occurrences, isLocal, error)) {
		for (const NameOccurrence& occurrence : occurrences) {
			if (occurrenceContains(occurrence, line, character)) {
				json_t* location = makeLocationAt(uri, occurrence.line, occurrence.column, occurrence.length);
				json_decref(result);
				result = json_object();
				json_object_set(result, "range", json_object_get(location, "range"));
				json_object_set_new(result, "placeholder", json_string(word.c_str()));
				json_decref(location);
				break;
			}
		}
	}

	json_object_set_new(response, "result", result);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleRename(
		const std::string& id, const std::string& uri, size_t line, size_t character, const std::string& newName) {
	if (!isValidIdentifier(newName)) {
		sendError(id, -32602, "'" + newName + "' is not a valid identifier");
		return;
	}
	if (lspIsReservedName(newName)) {
		sendError(id, -32602, "'" + newName + "' is a reserved name");
		return;
	}

	std::string documentText = getDocumentText(uri);
	std::string word;
	std::vector<NameOccurrence> occurrences;
	bool isLocal = false;
	std::string error;
	if (!findRenameTarget(documentText, line, character, word, occurrences, isLocal, error)) {
		sendError(id, -32803, error);
		return;
	}

	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", makeResponseId(id));

	json_t* changes = json_object();
	auto addEdits = [&](const std::string& editUri, const std::vector<NameOccurrence>& found) {
		if (found.empty()) {
			return;
		}
		json_t* edits = json_array();
		for (const NameOccurrence& occurrence : found) {
			json_t* location = makeLocationAt(editUri, occurrence.line, occurrence.column, occurrence.length);
			json_t* edit = json_object();
			json_object_set(edit, "range", json_object_get(location, "range"));
			json_object_set_new(edit, "newText", json_string(newName.c_str()));
			json_decref(location);
			json_array_append_new(edits, edit);
		}
		json_object_set_new(changes, editUri.c_str(), edits);
	};

	addEdits(uri, occurrences);

	std::string filePath = lspUriToPath(uri);
	if (!isLocal && !filePath.empty()) {
		for (const auto& siblingPath : getSiblingQdFiles(filePath)) {
			std::string siblingUri = lspPathToUri(siblingPath);
			std::string siblingText = getDocumentText(siblingUri);
			if (siblingText.empty()) {
				continue;
			}

			Qd::Ast siblingAst;
			Qd::IAstNode* siblingRoot = siblingAst.generate(siblingText.c_str(), false, nullptr);
			if (!siblingRoot || siblingAst.hasErrors()) {
				continue;
			}

			std::vector<Qd::IAstNode*> siblingRefs;
			findIdentifiersInNode(siblingRoot, word, siblingRefs);
			addEdits(siblingUri, nameOccurrences(siblingText, siblingRefs, word));
		}
	}

	json_t* workspaceEdit = json_object();
	json_object_set_new(workspaceEdit, "changes", changes);
	json_object_set_new(response, "result", workspaceEdit);

	sendMessage(response);
	json_decref(response);
}

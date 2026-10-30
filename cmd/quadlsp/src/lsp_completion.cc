// Completion handlers for QuadrateLSP
// Split from main.cc for maintainability

#include "lsp_impl.h"
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/reference.h>
#include <quadrate/qc/type_names.h>
#include <sstream>

// Check if cursor is at top level (not inside a function body)
// by counting brace depth up to the cursor position
static bool isAtTopLevel(const std::string& text, size_t line, size_t character) {
	int braceDepth = 0;
	size_t currentLine = 0;
	size_t currentCol = 0;
	bool inString = false;
	bool inLineComment = false;
	bool inBlockComment = false;

	for (size_t i = 0; i < text.size(); i++) {
		char c = text[i];
		char next = (i + 1 < text.size()) ? text[i + 1] : '\0';

		// Check if we've reached the cursor position
		if (currentLine == line && currentCol >= character) {
			break;
		}
		if (currentLine > line) {
			break;
		}

		// Handle newlines
		if (c == '\n') {
			currentLine++;
			currentCol = 0;
			inLineComment = false;
			continue;
		}

		// Handle comments
		if (!inString && !inBlockComment && c == '/' && next == '/') {
			inLineComment = true;
		}
		if (!inString && !inLineComment && c == '/' && next == '*') {
			inBlockComment = true;
			i++;
			currentCol += 2;
			continue;
		}
		if (inBlockComment && c == '*' && next == '/') {
			inBlockComment = false;
			i++;
			currentCol += 2;
			continue;
		}

		if (inLineComment || inBlockComment) {
			currentCol++;
			continue;
		}

		// Handle strings
		if (c == '"' && (i == 0 || text[i - 1] != '\\')) {
			inString = !inString;
		}

		if (!inString) {
			if (c == '{') {
				braceDepth++;
			} else if (c == '}') {
				braceDepth--;
			}
		}

		currentCol++;
	}

	return braceDepth == 0;
}

// Check if cursor is in a type position (after : in function signature, struct field, or variable declaration)
static bool isInTypePosition(const std::string& text, size_t line, size_t character) {
	// Split text into lines
	std::vector<std::string> lines;
	std::istringstream stream(text);
	std::string currentLine;
	while (std::getline(stream, currentLine)) {
		lines.push_back(currentLine);
	}

	if (line >= lines.size()) {
		return false;
	}

	const std::string& targetLine = lines[line];
	if (character > targetLine.length()) {
		return false;
	}

	// Look backwards from cursor to find if we're after a colon (for type annotation)
	size_t pos = character;

	// Skip back over any partial type name being typed (but NOT over colons - we need to find them)
	while (pos > 0 && (isalnum(targetLine[pos - 1]) || targetLine[pos - 1] == '_')) {
		pos--;
	}

	// Skip whitespace
	while (pos > 0 && isspace(targetLine[pos - 1])) {
		pos--;
	}

	// Check if there's a single ':' before (not ::)
	if (pos > 0 && targetLine[pos - 1] == ':') {
		// Make sure it's not a :: (scope operator)
		if (pos >= 2 && targetLine[pos - 2] == ':') {
			return false;
		}

		// Check if there's an identifier before the colon (parameter/field name pattern)
		size_t colonPos = pos - 1;
		size_t identEnd = colonPos;

		// Skip whitespace before colon
		while (identEnd > 0 && isspace(targetLine[identEnd - 1])) {
			identEnd--;
		}

		// Check for identifier before colon
		size_t identStart = identEnd;
		while (identStart > 0 && (isalnum(targetLine[identStart - 1]) || targetLine[identStart - 1] == '_')) {
			identStart--;
		}

		// If there's an identifier before the colon, this is a type position
		if (identEnd > identStart) {
			return true;
		}

		// Also check on the current line for context markers
		for (size_t i = 0; i < pos - 1; i++) {
			if (targetLine[i] == '(' || targetLine[i] == '{') {
				return true;
			}
			if (i + 1 < pos - 1 && targetLine[i] == '-' && targetLine[i + 1] == '>') {
				return true;
			}
		}
	}

	return false;
}

// The CompletionItemKind values used here, as the LSP specification numbers them
enum CompletionKind {
	KindMethod = 2,
	KindFunction = 3,
	KindField = 5,
	KindModule = 9,
	KindKeyword = 14,
	KindConstant = 21,
	KindStruct = 22,
	KindTypeParameter = 25
};

static json_t* completionItem(const std::string& label, int kind) {
	json_t* item = json_object();
	json_object_set_new(item, "label", json_stringn(label.data(), label.size()));
	json_object_set_new(item, "kind", json_integer(kind));
	return item;
}

static void setDetail(json_t* item, const std::string& detail) {
	json_object_set_new(item, "detail", json_stringn(detail.data(), detail.size()));
}

static void setMarkdown(json_t* item, const std::string& text) {
	json_t* documentation = json_object();
	json_object_set_new(documentation, "kind", json_string("markdown"));
	json_object_set_new(documentation, "value", json_stringn(text.data(), text.size()));
	json_object_set_new(item, "documentation", documentation);
}

static std::string joinNames(const std::vector<std::string>& names, const char* separator) {
	std::string joined;
	for (size_t i = 0; i < names.size(); i++) {
		joined += (i > 0 ? separator : "") + names[i];
	}
	return joined;
}

// "<T, U>" for a generic declaration, nothing otherwise
static std::string typeParamSuffix(bool isGeneric, const std::vector<std::string>& typeParams) {
	return isGeneric ? "<" + joinNames(typeParams, ", ") + ">" : "";
}

// A function or method, from the current document (`module` empty) or from a module
static json_t* functionItem(const FunctionInfo& func, const std::string& module) {
	json_t* item = completionItem(
			func.name + typeParamSuffix(func.isGeneric, func.typeParams), func.isMethod ? KindMethod : KindFunction);
	json_object_set_new(item, "insertTextFormat", json_integer(2)); // Snippet
	json_object_set_new(item, "insertText", json_string(func.snippet.c_str()));
	setDetail(item, func.signature);

	std::ostringstream doc;
	if (!module.empty()) {
		doc << "**Module:** `" << module << "`\n\n";
	}
	if (func.isMethod) {
		doc << "**Method on:** `" << func.receiverType
			<< typeParamSuffix(!func.receiverTypeParams.empty(), func.receiverTypeParams) << "`\n\n";
	}
	if (func.isGeneric) {
		doc << "**Type parameters:** `" << typeParamSuffix(true, func.typeParams) << "`\n\n";
	}
	doc << "**Signature:**\n```quadrate\n" << func.signature << "\n```\n\n";
	if (!func.inputParams.empty()) {
		doc << "**Stack before call:** " << joinNames(func.inputParams, ", ") << "\n";
	}
	if (!func.outputParams.empty()) {
		doc << "**Stack after call:** " << joinNames(func.outputParams, ", ");
	}
	setMarkdown(item, doc.str());
	return item;
}

// A struct to construct, from the current document (`module` empty) or from a module
static json_t* structItem(const StructInfo& info, const std::string& module) {
	const std::string label = info.name + typeParamSuffix(info.isGeneric, info.typeParams);
	json_t* item = completionItem(label, KindStruct);

	std::ostringstream doc;
	if (!module.empty()) {
		doc << "**Module:** `" << module << "`\n\n";
	}
	doc << "**Struct:** `" << label << "`\n\n";
	if (info.isGeneric) {
		doc << "**Type parameters:** `" << typeParamSuffix(true, info.typeParams) << "`\n\n";
	}
	doc << "**Fields:**\n";
	for (const auto& field : info.fields) {
		doc << "- `" << field.first << ": " << field.second << "`\n";
	}
	setMarkdown(item, doc.str());
	return item;
}

// A struct named in a type annotation, where a generic one is offered as `Name<>`
static json_t* structTypeItem(const StructInfo& info, const std::string& module) {
	json_t* item = completionItem(
			(module.empty() ? "" : module + "::") + info.name + (info.isGeneric ? "<>" : ""), KindStruct);
	setDetail(item, module.empty() ? "User-defined struct" : "Struct from " + module);
	return item;
}

// The modules a document names in `use` statements. Incomplete code still has its
// imports, so a parse with errors is read as far as it got.
static std::vector<std::string> usedModules(Qd::IAstNode* root) {
	std::vector<std::string> modules;
	if (!root || root->type() != Qd::IAstNode::Type::PROGRAM) {
		return modules;
	}
	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);
		if (child && child->type() == Qd::IAstNode::Type::USE_STATEMENT) {
			modules.push_back(static_cast<Qd::AstNodeUse*>(child)->module());
		}
	}
	return modules;
}

// The FFI import declaring `namespaceName`, or null
static Qd::AstNodeImport* findImport(Qd::IAstNode* root, const std::string& namespaceName) {
	if (!root || root->type() != Qd::IAstNode::Type::PROGRAM) {
		return nullptr;
	}
	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);
		if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
			auto* importNode = static_cast<Qd::AstNodeImport*>(child);
			if (importNode->namespaceName() == namespaceName) {
				return importNode;
			}
		}
	}
	return nullptr;
}

// Every .qd file of the module whose entry file is `modulePath`: a module is its whole
// directory. Just the entry file if the directory cannot be read.
static std::vector<std::string> moduleFiles(const std::string& modulePath) {
	std::vector<std::string> files;
	try {
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(modulePath).parent_path())) {
			if (entry.is_regular_file() && entry.path().extension() == ".qd") {
				files.push_back(entry.path().string());
			}
		}
	} catch (const std::filesystem::filesystem_error&) {
		files = {modulePath};
	}
	return files;
}

void QuadrateLSP::handleCompletion(const std::string& id, const std::string& uri, size_t line, size_t character) {
	const std::string documentText = getDocumentText(uri);

	// Module paths resolve from the document's own directory
	std::string sourceDir;
	const std::string filePath = lspUriToPath(uri);
	if (size_t lastSlash = filePath.rfind('/'); lastSlash != std::string::npos) {
		sourceDir = filePath.substr(0, lastSlash);
	}

	// What is being completed is usually not finished, so the parse is used as far as it got
	Qd::Ast ast;
	Qd::IAstNode* root = documentText.empty() ? nullptr : ast.generate(documentText.c_str(), false, nullptr);

	json_t* items = json_array();
	if (isInTypePosition(documentText, line, character)) {
		addTypeCompletions(items, documentText, root, sourceDir);
	} else if (std::string variable = getFieldAccessVariableAtPosition(documentText, line, character);
			!variable.empty()) {
		addFieldCompletions(items, documentText, root, variable, line, sourceDir);
	} else if (std::string module = getModulePrefixAtPosition(documentText, line, character); !module.empty()) {
		addModuleCompletions(items, root, module, sourceDir);
	} else {
		addBodyCompletions(items, documentText, root, isAtTopLevel(documentText, line, character));
	}

	json_t* result = json_object();
	json_object_set_new(result, "isIncomplete", json_false());
	json_object_set_new(result, "items", items);

	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", makeResponseId(id));
	json_object_set_new(response, "result", result);
	sendMessage(response);
	json_decref(response);
}

// After `name:` in a signature, a struct field or a declaration: the built-in types and
// every struct in reach
void QuadrateLSP::addTypeCompletions(
		json_t* items, const std::string& documentText, Qd::IAstNode* root, const std::string& sourceDir) {
	std::vector<Qd::BuiltinType> builtinTypes(std::begin(Qd::PRIMITIVE_TYPES), std::end(Qd::PRIMITIVE_TYPES));
	builtinTypes.insert(builtinTypes.end(), std::begin(Qd::SIZED_INT_TYPES), std::end(Qd::SIZED_INT_TYPES));
	for (const auto& alias : Qd::BUILTIN_TYPE_ALIASES) {
		builtinTypes.push_back({alias.name, alias.description});
	}
	for (const auto& type : builtinTypes) {
		json_t* item = completionItem(std::string(type.name), KindTypeParameter);
		setDetail(item, std::string(type.description));
		json_array_append_new(items, item);
	}

	for (const auto& info : extractStructs(documentText)) {
		json_array_append_new(items, structTypeItem(info, ""));
	}

	for (const auto& module : usedModules(root)) {
		const std::string modulePath = resolveModulePath(module, sourceDir);
		if (modulePath.empty()) {
			continue;
		}
		for (const auto& file : moduleFiles(modulePath)) {
			for (const auto& info : extractModuleStructs(file)) {
				json_array_append_new(items, structTypeItem(info, module));
			}
		}
	}
}

// After `variable <<`: the fields of the struct the variable holds
void QuadrateLSP::addFieldCompletions(json_t* items, const std::string& documentText, Qd::IAstNode* root,
		const std::string& variable, size_t line, const std::string& sourceDir) {
	if (!root) {
		return;
	}
	const std::string structType = findStructTypeOfVariable(root, variable, line);
	if (structType.empty()) {
		return;
	}
	for (const auto& field : findStructFields(structType, documentText, sourceDir)) {
		json_t* item = completionItem(field.first, KindField);
		setDetail(item, field.second);
		setMarkdown(item, "**Field:** `" + field.first + "`\n\n**Type:** `" + field.second + "`\n\n**Struct:** `" +
								  structType + "`");
		json_array_append_new(items, item);
	}
}

// After `module::`: what an FFI import of that name declares, or else everything the
// module declares
void QuadrateLSP::addModuleCompletions(
		json_t* items, Qd::IAstNode* root, const std::string& module, const std::string& sourceDir) {
	if (Qd::AstNodeImport* importNode = findImport(root, module)) {
		for (const auto& func : importNode->functions()) {
			std::vector<std::string> inputs;
			for (const auto& param : func->inputParameters) {
				inputs.push_back(param->name() + ":" + param->typeString());
			}
			std::vector<std::string> outputs;
			for (const auto& param : func->outputParameters) {
				outputs.push_back(param->name() + ":" + param->typeString());
			}
			std::string signature = "fn " + func->name + "(" + joinNames(inputs, " ");
			if (!inputs.empty() || !outputs.empty()) {
				signature += " -- ";
			}
			signature += joinNames(outputs, " ") + ")";

			json_t* item = completionItem(func->name, KindFunction);
			// Plain text, so that the editor does not add ()
			json_object_set_new(item, "insertTextFormat", json_integer(1));
			json_object_set_new(item, "insertText", json_string(func->name.c_str()));
			setDetail(item, signature);
			setMarkdown(item, "**FFI Function (from " + module + ")**\n\n```quadrate\n" + signature + "\n```");
			json_array_append_new(items, item);
		}
		return;
	}

	const std::string modulePath = resolveModulePath(module, sourceDir);
	if (modulePath.empty()) {
		return;
	}
	const std::vector<std::string> files = moduleFiles(modulePath);
	for (const auto& file : files) {
		for (const auto& func : extractModuleFunctions(file)) {
			json_array_append_new(items, functionItem(func, module));
		}
	}
	for (const auto& file : files) {
		for (const auto& info : extractModuleStructs(file)) {
			json_array_append_new(items, structItem(info, module));
		}
	}
	for (const auto& file : files) {
		for (const auto& constant : extractModuleConstants(file)) {
			json_t* item = completionItem(constant.name, KindConstant);
			setMarkdown(item,
					"**Module:** `" + module + "`\n\n**Constant:** `" + constant.name + " = " + constant.value + "`");
			json_array_append_new(items, item);
		}
	}
}

// Anywhere else: declaration keywords at top level; inside a function body, the
// builtins and keywords, the document's functions and structs and the modules it uses
void QuadrateLSP::addBodyCompletions(
		json_t* items, const std::string& documentText, Qd::IAstNode* root, bool topLevel) {
	struct Declaration {
		const char* keyword;
		const char* description;
	};

	static const Declaration declarations[] = {{"use", "Import a module"}, {"fn", "Declare a function"},
			{"test", "Declare a test function"}, {"struct", "Declare a struct type"},
			{"packed", "Modifier for packed struct layout (packed struct ...)"}, {"enum", "Declare an enum type"},
			{"type", "Declare a type alias"}, {"const", "Declare a constant"}, {"var", "Declare a mutable global"},
			{"pub", "Make declaration public"}, {"inline", "Inline the function at call sites (inline fn ...)"},
			{"stack", "Leave the arguments on the stack instead of binding them (stack fn ...)"}};

	if (topLevel) {
		for (const auto& declaration : declarations) {
			json_t* item = completionItem(declaration.keyword, KindKeyword);
			setDetail(item, declaration.description);
			json_array_append_new(items, item);
		}
		return;
	}

	// Every builtin and keyword with a name to type -- not `+` or `->` -- with its
	// signature and description from reference.def. The declaration keywords belong
	// at top level.
	auto isDeclarationKeyword = [](const char* name) {
		if (strcmp(name, "import") == 0) {
			return true;
		}
		for (const auto& declaration : declarations) {
			if (strcmp(name, declaration.keyword) == 0) {
				return true;
			}
		}
		return false;
	};
	for (const auto& entry : Qd::REFERENCE) {
		if (std::isalpha(static_cast<unsigned char>(entry.name[0])) == 0 || isDeclarationKeyword(entry.name)) {
			continue;
		}
		json_t* item = completionItem(entry.name, entry.signature != nullptr ? KindFunction : KindKeyword);
		setDetail(item, entry.signature != nullptr ? entry.signature : "Keyword");
		setMarkdown(item, entry.description);
		json_array_append_new(items, item);
	}

	for (const auto& func : extractFunctions(documentText)) {
		json_array_append_new(items, functionItem(func, ""));
	}

	for (const auto& module : usedModules(root)) {
		json_t* item = completionItem(module, KindModule);
		// The module name and `::`, ready for one of its members
		json_object_set_new(item, "insertText", json_string((module + "::").c_str()));
		setMarkdown(item,
				"**Module:** `" + module + "`\n\nType `" + module + "::` to access functions, structs, and constants.");
		json_array_append_new(items, item);
	}

	for (const auto& info : extractStructs(documentText)) {
		json_array_append_new(items, structItem(info, ""));
	}
}

std::string QuadrateLSP::getModulePrefixAtPosition(const std::string& text, size_t line, size_t character) {
	// Split text into lines
	std::vector<std::string> lines;
	std::istringstream stream(text);
	std::string currentLine;
	while (std::getline(stream, currentLine)) {
		lines.push_back(currentLine);
	}

	if (line >= lines.size()) {
		return "";
	}

	const std::string& targetLine = lines[line];
	if (character == 0 || character > targetLine.length()) {
		return "";
	}

	// Check if the characters before cursor end with "::"
	size_t pos = character;

	// Skip back over any partial word being typed after ::
	while (pos > 0 && (isalnum(targetLine[pos - 1]) || targetLine[pos - 1] == '_')) {
		pos--;
	}

	// Now check if we have "::" before that
	if (pos >= 2 && targetLine[pos - 1] == ':' && targetLine[pos - 2] == ':') {
		// Found "::", now extract the module name before it
		size_t colonPos = pos - 2;
		size_t moduleEnd = colonPos;
		size_t moduleStart = colonPos;

		// Move back to find start of module name
		while (moduleStart > 0 && (isalnum(targetLine[moduleStart - 1]) || targetLine[moduleStart - 1] == '_')) {
			moduleStart--;
		}

		if (moduleEnd > moduleStart) {
			return targetLine.substr(moduleStart, moduleEnd - moduleStart);
		}
	}

	return "";
}

std::string QuadrateLSP::getFieldAccessVariableAtPosition(const std::string& text, size_t line, size_t character) {
	// Split text into lines
	std::vector<std::string> lines;
	std::istringstream stream(text);
	std::string currentLine;
	while (std::getline(stream, currentLine)) {
		lines.push_back(currentLine);
	}

	if (line >= lines.size()) {
		return "";
	}

	const std::string& targetLine = lines[line];
	if (character == 0 || character > targetLine.length()) {
		return "";
	}

	// Check if the characters before cursor end with "<<" (field read operator)
	size_t pos = character;

	// Skip back over any partial field name being typed after <<
	while (pos > 0 && (isalnum(targetLine[pos - 1]) || targetLine[pos - 1] == '_')) {
		pos--;
	}

	// Now check if we have "<<" before that
	if (pos >= 2 && targetLine[pos - 1] == '<' && targetLine[pos - 2] == '<') {
		// Found "<<", now extract the variable name before it: `p <<x` is how a field
		// read is written, so there may be space between them
		size_t varEnd = pos - 2;
		while (varEnd > 0 && isspace(static_cast<unsigned char>(targetLine[varEnd - 1]))) {
			varEnd--;
		}
		size_t varStart = varEnd;

		// Move back to find start of variable name
		while (varStart > 0 && (isalnum(targetLine[varStart - 1]) || targetLine[varStart - 1] == '_')) {
			varStart--;
		}

		if (varEnd > varStart) {
			return targetLine.substr(varStart, varEnd - varStart);
		}
	}

	return "";
}

std::vector<std::pair<std::string, std::string>> QuadrateLSP::findStructFields(
		const std::string& structType, const std::string& documentText, const std::string& sourceDir) {
	std::vector<std::pair<std::string, std::string>> fields;

	// Check if it's a module-qualified struct (module::Struct)
	size_t colonPos = structType.find("::");
	if (colonPos != std::string::npos) {
		std::string moduleName = structType.substr(0, colonPos);
		std::string structName = structType.substr(colonPos + 2);

		std::string modulePath = resolveModulePath(moduleName, sourceDir);
		if (!modulePath.empty()) {
			std::vector<StructInfo> structs = extractModuleStructs(modulePath);
			for (const auto& s : structs) {
				if (s.name == structName) {
					return s.fields;
				}
			}
		}
	} else {
		// Local struct - search in current document
		std::vector<StructInfo> structs = extractStructs(documentText);
		for (const auto& s : structs) {
			if (s.name == structType) {
				return s.fields;
			}
		}
	}

	return fields;
}

std::vector<FunctionInfo> QuadrateLSP::extractModuleFunctions(const std::string& modulePath) {
	std::vector<FunctionInfo> functions;

	std::ifstream file(modulePath);
	if (!file.good()) {
		return functions;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string moduleText = buffer.str();

	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

	if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
		return functions;
	}

	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);

		if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);

			// Only include public functions
			if (!funcNode->isPublic()) {
				continue;
			}

			FunctionInfo info;
			info.name = funcNode->name();

			// Extract generic type parameters
			info.isGeneric = funcNode->isGeneric();
			if (info.isGeneric) {
				info.typeParams = funcNode->typeParams();
			}

			// Extract method receiver information
			info.isMethod = funcNode->hasReceiver();
			if (info.isMethod) {
				info.receiverName = funcNode->receiverName();
				info.receiverType = funcNode->receiverType();
				if (funcNode->hasReceiverTypeParams()) {
					info.receiverTypeParams = funcNode->receiverTypeParams();
				}
			}

			// Build signature parts
			std::ostringstream sigStream;

			// Include receiver in signature for methods
			if (info.isMethod) {
				sigStream << "fn (" << info.receiverName << ":" << info.receiverType;
				if (!info.receiverTypeParams.empty()) {
					sigStream << "<";
					for (size_t j = 0; j < info.receiverTypeParams.size(); j++) {
						if (j > 0) {
							sigStream << ", ";
						}
						sigStream << info.receiverTypeParams[j];
					}
					sigStream << ">";
				}
				sigStream << ") " << info.name;
			} else {
				sigStream << "fn " << info.name;
			}

			// Add type parameters to signature
			if (info.isGeneric) {
				sigStream << "<";
				for (size_t j = 0; j < info.typeParams.size(); j++) {
					if (j > 0) {
						sigStream << ", ";
					}
					sigStream << info.typeParams[j];
				}
				sigStream << ">";
			}

			sigStream << "(";

			// Extract input parameters
			const auto& inputs = funcNode->inputParameters();
			for (size_t j = 0; j < inputs.size(); j++) {
				Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(inputs[j].get());
				std::string paramStr = param->displayString();
				info.inputParams.push_back(paramStr);

				if (j > 0) {
					sigStream << " ";
				}
				sigStream << paramStr;
			}

			if (!inputs.empty() || !funcNode->outputParameters().empty()) {
				sigStream << " -- ";
			}

			// Extract output parameters
			const auto& outputs = funcNode->outputParameters();
			for (size_t j = 0; j < outputs.size(); j++) {
				Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(outputs[j].get());
				std::string paramStr = param->displayString();
				info.outputParams.push_back(paramStr);

				if (j > 0) {
					sigStream << " ";
				}
				sigStream << paramStr;
			}

			sigStream << ")";
			if (funcNode->throws()) {
				sigStream << "!";
			}
			info.signature = sigStream.str();

			// Build snippet with placeholders for input parameters
			std::ostringstream snippetStream;
			for (size_t j = 0; j < info.inputParams.size(); j++) {
				const std::string& param = info.inputParams[j];
				size_t colonPos = param.find(':');
				std::string paramName = (colonPos != std::string::npos) ? param.substr(0, colonPos) : param;

				snippetStream << "${" << (j + 1) << ":" << paramName << "}";
				if (j < info.inputParams.size() - 1) {
					snippetStream << " ";
				}
			}
			if (!info.inputParams.empty()) {
				snippetStream << " ";
			}
			snippetStream << info.name;
			if (funcNode->throws()) {
				snippetStream << "!";
			}

			info.snippet = snippetStream.str();

			functions.push_back(info);
		} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
			// Handle imported native functions (e.g., from C libraries)
			Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);

			for (const auto& importedFunc : importNode->functions()) {
				// Only include public imported functions
				if (!importedFunc->isPublic) {
					continue;
				}

				FunctionInfo info;
				info.name = importedFunc->name;

				// Build signature parts
				std::ostringstream sigStream;
				sigStream << "fn " << info.name << "(";

				// Extract input parameters
				const auto& inputs = importedFunc->inputParameters;
				for (size_t j = 0; j < inputs.size(); j++) {
					Qd::AstNodeParameter* param = inputs[j].get();
					std::string paramStr = param->displayString();
					info.inputParams.push_back(paramStr);

					if (j > 0) {
						sigStream << " ";
					}
					sigStream << paramStr;
				}

				if (!inputs.empty() || !importedFunc->outputParameters.empty()) {
					sigStream << " -- ";
				}

				// Extract output parameters
				const auto& outputs = importedFunc->outputParameters;
				for (size_t j = 0; j < outputs.size(); j++) {
					Qd::AstNodeParameter* param = outputs[j].get();
					std::string paramStr = param->displayString();
					info.outputParams.push_back(paramStr);

					if (j > 0) {
						sigStream << " ";
					}
					sigStream << paramStr;
				}

				sigStream << ")";
				info.signature = sigStream.str();

				// Build snippet with placeholders for input parameters
				std::ostringstream snippetStream;
				for (size_t j = 0; j < info.inputParams.size(); j++) {
					const std::string& param = info.inputParams[j];
					size_t colonPos = param.find(':');
					std::string paramName = (colonPos != std::string::npos) ? param.substr(0, colonPos) : param;

					snippetStream << "${" << (j + 1) << ":" << paramName << "}";
					if (j < info.inputParams.size() - 1) {
						snippetStream << " ";
					}
				}
				if (!info.inputParams.empty()) {
					snippetStream << " ";
				}
				snippetStream << info.name;

				info.snippet = snippetStream.str();

				functions.push_back(info);
			}
		}
	}

	return functions;
}

std::vector<StructInfo> QuadrateLSP::extractModuleStructs(const std::string& modulePath) {
	std::vector<StructInfo> structs;

	std::ifstream file(modulePath);
	if (!file.good()) {
		return structs;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string moduleText = buffer.str();

	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

	if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
		return structs;
	}

	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);

		if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
			Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);

			// Only include public structs
			if (!structNode->isPublic()) {
				continue;
			}

			StructInfo info;
			info.name = structNode->name();

			// Extract generic type parameters
			info.isGeneric = structNode->isGeneric();
			if (info.isGeneric) {
				info.typeParams = structNode->typeParams();
			}

			// Build signature
			std::ostringstream sig;
			sig << "struct " << info.name;

			// Include type parameters in signature
			if (info.isGeneric) {
				sig << "<";
				for (size_t j = 0; j < info.typeParams.size(); j++) {
					if (j > 0) {
						sig << ", ";
					}
					sig << info.typeParams[j];
				}
				sig << ">";
			}

			sig << " { ";
			for (const auto& field : structNode->fields()) {
				const Qd::AstNodeStructField* structField = static_cast<const Qd::AstNodeStructField*>(field.get());
				info.fields.push_back({structField->name(), structField->typeName()});
				sig << structField->name() << ":" << structField->typeName() << " ";
			}
			sig << "}";
			info.signature = sig.str();

			structs.push_back(info);
		}
	}

	return structs;
}

std::vector<ConstantInfo> QuadrateLSP::extractModuleConstants(const std::string& modulePath) {
	std::vector<ConstantInfo> constants;

	std::ifstream file(modulePath);
	if (!file.good()) {
		return constants;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string moduleText = buffer.str();

	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

	if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
		return constants;
	}

	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);

		if (child && child->type() == Qd::IAstNode::Type::CONSTANT_DECLARATION) {
			Qd::AstNodeConstant* constNode = static_cast<Qd::AstNodeConstant*>(child);

			// Only include public constants
			if (!constNode->isPublic()) {
				continue;
			}

			ConstantInfo info;
			info.name = constNode->name();
			info.value = constNode->value();

			constants.push_back(info);
		}

		// Extract enum variants as constants (EnumName::Variant = value)
		if (child && child->type() == Qd::IAstNode::Type::ENUM_DECLARATION) {
			Qd::AstNodeEnumDeclaration* enumNode = static_cast<Qd::AstNodeEnumDeclaration*>(child);

			if (!enumNode->isPublic()) {
				continue;
			}

			for (const auto& variant : enumNode->variants()) {
				ConstantInfo info;
				info.name = enumNode->name() + "::" + variant.name;
				info.value = std::to_string(variant.value);
				constants.push_back(info);
			}
		}
	}

	return constants;
}

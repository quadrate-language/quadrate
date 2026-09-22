#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/file_utils.h>
#include <quadrate/cli/help.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_as_cast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_global_var.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_program.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_struct_construction.h>
#include <quadrate/qc/ast_node_struct_field.h>
#include <quadrate/qc/ast_node_type_alias.h>
#include <quadrate/qc/ast_node_use.h>
#include <set>
#include <string>
#include <vector>

using namespace Qd;

struct UsesOptions {
	bool inPlace = false;
	bool check = false;
	bool dryRun = false;
};

void printHelp() {
	qdcli::Help help("quaduses", "Quadrate use-statement manager");
	help.description("Analyses a source file and adds the use statements it needs, removing the\n"
					 "ones it does not.")
			.usage("[options] <file|directory>...")
			.section("Options")
			.standardOptions()
			.option('w', "--write", "Update files in place")
			.option('c', "--check", "Report whether files need changes (exit 1 if so)")
			.option('n', "--dry-run", "Show what would change without modifying")
			.section("Examples")
			.item("quaduses file.qd", "Print the updated file to stdout")
			.item("quaduses -w file.qd", "Update use statements in place")
			.item("quaduses -w src/", "Update every .qd file in a directory, recursively")
			.item("quaduses -c src/", "Check whether any file needs updating (for CI)")
			.item("quaduses -n file.qd", "Show the changes that would be made");
	help.print();
}

struct References {
	std::set<std::string> scopes;
	std::set<std::string> typeNames;
	std::set<std::string> identifiers;
	std::set<std::string> definedTypes;
	std::set<std::string> definedNames;
};

static bool isPrimitiveTypeName(const std::string& name) {
	static const std::set<std::string> primitives = {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64",
			"str", "ptr", "any", "bool", "void", "fn", "int", "float", "char", "byte"};
	return primitives.count(name) > 0;
}

static bool isIdentChar(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static void scanTypeString(const std::string& typeName, References& refs) {
	size_t i = 0;
	while (i < typeName.size()) {
		if (!isIdentChar(typeName[i])) {
			i++;
			continue;
		}
		size_t start = i;
		while (i < typeName.size() && isIdentChar(typeName[i])) {
			i++;
		}
		std::string word = typeName.substr(start, i - start);
		bool qualified = start >= 2 && typeName.compare(start - 2, 2, "::") == 0;
		bool scope = typeName.compare(i, 2, "::") == 0;
		if (scope) {
			if (!qualified) {
				refs.scopes.insert(word);
			}
		} else if (!qualified && !std::isdigit(static_cast<unsigned char>(word[0])) && !isPrimitiveTypeName(word)) {
			refs.typeNames.insert(word);
		}
	}
}

void collectReferences(const IAstNode* node, References& refs) {
	if (!node) {
		return;
	}

	switch (node->type()) {
	case IAstNode::Type::SCOPED_IDENTIFIER:
		refs.scopes.insert(static_cast<const AstNodeScopedIdentifier*>(node)->scope());
		break;
	case IAstNode::Type::IDENTIFIER:
		refs.identifiers.insert(static_cast<const AstNodeIdentifier*>(node)->name());
		break;
	case IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
		const std::string& name = static_cast<const AstNodeFunctionPointerReference*>(node)->functionName();
		if (name.find("::") != std::string::npos) {
			scanTypeString(name, refs);
		} else {
			refs.identifiers.insert(name);
		}
		break;
	}
	case IAstNode::Type::STRUCT_FIELD:
		scanTypeString(static_cast<const AstNodeStructField*>(node)->typeName(), refs);
		break;
	case IAstNode::Type::VARIABLE_DECLARATION: {
		const AstNodeParameter* param = static_cast<const AstNodeParameter*>(node);
		scanTypeString(param->typeString(), refs);
		refs.definedNames.insert(param->name());
		break;
	}
	case IAstNode::Type::STRUCT_CONSTRUCTION: {
		const AstNodeStructConstruction* construction = static_cast<const AstNodeStructConstruction*>(node);
		scanTypeString(construction->structName(), refs);
		for (const auto& arg : construction->typeArgs()) {
			scanTypeString(arg, refs);
		}
		break;
	}
	case IAstNode::Type::AS_CAST:
		scanTypeString(static_cast<const AstNodeAsCast*>(node)->typeName(), refs);
		break;
	case IAstNode::Type::GLOBAL_VAR_DECLARATION: {
		const AstNodeGlobalVar* var = static_cast<const AstNodeGlobalVar*>(node);
		scanTypeString(var->typeName(), refs);
		refs.definedNames.insert(var->name());
		break;
	}
	case IAstNode::Type::TYPE_ALIAS_DECLARATION: {
		const AstNodeTypeAlias* alias = static_cast<const AstNodeTypeAlias*>(node);
		scanTypeString(alias->targetType(), refs);
		refs.definedTypes.insert(alias->name());
		break;
	}
	case IAstNode::Type::CONSTANT_DECLARATION:
		refs.definedNames.insert(static_cast<const AstNodeConstant*>(node)->name());
		break;
	case IAstNode::Type::FUNCTION_DECLARATION: {
		const AstNodeFunctionDeclaration* fn = static_cast<const AstNodeFunctionDeclaration*>(node);
		refs.definedNames.insert(fn->name());
		for (const auto& tp : fn->typeParams()) {
			refs.definedTypes.insert(tp);
		}
		if (fn->hasReceiver()) {
			refs.definedNames.insert(fn->receiverName());
			scanTypeString(fn->receiverType(), refs);
			for (const auto& tp : fn->receiverTypeParams()) {
				refs.definedTypes.insert(tp);
			}
		}
		break;
	}
	case IAstNode::Type::STRUCT_DECLARATION: {
		const AstNodeStructDeclaration* decl = static_cast<const AstNodeStructDeclaration*>(node);
		refs.definedTypes.insert(decl->name());
		for (const auto& tp : decl->typeParams()) {
			refs.definedTypes.insert(tp);
		}
		break;
	}
	case IAstNode::Type::ENUM_DECLARATION:
		refs.definedTypes.insert(static_cast<const AstNodeEnumDeclaration*>(node)->name());
		break;
	case IAstNode::Type::LOCAL:
		for (const auto& name : static_cast<const AstNodeLocal*>(node)->names()) {
			refs.definedNames.insert(name);
		}
		break;
	case IAstNode::Type::FOR_STATEMENT:
		refs.definedNames.insert(static_cast<const AstNodeForStatement*>(node)->iteratorName());
		break;
	default:
		break;
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectReferences(node->child(i), refs);
	}
}

static bool codeMentionsScope(const std::string& source, const std::string& scope) {
	const std::string needle = scope + "::";
	int blockDepth = 0;
	bool inStr = false;
	for (size_t i = 0; i < source.size(); i++) {
		char c = source[i];
		if (blockDepth > 0) {
			if (source.compare(i, 2, "/*") == 0) {
				blockDepth++;
				i++;
			} else if (source.compare(i, 2, "*/") == 0) {
				blockDepth--;
				i++;
			}
			continue;
		}
		if (inStr) {
			if (c == '\\') {
				i++;
			} else if (c == '"') {
				inStr = false;
			}
			continue;
		}
		if (c == '"') {
			inStr = true;
		} else if (source.compare(i, 2, "//") == 0) {
			while (i < source.size() && source[i] != '\n') {
				i++;
			}
		} else if (source.compare(i, 2, "/*") == 0) {
			blockDepth = 1;
			i++;
		} else if (source.compare(i, needle.size(), needle) == 0 && (i == 0 || !isIdentChar(source[i - 1]))) {
			return true;
		}
	}
	return false;
}

void collectImportNamespaces(const IAstNode* node, std::set<std::string>& importedNamespaces) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
		const AstNodeImport* importNode = static_cast<const AstNodeImport*>(node);
		importedNamespaces.insert(importNode->namespaceName());
	}

	// Check direct children of program node
	for (size_t i = 0; i < node->childCount(); i++) {
		const IAstNode* child = node->child(i);
		if (child && child->type() == IAstNode::Type::IMPORT_STATEMENT) {
			const AstNodeImport* importNode = static_cast<const AstNodeImport*>(child);
			importedNamespaces.insert(importNode->namespaceName());
		}
	}
}

// Helper to extract package name from module identifier
static std::string getPackageFromModuleName(const std::string& moduleName) {
	// Check if this is a file path (ends with .qd)
	bool isFilePath = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isFilePath) {
		// Extract filename from path
		size_t lastSlash = moduleName.find_last_of('/');
		std::string filename = (lastSlash != std::string::npos) ? moduleName.substr(lastSlash + 1) : moduleName;

		// Remove .qd extension
		if (filename.size() >= 3 && filename.substr(filename.size() - 3) == ".qd") {
			filename = filename.substr(0, filename.size() - 3);
		}

		return filename;
	}

	// Not a file path, return as-is
	return moduleName;
}

struct UseLine {
	bool parsed = false;
	std::string indent;
	std::vector<std::string> segments;
	std::vector<std::string> modules;
	std::string trailer;
};

static UseLine parseUseLine(const std::string& line) {
	UseLine result;
	auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
	size_t pos = 0;
	while (pos < line.size() && isSpace(line[pos])) {
		pos++;
	}
	result.indent = line.substr(0, pos);
	while (pos < line.size() && line.compare(pos, 3, "use") == 0 && pos + 3 < line.size() &&
			(line[pos + 3] == ' ' || line[pos + 3] == '\t' || line[pos + 3] == '"')) {
		size_t start = pos;
		pos += 3;
		while (pos < line.size() && isSpace(line[pos])) {
			pos++;
		}
		std::string module;
		if (pos < line.size() && line[pos] == '"') {
			size_t close = line.find('"', pos + 1);
			if (close == std::string::npos) {
				return UseLine{};
			}
			module = line.substr(pos + 1, close - pos - 1);
			pos = close + 1;
		} else {
			size_t tokStart = pos;
			while (pos < line.size() && !isSpace(line[pos]) && line[pos] != '"' && line.compare(pos, 2, "//") != 0 &&
					line.compare(pos, 2, "/*") != 0) {
				pos++;
			}
			module = line.substr(tokStart, pos - tokStart);
		}
		if (module.empty()) {
			return UseLine{};
		}
		result.segments.push_back(line.substr(start, pos - start));
		result.modules.push_back(module);
		while (pos < line.size() && isSpace(line[pos])) {
			pos++;
		}
	}
	std::string rest = line.substr(pos);
	if (result.segments.empty() || (!rest.empty() && rest.compare(0, 2, "//") != 0)) {
		return UseLine{};
	}
	while (!rest.empty() && isSpace(rest.back())) {
		rest.pop_back();
	}
	result.trailer = rest;
	result.parsed = true;
	return result;
}

struct RealUse {
	std::string module;
	size_t line;
};

static std::vector<RealUse> collectRealUses(const IAstNode* root) {
	std::vector<RealUse> uses;
	for (size_t i = 0; i < root->childCount(); i++) {
		const IAstNode* child = root->child(i);
		if (child && child->type() == IAstNode::Type::USE_STATEMENT && child->line() > 0) {
			uses.push_back({static_cast<const AstNodeUse*>(child)->module(), child->line()});
		}
	}
	return uses;
}

struct SourceLine {
	std::string text;
	std::string eol;
	bool removed = false;
	std::vector<std::string> before;
	std::vector<std::string> after;
};

static std::vector<SourceLine> splitLines(const std::string& source) {
	std::vector<SourceLine> lines;
	size_t start = 0;
	while (start < source.size()) {
		size_t nl = source.find('\n', start);
		SourceLine line;
		if (nl == std::string::npos) {
			line.text = source.substr(start);
			start = source.size();
		} else {
			line.text = source.substr(start, nl - start);
			line.eol = "\n";
			start = nl + 1;
		}
		lines.push_back(line);
	}
	return lines;
}

static bool isBlank(const std::string& s) {
	for (char c : s) {
		if (!std::isspace(static_cast<unsigned char>(c))) {
			return false;
		}
	}
	return true;
}

static std::string formatUseStatement(const std::string& scope, const std::map<std::string, std::string>& originals) {
	auto it = originals.find(scope);
	std::string name = it != originals.end() ? it->second : scope;
	for (char c : name) {
		if (!isIdentChar(c) && c != '.') {
			return "use \"" + name + "\"";
		}
	}
	return "use " + name;
}

static size_t findUseInsertionLine(const IAstNode* root, const std::vector<SourceLine>& lines) {
	const IAstNode* first = nullptr;
	for (size_t i = 0; i < root->childCount(); i++) {
		const IAstNode* child = root->child(i);
		if (!child || child->line() == 0 || child->type() == IAstNode::Type::COMMENT) {
			continue;
		}
		first = child;
		break;
	}
	if (!first) {
		return lines.size() + 1;
	}
	auto trimmed = [&lines](size_t lineNo) {
		const std::string& text = lines[lineNo - 1].text;
		size_t start = text.find_first_not_of(" \t\r");
		size_t end = text.find_last_not_of(" \t\r");
		return start == std::string::npos ? std::string() : text.substr(start, end - start + 1);
	};
	size_t insertAt = first->line();
	size_t above = insertAt - 1;
	while (above >= 1 && above <= lines.size()) {
		std::string t = trimmed(above);
		if (t.rfind("//", 0) == 0) {
			insertAt = above--;
			continue;
		}
		if (t.size() >= 2 && t.compare(t.size() - 2, 2, "*/") == 0) {
			size_t open = above;
			while (open >= 1 && lines[open - 1].text.find("/*") == std::string::npos) {
				open--;
			}
			if (open < 1 || trimmed(open).rfind("/*", 0) != 0) {
				break;
			}
			insertAt = open;
			above = open - 1;
			continue;
		}
		break;
	}
	return insertAt;
}

struct UsesPlan {
	std::string result;
	std::vector<std::string> removed;
	std::vector<std::string> added;
	std::set<std::string> expectedModules;
};

static UsesPlan planUseStatements(const std::string& source, const IAstNode* root) {
	References refs;
	collectReferences(root, refs);

	std::set<std::string> usedScopes = refs.scopes;
	if (usedScopes.count("sb") && !codeMentionsScope(source, "sb")) {
		usedScopes.erase("sb");
	}

	std::set<std::string> importedNamespaces;
	collectImportNamespaces(root, importedNamespaces);
	for (const auto& ns : importedNamespaces) {
		usedScopes.erase(ns);
	}
	for (const auto& name : refs.definedTypes) {
		usedScopes.erase(name);
	}

	bool unresolved = false;
	for (const auto& name : refs.typeNames) {
		if (!refs.definedTypes.count(name)) {
			unresolved = true;
		}
	}
	for (const auto& name : refs.identifiers) {
		if (!refs.definedNames.count(name)) {
			unresolved = true;
		}
	}

	std::vector<RealUse> realUses = collectRealUses(root);
	std::map<std::string, std::string> scopeToOriginalImport;
	for (const auto& use : realUses) {
		scopeToOriginalImport[getPackageFromModuleName(use.module)] = use.module;
	}

	std::vector<SourceLine> lines = splitLines(source);
	std::map<size_t, UseLine> useLines;
	std::map<size_t, std::vector<std::string>> astModulesByLine;
	for (const auto& use : realUses) {
		astModulesByLine[use.line].push_back(use.module);
	}
	for (const auto& [lineNo, modules] : astModulesByLine) {
		UseLine parsed = lineNo <= lines.size() ? parseUseLine(lines[lineNo - 1].text) : UseLine{};
		if (parsed.parsed) {
			std::vector<std::string> a = parsed.modules;
			std::vector<std::string> b = modules;
			std::sort(a.begin(), a.end());
			std::sort(b.begin(), b.end());
			if (a != b) {
				parsed = UseLine{};
			}
		}
		useLines[lineNo] = parsed;
	}

	UsesPlan plan;
	std::set<std::string> present;
	std::vector<size_t> keptUseLines;
	size_t firstUseLine = 0;
	for (auto& [lineNo, parsed] : useLines) {
		if (firstUseLine == 0) {
			firstUseLine = lineNo;
		}
		if (!parsed.parsed) {
			for (const auto& module : astModulesByLine[lineNo]) {
				present.insert(getPackageFromModuleName(module));
				plan.expectedModules.insert(module);
			}
			keptUseLines.push_back(lineNo);
			continue;
		}
		std::vector<std::string> keptSegments;
		for (size_t s = 0; s < parsed.segments.size(); s++) {
			const std::string& module = parsed.modules[s];
			std::string package = getPackageFromModuleName(module);
			bool isFileImport = module.size() >= 3 && module.compare(module.size() - 3, 3, ".qd") == 0;
			bool keep = !present.count(package) && (unresolved || isFileImport || usedScopes.count(package));
			if (keep) {
				present.insert(package);
				plan.expectedModules.insert(module);
				keptSegments.push_back(parsed.segments[s]);
			} else {
				plan.removed.push_back(parsed.segments[s]);
			}
		}
		SourceLine& line = lines[lineNo - 1];
		if (keptSegments.empty()) {
			line.removed = true;
		} else {
			keptUseLines.push_back(lineNo);
			if (keptSegments.size() != parsed.segments.size()) {
				std::string rebuilt = parsed.indent;
				for (size_t s = 0; s < keptSegments.size(); s++) {
					rebuilt += (s > 0 ? " " : "") + keptSegments[s];
				}
				if (!parsed.trailer.empty()) {
					rebuilt += " " + parsed.trailer;
				}
				line.text = rebuilt;
			}
		}
	}

	std::vector<std::string> toAdd;
	for (const auto& scope : usedScopes) {
		if (!present.count(scope)) {
			toAdd.push_back(scope);
		}
	}
	std::sort(toAdd.begin(), toAdd.end());

	if (!toAdd.empty()) {
		std::vector<std::string> statements;
		for (const auto& scope : toAdd) {
			statements.push_back(formatUseStatement(scope, scopeToOriginalImport));
			plan.expectedModules.insert(scope);
			plan.added.push_back(statements.back());
		}
		if (!keptUseLines.empty()) {
			for (size_t a = 0; a < toAdd.size(); a++) {
				size_t anchor = 0;
				for (size_t lineNo : keptUseLines) {
					const UseLine& parsed = useLines[lineNo];
					if (parsed.parsed && parsed.modules.size() == 1 &&
							getPackageFromModuleName(parsed.modules[0]) > toAdd[a]) {
						anchor = lineNo;
						break;
					}
				}
				if (anchor != 0) {
					lines[anchor - 1].before.push_back(statements[a]);
				} else {
					lines[keptUseLines.back() - 1].after.push_back(statements[a]);
				}
			}
		} else if (firstUseLine != 0) {
			lines[firstUseLine - 1].before = statements;
		} else {
			size_t insertAt = findUseInsertionLine(root, lines);
			if (insertAt > 1 && insertAt - 2 < lines.size() && !isBlank(lines[insertAt - 2].text)) {
				statements.insert(statements.begin(), "");
			}
			if (insertAt <= lines.size()) {
				statements.push_back("");
				lines[insertAt - 1].before = statements;
			} else {
				if (!lines.empty() && lines.back().eol.empty()) {
					lines.back().eol = "\n";
				}
				SourceLine tail;
				tail.eol = "\n";
				tail.removed = true;
				tail.before = statements;
				lines.push_back(tail);
			}
		}
	}

	for (size_t i = 0; i < lines.size(); i++) {
		if (!lines[i].removed || !lines[i].before.empty()) {
			continue;
		}
		size_t end = i;
		bool insertions = false;
		while (end < lines.size() && lines[end].removed) {
			insertions = insertions || !lines[end].before.empty() || !lines[end].after.empty();
			end++;
		}
		bool blankAbove = i == 0 || (!lines[i - 1].removed && isBlank(lines[i - 1].text));
		if (!insertions && blankAbove && end < lines.size() && isBlank(lines[end].text) && lines[end].before.empty()) {
			lines[end].removed = true;
		}
		i = end;
	}

	std::string out;
	for (const auto& line : lines) {
		for (const auto& s : line.before) {
			out += s + "\n";
		}
		if (!line.removed) {
			out += line.text + line.eol;
		}
		for (const auto& s : line.after) {
			out += s + "\n";
		}
	}
	plan.result = out;
	return plan;
}

bool processFile(const std::string& filename, const UsesOptions& opts, bool& needsChanges) {
	try {
		// Read source file
		std::string source = qdcli::readFile(filename);

		// Validate UTF-8 encoding
		if (!qdcli::isValidUtf8(source)) {
			std::cerr << "quaduses: " << filename << ": invalid UTF-8 encoding or binary file\n";
			return false;
		}

		// Parse to get AST
		Ast ast;
		IAstNode* root = ast.generate(source.c_str(), false, filename.c_str());

		if (!root || ast.hasErrors()) {
			std::cerr << "quaduses: " << filename << ": failed to parse (contains errors)\n";
			return false;
		}

		UsesPlan plan = planUseStatements(source, root);
		bool changed = plan.result != source;

		if (changed) {
			Ast check;
			IAstNode* checkRoot = check.generate(plan.result.c_str(), false, filename.c_str());
			std::set<std::string> resultModules;
			if (checkRoot && !check.hasErrors()) {
				for (const auto& use : collectRealUses(checkRoot)) {
					resultModules.insert(use.module);
				}
			}
			if (!checkRoot || check.hasErrors() || resultModules != plan.expectedModules) {
				std::cerr << "quaduses: " << filename << ": rewriting the use statements would not produce valid "
						  << "source; leaving the file unchanged\n";
				return false;
			}
		}

		if (opts.check) {
			if (changed) {
				std::cout << filename << ": needs updating\n";
				needsChanges = true;
			}
			return true;
		} else if (opts.dryRun) {
			if (changed) {
				std::cout << filename << ":\n";
				for (const auto& u : plan.removed) {
					std::cout << "  - " << u << "\n";
				}
				for (const auto& u : plan.added) {
					std::cout << "  + " << u << "\n";
				}
			} else {
				std::cout << filename << ": no changes needed\n";
			}
			return true;
		} else if (opts.inPlace) {
			if (changed) {
				qdcli::writeFile(filename, plan.result);
				std::cout << filename << ": updated\n";
			} else {
				std::cout << filename << ": no changes needed\n";
			}
			return true;
		} else {
			std::cout << plan.result;
			return true;
		}
	} catch (const std::exception& e) {
		std::cerr << "quaduses: " << filename << ": " << e.what() << "\n";
		return false;
	}
}

int main(int argc, char* argv[]) {
	qdcli::BaseOptions base;
	UsesOptions opts;

	auto handler = [&opts](const char* arg, int& /*i*/, int /*ac*/, char* /*av*/[]) -> bool {
		if (strcmp(arg, "-w") == 0 || strcmp(arg, "--write") == 0) {
			opts.inPlace = true;
			return true;
		}
		if (strcmp(arg, "-c") == 0 || strcmp(arg, "--check") == 0) {
			opts.check = true;
			return true;
		}
		if (strcmp(arg, "-n") == 0 || strcmp(arg, "--dry-run") == 0) {
			opts.dryRun = true;
			return true;
		}
		return false;
	};

	if (!qdcli::parseArgs(argc, argv, base, "quaduses", handler)) {
		return 1;
	}

	if (base.help) {
		printHelp();
		return 0;
	}

	if (base.version) {
		qdcli::printVersion("quaduses");
		return 0;
	}

	if (qdcli::checkNoInputFiles(base, "quaduses")) {
		return 1;
	}

	// Check for conflicting options
	int modeCount = (opts.inPlace ? 1 : 0) + (opts.check ? 1 : 0) + (opts.dryRun ? 1 : 0);
	if (modeCount > 1) {
		std::cerr << "quaduses: options -w, -c, and -n are mutually exclusive\n";
		return 1;
	}

	// Collect all files from paths
	std::vector<std::string> allFiles;
	for (const auto& path : base.paths) {
		auto files = qdcli::collectFiles(path);
		allFiles.insert(allFiles.end(), files.begin(), files.end());
	}

	bool allSuccess = true;
	bool needsChanges = false;

	for (const auto& file : allFiles) {
		if (!processFile(file, opts, needsChanges)) {
			allSuccess = false;
		}
	}

	// In check mode, exit 1 if any files need changes
	if (opts.check && needsChanges) {
		return 1;
	}

	return allSuccess ? 0 : 1;
}

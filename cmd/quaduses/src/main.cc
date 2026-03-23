#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/file_utils.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_program.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct_construction.h>
#include <quadrate/qc/ast_node_struct_field.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/formatter.h>
#include <set>
#include <sstream>
#include <string>
#include <u8t/scanner.h>
#include <vector>

using namespace Qd;

struct UsesOptions {
	bool inPlace = false;
	bool check = false;
	bool dryRun = false;
};

void printHelp() {
	std::cout << "quaduses - Manage use statements automatically\n\n";
	std::cout << "Analyzes code and adds/removes use statements as needed.\n\n";
	std::cout << "Usage: quaduses [options] <file|directory>...\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n";
	std::cout << "  -w, --write      Update file in-place\n";
	std::cout << "  -c, --check      Check if files need changes (exit 1 if so)\n";
	std::cout << "  -n, --dry-run    Show what would change without modifying\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quaduses file.qd             Show updated file with use statements\n";
	std::cout << "  quaduses -w file.qd          Update use statements in-place\n";
	std::cout << "  quaduses -w src/             Update all .qd files in directory recursively\n";
	std::cout << "  quaduses -c src/             Check if any files need updating (for CI)\n";
	std::cout << "  quaduses -n file.qd          Show changes that would be made\n";
}

// Helper to extract module name from a scoped type like "math::Vec3" -> "math"
// Also handles generics like "Box<math::Vec3>" -> "math"
static void extractModulesFromType(const std::string& typeName, std::set<std::string>& scopes) {
	// Find all occurrences of "::" which indicate scoped types
	size_t pos = 0;
	while ((pos = typeName.find("::", pos)) != std::string::npos) {
		// Find the start of the module name (scan backwards for identifier start)
		size_t start = pos;
		while (start > 0 && (std::isalnum(typeName[start - 1]) || typeName[start - 1] == '_')) {
			start--;
		}
		if (start < pos) {
			std::string moduleName = typeName.substr(start, pos - start);
			scopes.insert(moduleName);
		}
		pos += 2; // Skip past "::"
	}
}

// Collect all scoped identifiers (namespace::function calls) from the AST
void collectScopedIdentifiers(const IAstNode* node, std::set<std::string>& scopes) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
		const AstNodeScopedIdentifier* scoped = static_cast<const AstNodeScopedIdentifier*>(node);
		scopes.insert(scoped->scope());
	}

	// Check struct field types for scoped types like "math::Vec3"
	if (node->type() == IAstNode::Type::STRUCT_FIELD) {
		const AstNodeStructField* field = static_cast<const AstNodeStructField*>(node);
		extractModulesFromType(field->typeName(), scopes);
	}

	// Check parameter types for scoped types
	if (node->type() == IAstNode::Type::VARIABLE_DECLARATION) {
		const AstNodeParameter* param = static_cast<const AstNodeParameter*>(node);
		extractModulesFromType(param->typeString(), scopes);
	}

	// Check struct construction for scoped struct names like "spline::ControlPoint { }"
	if (node->type() == IAstNode::Type::STRUCT_CONSTRUCTION) {
		const AstNodeStructConstruction* construction = static_cast<const AstNodeStructConstruction*>(node);
		extractModulesFromType(construction->structName(), scopes);
	}

	// Recursively visit all children
	for (size_t i = 0; i < node->childCount(); i++) {
		collectScopedIdentifiers(node->child(i), scopes);
	}
}

// Collect all namespaces defined by import statements (import "lib" as "namespace" { ... })
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

// Collect all existing use statements from the AST
void collectUseStatements(const IAstNode* node, std::set<std::string>& uses) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::USE_STATEMENT) {
		const AstNodeUse* useNode = static_cast<const AstNodeUse*>(node);
		uses.insert(useNode->module());
	}

	// Only check top-level nodes (direct children of program)
	for (size_t i = 0; i < node->childCount(); i++) {
		const IAstNode* child = node->child(i);
		if (child && child->type() == IAstNode::Type::USE_STATEMENT) {
			const AstNodeUse* useNode = static_cast<const AstNodeUse*>(child);
			uses.insert(useNode->module());
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

std::string generateWithUseStatements(const std::string& source, const std::set<std::string>& neededUses,
		const std::map<std::string, std::string>& scopeToOriginalImport) {
	std::istringstream input(source);
	std::ostringstream output;
	std::string line;
	bool inUseSection = false;
	bool useStatementsWritten = false;
	bool isFirstLine = true;
	bool hadShebang = false;

	// Helper to format a use statement (with quotes if needed)
	auto formatUseStatement = [&](const std::string& scope) -> std::string {
		// Check if we have the original import format
		auto it = scopeToOriginalImport.find(scope);
		if (it != scopeToOriginalImport.end()) {
			const std::string& original = it->second;
			// Wrap in quotes if the path contains:
			// - whitespace or special characters that would be invalid in the token stream
			// - forward slash (path separator) since it would be tokenized separately
			bool needsQuotes = false;
			for (char c : original) {
				if (std::isspace(static_cast<unsigned char>(c)) || c == '/' || c == '(' || c == ')' || c == '[' ||
						c == ']' || c == '{' || c == '}' || c == '<' || c == '>' || c == ',' || c == ';' || c == ':' ||
						c == '!' || c == '?' || c == '*' || c == '&' || c == '|' || c == '^' || c == '%' || c == '@' ||
						c == '#' || c == '$' || c == '`' || c == '~' || c == '\\') {
					needsQuotes = true;
					break;
				}
			}
			if (needsQuotes) {
				return "\"" + original + "\"";
			}
			return original;
		}
		// No original format found, use scope as-is
		return scope;
	};

	// Helper to check if line starts with "use " (after trimming)
	auto isUseLine = [](const std::string& l) {
		size_t start = 0;
		while (start < l.length() && std::isspace(static_cast<unsigned char>(l[start]))) {
			start++;
		}
		std::string trimmed = l.substr(start);
		return trimmed.rfind("use ", 0) == 0;
	};

	// Helper to check if line is empty or only whitespace
	auto isEmptyLine = [](const std::string& l) {
		for (char c : l) {
			if (!std::isspace(static_cast<unsigned char>(c))) {
				return false;
			}
		}
		return true;
	};

	// Helper to check if line is a comment
	auto isCommentLine = [](const std::string& l) {
		size_t start = 0;
		while (start < l.length() && std::isspace(static_cast<unsigned char>(l[start]))) {
			start++;
		}
		if (start + 1 < l.length()) {
			return l[start] == '/' && (l[start + 1] == '/' || l[start + 1] == '*');
		}
		return false;
	};

	// Helper to check if line is a shebang
	auto isShebangLine = [](const std::string& l) {
		size_t start = 0;
		while (start < l.length() && std::isspace(static_cast<unsigned char>(l[start]))) {
			start++;
		}
		if (start + 1 < l.length()) {
			return l[start] == '#' && l[start + 1] == '!';
		}
		return false;
	};

	while (std::getline(input, line)) {
		// Handle shebang - must be first line
		if (isFirstLine && isShebangLine(line)) {
			output << line << '\n';
			hadShebang = true;
			isFirstLine = false;
			continue;
		}
		isFirstLine = false;
		// Check if we're entering or in the use statement section
		if (isUseLine(line)) {
			inUseSection = true;
			// Skip old use statements, we'll write new ones
			continue;
		}

		// If we were in use section and hit a non-use, non-empty line, write use statements
		if (inUseSection && !isEmptyLine(line) && !useStatementsWritten) {
			// Add blank line after shebang before use statements
			if (hadShebang) {
				output << '\n';
			}
			// Write all needed use statements, sorted
			std::vector<std::string> sortedUses(neededUses.begin(), neededUses.end());
			std::sort(sortedUses.begin(), sortedUses.end());

			for (const auto& use : sortedUses) {
				output << "use " << formatUseStatement(use) << '\n';
			}

			useStatementsWritten = true;
			inUseSection = false;
		}

		// If we hit a non-comment, non-empty line and haven't written use statements yet, write them
		if (!useStatementsWritten && !isUseLine(line) && !isEmptyLine(line) && !isCommentLine(line)) {
			// Add blank line after shebang before use statements
			if (hadShebang) {
				output << '\n';
			}
			// Write all needed use statements, sorted
			std::vector<std::string> sortedUses(neededUses.begin(), neededUses.end());
			std::sort(sortedUses.begin(), sortedUses.end());

			for (const auto& use : sortedUses) {
				output << "use " << formatUseStatement(use) << '\n';
			}

			useStatementsWritten = true;
		}

		// Write the current line (preserve all blank lines as-is)
		output << line << '\n';
	}

	// If we never wrote use statements (file had only use statements or was empty)
	if (!useStatementsWritten && !neededUses.empty()) {
		// Add blank line after shebang before use statements
		if (hadShebang) {
			output << '\n';
		}
		std::vector<std::string> sortedUses(neededUses.begin(), neededUses.end());
		std::sort(sortedUses.begin(), sortedUses.end());

		for (const auto& use : sortedUses) {
			output << "use " << formatUseStatement(use) << '\n';
		}
	}

	return output.str();
}

// Extract use statements from source
std::set<std::string> extractUseStatements(const std::string& source) {
	std::set<std::string> uses;
	std::istringstream stream(source);
	std::string line;
	while (std::getline(stream, line)) {
		// Trim leading whitespace
		size_t start = 0;
		while (start < line.length() && std::isspace(static_cast<unsigned char>(line[start]))) {
			start++;
		}
		std::string trimmed = line.substr(start);
		if (trimmed.rfind("use ", 0) == 0) {
			uses.insert(trimmed);
		}
	}
	return uses;
}

// Show what use statements would change
void printUseDiff(const std::string& filename, const std::string& original, const std::string& modified) {
	std::set<std::string> origUses = extractUseStatements(original);
	std::set<std::string> modUses = extractUseStatements(modified);

	// Find removed uses
	std::vector<std::string> removed;
	for (const auto& u : origUses) {
		if (modUses.find(u) == modUses.end()) {
			removed.push_back(u);
		}
	}

	// Find added uses
	std::vector<std::string> added;
	for (const auto& u : modUses) {
		if (origUses.find(u) == origUses.end()) {
			added.push_back(u);
		}
	}

	if (removed.empty() && added.empty()) {
		std::cout << filename << ": no changes to use statements\n";
		return;
	}

	std::cout << filename << ":\n";
	for (const auto& u : removed) {
		std::cout << "  - " << u << "\n";
	}
	for (const auto& u : added) {
		std::cout << "  + " << u << "\n";
	}
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

		// Collect all scoped identifiers (namespaces used in code)
		std::set<std::string> usedScopes;
		collectScopedIdentifiers(root, usedScopes);

		// Collect namespaces already defined by import statements
		// These don't need use statements since the import block defines the namespace
		std::set<std::string> importedNamespaces;
		collectImportNamespaces(root, importedNamespaces);

		// Remove namespaces that are already defined by import blocks
		for (const auto& ns : importedNamespaces) {
			usedScopes.erase(ns);
		}

		// Collect original use statements to preserve file paths vs module names
		std::map<std::string, std::string> scopeToOriginalImport;
		std::set<std::string> explicitFileImports; // Track file imports that should always be preserved
		std::function<void(IAstNode*)> collectOriginalUses = [&](IAstNode* node) {
			if (!node) {
				return;
			}
			if (node->type() == IAstNode::Type::USE_STATEMENT) {
				AstNodeUse* useNode = static_cast<AstNodeUse*>(node);
				std::string moduleName = useNode->module();
				std::string packageName = getPackageFromModuleName(moduleName);
				scopeToOriginalImport[packageName] = moduleName;

				// If this is a file import (ends with .qd), always preserve it
				// This prevents confusing behavior where explicit file imports disappear
				bool isFileImport = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";
				if (isFileImport) {
					explicitFileImports.insert(packageName);
					usedScopes.insert(packageName); // Ensure it's included in output
				}
			}
			for (size_t i = 0; i < node->childCount(); i++) {
				collectOriginalUses(node->child(i));
			}
		};
		collectOriginalUses(root);

		// Generate new source with only needed use statements
		std::string result = generateWithUseStatements(source, usedScopes, scopeToOriginalImport);

		// Format the result to ensure proper formatting
		result = formatSource(result);

		// Check if file changed
		bool changed = (source != result);

		if (opts.check) {
			// Check mode: report if changes needed
			if (changed) {
				std::cout << filename << ": needs updating\n";
				needsChanges = true;
			}
			return true;
		} else if (opts.dryRun) {
			// Dry-run mode: show what use statements would change
			if (changed) {
				printUseDiff(filename, source, result);
			} else {
				std::cout << filename << ": no changes needed\n";
			}
			return true;
		} else if (opts.inPlace) {
			// In-place mode: write back to file
			if (changed) {
				qdcli::writeFile(filename, result);
				std::cout << filename << ": updated\n";
			} else {
				std::cout << filename << ": no changes needed\n";
			}
			return true;
		} else {
			// Stdout mode: write to stdout
			std::cout << result;
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

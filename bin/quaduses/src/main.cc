#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <qc/ast.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_use.h>
#include <qc/formatter.h>
#include <set>
#include <sstream>
#include <string>
#include <u8t/scanner.h>
#include <vector>

using namespace Qd;

struct Options {
	std::string file;
	bool inPlace = false;
	bool help = false;
	bool version = false;
};

void printHelp() {
	std::cout << "quaduses - Manage use statements automatically\n\n";
	std::cout << "Analyzes code and adds/removes use statements as needed.\n\n";
	std::cout << "Usage: quaduses [options] <file>\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n";
	std::cout << "  -w, --write      Update file in-place\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quaduses file.qd             Show updated file with use statements\n";
	std::cout << "  quaduses -w file.qd          Update use statements in-place\n";
}

void printVersion() {
	std::cout << "quaduses version 0.1.0\n";
}

bool parseArgs(int argc, char* argv[], Options& opts) {
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help") {
			opts.help = true;
			return true;
		} else if (arg == "-v" || arg == "--version") {
			opts.version = true;
			return true;
		} else if (arg == "-w" || arg == "--write") {
			opts.inPlace = true;
		} else if (arg[0] == '-') {
			std::cerr << "quaduses: unknown option: " << arg << "\n";
			std::cerr << "Try 'quaduses --help' for more information.\n";
			return false;
		} else {
			opts.file = arg;
		}
	}

	if (opts.file.empty() && !opts.help && !opts.version) {
		std::cerr << "quaduses: no input file\n";
		std::cerr << "Try 'quaduses --help' for more information.\n";
		return false;
	}

	return true;
}

std::string readFile(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("No such file or directory");
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

void writeFile(const std::string& filename, const std::string& content) {
	std::ofstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("Cannot write to file");
	}
	file << content;
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

	// Recursively visit all children
	for (size_t i = 0; i < node->childCount(); i++) {
		collectScopedIdentifiers(node->child(i), scopes);
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

// Generate new source with updated use statements
// Validate UTF-8 encoding
bool isValidUtf8(const std::string& source) {
	size_t i = 0;
	while (i < source.length()) {
		unsigned char c = static_cast<unsigned char>(source[i]);

		// Check for null bytes (binary file indicator)
		if (c == 0) {
			return false;
		}

		// ASCII (0xxxxxxx)
		if ((c & 0x80) == 0) {
			i++;
			continue;
		}

		// Determine number of continuation bytes
		size_t cont_bytes = 0;
		if ((c & 0xE0) == 0xC0) cont_bytes = 1;      // 110xxxxx
		else if ((c & 0xF0) == 0xE0) cont_bytes = 2; // 1110xxxx
		else if ((c & 0xF8) == 0xF0) cont_bytes = 3; // 11110xxx
		else return false; // Invalid UTF-8 start byte

		// Check we have enough bytes
		if (i + cont_bytes >= source.length()) {
			return false;
		}

		// Validate continuation bytes (10xxxxxx)
		for (size_t j = 1; j <= cont_bytes; j++) {
			unsigned char next = static_cast<unsigned char>(source[i + j]);
			if ((next & 0xC0) != 0x80) {
				return false;
			}
		}

		i += cont_bytes + 1;
	}
	return true;
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

	while (std::getline(input, line)) {
		// Check if we're entering or in the use statement section
		if (isUseLine(line)) {
			inUseSection = true;
			// Skip old use statements, we'll write new ones
			continue;
		}

		// If we were in use section and hit a non-use, non-empty line, write use statements
		if (inUseSection && !isEmptyLine(line) && !useStatementsWritten) {
			// Write all needed use statements, sorted
			std::vector<std::string> sortedUses(neededUses.begin(), neededUses.end());
			std::sort(sortedUses.begin(), sortedUses.end());

			for (const auto& use : sortedUses) {
				output << "use " << formatUseStatement(use) << '\n';
			}

			// Add blank line after use statements if there are any
			if (!sortedUses.empty()) {
				output << '\n';
			}

			useStatementsWritten = true;
			inUseSection = false;
		}

		// If we hit a non-comment, non-empty line and haven't written use statements yet, write them
		if (!useStatementsWritten && !isUseLine(line) && !isEmptyLine(line) && !isCommentLine(line)) {
			// Write all needed use statements, sorted
			std::vector<std::string> sortedUses(neededUses.begin(), neededUses.end());
			std::sort(sortedUses.begin(), sortedUses.end());

			for (const auto& use : sortedUses) {
				output << "use " << formatUseStatement(use) << '\n';
			}

			// Add blank line after use statements if there are any
			if (!sortedUses.empty()) {
				output << '\n';
			}

			useStatementsWritten = true;
		}

		// Write the current line
		output << line << '\n';
	}

	// If we never wrote use statements (file had only use statements or was empty)
	if (!useStatementsWritten && !neededUses.empty()) {
		std::vector<std::string> sortedUses(neededUses.begin(), neededUses.end());
		std::sort(sortedUses.begin(), sortedUses.end());

		for (const auto& use : sortedUses) {
			output << "use " << formatUseStatement(use) << '\n';
		}
	}

	return output.str();
}

bool processFile(const std::string& filename, const Options& opts) {
	try {
		// Read source file
		std::string source = readFile(filename);

		// Validate UTF-8 encoding
		if (!isValidUtf8(source)) {
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

		// Collect original use statements to preserve file paths vs module names
		std::map<std::string, std::string> scopeToOriginalImport;
		std::set<std::string> explicitFileImports; // Track file imports that should always be preserved
		std::function<void(IAstNode*)> collectOriginalUses = [&](IAstNode* node) {
			if (!node)
				return;
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

		if (opts.inPlace) {
			// In-place mode: write back to file
			writeFile(filename, result);
			std::cout << filename << ": updated use statements\n";
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
	Options opts;

	if (!parseArgs(argc, argv, opts)) {
		return 1;
	}

	if (opts.help) {
		printHelp();
		return 0;
	}

	if (opts.version) {
		printVersion();
		return 0;
	}

	return processFile(opts.file, opts) ? 0 : 1;
}

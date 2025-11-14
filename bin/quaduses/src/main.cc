#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
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
	std::cout << "Usage: quaduses [options] <file>\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n";
	std::cout << "  -w, --write      Write output in-place (overwrite source file)\n";
	std::cout << "\n";
	std::cout << "By default, output is written to stdout.\n";
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
		throw std::runtime_error("Cannot open file: " + filename);
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

void writeFile(const std::string& filename, const std::string& content) {
	std::ofstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("Cannot write to file: " + filename);
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
std::string generateWithUseStatements(const std::string& source, const std::set<std::string>& neededUses) {
	std::istringstream input(source);
	std::ostringstream output;
	std::string line;
	bool inUseSection = false;
	bool useStatementsWritten = false;

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
				output << "use " << use << '\n';
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
				output << "use " << use << '\n';
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
			output << "use " << use << '\n';
		}
	}

	return output.str();
}

bool processFile(const std::string& filename, const Options& opts) {
	try {
		// Read source file
		std::string source = readFile(filename);

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

		// Generate new source with only needed use statements
		std::string result = generateWithUseStatements(source, usedScopes);

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

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_local.h>
#include <qc/colors.h>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace Qd;

struct Options {
	std::vector<std::string> files;
	bool help = false;
	bool version = false;
	bool noUnusedFunctions = false;
	bool noUnusedVariables = false;
};

struct LintIssue {
	std::string filename;
	size_t line;
	size_t column;
	std::string message;
	std::string level; // "warning", "error"
};

void printHelp() {
	std::cout << "quadlint - Quadrate code linter\n\n";
	std::cout << "Checks Quadrate source files for code quality issues.\n\n";
	std::cout << "Usage: quadlint [options] <file>...\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help                Show this help message\n";
	std::cout << "  -v, --version             Show version information\n";
	std::cout << "  --no-unused-functions     Disable unused function warnings\n";
	std::cout << "  --no-unused-variables     Disable unused variable warnings\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quadlint file.qd          Lint a single file\n";
	std::cout << "  quadlint *.qd             Lint multiple files\n";
}

void printVersion() {
	std::cout << "0.1.0\n";
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
		} else if (arg == "--no-unused-functions") {
			opts.noUnusedFunctions = true;
		} else if (arg == "--no-unused-variables") {
			opts.noUnusedVariables = true;
		} else if (arg[0] == '-') {
			std::cerr << "quadlint: unknown option: " << arg << "\n";
			std::cerr << "Try 'quadlint --help' for more information.\n";
			return false;
		} else {
			opts.files.push_back(arg);
		}
	}

	if (opts.files.empty() && !opts.help && !opts.version) {
		std::cerr << "quadlint: no input files\n";
		std::cerr << "Try 'quadlint --help' for more information.\n";
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

// Recursively collect all function definitions
void collectFunctions(IAstNode* node, std::unordered_map<std::string, IAstNode*>& functions) {
	if (!node) return;

	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
		AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
		functions[func->name()] = node;
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectFunctions(node->child(i), functions);
	}
}

// Recursively collect all function calls
void collectFunctionCalls(IAstNode* node, std::unordered_set<std::string>& calls) {
	if (!node) return;

	if (node->type() == IAstNode::Type::IDENTIFIER) {
		AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
		calls.insert(ident->name());
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectFunctionCalls(node->child(i), calls);
	}
}

// Recursively collect all local variable bindings
void collectLocalBindings(IAstNode* node, std::unordered_map<std::string, IAstNode*>& locals) {
	if (!node) return;

	if (node->type() == IAstNode::Type::LOCAL) {
		AstNodeLocal* local = static_cast<AstNodeLocal*>(node);
		locals[local->name()] = node;
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectLocalBindings(node->child(i), locals);
	}
}

// Recursively collect all variable usages
void collectVariableUsages(IAstNode* node, std::unordered_set<std::string>& usages) {
	if (!node) return;

	if (node->type() == IAstNode::Type::IDENTIFIER) {
		AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
		usages.insert(ident->name());
	}

	// Don't traverse into LOCAL nodes themselves (the binding), only their children
	if (node->type() != IAstNode::Type::LOCAL) {
		for (size_t i = 0; i < node->childCount(); i++) {
			collectVariableUsages(node->child(i), usages);
		}
	}
}

std::vector<LintIssue> lintFile(const std::string& filename, const Options& opts) {
	std::vector<LintIssue> issues;

	try {
		// Read and parse file
		std::string source = readFile(filename);
		Ast ast;
		IAstNode* root = ast.generate(source.c_str(), false, filename.c_str());

		if (!root || ast.hasErrors()) {
			// Don't lint files with parse errors
			return issues;
		}

		// Check for unused functions
		if (!opts.noUnusedFunctions) {
			std::unordered_map<std::string, IAstNode*> functions;
			std::unordered_set<std::string> calls;

			collectFunctions(root, functions);
			collectFunctionCalls(root, calls);

			for (const auto& pair : functions) {
				const std::string& funcName = pair.first;
				IAstNode* funcNode = pair.second;

				// Skip main function
				if (funcName == "main") continue;

				// Check if function is called
				if (calls.find(funcName) == calls.end()) {
					LintIssue issue;
					issue.filename = filename;
					issue.line = funcNode->line();
					issue.column = funcNode->column();
					issue.message = "Unused function '" + funcName + "'";
					issue.level = "warning";
					issues.push_back(issue);
				}
			}
		}

		// Check for unused local variables
		if (!opts.noUnusedVariables) {
			std::unordered_map<std::string, IAstNode*> locals;
			std::unordered_set<std::string> usages;

			collectLocalBindings(root, locals);
			collectVariableUsages(root, usages);

			for (const auto& pair : locals) {
				const std::string& varName = pair.first;
				IAstNode* varNode = pair.second;

				// Check if variable is used
				if (usages.find(varName) == usages.end()) {
					LintIssue issue;
					issue.filename = filename;
					issue.line = varNode->line();
					issue.column = varNode->column();
					issue.message = "Unused local variable '" + varName + "'";
					issue.level = "warning";
					issues.push_back(issue);
				}
			}
		}

	} catch (const std::exception& e) {
		// Silently skip files that can't be read/parsed
	}

	return issues;
}

void printIssues(const std::vector<LintIssue>& issues) {
	for (const auto& issue : issues) {
		std::cout << Colors::bold() << issue.filename << ":" << issue.line << ":" << issue.column
		          << ":" << Colors::reset() << " ";

		if (issue.level == "warning") {
			std::cout << Colors::bold() << Colors::magenta() << "warning:" << Colors::reset();
		} else {
			std::cout << Colors::bold() << Colors::red() << "error:" << Colors::reset();
		}

		std::cout << " " << Colors::bold() << issue.message << Colors::reset() << "\n";
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

	size_t totalIssues = 0;
	for (const auto& file : opts.files) {
		std::vector<LintIssue> issues = lintFile(file, opts);
		printIssues(issues);
		totalIssues += issues.size();
	}

	if (totalIssues > 0) {
		std::cerr << "\n" << totalIssues << " issue" << (totalIssues == 1 ? "" : "s")
		          << " found\n";
		return 1;
	}

	return 0;
}

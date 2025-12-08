#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_struct.h>
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
	bool noDeadCode = false;
	bool noDeepNesting = false;
	bool noMissingDefer = false;
	int maxNestingDepth = 4;
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
	std::cout << "  --no-dead-code            Disable dead code warnings\n";
	std::cout << "  --no-deep-nesting         Disable deep nesting warnings\n";
	std::cout << "  --no-missing-defer        Disable missing defer warnings\n";
	std::cout << "  --max-nesting <N>         Maximum nesting depth (default: 4)\n";
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
		} else if (arg == "--no-dead-code") {
			opts.noDeadCode = true;
		} else if (arg == "--no-deep-nesting") {
			opts.noDeepNesting = true;
		} else if (arg == "--no-missing-defer") {
			opts.noMissingDefer = true;
		} else if (arg == "--max-nesting") {
			if (i + 1 >= argc) {
				std::cerr << "quadlint: --max-nesting requires an argument\n";
				return false;
			}
			opts.maxNestingDepth = std::atoi(argv[++i]);
			if (opts.maxNestingDepth < 1) {
				std::cerr << "quadlint: --max-nesting must be at least 1\n";
				return false;
			}
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
	if (!node) {
		return;
	}

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
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::IDENTIFIER) {
		AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
		calls.insert(ident->name());
	}

	// Also track function pointer references (&func)
	if (node->type() == IAstNode::Type::FUNCTION_POINTER_REFERENCE) {
		AstNodeFunctionPointerReference* funcPtr = static_cast<AstNodeFunctionPointerReference*>(node);
		calls.insert(funcPtr->functionName());
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectFunctionCalls(node->child(i), calls);
	}
}

// Recursively collect all local variable bindings
void collectLocalBindings(IAstNode* node, std::unordered_map<std::string, IAstNode*>& locals) {
	if (!node) {
		return;
	}

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
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::IDENTIFIER) {
		AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
		usages.insert(ident->name());
	}

	// Also check field access nodes (v@x uses variable v)
	if (node->type() == IAstNode::Type::FIELD_ACCESS) {
		AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(node);
		usages.insert(fieldAccess->varName());
	}

	// Don't traverse into LOCAL nodes themselves (the binding), only their children
	if (node->type() != IAstNode::Type::LOCAL) {
		for (size_t i = 0; i < node->childCount(); i++) {
			collectVariableUsages(node->child(i), usages);
		}
	}
}

// Detect dead code after return/break/continue
void detectDeadCode(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	// Check if this is a block with statements
	if (node->type() == IAstNode::Type::BLOCK) {
		bool foundTerminator = false;
		IAstNode* terminator = nullptr;

		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}

			// Check if we already found a terminator
			if (foundTerminator) {
				LintIssue issue;
				issue.filename = filename;
				issue.line = child->line();
				issue.column = child->column();
				issue.message = "Unreachable code after " +
								std::string(terminator->type() == IAstNode::Type::RETURN_STATEMENT	? "return"
											: terminator->type() == IAstNode::Type::BREAK_STATEMENT ? "break"
																									: "continue");
				issue.level = "warning";
				issues.push_back(issue);
				break; // Only report first unreachable statement
			}

			// Check if this child is a terminator
			if (child->type() == IAstNode::Type::RETURN_STATEMENT || child->type() == IAstNode::Type::BREAK_STATEMENT ||
					child->type() == IAstNode::Type::CONTINUE_STATEMENT) {
				foundTerminator = true;
				terminator = child;
			}
		}
	}

	// Recursively check child nodes
	for (size_t i = 0; i < node->childCount(); i++) {
		detectDeadCode(node->child(i), filename, issues);
	}
}

// Detect deep nesting
void detectDeepNesting(IAstNode* node, const std::string& filename, int maxDepth, std::vector<LintIssue>& issues,
		int currentDepth = 0) {
	if (!node) {
		return;
	}

	// Check if this node increases nesting depth
	if (node->type() == IAstNode::Type::IF_STATEMENT || node->type() == IAstNode::Type::FOR_STATEMENT ||
			node->type() == IAstNode::Type::LOOP_STATEMENT || node->type() == IAstNode::Type::SWITCH_STATEMENT) {
		currentDepth++;

		if (currentDepth > maxDepth) {
			LintIssue issue;
			issue.filename = filename;
			issue.line = node->line();
			issue.column = node->column();
			issue.message = "Deep nesting detected (depth: " + std::to_string(currentDepth) +
							", max: " + std::to_string(maxDepth) + ")";
			issue.level = "warning";
			issues.push_back(issue);
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectDeepNesting(node->child(i), filename, maxDepth, issues, currentDepth);
	}
}

// Detect missing defer for struct allocations
void detectMissingDefer(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	// Look for struct constructor calls (integer followed by struct name)
	// This is a simplified check - full implementation would need more context
	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
		bool hasStructAlloc = false;
		bool hasDefer = false;

		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}

			// Check for defer blocks
			if (child->type() == IAstNode::Type::DEFER_STATEMENT) {
				hasDefer = true;
			}

			// Check for potential struct construction (simplified)
			// A more complete implementation would track identifiers and struct types
			if (child->type() == IAstNode::Type::IDENTIFIER) {
				// This is a heuristic - struct names typically start with uppercase
				AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(child);
				std::string name = ident->name();
				if (!name.empty() && isupper(static_cast<unsigned char>(name[0]))) {
					hasStructAlloc = true;
				}
			}
		}

		// If we found a struct allocation but no defer, warn
		if (hasStructAlloc && !hasDefer) {
			LintIssue issue;
			issue.filename = filename;
			issue.line = node->line();
			issue.column = node->column();
			issue.message = "Potential struct allocation without defer - consider using defer for cleanup";
			issue.level = "warning";
			issues.push_back(issue);
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectMissingDefer(node->child(i), filename, issues);
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
				if (funcName == "main") {
					continue;
				}

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

		// Check for dead code
		if (!opts.noDeadCode) {
			detectDeadCode(root, filename, issues);
		}

		// Check for deep nesting
		if (!opts.noDeepNesting) {
			detectDeepNesting(root, filename, opts.maxNestingDepth, issues);
		}

		// Check for missing defer
		if (!opts.noMissingDefer) {
			detectMissingDefer(root, filename, issues);
		}

	} catch (const std::exception& e) {
		// Silently skip files that can't be read/parsed
	}

	return issues;
}

void printIssues(const std::vector<LintIssue>& issues) {
	for (const auto& issue : issues) {
		std::cout << Colors::bold() << issue.filename << ":" << issue.line << ":" << issue.column << ":"
				  << Colors::reset() << " ";

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
		std::cerr << "\n" << totalIssues << " issue" << (totalIssues == 1 ? "" : "s") << " found\n";
		return 1;
	}

	return 0;
}

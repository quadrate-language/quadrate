#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_struct.h>
#include <qc/colors.h>
#include <qdcli/file_utils.h>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "version.h"

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
	bool noShadowVariables = false;
	bool noEmptyBlocks = false;
	bool noConstantConditions = false;
	// Stricter checks (disabled by default, enable with flags)
	bool checkMagicNumbers = false;
	bool checkLongFunctions = false;
	bool checkNamingConventions = false;
	int maxNestingDepth = 4;
	int maxFunctionLines = 50;
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
	std::cout << "  --no-shadow-variables     Disable shadow variable warnings\n";
	std::cout << "  --no-empty-blocks         Disable empty block warnings\n";
	std::cout << "  --no-constant-conditions  Disable constant condition warnings\n";
	std::cout << "  --max-nesting <N>         Maximum nesting depth (default: 4)\n";
	std::cout << "\nStricter checks (disabled by default):\n";
	std::cout << "  --check-magic-numbers     Enable magic number detection\n";
	std::cout << "  --check-long-functions    Enable long function detection\n";
	std::cout << "  --check-naming            Enable naming convention checks\n";
	std::cout << "  --max-function-lines <N>  Maximum function lines (default: 50)\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quadlint file.qd          Lint a single file\n";
	std::cout << "  quadlint *.qd             Lint multiple files\n";
}

void printVersion() {
	std::cout << quadrate_version_string("quadlint") << "\n";
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
		} else if (arg == "--no-shadow-variables") {
			opts.noShadowVariables = true;
		} else if (arg == "--no-empty-blocks") {
			opts.noEmptyBlocks = true;
		} else if (arg == "--no-constant-conditions") {
			opts.noConstantConditions = true;
		} else if (arg == "--check-magic-numbers") {
			opts.checkMagicNumbers = true;
		} else if (arg == "--check-long-functions") {
			opts.checkLongFunctions = true;
		} else if (arg == "--check-naming") {
			opts.checkNamingConventions = true;
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
		} else if (arg == "--max-function-lines") {
			if (i + 1 >= argc) {
				std::cerr << "quadlint: --max-function-lines requires an argument\n";
				return false;
			}
			opts.maxFunctionLines = std::atoi(argv[++i]);
			if (opts.maxFunctionLines < 1) {
				std::cerr << "quadlint: --max-function-lines must be at least 1\n";
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

// Detect shadow variables (inner scope variable shadows outer scope)
void detectShadowVariables(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues,
		std::vector<std::unordered_set<std::string>>& scopeStack) {
	if (!node) {
		return;
	}

	// Enter new scope for functions and blocks
	bool newScope = false;
	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION || node->type() == IAstNode::Type::BLOCK) {
		scopeStack.emplace_back();
		newScope = true;
	}

	// Check for local variable declarations
	if (node->type() == IAstNode::Type::LOCAL) {
		AstNodeLocal* local = static_cast<AstNodeLocal*>(node);
		std::string varName = local->name();

		// Check if this variable shadows one in an outer scope
		for (size_t i = 0; i < scopeStack.size() - 1; i++) {
			if (scopeStack[i].find(varName) != scopeStack[i].end()) {
				LintIssue issue;
				issue.filename = filename;
				issue.line = node->line();
				issue.column = node->column();
				issue.message = "Variable '" + varName + "' shadows variable from outer scope";
				issue.level = "warning";
				issues.push_back(issue);
				break;
			}
		}

		// Add to current scope
		if (!scopeStack.empty()) {
			scopeStack.back().insert(varName);
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectShadowVariables(node->child(i), filename, issues, scopeStack);
	}

	// Exit scope
	if (newScope) {
		scopeStack.pop_back();
	}
}

// Count actual statements in a block (excluding comments/unknown nodes)
size_t countStatements(IAstNode* block) {
	size_t count = 0;
	for (size_t i = 0; i < block->childCount(); i++) {
		IAstNode* child = block->child(i);
		if (child && child->type() != IAstNode::Type::UNKNOWN && child->type() != IAstNode::Type::COMMENT) {
			count++;
		}
	}
	return count;
}

// Detect empty blocks (if/for/loop/switch with no statements)
void detectEmptyBlocks(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	// Check for control structures with empty bodies
	if (node->type() == IAstNode::Type::IF_STATEMENT || node->type() == IAstNode::Type::FOR_STATEMENT ||
			node->type() == IAstNode::Type::LOOP_STATEMENT) {
		// Find the block child
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (child && child->type() == IAstNode::Type::BLOCK && countStatements(child) == 0) {
				LintIssue issue;
				issue.filename = filename;
				issue.line = node->line();
				issue.column = node->column();
				std::string blockType = node->type() == IAstNode::Type::IF_STATEMENT	 ? "if"
										: node->type() == IAstNode::Type::FOR_STATEMENT	 ? "for"
										: node->type() == IAstNode::Type::LOOP_STATEMENT ? "loop"
																						 : "block";
				issue.message = "Empty '" + blockType + "' block";
				issue.level = "warning";
				issues.push_back(issue);
				break;
			}
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectEmptyBlocks(node->child(i), filename, issues);
	}
}

// Detect constant conditions (if with literal condition)
// In Quadrate's stack-based syntax, the condition comes BEFORE the if statement as a sibling
void detectConstantConditions(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	// Check blocks for patterns like: LITERAL IF_STATEMENT
	if (node->type() == IAstNode::Type::BLOCK) {
		for (size_t i = 1; i < node->childCount(); i++) {
			IAstNode* current = node->child(i);
			IAstNode* previous = node->child(i - 1);

			if (!current || !previous) {
				continue;
			}

			// Check for if statement preceded by a literal
			if (current->type() == IAstNode::Type::IF_STATEMENT && previous->type() == IAstNode::Type::LITERAL) {
				AstNodeLiteral* literal = static_cast<AstNodeLiteral*>(previous);
				if (literal->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
					std::string value = literal->value();
					bool isAlwaysTrue = (value != "0");
					LintIssue issue;
					issue.filename = filename;
					issue.line = previous->line();
					issue.column = previous->column();
					issue.message =
							"Constant condition: expression is always " + std::string(isAlwaysTrue ? "true" : "false");
					issue.level = "warning";
					issues.push_back(issue);
				}
			}
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectConstantConditions(node->child(i), filename, issues);
	}
}

// Detect magic numbers (numeric literals other than -1, 0, 1)
void detectMagicNumbers(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::LITERAL) {
		AstNodeLiteral* literal = static_cast<AstNodeLiteral*>(node);
		if (literal->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
			std::string value = literal->value();
			// Allow -1, 0, 1 as common values
			if (value != "0" && value != "1" && value != "-1") {
				// Check if inside a constant declaration (which is acceptable)
				IAstNode* parent = node->parent();
				bool inConstant = false;
				while (parent) {
					if (parent->type() == IAstNode::Type::CONSTANT_DECLARATION) {
						inConstant = true;
						break;
					}
					parent = parent->parent();
				}

				if (!inConstant) {
					LintIssue issue;
					issue.filename = filename;
					issue.line = node->line();
					issue.column = node->column();
					issue.message = "Magic number '" + value + "' - consider using a named constant";
					issue.level = "warning";
					issues.push_back(issue);
				}
			}
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectMagicNumbers(node->child(i), filename, issues);
	}
}

// Detect long functions
void detectLongFunctions(IAstNode* node, const std::string& filename, int maxLines, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
		AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);

		// Find the end line by traversing all children
		size_t startLine = func->line();
		size_t endLine = startLine;

		std::function<void(Qd::IAstNode*)> findMaxLine = [&](Qd::IAstNode* n) {
			if (!n) {
				return;
			}
			if (n->line() > endLine) {
				endLine = n->line();
			}
			for (size_t i = 0; i < n->childCount(); i++) {
				findMaxLine(n->child(i));
			}
		};
		findMaxLine(func);

		size_t functionLines = endLine - startLine + 1;
		if (functionLines > static_cast<size_t>(maxLines)) {
			LintIssue issue;
			issue.filename = filename;
			issue.line = func->line();
			issue.column = func->column();
			issue.message = "Function '" + func->name() + "' is too long (" + std::to_string(functionLines) +
							" lines, max: " + std::to_string(maxLines) + ")";
			issue.level = "warning";
			issues.push_back(issue);
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectLongFunctions(node->child(i), filename, maxLines, issues);
	}
}

// Helper to check if string is snake_case
bool isSnakeCase(const std::string& name) {
	if (name.empty()) {
		return true;
	}
	for (size_t i = 0; i < name.length(); i++) {
		char c = name[i];
		if (!(islower(static_cast<unsigned char>(c)) || isdigit(static_cast<unsigned char>(c)) || c == '_')) {
			return false;
		}
	}
	// Should not start or end with underscore
	if (name[0] == '_' || name[name.length() - 1] == '_') {
		return false;
	}
	return true;
}

// Helper to check if string is PascalCase
bool isPascalCase(const std::string& name) {
	if (name.empty()) {
		return true;
	}
	// First character should be uppercase
	if (!isupper(static_cast<unsigned char>(name[0]))) {
		return false;
	}
	// Rest should be alphanumeric (no underscores)
	for (size_t i = 1; i < name.length(); i++) {
		char c = name[i];
		if (!isalnum(static_cast<unsigned char>(c))) {
			return false;
		}
	}
	return true;
}

// Detect naming convention violations
void detectNamingConventions(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	// Check function names (should be snake_case)
	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
		AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
		std::string name = func->name();
		if (!isSnakeCase(name)) {
			LintIssue issue;
			issue.filename = filename;
			issue.line = func->line();
			issue.column = func->column();
			issue.message = "Function '" + name + "' should use snake_case";
			issue.level = "warning";
			issues.push_back(issue);
		}
	}
	// Check struct names (should be PascalCase)
	else if (node->type() == IAstNode::Type::STRUCT_DECLARATION) {
		AstNodeStructDeclaration* structDecl = static_cast<AstNodeStructDeclaration*>(node);
		std::string name = structDecl->name();
		if (!isPascalCase(name)) {
			LintIssue issue;
			issue.filename = filename;
			issue.line = structDecl->line();
			issue.column = structDecl->column();
			issue.message = "Struct '" + name + "' should use PascalCase";
			issue.level = "warning";
			issues.push_back(issue);
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectNamingConventions(node->child(i), filename, issues);
	}
}

std::vector<LintIssue> lintFile(const std::string& filename, const Options& opts) {
	std::vector<LintIssue> issues;

	try {
		// Read and parse file
		std::string source = qdcli::readFile(filename);
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

				// Skip public functions (they may be used externally)
				AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(funcNode);
				if (func->isPublic()) {
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

		// Check for unused local variables (per-function scope)
		if (!opts.noUnusedVariables) {
			// Process each function separately to respect scoping
			for (size_t i = 0; i < root->childCount(); i++) {
				IAstNode* child = root->child(i);
				if (child && child->type() == IAstNode::Type::FUNCTION_DECLARATION) {
					std::unordered_map<std::string, IAstNode*> locals;
					std::unordered_set<std::string> usages;

					// Collect locals and usages only within this function
					collectLocalBindings(child, locals);
					collectVariableUsages(child, usages);

					for (const auto& pair : locals) {
						const std::string& varName = pair.first;
						IAstNode* varNode = pair.second;

						// Check if variable is used within this function
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

		// Check for shadow variables
		if (!opts.noShadowVariables) {
			std::vector<std::unordered_set<std::string>> scopeStack;
			detectShadowVariables(root, filename, issues, scopeStack);
		}

		// Check for empty blocks
		if (!opts.noEmptyBlocks) {
			detectEmptyBlocks(root, filename, issues);
		}

		// Check for constant conditions
		if (!opts.noConstantConditions) {
			detectConstantConditions(root, filename, issues);
		}

		// Check for magic numbers (opt-in)
		if (opts.checkMagicNumbers) {
			detectMagicNumbers(root, filename, issues);
		}

		// Check for long functions (opt-in)
		if (opts.checkLongFunctions) {
			detectLongFunctions(root, filename, opts.maxFunctionLines, issues);
		}

		// Check for naming conventions (opt-in)
		if (opts.checkNamingConventions) {
			detectNamingConventions(root, filename, issues);
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

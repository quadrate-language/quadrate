#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <jansson.h>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/file_utils.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/colors.h>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace Qd;

struct LintOptions {
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
	// Output options
	bool jsonOutput = false;
	bool quiet = false;
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
	std::cout << "Usage: quadlint [options] <file|directory>...\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help                Show this help message\n";
	std::cout << "  -v, --version             Show version information\n";
	std::cout << "  --json                    Output results in JSON format\n";
	std::cout << "  -q, --quiet               Only show summary (no individual issues)\n";
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
	std::cout << "\nExamples:\n";
	std::cout << "  quadlint file.qd          Lint a single file\n";
	std::cout << "  quadlint src/             Lint all .qd files in directory recursively\n";
	std::cout << "  quadlint --json file.qd   Output results in JSON format for IDEs\n";
	std::cout << "  quadlint -q *.qd          Only show summary\n";
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
		// Skip "_" which is the discard operator (intentionally unused)
		if (local->name() != "_") {
			locals[local->name()] = node;
		}
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectLocalBindings(node->child(i), locals);
	}
}

// Recursively collect all variable usages
// Takes the set of known local variable names to detect instruction nodes that shadow them
void collectVariableUsages(
		IAstNode* node, std::unordered_set<std::string>& usages, const std::unordered_set<std::string>& localNames) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::IDENTIFIER) {
		AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
		usages.insert(ident->name());
	}

	// Also check field access nodes (v <<x uses variable v)
	if (node->type() == IAstNode::Type::FIELD_ACCESS) {
		AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(node);
		usages.insert(fieldAccess->varName());
	}

	// Check instruction nodes - if the name matches a local variable, it's a variable reference
	// (variable names can shadow builtin instruction names like 'inc', 'dec', 'add', etc.)
	if (node->type() == IAstNode::Type::INSTRUCTION) {
		AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(node);
		const std::string& name = instr->name();
		if (localNames.find(name) != localNames.end()) {
			usages.insert(name);
		}
	}

	// Don't traverse into LOCAL nodes themselves (the binding), only their children
	if (node->type() != IAstNode::Type::LOCAL) {
		for (size_t i = 0; i < node->childCount(); i++) {
			collectVariableUsages(node->child(i), usages, localNames);
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

// Check if a variable exists in any scope
bool variableExistsInAnyScope(const std::string& name, const std::vector<std::unordered_set<std::string>>& scopeStack) {
	for (const auto& scope : scopeStack) {
		if (scope.find(name) != scope.end()) {
			return true;
		}
	}
	return false;
}

// Detect shadow variables (inner scope variable shadows outer scope)
// Quadrate uses function-level scoping for local variables (-> x), so blocks don't create new scopes.
// Only for-loop iterators create a new scoped variable that can shadow.
void detectShadowVariables(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues,
		std::vector<std::unordered_set<std::string>>& scopeStack) {
	if (!node) {
		return;
	}

	// Enter new scope only for functions (not blocks - Quadrate has function-level scoping)
	bool newScope = false;
	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
		scopeStack.emplace_back();
		newScope = true;
	}

	// For-loop iterators create a block-scoped variable that can shadow outer variables
	if (node->type() == IAstNode::Type::FOR_STATEMENT) {
		AstNodeForStatement* forStmt = static_cast<AstNodeForStatement*>(node);
		const std::string& iterName = forStmt->iteratorName();

		// Check if iterator shadows an outer variable
		if (variableExistsInAnyScope(iterName, scopeStack)) {
			LintIssue issue;
			issue.filename = filename;
			issue.line = node->line();
			issue.column = node->column();
			issue.message = "Variable '" + iterName + "' shadows variable from outer scope";
			issue.level = "warning";
			issues.push_back(issue);
		}

		// Create a temporary scope for the for-loop body with the iterator
		scopeStack.emplace_back();
		scopeStack.back().insert(iterName);

		// Process body with the iterator in scope
		if (forStmt->body()) {
			detectShadowVariables(forStmt->body(), filename, issues, scopeStack);
		}

		scopeStack.pop_back();
		return; // Already processed children
	}

	// Check for local variable declarations (-> x)
	// In Quadrate, -> x either declares a new variable or reassigns an existing one.
	// It's only "shadowing" if we're declaring a new variable with the same name as an outer one,
	// but since Quadrate has function-level scoping, this can't happen with regular -> assignments.
	if (node->type() == IAstNode::Type::LOCAL) {
		AstNodeLocal* local = static_cast<AstNodeLocal*>(node);
		std::string varName = local->name();

		// Only add to scope if it's a new variable (first declaration in this function)
		if (!variableExistsInAnyScope(varName, scopeStack) && !scopeStack.empty()) {
			scopeStack.back().insert(varName);
		}
		// No shadowing warning for -> x (it's either new declaration or reassignment)
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

std::vector<LintIssue> lintFile(const std::string& filename, const LintOptions& opts) {
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

					// Also register named function parameters as implicit locals
					Qd::AstNodeFunctionDeclaration* func = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
					for (size_t j = 0; j < func->inputParameters().size(); j++) {
						Qd::AstNodeParameter* param =
								static_cast<Qd::AstNodeParameter*>(func->inputParameters()[j].get());
						if (param->hasName() && param->name()[0] != '_') {
							locals[param->name()] = param;
						}
					}

					// Build set of local variable names for instruction shadowing detection
					std::unordered_set<std::string> localNames;
					for (const auto& pair : locals) {
						localNames.insert(pair.first);
					}

					collectVariableUsages(child, usages, localNames);

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

void printIssuesJson(const std::vector<LintIssue>& issues) {
	json_t* array = json_array();

	for (const auto& issue : issues) {
		json_t* obj = json_object();
		json_object_set_new(obj, "file", json_string(issue.filename.c_str()));
		json_object_set_new(obj, "line", json_integer(static_cast<json_int_t>(issue.line)));
		json_object_set_new(obj, "column", json_integer(static_cast<json_int_t>(issue.column)));
		json_object_set_new(obj, "level", json_string(issue.level.c_str()));
		json_object_set_new(obj, "message", json_string(issue.message.c_str()));
		json_array_append_new(array, obj);
	}

	char* jsonStr = json_dumps(array, JSON_INDENT(2));
	if (jsonStr) {
		std::cout << jsonStr << "\n";
		free(jsonStr);
	}
	json_decref(array);
}

int main(int argc, char* argv[]) {
	qdcli::BaseOptions base;
	LintOptions opts;

	auto handler = [&opts](const char* arg, int& i, int ac, char* av[]) -> bool {
		if (strcmp(arg, "--no-unused-functions") == 0) {
			opts.noUnusedFunctions = true;
			return true;
		}
		if (strcmp(arg, "--no-unused-variables") == 0) {
			opts.noUnusedVariables = true;
			return true;
		}
		if (strcmp(arg, "--no-dead-code") == 0) {
			opts.noDeadCode = true;
			return true;
		}
		if (strcmp(arg, "--no-deep-nesting") == 0) {
			opts.noDeepNesting = true;
			return true;
		}
		if (strcmp(arg, "--no-missing-defer") == 0) {
			opts.noMissingDefer = true;
			return true;
		}
		if (strcmp(arg, "--no-shadow-variables") == 0) {
			opts.noShadowVariables = true;
			return true;
		}
		if (strcmp(arg, "--no-empty-blocks") == 0) {
			opts.noEmptyBlocks = true;
			return true;
		}
		if (strcmp(arg, "--no-constant-conditions") == 0) {
			opts.noConstantConditions = true;
			return true;
		}
		if (strcmp(arg, "--check-magic-numbers") == 0) {
			opts.checkMagicNumbers = true;
			return true;
		}
		if (strcmp(arg, "--check-long-functions") == 0) {
			opts.checkLongFunctions = true;
			return true;
		}
		if (strcmp(arg, "--check-naming") == 0) {
			opts.checkNamingConventions = true;
			return true;
		}
		if (strcmp(arg, "--json") == 0) {
			opts.jsonOutput = true;
			return true;
		}
		if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0) {
			opts.quiet = true;
			return true;
		}
		if (strcmp(arg, "--max-nesting") == 0) {
			if (i + 1 >= ac) {
				std::cerr << "quadlint: --max-nesting requires an argument\n";
				return false;
			}
			opts.maxNestingDepth = std::atoi(av[++i]);
			if (opts.maxNestingDepth < 1) {
				std::cerr << "quadlint: --max-nesting must be at least 1\n";
				return false;
			}
			return true;
		}
		if (strcmp(arg, "--max-function-lines") == 0) {
			if (i + 1 >= ac) {
				std::cerr << "quadlint: --max-function-lines requires an argument\n";
				return false;
			}
			opts.maxFunctionLines = std::atoi(av[++i]);
			if (opts.maxFunctionLines < 1) {
				std::cerr << "quadlint: --max-function-lines must be at least 1\n";
				return false;
			}
			return true;
		}
		return false;
	};

	if (!qdcli::parseArgs(argc, argv, base, "quadlint", handler)) {
		return 1;
	}

	if (base.help) {
		printHelp();
		return 0;
	}

	if (base.version) {
		qdcli::printVersion("quadlint");
		return 0;
	}

	if (qdcli::checkNoInputFiles(base, "quadlint")) {
		return 1;
	}

	// Collect all files from paths (now supports directories)
	std::vector<std::string> allFiles;
	for (const auto& path : base.paths) {
		auto files = qdcli::collectFiles(path);
		allFiles.insert(allFiles.end(), files.begin(), files.end());
	}

	std::vector<LintIssue> allIssues;
	for (const auto& file : allFiles) {
		std::vector<LintIssue> issues = lintFile(file, opts);
		allIssues.insert(allIssues.end(), issues.begin(), issues.end());
	}

	if (opts.jsonOutput) {
		printIssuesJson(allIssues);
	} else if (!opts.quiet) {
		printIssues(allIssues);
	}

	size_t totalIssues = allIssues.size();
	if (totalIssues > 0) {
		if (!opts.jsonOutput) {
			std::cerr << "\n" << totalIssues << " issue" << (totalIssues == 1 ? "" : "s") << " found\n";
		}
		return 1;
	}

	return 0;
}

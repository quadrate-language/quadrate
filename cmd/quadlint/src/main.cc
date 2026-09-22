#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <jansson.h>
#include <memory>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/file_utils.h>
#include <quadrate/cli/help.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_global_var.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_while.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/numeric_literal.h>
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
	std::string rule;  // e.g. "unused-functions", "dead-code"
	std::string keyword;
};

// Check if a source line contains a //nolint directive that suppresses the given rule.
// Supports: //nolint (suppresses all), //nolint:rule1,rule2 (suppresses named rules)
static bool isNolint(const std::string& sourceLine, const std::string& rule) {
	size_t pos = sourceLine.find("//nolint");
	if (pos == std::string::npos) {
		return false;
	}
	pos += 8; // skip "//nolint"
	if (pos >= sourceLine.size() || sourceLine[pos] != ':') {
		return true; // bare //nolint — suppresses everything
	}
	// Parse comma-separated rule names after the colon
	pos++; // skip ':'
	size_t end = sourceLine.size();
	while (pos < end) {
		size_t comma = sourceLine.find(',', pos);
		if (comma == std::string::npos) {
			comma = end;
		}
		// Trim whitespace from rule name
		size_t start = pos;
		while (start < comma && std::isspace(sourceLine[start])) {
			start++;
		}
		size_t rend = comma;
		while (rend > start && std::isspace(sourceLine[rend - 1])) {
			rend--;
		}
		if (sourceLine.substr(start, rend - start) == rule) {
			return true;
		}
		pos = comma + 1;
	}
	return false;
}

// Split source into lines (1-indexed: lines[1] is the first line)
static std::vector<std::string> splitLines(const std::string& source) {
	std::vector<std::string> lines;
	lines.push_back(""); // index 0 unused (lines are 1-based)
	size_t start = 0;
	while (start < source.size()) {
		size_t nl = source.find('\n', start);
		if (nl == std::string::npos) {
			lines.push_back(source.substr(start));
			break;
		}
		lines.push_back(source.substr(start, nl - start));
		start = nl + 1;
	}
	return lines;
}

void printHelp() {
	qdcli::Help help("quadlint", "Quadrate code linter");
	help.description("Checks Quadrate source files for code quality issues.")
			.usage("[options] <file|directory>...")
			.section("Options")
			.standardOptions()
			.option("--json", "Report issues as JSON (for editors and CI)")
			.option('q', "--quiet", "Only show the summary, not individual issues")
			.option("--max-nesting", "<N>", "Maximum nesting depth (default: 4)")
			.option("--no-unused-functions", "Disable unused function warnings")
			.option("--no-unused-variables", "Disable unused variable warnings")
			.option("--no-dead-code", "Disable dead code warnings")
			.option("--no-deep-nesting", "Disable deep nesting warnings")
			.option("--no-missing-defer", "Disable missing defer warnings")
			.option("--no-shadow-variables", "Disable shadow variable warnings")
			.option("--no-empty-blocks", "Disable empty block warnings")
			.option("--no-constant-conditions", "Disable constant condition warnings")
			.section("Stricter checks (disabled by default)")
			.option("--check-magic-numbers", "Enable magic number detection")
			.option("--check-long-functions", "Enable long function detection")
			.option("--check-naming", "Enable naming convention checks")
			.option("--max-function-lines", "<N>", "Maximum function lines (default: 50)")
			.section("Inline suppression")
			.text("Add //nolint on a line to suppress all warnings on that line, or")
			.text("//nolint:rule1,rule2 to suppress specific rules. Rule names are:")
			.text()
			.text("unused-functions, unused-variables, dead-code, deep-nesting,")
			.text("missing-defer, shadow-variables, empty-blocks, constant-conditions,")
			.text("magic-numbers, long-functions, naming")
			.section("Examples")
			.item("quadlint file.qd", "Lint a single file")
			.item("quadlint src/", "Lint every .qd file in a directory, recursively")
			.item("quadlint --json src/", "Report issues as JSON for an editor or CI")
			.item("quadlint -q src/", "Only show the summary");
	help.print();
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

// Recursively collect all function calls, ignoring a function's calls to itself
void collectFunctionCalls(
		IAstNode* node, std::unordered_set<std::string>& calls, const std::string& enclosingFunction = "") {
	if (!node) {
		return;
	}

	std::string current = enclosingFunction;
	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
		current = static_cast<AstNodeFunctionDeclaration*>(node)->name();
	}

	std::string callee;
	if (node->type() == IAstNode::Type::IDENTIFIER) {
		callee = static_cast<AstNodeIdentifier*>(node)->name();
	} else if (node->type() == IAstNode::Type::INSTRUCTION) {
		callee = static_cast<AstNodeInstruction*>(node)->name();
	} else if (node->type() == IAstNode::Type::FUNCTION_POINTER_REFERENCE) {
		callee = static_cast<AstNodeFunctionPointerReference*>(node)->functionName();
	}
	if (!callee.empty() && callee != current) {
		calls.insert(callee);
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectFunctionCalls(node->child(i), calls, current);
	}
}

// Collect module-level `var` declarations
void collectGlobals(IAstNode* root, std::unordered_set<std::string>& globals) {
	for (size_t i = 0; i < root->childCount(); i++) {
		IAstNode* child = root->child(i);
		if (child && child->type() == IAstNode::Type::GLOBAL_VAR_DECLARATION) {
			globals.insert(static_cast<AstNodeGlobalVar*>(child)->name());
		}
	}
}

bool hasMainFunction(IAstNode* root) {
	for (size_t i = 0; i < root->childCount(); i++) {
		IAstNode* child = root->child(i);
		if (child && child->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(child);
			if (func->name() == "main" && !func->hasReceiver()) {
				return true;
			}
		}
	}
	return false;
}

struct FileSymbols {
	bool hasMain = false;
	std::unordered_set<std::string> calls;
	std::unordered_set<std::string> globals;
};

FileSymbols collectFileSymbols(IAstNode* root) {
	FileSymbols symbols;
	symbols.hasMain = hasMainFunction(root);
	collectFunctionCalls(root, symbols.calls);
	collectGlobals(root, symbols.globals);
	return symbols;
}

class SymbolCache {
public:
	const FileSymbols* get(const std::string& path) {
		auto it = mFiles.find(path);
		if (it != mFiles.end()) {
			return it->second.get();
		}
		std::unique_ptr<FileSymbols> symbols;
		try {
			std::string source = qdcli::readFile(path);
			Ast ast;
			IAstNode* root = ast.generate(source.c_str(), false, path.c_str());
			if (root && !ast.hasErrors()) {
				symbols = std::make_unique<FileSymbols>(collectFileSymbols(root));
			}
		} catch (const std::exception&) {
		}
		const FileSymbols* result = symbols.get();
		mFiles[path] = std::move(symbols);
		return result;
	}

	const std::vector<std::string>& qdFilesIn(const std::string& dir) {
		auto it = mDirs.find(dir);
		if (it != mDirs.end()) {
			return it->second;
		}
		std::vector<std::string> files;
		std::error_code ec;
		for (std::filesystem::directory_iterator entry(dir, ec), end; !ec && entry != end; entry.increment(ec)) {
			const std::string name = entry->path().filename().string();
			if (name.size() > 3 && name.compare(name.size() - 3, 3, ".qd") == 0 && entry->is_regular_file(ec)) {
				files.push_back(name);
			}
		}
		return mDirs.emplace(dir, std::move(files)).first->second;
	}

private:
	std::unordered_map<std::string, std::unique_ptr<FileSymbols>> mFiles;
	std::unordered_map<std::string, std::vector<std::string>> mDirs;
};

void mergeSiblingSymbols(const std::string& filename, FileSymbols& symbols, SymbolCache& cache) {
	std::filesystem::path path(filename);
	std::string dir = path.parent_path().string();
	if (dir.empty()) {
		dir = ".";
	}
	const std::string self = path.filename().string();
	for (const auto& name : cache.qdFilesIn(dir)) {
		if (name == self) {
			continue;
		}
		const FileSymbols* sibling = cache.get((std::filesystem::path(dir) / name).string());
		if (!sibling || (symbols.hasMain && sibling->hasMain)) {
			continue;
		}
		symbols.calls.insert(sibling->calls.begin(), sibling->calls.end());
		symbols.globals.insert(sibling->globals.begin(), sibling->globals.end());
	}
}

// Recursively collect all local variable bindings
void collectLocalBindings(IAstNode* node, std::unordered_map<std::string, IAstNode*>& locals,
		const std::unordered_set<std::string>& globals) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::LOCAL) {
		AstNodeLocal* local = static_cast<AstNodeLocal*>(node);
		// Skip "_" which is the discard operator (intentionally unused), and stores to globals
		const std::string& name = local->name();
		if (name != "_" && globals.find(name) == globals.end() && name.find("::") == std::string::npos) {
			locals[name] = node;
		}
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectLocalBindings(node->child(i), locals, globals);
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
				issue.rule = "dead-code";
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

const char* controlKeyword(IAstNode* node) {
	switch (node->type()) {
	case IAstNode::Type::IF_STATEMENT:
		return "if";
	case IAstNode::Type::FOR_STATEMENT:
		return "for";
	case IAstNode::Type::LOOP_STATEMENT:
		return "loop";
	case IAstNode::Type::WHILE_STATEMENT:
		return "while";
	case IAstNode::Type::SWITCH_STATEMENT:
		return "switch";
	default:
		return "";
	}
}

bool isControlStatement(IAstNode* node) {
	return controlKeyword(node)[0] != '\0';
}

// Detect deep nesting
void detectDeepNesting(IAstNode* node, const std::string& filename, int maxDepth, std::vector<LintIssue>& issues,
		int currentDepth = 0) {
	if (!node) {
		return;
	}

	// Check if this node increases nesting depth
	if (isControlStatement(node)) {
		currentDepth++;

		if (currentDepth > maxDepth) {
			LintIssue issue;
			issue.filename = filename;
			issue.line = node->line();
			issue.column = node->column();
			issue.keyword = controlKeyword(node);
			issue.message = "Deep nesting detected (depth: " + std::to_string(currentDepth) +
							", max: " + std::to_string(maxDepth) + ")";
			issue.level = "warning";
			issue.rule = "deep-nesting";
			issues.push_back(issue);
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectDeepNesting(node->child(i), filename, maxDepth, issues, currentDepth);
	}
}

static bool isScopedCall(IAstNode* node, const char* scope, const char* name) {
	if (!node || node->type() != IAstNode::Type::SCOPED_IDENTIFIER) {
		return false;
	}
	AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(node);
	return scoped->scope() == scope && scoped->name() == name;
}

static bool isAcquisition(IAstNode* node) {
	return isScopedCall(node, "mem", "alloc") || isScopedCall(node, "io", "open");
}

static bool isRelease(IAstNode* node) {
	return isScopedCall(node, "mem", "free") || isScopedCall(node, "io", "close");
}

static bool propagatesError(IAstNode* node) {
	switch (node->type()) {
	case IAstNode::Type::IDENTIFIER:
		return static_cast<AstNodeIdentifier*>(node)->propagateOnError();
	case IAstNode::Type::SCOPED_IDENTIFIER:
		return static_cast<AstNodeScopedIdentifier*>(node)->propagateOnError();
	case IAstNode::Type::INSTRUCTION:
		return static_cast<AstNodeInstruction*>(node)->propagateOnError();
	default:
		return false;
	}
}

struct Resource {
	std::string name;
	IAstNode* acquisition;
	bool held = true;
	bool exposed = false;
	bool releasedInDefer = false;
};

static void scanResources(IAstNode* node, bool inDefer, std::vector<Resource>& resources) {
	for (size_t i = 0; i < node->childCount(); i++) {
		IAstNode* child = node->child(i);
		if (!child || child->type() == IAstNode::Type::FUNCTION_DECLARATION ||
				child->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
			continue;
		}
		IAstNode* next = i + 1 < node->childCount() ? node->child(i + 1) : nullptr;
		if (!inDefer && (child->type() == IAstNode::Type::RETURN_STATEMENT || propagatesError(child))) {
			for (auto& resource : resources) {
				resource.exposed = resource.exposed || resource.held;
			}
		}
		if (!inDefer && isAcquisition(child) && next && next->type() == IAstNode::Type::LOCAL) {
			resources.push_back({static_cast<AstNodeLocal*>(next)->name(), child});
		}
		if (child->type() == IAstNode::Type::IDENTIFIER && isRelease(next)) {
			const std::string& name = static_cast<AstNodeIdentifier*>(child)->name();
			for (auto& resource : resources) {
				if (resource.name == name) {
					if (inDefer) {
						resource.releasedInDefer = true;
					} else {
						resource.held = false;
					}
				}
			}
		}
		scanResources(child, inDefer || child->type() == IAstNode::Type::DEFER_STATEMENT, resources);
	}
}

// Detect resources released by hand while an early return or `?` can skip the release
void detectMissingDefer(IAstNode* node, const std::string& filename, std::vector<LintIssue>& issues) {
	if (!node) {
		return;
	}

	if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
		std::vector<Resource> resources;
		scanResources(node, false, resources);
		for (const auto& resource : resources) {
			if (!resource.held && resource.exposed && !resource.releasedInDefer) {
				LintIssue issue;
				issue.filename = filename;
				issue.line = resource.acquisition->line();
				issue.column = resource.acquisition->column();
				issue.message =
						"'" + resource.name + "' is not released on early exit - consider using defer for cleanup";
				issue.level = "warning";
				issue.rule = "missing-defer";
				issues.push_back(issue);
			}
		}
	}

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
		AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
		if (func->hasReceiver()) {
			scopeStack.back().insert(func->receiverName());
		}
		if (!func->isStack()) {
			for (const auto& p : func->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(p.get());
				if (param->hasName()) {
					scopeStack.back().insert(param->name());
				}
			}
		}
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
			issue.keyword = "for";
			issue.message = "Variable '" + iterName + "' shadows variable from outer scope";
			issue.level = "warning";
			issue.rule = "shadow-variables";
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
	if (isControlStatement(node) && node->type() != IAstNode::Type::SWITCH_STATEMENT) {
		// Find the block child
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (node->type() == IAstNode::Type::WHILE_STATEMENT &&
					child != static_cast<AstNodeWhileStatement*>(node)->body()) {
				continue;
			}
			if (child && child->type() == IAstNode::Type::BLOCK && countStatements(child) == 0) {
				LintIssue issue;
				issue.filename = filename;
				issue.line = node->line();
				issue.column = node->column();
				issue.keyword = controlKeyword(node);
				issue.message = "Empty '" + std::string(controlKeyword(node)) + "' block";
				issue.level = "warning";
				issue.rule = "empty-blocks";
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
					int64_t value = 0;
					if (readIntegerLiteral(literal->value(), value) != std::errc()) {
						continue;
					}
					bool isAlwaysTrue = value != 0;
					LintIssue issue;
					issue.filename = filename;
					issue.line = previous->line();
					issue.column = previous->column();
					issue.message =
							"Constant condition: expression is always " + std::string(isAlwaysTrue ? "true" : "false");
					issue.level = "warning";
					issue.rule = "constant-conditions";
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
				// Check if inside a constant or var declaration (which is acceptable)
				IAstNode* parent = node->parent();
				bool inConstant = false;
				while (parent) {
					if (parent->type() == IAstNode::Type::CONSTANT_DECLARATION ||
							parent->type() == IAstNode::Type::GLOBAL_VAR_DECLARATION) {
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
					issue.rule = "magic-numbers";
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
			issue.rule = "long-functions";
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
			issue.rule = "naming";
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
			issue.rule = "naming";
			issues.push_back(issue);
		}
	}

	// Recursively check children
	for (size_t i = 0; i < node->childCount(); i++) {
		detectNamingConventions(node->child(i), filename, issues);
	}
}

void anchorAtKeyword(LintIssue& issue, const std::vector<std::string>& lines) {
	if (issue.line == 0 || issue.line >= lines.size() || issue.column == 0) {
		return;
	}
	const std::string& text = lines[issue.line];
	const size_t len = issue.keyword.size();
	size_t end = std::min(issue.column - 1, text.size());
	auto isWordChar = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
	while (end >= len) {
		size_t pos = text.rfind(issue.keyword, end - len);
		if (pos == std::string::npos) {
			return;
		}
		bool startsWord = pos == 0 || !isWordChar(text[pos - 1]);
		bool endsWord = pos + len >= text.size() || !isWordChar(text[pos + len]);
		if (startsWord && endsWord) {
			issue.column = pos + 1;
			return;
		}
		end = pos + len - 1;
	}
}

std::vector<LintIssue> lintFile(
		const std::string& filename, const LintOptions& opts, SymbolCache& cache, bool& readable) {
	std::vector<LintIssue> issues;
	readable = true;

	try {
		// Read and parse file
		std::string source = qdcli::readFile(filename);
		Ast ast;
		IAstNode* root = ast.generate(source.c_str(), false, filename.c_str());

		if (!root || ast.hasErrors()) {
			for (const auto& err : ast.getErrors()) {
				LintIssue issue;
				issue.filename = filename;
				issue.line = err.line;
				issue.column = err.column;
				issue.message = err.message;
				issue.level = "error";
				issue.rule = "parse-error";
				issues.push_back(issue);
			}
			if (issues.empty()) {
				LintIssue issue;
				issue.filename = filename;
				issue.line = 1;
				issue.column = 1;
				issue.message = "failed to parse";
				issue.level = "error";
				issue.rule = "parse-error";
				issues.push_back(issue);
			}
			return issues;
		}

		FileSymbols symbols = collectFileSymbols(root);
		mergeSiblingSymbols(filename, symbols, cache);

		// Check for unused functions
		if (!opts.noUnusedFunctions) {
			std::unordered_map<std::string, IAstNode*> functions;
			const std::unordered_set<std::string>& calls = symbols.calls;

			collectFunctions(root, functions);

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
					issue.rule = "unused-functions";
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
					collectLocalBindings(child, locals, symbols.globals);

					// Also register named function parameters as implicit locals
					Qd::AstNodeFunctionDeclaration* func = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
					for (size_t j = 0; !func->isStack() && j < func->inputParameters().size(); j++) {
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
							issue.rule = "unused-variables";
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

		auto lines = splitLines(source);
		for (auto& issue : issues) {
			if (!issue.keyword.empty()) {
				anchorAtKeyword(issue, lines);
			}
		}

		// Filter out issues suppressed by //nolint directives
		issues.erase(std::remove_if(issues.begin(), issues.end(),
							 [&lines](const LintIssue& issue) {
								 if (issue.line > 0 && issue.line < lines.size()) {
									 return isNolint(lines[issue.line], issue.rule);
								 }
								 return false;
							 }),
				issues.end());

	} catch (const std::exception& e) {
		std::cerr << "quadlint: " << filename << ": " << e.what() << "\n";
		readable = false;
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

std::string toValidUtf8(const std::string& text) {
	std::string out;
	size_t i = 0;
	while (i < text.size()) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		size_t len = 0;
		uint32_t cp = 0;
		if (c < 0x80) {
			len = 1;
			cp = c;
		} else if (c >= 0xC2 && c <= 0xDF) {
			len = 2;
			cp = c & 0x1Fu;
		} else if (c >= 0xE0 && c <= 0xEF) {
			len = 3;
			cp = c & 0x0Fu;
		} else if (c >= 0xF0 && c <= 0xF4) {
			len = 4;
			cp = c & 0x07u;
		}
		bool valid = len > 0 && i + len <= text.size();
		for (size_t k = 1; valid && k < len; k++) {
			const unsigned char cc = static_cast<unsigned char>(text[i + k]);
			valid = (cc & 0xC0) == 0x80;
			cp = (cp << 6) | (cc & 0x3Fu);
		}
		if (valid && ((len == 3 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) ||
							 (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)))) {
			valid = false;
		}
		if (valid) {
			out.append(text, i, len);
			i += len;
		} else {
			out += "\xEF\xBF\xBD";
			i++;
		}
	}
	return out;
}

void printIssuesJson(const std::vector<LintIssue>& issues) {
	json_t* array = json_array();

	for (const auto& issue : issues) {
		json_t* obj = json_object();
		json_object_set_new(obj, "file", json_string(toValidUtf8(issue.filename).c_str()));
		json_object_set_new(obj, "line", json_integer(static_cast<json_int_t>(issue.line)));
		json_object_set_new(obj, "column", json_integer(static_cast<json_int_t>(issue.column)));
		json_object_set_new(obj, "level", json_string(issue.level.c_str()));
		json_object_set_new(obj, "rule", json_string(issue.rule.c_str()));
		json_object_set_new(obj, "message", json_string(toValidUtf8(issue.message).c_str()));
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

	auto handler = [&opts, &base](const char* arg, int& i, int ac, char* av[]) -> bool {
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
				base.optionError = "option '--max-nesting' requires an argument";
				return false;
			}
			opts.maxNestingDepth = std::atoi(av[++i]);
			if (opts.maxNestingDepth < 1) {
				base.optionError = "option '--max-nesting' must be at least 1";
				return false;
			}
			return true;
		}
		if (strcmp(arg, "--max-function-lines") == 0) {
			if (i + 1 >= ac) {
				base.optionError = "option '--max-function-lines' requires an argument";
				return false;
			}
			opts.maxFunctionLines = std::atoi(av[++i]);
			if (opts.maxFunctionLines < 1) {
				base.optionError = "option '--max-function-lines' must be at least 1";
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

	if (base.noColor) {
		Colors::setEnabled(false);
	}

	if (!qdcli::checkPathsExist(base.paths, "quadlint")) {
		return 1;
	}

	// Collect all files from paths (now supports directories)
	std::vector<std::string> allFiles;
	for (const auto& path : base.paths) {
		auto files = qdcli::collectFiles(path);
		allFiles.insert(allFiles.end(), files.begin(), files.end());
	}

	// A path that exists but yields nothing to lint is also not success: it means
	// the caller pointed at the wrong directory and every check silently passed.
	if (allFiles.empty()) {
		qdcli::error("quadlint", "no .qd files found");
		return 1;
	}

	std::vector<LintIssue> allIssues;
	SymbolCache cache;
	bool allReadable = true;
	for (const auto& file : allFiles) {
		bool readable = true;
		std::vector<LintIssue> issues = lintFile(file, opts, cache, readable);
		allReadable = allReadable && readable;
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

	return allReadable ? 0 : 1;
}

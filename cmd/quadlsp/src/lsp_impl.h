#ifndef QUADLSP_LSP_IMPL_H
#define QUADLSP_LSP_IMPL_H

#include "constant_info.h"
#include "function_info.h"
#include "struct_info.h"
#include <jansson.h>
#include <map>
#include <qc/ast_node.h>
#include <qc/ast_node_local.h>
#include <string>
#include <utility>
#include <vector>

// Default error span length in characters for diagnostic highlighting
static const int ERROR_SPAN_LENGTH = 10;

// Expand tilde (~) in file paths
std::string expandTilde(const std::string& path);

// Load dependencies from qd.json and return include paths
std::vector<std::string> loadDependenciesFromManifest(const std::string& manifestDir);

// LSP Server using jansson for JSON handling
class QuadrateLSP {
public:
	QuadrateLSP() : messageId_(0) {
	}

	void run();

private:
	// Core messaging
	std::string readMessage();
	void sendMessage(json_t* json);
	std::string getJsonString(json_t* obj, const char* key);
	json_t* getJsonObject(json_t* obj, const char* key);
	void handleMessage(const std::string& message);

	// Initialization
	void handleInitialize(const std::string& id, json_t* initOptions);
	void handleDidOpen(const std::string& uri, const std::string& text);
	void handleShutdown(const std::string& id);

	// Structure to hold lint warnings
	struct LintWarning {
		size_t line;
		size_t column;
		std::string message;
	};

	// Strip ANSI escape codes from a string
	static std::string stripAnsiCodes(const std::string& input);

	// Run quadlint on source content and return warnings
	std::vector<LintWarning> runQuadlint(const std::string& source);

	// Diagnostics and formatting
	void publishDiagnostics(const std::string& uri, const std::string& text);
	void handleFormatting(const std::string& id, const std::string& uri);

	// Completion (implemented in lsp_completion.cc)
	void handleCompletion(const std::string& id, const std::string& uri, size_t line, size_t character);
	std::string getModulePrefixAtPosition(const std::string& text, size_t line, size_t character);
	std::string getFieldAccessVariableAtPosition(const std::string& text, size_t line, size_t character);
	std::vector<std::pair<std::string, std::string>> findStructFields(
			const std::string& structType, const std::string& documentText, const std::string& sourceDir);
	std::vector<FunctionInfo> extractModuleFunctions(const std::string& modulePath);
	std::vector<StructInfo> extractModuleStructs(const std::string& modulePath);
	std::vector<ConstantInfo> extractModuleConstants(const std::string& modulePath);

	// Document features
	void handleHover(const std::string& id, const std::string& uri, size_t line, size_t character);
	void handleSignatureHelp(const std::string& id, const std::string& uri, size_t line, size_t character);
	void handleDocumentSymbols(const std::string& id, const std::string& uri);

	// Navigation features (implemented in lsp_navigation.cc)
	void handleDefinition(const std::string& id, const std::string& uri, size_t line, size_t character);
	void handleFoldingRange(const std::string& id, const std::string& uri);
	void handleDocumentHighlight(const std::string& id, const std::string& uri, size_t line, size_t character);
	void handleReferences(const std::string& id, const std::string& uri, size_t line, size_t character);
	void handleRename(
			const std::string& id, const std::string& uri, size_t line, size_t character, const std::string& newName);

	// Navigation helpers (implemented in lsp_navigation.cc)
	void findIdentifiersInNode(Qd::IAstNode* node, const std::string& targetName, std::vector<Qd::IAstNode*>& results);
	Qd::AstNodeLocal* findLocalDeclaration(Qd::IAstNode* startNode, const std::string& varName, size_t requestLine);
	json_t* findDefinitionInModule(
			const std::string& modulePath, const std::string& symbolName, const std::string& symbolType);
	std::string findStructTypeOfVariable(Qd::IAstNode* root, const std::string& varName, size_t requestLine);
	json_t* handleFieldAccessDefinition(Qd::IAstNode* root, const std::string& uri, size_t line, bool cursorOnVariable);

	// Utility functions
	std::string getBuiltInDocumentation(const std::string& word);
	std::string getWordAtPosition(const std::string& text, size_t line, size_t character);
	std::string getPackagesDir();
	std::string findLatestPackageVersion(const std::string& moduleName);
	std::string resolveModulePath(const std::string& moduleName, const std::string& sourceDir);

	// Extraction functions
	std::vector<FunctionInfo> extractFunctions(const std::string& text);
	std::vector<StructInfo> extractStructs(const std::string& text);

	// Member variables
	std::map<std::string, std::string> documents_;
	[[maybe_unused]] int messageId_;
	bool lintEnabled_ = true;
	std::string quadlintPath_ = "quadlint";
};

#endif // QUADLSP_LSP_IMPL_H

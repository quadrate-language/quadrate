#ifndef QUADC_AST_CACHE_H
#define QUADC_AST_CACHE_H

#include <memory>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/semantic_validator.h>
#include <string>
#include <unordered_map>
#include <vector>

// Cached AST entry containing the parsed AST, root node, and source
struct CachedAst {
	std::unique_ptr<Qd::Ast> ast;
	Qd::IAstNode* root = nullptr;
	std::string source;
};

// Global AST cache for avoiding re-parsing module files
class AstCache {
public:
	// Get singleton instance
	static AstCache& instance();

	// Get or parse a file, using the cache
	// Returns root node on success (cached or freshly parsed), nullptr on failure
	// On failure, outAst will point to the Ast object so caller can retrieve parse errors
	// outFromCache is set to true if the AST came from the cache (already validated)
	Qd::IAstNode* getOrParse(const std::string& filePath, Qd::Ast** outAst = nullptr, std::string* outSource = nullptr,
			bool* outFromCache = nullptr);

	// Import cached ASTs from semantic validator
	void importFromValidator(Qd::SemanticValidator& validator);

	// Check if a file is cached
	bool contains(const std::string& filePath) const;

	// Clear the cache
	void clear();

private:
	AstCache() = default;

	// Canonicalize a file path for cache key
	std::string canonicalize(const std::string& filePath) const;

	std::unordered_map<std::string, CachedAst> mCache;

	// Temporary storage for failed parses (so caller can get errors)
	CachedAst mFailedParse;
};

// Collect all imported module names from an AST by traversing USE statements
std::vector<std::string> collectImportedModules(Qd::IAstNode* root);

// Check if an AST has FFI import statements (import "lib.a" as "name" { ... })
bool hasFFIImportsInAST(Qd::IAstNode* root);

#endif

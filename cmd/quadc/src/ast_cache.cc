#include "ast_cache.h"
#include "diagnostics.h"
#include <filesystem>
#include <functional>
#include <quadrate/qc/ast_node_use.h>

AstCache& AstCache::instance() {
	static AstCache cache;
	return cache;
}

std::string AstCache::canonicalize(const std::string& filePath) const {
	try {
		return std::filesystem::canonical(filePath).string();
	} catch (...) {
		return filePath;
	}
}

Qd::IAstNode* AstCache::getOrParse(
		const std::string& filePath, Qd::Ast** outAst, std::string* outSource, bool* outAlreadyValidated) {
	std::string canonicalPath = canonicalize(filePath);

	static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
	if (debug) {
		std::cerr << "[DEBUG CACHE] getOrParse: " << filePath << " -> canonical: " << canonicalPath << std::endl;
	}

	// Check cache for successful parse
	auto it = mCache.find(canonicalPath);
	if (it != mCache.end()) {
		if (debug) {
			std::cerr << "[DEBUG CACHE]   HIT - using cached AST" << std::endl;
		}
		if (outAst) {
			*outAst = it->second.ast.get();
		}
		if (outSource) {
			*outSource = it->second.source;
		}
		if (outAlreadyValidated) {
			*outAlreadyValidated = it->second.validated;
		}
		return it->second.root;
	}
	if (debug) {
		std::cerr << "[DEBUG CACHE]   MISS - will parse fresh" << std::endl;
	}

	if (outAlreadyValidated) {
		*outAlreadyValidated = false;
	}

	// Not cached, read and parse
	auto bufferOpt = readFileContents(filePath);
	if (!bufferOpt) {
		if (outAst) {
			*outAst = nullptr;
		}
		return nullptr;
	}
	std::string buffer = std::move(*bufferOpt);

	auto ast = std::make_unique<Qd::Ast>();
	auto root = ast->generate(buffer.c_str(), false, filePath.c_str());

	// Don't cache failed parses, but store so we can report errors
	if (!root || ast->hasErrors()) {
		mFailedParse.ast = std::move(ast);
		mFailedParse.root = nullptr;
		mFailedParse.source = std::move(buffer);
		if (outAst) {
			*outAst = mFailedParse.ast.get();
		}
		if (outSource) {
			*outSource = mFailedParse.source;
		}
		return nullptr;
	}

	// Store successful parse in cache
	CachedAst cached;
	cached.ast = std::move(ast);
	cached.root = root;
	cached.source = std::move(buffer);

	auto& entry = mCache[canonicalPath];
	entry = std::move(cached);

	if (outAst) {
		*outAst = entry.ast.get();
	}
	if (outSource) {
		*outSource = entry.source;
	}
	return entry.root;
}

void AstCache::importFromValidator(Qd::SemanticValidator& validator) {
	static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
	for (auto& [path, cached] : validator.getParsedModuleAsts()) {
		std::string canonicalPath = canonicalize(path);

		if (debug) {
			std::cerr << "[DEBUG CACHE] importFromValidator: " << path << " -> canonical: " << canonicalPath
					  << std::endl;
		}

		// Only import if not already cached
		if (mCache.find(canonicalPath) == mCache.end()) {
			if (debug) {
				std::cerr << "[DEBUG CACHE]   Importing AST to cache" << std::endl;
			}
			CachedAst entry;
			entry.ast = std::move(cached.ast);
			entry.root = cached.root;
			entry.source = std::move(cached.source);
			mCache[canonicalPath] = std::move(entry);
		} else {
			if (debug) {
				std::cerr << "[DEBUG CACHE]   Already cached, skipping" << std::endl;
			}
		}
	}
}

void AstCache::markValidated(const std::string& filePath) {
	auto it = mCache.find(canonicalize(filePath));
	if (it != mCache.end()) {
		it->second.validated = true;
	}
}

bool AstCache::contains(const std::string& filePath) const {
	return mCache.find(canonicalize(filePath)) != mCache.end();
}

void AstCache::clear() {
	mCache.clear();
}

std::vector<std::string> collectImportedModules(Qd::IAstNode* root) {
	std::vector<std::string> imports;
	std::function<void(Qd::IAstNode*)> collect = [&](Qd::IAstNode* node) {
		if (!node) {
			return;
		}
		if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
			auto* useNode = static_cast<Qd::AstNodeUse*>(node);
			imports.push_back(useNode->module());
		}
		for (auto* child : node->children()) {
			collect(child);
		}
	};
	collect(root);
	return imports;
}

bool hasFFIImportsInAST(Qd::IAstNode* root) {
	std::function<bool(Qd::IAstNode*)> check = [&](Qd::IAstNode* node) -> bool {
		if (!node) {
			return false;
		}
		if (node->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
			return true;
		}
		for (auto* child : node->children()) {
			if (check(child)) {
				return true;
			}
		}
		return false;
	};
	return check(root);
}

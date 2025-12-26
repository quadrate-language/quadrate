// Internal helpers for semantic_validator implementation files
// Not part of public API

#ifndef QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H
#define QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <qc/ast_node_literal.h>
#include <qc/instructions.h>
#include <qc/semantic_validator.h>
#include <string>
#include <unordered_set>
#include <vector>

// NOTE: This header is intended to be included inside namespace Qd {}
// in semantic_validator implementation files.

// Helper function to expand tilde (~) in file paths
inline std::string expandTilde(const std::string& path) {
	if (path.empty() || path[0] != '~') {
		return path;
	}

	const char* home = std::getenv("HOME");
	if (!home) {
		return path;
	}

	if (path.length() == 1) {
		return std::string(home);
	} else if (path[1] == '/') {
		return std::string(home) + path.substr(1);
	}

	return path;
}

// Extract package name from module identifier
inline std::string getPackageFromModuleName(const std::string& moduleName) {
	bool isFilePath = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isFilePath) {
		size_t lastSlash = moduleName.find_last_of('/');
		std::string filename = (lastSlash != std::string::npos) ? moduleName.substr(lastSlash + 1) : moduleName;

		if (filename.size() >= 3 && filename.substr(filename.size() - 3) == ".qd") {
			filename = filename.substr(0, filename.size() - 3);
		}

		return filename;
	}

	return moduleName;
}

// Check if a name is a reserved keyword
inline bool isReservedKeyword(const std::string& name) {
	static const std::unordered_set<std::string> KEYWORDS = {"if", "else", "for", "while", "loop", "switch", "case",
			"break", "continue", "return", "fn", "struct", "const", "pub", "test", "use", "import", "ctx", "defer",
			"true", "false", "Ok", "Err", "i64", "f64", "str", "ptr", "void"};
	return KEYWORDS.find(name) != KEYWORDS.end();
}

// Serialize a case value for comparison
inline std::string serializeCaseValue(IAstNode* node) {
	if (!node) {
		return "";
	}

	if (node->type() == IAstNode::Type::LITERAL) {
		AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(node);
		switch (lit->literalType()) {
		case AstNodeLiteral::LiteralType::INTEGER:
			return "int:" + lit->value();
		case AstNodeLiteral::LiteralType::FLOAT:
			return "float:" + lit->value();
		case AstNodeLiteral::LiteralType::STRING:
			return "string:" + lit->value();
		}
	}

	return "node:" + std::to_string(reinterpret_cast<std::uintptr_t>(node));
}

// Convert stack value type to string
inline const char* stackValueTypeToString(StackValueType type) {
	switch (type) {
	case StackValueType::INT:
		return "int";
	case StackValueType::FLOAT:
		return "float";
	case StackValueType::STRING:
		return "string";
	case StackValueType::PTR:
		return "ptr";
	case StackValueType::ANY:
		return "any";
	case StackValueType::UNKNOWN:
		return "unknown";
	default:
		return "unknown";
	}
}

// Check if actual type can be implicitly cast to expected type
inline bool isImplicitCastAllowed(StackValueType actual, StackValueType expected) {
	if ((actual == StackValueType::INT && expected == StackValueType::FLOAT) ||
			(actual == StackValueType::FLOAT && expected == StackValueType::INT)) {
		return true;
	}
	if (actual == StackValueType::INT && expected == StackValueType::PTR) {
		return true;
	}
	return false;
}

// Check if implicit cast should generate a warning
inline bool shouldWarnImplicitCast(StackValueType actual, StackValueType expected) {
	if (actual == StackValueType::INT && expected == StackValueType::PTR) {
		return false;
	}
	return true;
}

// Calculate Levenshtein distance between two strings
inline size_t levenshteinDistance(const std::string& s1, const std::string& s2) {
	size_t m = s1.size();
	size_t n = s2.size();

	if (m == 0) return n;
	if (n == 0) return m;

	std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

	for (size_t i = 0; i <= m; i++) dp[i][0] = i;
	for (size_t j = 0; j <= n; j++) dp[0][j] = j;

	for (size_t i = 1; i <= m; i++) {
		for (size_t j = 1; j <= n; j++) {
			size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
			dp[i][j] = std::min({
				dp[i - 1][j] + 1,      // deletion
				dp[i][j - 1] + 1,      // insertion
				dp[i - 1][j - 1] + cost // substitution
			});
		}
	}

	return dp[m][n];
}

// Find similar names from a set of candidates (returns empty string if no good match)
inline std::string findSimilarName(const std::string& name, const std::unordered_set<std::string>& candidates) {
	std::string bestMatch;
	size_t bestDistance = SIZE_MAX;

	// Maximum distance threshold - larger names allow more errors
	size_t maxDistance = std::max(size_t(2), name.size() / 3);

	for (const auto& candidate : candidates) {
		// Skip if lengths are too different
		if (candidate.size() > name.size() * 2 || name.size() > candidate.size() * 2) {
			continue;
		}

		size_t distance = levenshteinDistance(name, candidate);
		if (distance < bestDistance && distance <= maxDistance) {
			bestDistance = distance;
			bestMatch = candidate;
		}
	}

	return bestMatch;
}

// Find similar names from a map of candidates
template<typename T>
inline std::string findSimilarNameInMap(const std::string& name,
		const std::unordered_map<std::string, T>& candidates) {
	std::unordered_set<std::string> keys;
	for (const auto& pair : candidates) {
		keys.insert(pair.first);
	}
	return findSimilarName(name, keys);
}

// Find similar names from a C-style array of strings
inline std::string findSimilarNameInArray(const std::string& name, const char* const* arr, size_t count) {
	std::unordered_set<std::string> candidates;
	for (size_t i = 0; i < count; i++) {
		candidates.insert(arr[i]);
	}
	return findSimilarName(name, candidates);
}

// Find similar function name from user functions OR builtins
inline std::string findSimilarFunctionName(const std::string& name,
		const std::unordered_set<std::string>& userFunctions) {
	// First check user-defined functions
	std::string suggestion = findSimilarName(name, userFunctions);
	if (!suggestion.empty()) {
		return suggestion;
	}
	// Then check builtins
	return findSimilarNameInArray(name, VALIDATOR_INSTRUCTIONS, VALIDATOR_INSTRUCTION_COUNT);
}

#endif // QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H

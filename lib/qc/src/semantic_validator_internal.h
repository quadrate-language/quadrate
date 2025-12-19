// Internal helpers for semantic_validator implementation files
// Not part of public API

#ifndef QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H
#define QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H

#include <cstdint>
#include <cstdlib>
#include <qc/ast_node_literal.h>
#include <qc/semantic_validator.h>
#include <string>
#include <unordered_set>

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

#endif // QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H

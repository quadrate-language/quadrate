// Internal helpers for semantic_validator implementation files
// Not part of public API

#ifndef QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H
#define QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/semantic_validator.h>
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

// Check if a type name looks like a struct type
// Supports both unqualified (Point) and qualified (math::Vec3) names
inline bool looksLikeStructType(const std::string& typeName) {
	if (typeName.empty()) {
		return false;
	}
	// Check for qualified name (module::StructName)
	size_t colonPos = typeName.find("::");
	if (colonPos != std::string::npos) {
		// Check the character after ::
		if (colonPos + 2 < typeName.size()) {
			return std::isupper(typeName[colonPos + 2]);
		}
		return false;
	}
	// Unqualified name - check first character
	return std::isupper(typeName[0]);
}

// " in parameter 'x'" when the parameter is named, nothing when it is not:
// a `stack fn f(i64 -- )` parameter has a type to complain about and no name,
// and "in parameter ''" reads as though the name were the problem.
// Templated on the parameter node so this header needs no AST include: it is
// pulled in from inside `namespace Qd {}`, where an include would nest.
template <typename Param>
inline std::string parameterSuffix(const Param* param) {
	return param->hasName() ? " in parameter '" + param->name() + "'" : "";
}

// Check if a name is a reserved keyword
// The sized integer types. Specification 3.1.1: they describe a width **in memory**, and are
// valid on a struct field and as the width selector on a `mem` accessor. Everywhere else --
// a parameter, a return, a module global, a `cast` target -- the value lives in a 64-bit
// stack slot and the annotation has nothing to act on. They used to be accepted there and
// silently do nothing, which is the divergence the specification called out.
inline bool isSizedIntType(const std::string& name) {
	return name == "i8" || name == "i16" || name == "i32" || name == "u8" || name == "u16" || name == "u32" ||
		   name == "u64";
}

// `where` names the position: "a parameter", "a return value", "a module global", "a cast target".
inline std::string sizedTypeMisuseMessage(const std::string& name, const std::string& where) {
	return "'" + name + "' is a memory-layout type and means nothing on " + where +
		   ": a stack value is always 64 bits. Sized types belong on a struct field or a mem accessor "
		   "(mem::get_" +
		   name + "); use 'i64' here";
}

inline bool isReservedKeyword(const std::string& name) {
	static const std::unordered_set<std::string> KEYWORDS = {"if", "else", "for", "loop", "switch", "case", "break",
			"continue", "return", "fn", "struct", "enum", "type", "const", "pub", "test", "use", "import", "defer",
			"as", "true", "false", "Ok", "Err", "null", "i64", "f64", "str", "ptr", "void"};
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
		case AstNodeLiteral::LiteralType::NULL_PTR:
			return "null:0";
		case AstNodeLiteral::LiteralType::BOOL:
			return "bool:" + lit->value();
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

	if (m == 0) {
		return n;
	}
	if (n == 0) {
		return m;
	}

	std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

	for (size_t i = 0; i <= m; i++) {
		dp[i][0] = i;
	}
	for (size_t j = 0; j <= n; j++) {
		dp[0][j] = j;
	}

	for (size_t i = 1; i <= m; i++) {
		for (size_t j = 1; j <= n; j++) {
			size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
			dp[i][j] = std::min({
					dp[i - 1][j] + 1,		// deletion
					dp[i][j - 1] + 1,		// insertion
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
	const bool largeSet = candidates.size() > 8;
	size_t maxDistance = std::max(size_t(2), name.size() / 3);
	if (largeSet && name.size() <= 2) {
		maxDistance = 0;
	} else if (largeSet && name.size() <= 3) {
		maxDistance = 1;
	}
	auto lowered = [](std::string v) {
		for (char& ch : v) {
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		return v;
	};
	const std::string nameLower = lowered(name);

	auto isIdentifierLike = [](const std::string& s) {
		for (char ch : s) {
			const unsigned char c = static_cast<unsigned char>(ch);
			if (!std::isalnum(c) && c != '_') {
				return false;
			}
		}
		return true;
	};
	const bool nameIsIdentifier = isIdentifierLike(name);

	for (const auto& candidate : candidates) {
		// Skip if lengths are too different
		if (candidate.size() > name.size() * 2 || name.size() > candidate.size() * 2) {
			continue;
		}
		if (isIdentifierLike(candidate) != nameIsIdentifier) {
			continue;
		}

		size_t distance = levenshteinDistance(
				maxDistance == 0 ? nameLower : name, maxDistance == 0 ? lowered(candidate) : candidate);
		if (distance < bestDistance && distance <= maxDistance) {
			bestDistance = distance;
			bestMatch = candidate;
		}
	}

	return bestMatch;
}

// Find similar names from a map of candidates
template <typename T>
inline std::string findSimilarNameInMap(const std::string& name, const std::unordered_map<std::string, T>& candidates) {
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
inline std::string findSimilarFunctionName(
		const std::string& name, const std::unordered_set<std::string>& userFunctions) {
	// First check user-defined functions
	std::string suggestion = findSimilarName(name, userFunctions);
	if (!suggestion.empty()) {
		return suggestion;
	}
	// Then check builtins
	return findSimilarNameInArray(name, BUILTIN_INSTRUCTIONS, BUILTIN_INSTRUCTION_COUNT);
}

// Check if two struct types are equivalent, considering merged modules
// For merged modules, "module::Struct" and "Struct" are equivalent
inline bool structTypesMatch(
		const std::string& actual, const std::string& expected, const std::unordered_set<std::string>& mergedModules) {
	// Direct match
	if (actual == expected) {
		return true;
	}

	// Check if expected is "module::Struct" and actual is "Struct" (or vice versa)
	size_t colonPos = expected.find("::");
	if (colonPos != std::string::npos) {
		std::string moduleName = expected.substr(0, colonPos);
		std::string structName = expected.substr(colonPos + 2);
		// If the module is merged and actual matches the unqualified struct name
		if (mergedModules.count(moduleName) > 0 && actual == structName) {
			return true;
		}
	}

	// Check the reverse: actual is "module::Struct", expected is "Struct"
	colonPos = actual.find("::");
	if (colonPos != std::string::npos) {
		std::string moduleName = actual.substr(0, colonPos);
		std::string structName = actual.substr(colonPos + 2);
		if (mergedModules.count(moduleName) > 0 && expected == structName) {
			return true;
		}
	}

	return false;
}

// ---- Generic type parameters at call sites ----------------------------------------------
//
// Type names are kept as the strings the programmer wrote (`[]T`, `fn(T -- T)`, `Box<T>`), so
// unification is structural recursion over those strings. A binding maps a type parameter to
// the concrete type name it was first unified with.

inline std::string typeBaseName(const std::string& t) {
	size_t p = t.find('<');
	return p == std::string::npos ? t : t.substr(0, p);
}

// Whether a type name denotes a `[]T`.
inline bool isArrayTypeName(const std::string& t) {
	return t.size() > 2 && t[0] == '[' && t[1] == ']';
}

// A `[]T` handed to something that declared a raw `ptr`. The reason is the same wherever it is
// caught -- a call argument, a function output, a struct field -- so the wording is too.
inline std::string rawPtrArrayWhy(const std::string& actual) {
	return "expects a raw 'ptr' buffer but got '" + actual + "': an array points at its header, not at its elements";
}

// "Box<A, []B>" -> ["A", "[]B"]; top-level split on ',' respecting nested <> and ().
inline std::vector<std::string> typeArgsOf(const std::string& t) {
	std::vector<std::string> args;
	size_t open = t.find('<');
	if (open == std::string::npos || t.back() != '>') {
		return args;
	}
	std::string inner = t.substr(open + 1, t.size() - open - 2);
	int depth = 0;
	std::string cur;
	for (char c : inner) {
		if (c == '<' || c == '(') {
			depth++;
		} else if (c == '>' || c == ')') {
			depth--;
		}
		if (c == ',' && depth == 0) {
			args.push_back(cur);
			cur.clear();
		} else {
			cur += c;
		}
	}
	if (!cur.empty()) {
		args.push_back(cur);
	}
	for (auto& a : args) {
		size_t b = a.find_first_not_of(' ');
		size_t e = a.find_last_not_of(' ');
		a = b == std::string::npos ? "" : a.substr(b, e - b + 1);
	}
	return args;
}

// "fn(A B -- C D)" -> ins ["A","B"], outs ["C","D"]. Space-separated at top level.
inline bool splitFnType(const std::string& t, std::vector<std::string>& ins, std::vector<std::string>& outs) {
	if (t.size() < 5 || t.compare(0, 3, "fn(") != 0 || t.back() != ')') {
		return false;
	}
	std::string inner = t.substr(3, t.size() - 4);
	size_t sep = inner.find("--");
	std::string left = sep == std::string::npos ? inner : inner.substr(0, sep);
	std::string right = sep == std::string::npos ? "" : inner.substr(sep + 2);
	auto split = [](const std::string& part, std::vector<std::string>& out) {
		int depth = 0;
		std::string cur;
		for (char c : part) {
			if (c == '<' || c == '(') {
				depth++;
			} else if (c == '>' || c == ')') {
				depth--;
			}
			if (c == ' ' && depth == 0) {
				if (!cur.empty()) {
					out.push_back(cur);
					cur.clear();
				}
			} else {
				cur += c;
			}
		}
		if (!cur.empty()) {
			out.push_back(cur);
		}
	};
	split(left, ins);
	split(right, outs);
	return true;
}

inline bool isTypeParamName(const std::string& t, const std::vector<std::string>& typeParams) {
	return std::find(typeParams.begin(), typeParams.end(), t) != typeParams.end();
}

// The type name a stack element has when nothing more specific is known.
inline const char* qdTypeNameOf(StackValueType t) {
	switch (t) {
	case StackValueType::INT:
		return "i64";
	case StackValueType::FLOAT:
		return "f64";
	case StackValueType::STRING:
		return "str";
	default:
		return "";
	}
}

// Unify a declared type `expected` with an argument's type `actual` ("" when unknown), binding
// type parameters into `bindings`. Returns false on a mismatch; `why` then explains it.
// `canon` maps a struct name to its canonical spelling -- an unqualified name that refers to a
// module's struct becomes `module::Name` -- so `Flag` declared in a caller that did `use flag`
// unifies with the `flag::Flag` a value actually carries.
inline bool unifyTypeName(const std::string& expected, const std::string& actual,
		const std::vector<std::string>& typeParams, std::map<std::string, std::string>& bindings,
		const std::unordered_set<std::string>& mergedModules,
		const std::function<std::string(const std::string&)>& canon, std::string& why, bool strict = false) {
	if (expected.empty() || expected == "any" || actual.empty()) {
		return true; // the declaration accepts anything, or nothing is known about the argument
	}
	if (expected == "ptr") {
		// A declared `ptr` is the escape hatch, and accepts any pointer-shaped value: what a raw
		// buffer holds is the caller's business. The exception is a `[]T`, because an array value
		// does not point at its elements -- it points at the `qd_array_t` header, and magic,
		// refcount, length and capacity sit in front of the data. Handing one to a function that
		// takes `arr:ptr count:i64` therefore aims it at the header: `[3 1 2] -> a
		// a a len sort::ints` sorted magic, refcount and length into ascending order and left
		// `a len` reading the `QDAR` magic as the length, with the bounds check passing for any
		// index. A struct is not affected -- `qd_struct_alloc` returns the address of the fields,
		// with its header behind them -- so a struct handed to a `ptr` still points at its data.
		if (isArrayTypeName(actual)) {
			why = rawPtrArrayWhy(actual);
			return false;
		}
		return true;
	}
	if (!strict && (actual == "any" || actual == "ptr")) {
		// The argument's type is not known well enough to check: a bare `ptr`, or an `any`. This
		// applies only where the check started -- nested, `any` and `ptr` are real element types,
		// so an empty `[]` literal (`[]any`) does not satisfy a declared `[]i64`. It also does not
		// apply to a field initializer, which is a definite assignment rather than a value of
		// unknown provenance: `old_make -> x  Container { data = x }` with `x:ptr` stays an error.
		return true;
	}
	if (isTypeParamName(expected, typeParams)) {
		auto it = bindings.find(expected);
		if (it == bindings.end()) {
			bindings[expected] = actual;
			return true;
		}
		if (it->second == actual || canon(it->second) == canon(actual) ||
				structTypesMatch(actual, it->second, mergedModules)) {
			return true;
		}
		why = "type parameter " + expected + " is '" + it->second + "' from an earlier argument, but this one is '" +
			  actual + "'";
		return false;
	}
	bool expArr = isArrayTypeName(expected);
	bool actArr = isArrayTypeName(actual);
	if (expArr || actArr) {
		if (!(expArr && actArr)) {
			why = "expects type '" + expected + "' but got '" + actual + "'";
			return false;
		}
		if (!unifyTypeName(
					expected.substr(2), actual.substr(2), typeParams, bindings, mergedModules, canon, why, true)) {
			if (why.compare(0, 14, "type parameter") != 0) {
				why = "expects type '" + expected + "' but got '" + actual + "'";
			}
			return false;
		}
		return true;
	}
	bool expFn = expected.compare(0, 3, "fn(") == 0;
	bool actFn = actual.compare(0, 3, "fn(") == 0;
	if (expFn || actFn) {
		std::vector<std::string> ei, eo, ai, ao;
		if (!(expFn && actFn) || !splitFnType(expected, ei, eo) || !splitFnType(actual, ai, ao) ||
				ei.size() != ai.size() || eo.size() != ao.size()) {
			why = "expects type '" + expected + "' but got '" + actual + "'";
			return false;
		}
		for (size_t i = 0; i < ei.size(); i++) {
			if (!unifyTypeName(ei[i], ai[i], typeParams, bindings, mergedModules, canon, why, true)) {
				if (why.compare(0, 14, "type parameter") != 0) {
					why = "expects type '" + expected + "' but got '" + actual + "'";
				}
				return false;
			}
		}
		for (size_t i = 0; i < eo.size(); i++) {
			if (!unifyTypeName(eo[i], ao[i], typeParams, bindings, mergedModules, canon, why, true)) {
				if (why.compare(0, 14, "type parameter") != 0) {
					why = "expects type '" + expected + "' but got '" + actual + "'";
				}
				return false;
			}
		}
		return true;
	}
	std::string expBase = canon(typeBaseName(expected));
	std::string actBase = canon(typeBaseName(actual));
	if (actBase != expBase && !structTypesMatch(actBase, expBase, mergedModules)) {
		why = "expects type '" + expected + "' but got '" + actual + "'";
		return false;
	}
	std::vector<std::string> ea = typeArgsOf(expected), aa = typeArgsOf(actual);
	for (size_t i = 0; i < ea.size() && i < aa.size(); i++) {
		if (!unifyTypeName(ea[i], aa[i], typeParams, bindings, mergedModules, canon, why, true)) {
			if (why.compare(0, 14, "type parameter") != 0) {
				why = "expects type '" + expected + "' but got '" + actual + "'";
			}
			return false;
		}
	}
	return true;
}

// Replace every whole-identifier occurrence of a bound type parameter in `t`.
inline std::string substituteTypeParams(const std::string& t, const std::map<std::string, std::string>& bindings) {
	if (bindings.empty()) {
		return t;
	}
	std::string out;
	size_t i = 0;
	while (i < t.size()) {
		char c = t[i];
		if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
			size_t j = i;
			while (j < t.size() && (std::isalnum(static_cast<unsigned char>(t[j])) || t[j] == '_')) {
				j++;
			}
			std::string ident = t.substr(i, j - i);
			auto it = bindings.find(ident);
			out += it != bindings.end() ? it->second : ident;
			i = j;
		} else {
			out += c;
			i++;
		}
	}
	return out;
}

// Get all .qd files in a directory, sorted alphabetically
// Returns empty vector if directory doesn't exist or is empty
inline std::vector<std::string> globQdFiles(const std::string& directory) {
	std::vector<std::string> files;

	try {
		if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
			return files;
		}

		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			if (entry.is_regular_file()) {
				std::string filename = entry.path().filename().string();
				if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".qd") {
					// Exclude test files (*_test.qd) from module loading
					if (filename.size() > 8 && filename.substr(filename.size() - 8) == "_test.qd") {
						continue;
					}
					files.push_back(entry.path().string());
				}
			}
		}

		// Sort alphabetically for deterministic order
		std::sort(files.begin(), files.end());
	} catch (const std::filesystem::filesystem_error&) {
		// Ignore filesystem errors
	}

	return files;
}

#endif // QD_QC_SEMANTIC_VALIDATOR_INTERNAL_H

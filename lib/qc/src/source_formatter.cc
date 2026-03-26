#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <quadrate/qc/formatter.h>
#include <sstream>
#include <unistd.h>
#include <vector>

// Use simple manual JSON parsing to avoid jansson dependency in libqc
#define HAS_JANSSON 0

namespace Qd {

	// ============================================================
	// FormatOptions implementation
	// ============================================================

	static std::string findConfigFile(const std::string& startDir) {
		std::string dir = startDir;
		if (dir.empty()) {
			char* cwd = getcwd(nullptr, 0);
			if (cwd) {
				dir = cwd;
				free(cwd);
			} else {
				return "";
			}
		}

		// Walk up directory tree looking for .quadfmt.json
		while (!dir.empty() && dir != "/") {
			std::string configPath = dir + "/.quadfmt.json";
			std::ifstream f(configPath);
			if (f.good()) {
				return configPath;
			}
			// Go to parent directory
			size_t lastSlash = dir.rfind('/');
			if (lastSlash == std::string::npos) {
				break;
			}
			dir = dir.substr(0, lastSlash);
		}
		return "";
	}

	bool FormatOptions::configExists(const std::string& startDir) {
		return !findConfigFile(startDir).empty();
	}

	FormatOptions FormatOptions::loadFromFile(const std::string& startDir) {
		FormatOptions opts;
		std::string configPath = findConfigFile(startDir);
		if (configPath.empty()) {
			return opts;
		}

		// Simple manual JSON parsing for basic options
		std::ifstream f(configPath);
		std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

		// Look for "lineWidth": <number>
		size_t pos = content.find("\"lineWidth\"");
		if (pos != std::string::npos) {
			pos = content.find(':', pos);
			if (pos != std::string::npos) {
				pos++;
				while (pos < content.length() && std::isspace(content[pos])) {
					pos++;
				}
				int val = 0;
				while (pos < content.length() && std::isdigit(content[pos])) {
					val = val * 10 + (content[pos] - '0');
					pos++;
				}
				if (val > 0) {
					opts.lineWidth = val;
				}
			}
		}

		// Look for "sortImports": true/false
		pos = content.find("\"sortImports\"");
		if (pos != std::string::npos) {
			pos = content.find(':', pos);
			if (pos != std::string::npos) {
				if (content.find("false", pos) < content.find(',', pos) &&
						content.find("false", pos) < content.find('}', pos)) {
					opts.sortImports = false;
				}
			}
		}

		// Look for "alignStructFields": true/false
		pos = content.find("\"alignStructFields\"");
		if (pos != std::string::npos) {
			pos = content.find(':', pos);
			if (pos != std::string::npos) {
				if (content.find("false", pos) < content.find(',', pos) &&
						content.find("false", pos) < content.find('}', pos)) {
					opts.alignStructFields = false;
				}
			}
		}

		return opts;
	}

	// ============================================================
	// Block analysis helpers
	// ============================================================

	// Check if a block body represents a single statement
	// Single statement = should stay on one line
	// Rules:
	// - Empty block: single
	// - Single -> at end: single (e.g., "x 1 + -> x")
	// - Single action chain with no ->: single (e.g., "err panic", "print nl")
	// - Multiple -> or -> not at end: multiple statements
	static bool isSingleStatement(const std::string& body) {
		std::string trimmed = body;
		// Trim whitespace
		size_t start = 0;
		while (start < trimmed.length() && std::isspace(static_cast<unsigned char>(trimmed[start]))) {
			start++;
		}
		size_t end = trimmed.length();
		while (end > start && std::isspace(static_cast<unsigned char>(trimmed[end - 1]))) {
			end--;
		}
		trimmed = trimmed.substr(start, end - start);

		if (trimmed.empty()) {
			return true; // Empty block
		}

		// Count -> occurrences (outside strings)
		int arrowCount = 0;
		size_t lastArrowPos = std::string::npos;
		bool inString = false;
		for (size_t i = 0; i < trimmed.length(); i++) {
			if (trimmed[i] == '"' && (i == 0 || trimmed[i - 1] != '\\')) {
				inString = !inString;
			}
			if (!inString && i + 1 < trimmed.length() && trimmed[i] == '-' && trimmed[i + 1] == '>') {
				arrowCount++;
				lastArrowPos = i;
			}
		}

		// Multiple -> means multiple statements
		if (arrowCount > 1) {
			return false;
		}

		// Single -> must be "at the end" (only identifiers after it)
		if (arrowCount == 1 && lastArrowPos != std::string::npos) {
			// Check what comes after the ->
			std::string afterArrow = trimmed.substr(lastArrowPos + 2);
			// Trim leading space
			size_t s = 0;
			while (s < afterArrow.length() && std::isspace(static_cast<unsigned char>(afterArrow[s]))) {
				s++;
			}
			afterArrow = afterArrow.substr(s);

			// After -> should be just identifier(s) - no more operations
			// If there's anything other than identifiers/spaces, it's multiple statements
			for (size_t i = 0; i < afterArrow.length(); i++) {
				char c = afterArrow[i];
				if (!std::isalnum(c) && c != '_' && !std::isspace(static_cast<unsigned char>(c))) {
					return false; // Has operators or other stuff after ->
				}
			}
		}

		return true;
	}

	// Helper to trim whitespace from start and end
	static std::string trim(const std::string& str) {
		size_t start = 0;
		while (start < str.length() && std::isspace(static_cast<unsigned char>(str[start]))) {
			start++;
		}

		size_t end = str.length();
		while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
			end--;
		}

		return str.substr(start, end - start);
	}

	// Find position of substring outside of string literals
	// Returns std::string::npos if not found outside strings
	static size_t findOutsideStrings(const std::string& s, const std::string& needle) {
		bool inString = false;
		for (size_t i = 0; i < s.length(); i++) {
			if (s[i] == '"' && (i == 0 || s[i - 1] != '\\')) {
				inString = !inString;
			}
			if (!inString && i + needle.length() <= s.length()) {
				if (s.substr(i, needle.length()) == needle) {
					return i;
				}
			}
		}
		return std::string::npos;
	}

	// Check if a line is a comment (single-line or starts a block comment)
	static bool isComment(const std::string& line) {
		std::string trimmed = trim(line);
		return trimmed.length() >= 2 && (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*");
	}

	// Check if a line looks like a struct field definition (name: type)
	static bool isStructFieldDefinition(const std::string& line) {
		std::string trimmed = trim(line);
		// Must contain a colon
		size_t colonPos = trimmed.find(':');
		if (colonPos == std::string::npos || colonPos == 0) {
			return false;
		}
		// Must not be a comment
		if (isComment(trimmed)) {
			return false;
		}
		// Field name before colon must be a valid identifier
		std::string fieldName = trim(trimmed.substr(0, colonPos));
		if (fieldName.empty()) {
			return false;
		}
		// First char must be letter or underscore
		if (!std::isalpha(fieldName[0]) && fieldName[0] != '_') {
			return false;
		}
		return true;
	}

	// Parse a struct field definition line into name and type
	static std::pair<std::string, std::string> parseStructField(const std::string& line) {
		std::string trimmed = trim(line);
		size_t colonPos = trimmed.find(':');
		if (colonPos == std::string::npos) {
			return {"", trimmed};
		}
		std::string name = trim(trimmed.substr(0, colonPos));
		std::string type = trim(trimmed.substr(colonPos + 1));
		return {name, type};
	}

	// Format struct fields with aligned types (spaces for alignment, tabs for indent)
	static std::vector<std::string> formatStructFieldsAligned(const std::vector<std::string>& fields, int indentLevel) {
		if (fields.empty()) {
			return fields;
		}

		// Build indent string (tabs)
		std::string indent;
		for (int i = 0; i < indentLevel; i++) {
			indent += '\t';
		}

		// Find maximum field name length (including colon)
		size_t maxNameLen = 0;
		std::vector<std::pair<std::string, std::string>> parsed;
		for (const auto& field : fields) {
			std::string trimmed = trim(field);
			if (isStructFieldDefinition(trimmed)) {
				auto [name, type] = parseStructField(trimmed);
				parsed.push_back({name, type});
				if (name.length() > maxNameLen) {
					maxNameLen = name.length();
				}
			} else {
				// Not a field definition (comment, blank line, etc.)
				parsed.push_back({"", trimmed});
			}
		}

		// Format with alignment (spaces after colon for alignment)
		std::vector<std::string> result;
		for (const auto& [name, type] : parsed) {
			if (name.empty()) {
				// Not a field - output with indent
				if (!type.empty()) {
					result.push_back(indent + type);
				} else {
					result.push_back("");
				}
			} else {
				// Format: indent + name + ":" + spaces + type
				std::string formatted = indent + name + ":";
				// Add padding spaces after colon to align types
				size_t padding = maxNameLen - name.length();
				for (size_t i = 0; i < padding + 1; i++) {
					formatted += ' ';
				}
				formatted += type;
				result.push_back(formatted);
			}
		}
		return result;
	}

	// Check if a line is a shebang (#!)
	static bool isShebang(const std::string& line) {
		std::string trimmed = trim(line);
		return trimmed.length() >= 2 && trimmed.substr(0, 2) == "#!";
	}

	// Check if line starts with a keyword (after trimming), but skip if it's a comment
	static bool startsWithKeyword(const std::string& line, const std::string& keyword) {
		std::string trimmed = trim(line);
		// Don't detect keywords inside comments
		if (isComment(trimmed)) {
			return false;
		}
		if (trimmed.length() < keyword.length()) {
			return false;
		}
		if (trimmed.substr(0, keyword.length()) != keyword) {
			return false;
		}
		// Make sure it's followed by space or special char
		if (trimmed.length() > keyword.length()) {
			char next = trimmed[keyword.length()];
			if (std::isalnum(next) || next == '_') {
				return false;
			}
		}
		return true;
	}

	// Normalize whitespace in use statements (e.g., "use  os" -> "use os")
	static std::string normalizeUseStatement(const std::string& line) {
		std::string trimmed = trim(line);

		// Check if it's a use statement
		if (!startsWithKeyword(trimmed, "use")) {
			return trimmed;
		}

		// Find "use" keyword
		size_t usePos = trimmed.find("use");
		if (usePos == std::string::npos) {
			return trimmed;
		}

		// Skip "use" and any whitespace after it
		size_t pos = usePos + 3;
		while (pos < trimmed.length() && std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
			pos++;
		}

		// Extract module name (everything after whitespace)
		if (pos >= trimmed.length()) {
			return "use";
		}

		std::string moduleName = trimmed.substr(pos);
		return "use " + moduleName;
	}

	// Normalize whitespace in const statements (e.g., "const  PI=3.14" -> "const PI = 3.14")
	static std::string normalizeConstStatement(const std::string& line) {
		std::string trimmed = trim(line);

		// Check for optional "pub" keyword
		bool isPublic = startsWithKeyword(trimmed, "pub");
		std::string workingLine = trimmed;
		if (isPublic) {
			size_t pubEnd = trimmed.find("pub");
			if (pubEnd != std::string::npos) {
				workingLine = trim(trimmed.substr(pubEnd + 3));
			}
		}

		// Check if it's a const statement
		if (!startsWithKeyword(workingLine, "const")) {
			return trimmed;
		}

		// Find "const" keyword
		size_t constPos = workingLine.find("const");
		if (constPos == std::string::npos) {
			return trimmed;
		}

		// Skip "const" and any whitespace after it
		size_t pos = constPos + 5;
		while (pos < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[pos]))) {
			pos++;
		}

		if (pos >= workingLine.length()) {
			return isPublic ? "pub const" : "const";
		}

		// Extract constant name (identifier before =)
		size_t nameStart = pos;
		while (pos < workingLine.length() &&
				(std::isalnum(static_cast<unsigned char>(workingLine[pos])) || workingLine[pos] == '_')) {
			pos++;
		}

		if (pos == nameStart) {
			return trimmed; // No valid name found
		}

		std::string constName = workingLine.substr(nameStart, pos - nameStart);

		// Skip whitespace before =
		while (pos < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[pos]))) {
			pos++;
		}

		// Expect =
		if (pos >= workingLine.length() || workingLine[pos] != '=') {
			return trimmed; // No = found, return as-is
		}
		pos++; // Skip =

		// Skip whitespace after =
		while (pos < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[pos]))) {
			pos++;
		}

		// Extract value (rest of line)
		std::string value = trim(workingLine.substr(pos));

		// Build formatted result
		std::string result;
		if (isPublic) {
			result = "pub const " + constName + " = " + value;
		} else {
			result = "const " + constName + " = " + value;
		}

		return result;
	}

	// Format an anonymous function: fn (signature) { body } -> var
	static std::string formatAnonymousFunctionSignature(const std::string& line) {
		std::string trimmed = trim(line);

		// Must start with "fn"
		if (trimmed.length() < 4 || trimmed.substr(0, 2) != "fn") {
			return "";
		}

		// Skip "fn" and optional whitespace
		size_t pos = 2;
		while (pos < trimmed.length() && std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
			pos++;
		}

		// Must have '(' immediately (anonymous function, no name)
		if (pos >= trimmed.length() || trimmed[pos] != '(') {
			return "";
		}

		size_t parenPos = pos;

		// Find matching closing paren
		int depth = 0;
		size_t closeParenPos = parenPos;
		for (size_t i = parenPos; i < trimmed.length(); i++) {
			if (trimmed[i] == '(') {
				depth++;
			}
			if (trimmed[i] == ')') {
				depth--;
				if (depth == 0) {
					closeParenPos = i;
					break;
				}
			}
		}

		if (closeParenPos == parenPos) {
			return ""; // No closing paren found
		}

		// Extract signature part (everything between parens)
		std::string signature = trimmed.substr(parenPos + 1, closeParenPos - parenPos - 1);
		std::string formattedSig = trim(signature);

		// Find the "--" separator to determine if inputs/outputs are present
		size_t dashPos = formattedSig.find("--");
		bool hasInputs = false;
		bool hasOutputs = false;

		if (dashPos != std::string::npos) {
			std::string beforeDash = formattedSig.substr(0, dashPos);
			std::string trimmedInputs = trim(beforeDash);
			hasInputs = (trimmedInputs.length() > 0);

			std::string afterDash = formattedSig.substr(dashPos + 2);
			std::string trimmedOutputs = trim(afterDash);
			hasOutputs = (trimmedOutputs.length() > 0);

			// If both inputs and outputs are empty, use empty signature
			if (!hasInputs && !hasOutputs) {
				formattedSig = "";
			} else {
				std::string result;
				if (!hasInputs) {
					result = " --";
				} else {
					result = trimmedInputs + " --";
				}
				result += " ";
				if (hasOutputs) {
					result += trimmedOutputs;
				}
				formattedSig = result;
			}
		}

		// Look for opening brace after closing paren
		pos = closeParenPos + 1;
		while (pos < trimmed.length() && std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
			pos++;
		}

		if (pos >= trimmed.length() || trimmed[pos] != '{') {
			return ""; // No opening brace - not a complete anonymous function
		}

		// Find matching closing brace
		size_t braceStart = pos;
		depth = 0;
		size_t closeBracePos = braceStart;
		bool inString = false;
		for (size_t i = braceStart; i < trimmed.length(); i++) {
			char c = trimmed[i];
			if (c == '"' && (i == 0 || trimmed[i - 1] != '\\')) {
				inString = !inString;
			}
			if (!inString) {
				if (c == '{') {
					depth++;
				} else if (c == '}') {
					depth--;
					if (depth == 0) {
						closeBracePos = i;
						break;
					}
				}
			}
		}

		if (closeBracePos == braceStart) {
			return ""; // No closing brace found
		}

		// Extract body and normalize spacing around standalone operators
		std::string body = trim(trimmed.substr(braceStart + 1, closeBracePos - braceStart - 1));
		// Add space before standalone * / + - operators (stack instructions)
		std::string normalizedBody;
		for (size_t i = 0; i < body.length(); i++) {
			char c = body[i];
			if ((c == '*' || c == '/' || c == '+' || c == '-') && i > 0) {
				// Check if preceded by non-space (needs space before)
				if (!std::isspace(static_cast<unsigned char>(body[i - 1]))) {
					normalizedBody += ' ';
				}
			}
			normalizedBody += c;
		}
		body = normalizedBody;

		// Check for -> assignment after closing brace
		std::string afterBrace = trim(trimmed.substr(closeBracePos + 1));
		std::string assignment;
		if (afterBrace.length() >= 2 && afterBrace.substr(0, 2) == "->") {
			// Normalize: "->var" or "-> var" becomes " -> var"
			std::string varName = trim(afterBrace.substr(2));
			assignment = " -> " + varName;
		}

		// Format: fn (signature) { body }[ -> var]
		return "fn (" + formattedSig + ") { " + body + " }" + assignment;
	}

	// Check if a line is an anonymous function definition
	static bool isAnonymousFunction(const std::string& line) {
		std::string trimmed = trim(line);
		if (trimmed.length() < 4 || trimmed.substr(0, 2) != "fn") {
			return false;
		}
		// Skip "fn" and optional whitespace
		size_t pos = 2;
		while (pos < trimmed.length() && std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
			pos++;
		}
		// Anonymous function starts with '(' immediately after "fn" (with or without space)
		return pos < trimmed.length() && trimmed[pos] == '(';
	}

	// Format a function signature line
	// Helper to find matching closing bracket for angle brackets
	static size_t findMatchingAngleBracket(const std::string& s, size_t openPos) {
		int depth = 0;
		for (size_t i = openPos; i < s.length(); i++) {
			if (s[i] == '<') {
				depth++;
			} else if (s[i] == '>') {
				depth--;
				if (depth == 0) {
					return i;
				}
			}
		}
		return std::string::npos;
	}

	// Helper to find matching closing paren
	static size_t findMatchingParen(const std::string& s, size_t openPos) {
		int depth = 0;
		for (size_t i = openPos; i < s.length(); i++) {
			if (s[i] == '(') {
				depth++;
			} else if (s[i] == ')') {
				depth--;
				if (depth == 0) {
					return i;
				}
			}
		}
		return std::string::npos;
	}

	static std::string formatFunctionSignature(const std::string& line) {
		std::string trimmed = trim(line);

		// Check for optional "pub" keyword
		bool isPublic = startsWithKeyword(trimmed, "pub");
		std::string workingLine = trimmed;
		if (isPublic) {
			// Skip past "pub " to find "fn"
			size_t pubEnd = trimmed.find("pub");
			if (pubEnd != std::string::npos) {
				workingLine = trim(trimmed.substr(pubEnd + 3));
			}
		}

		// Must start with "fn " (after optional "pub")
		if (!startsWithKeyword(workingLine, "fn")) {
			return line;
		}

		// Find "fn " position
		size_t fnPos = workingLine.find("fn ");
		if (fnPos == std::string::npos) {
			return line;
		}

		size_t pos = fnPos + 3;
		while (pos < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[pos]))) {
			pos++;
		}

		// Check for receiver: (receiverName:Type<Params>)
		std::string receiver;
		if (pos < workingLine.length() && workingLine[pos] == '(') {
			// Find matching close paren for receiver
			size_t closeReceiverPos = findMatchingParen(workingLine, pos);
			if (closeReceiverPos != std::string::npos) {
				// Extract receiver content
				std::string receiverContent = workingLine.substr(pos + 1, closeReceiverPos - pos - 1);
				receiver = "(" + trim(receiverContent) + ") ";
				pos = closeReceiverPos + 1;
				// Skip whitespace after receiver
				while (pos < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[pos]))) {
					pos++;
				}
			}
		}

		// Extract function name (up to '<' for type params or '(' for params)
		size_t nameStart = pos;
		size_t nameEnd = pos;
		while (nameEnd < workingLine.length()) {
			char c = workingLine[nameEnd];
			if (c == '<' || c == '(' || std::isspace(static_cast<unsigned char>(c))) {
				break;
			}
			nameEnd++;
		}

		std::string name = workingLine.substr(nameStart, nameEnd - nameStart);

		// If name is empty, this is an anonymous function - delegate to that formatter
		if (name.empty()) {
			return formatAnonymousFunctionSignature(trimmed);
		}

		pos = nameEnd;
		while (pos < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[pos]))) {
			pos++;
		}

		// Check for type parameters: <T, U>
		std::string typeParams;
		if (pos < workingLine.length() && workingLine[pos] == '<') {
			size_t closeAnglePos = findMatchingAngleBracket(workingLine, pos);
			if (closeAnglePos != std::string::npos) {
				std::string typeParamsContent = workingLine.substr(pos + 1, closeAnglePos - pos - 1);
				typeParams = "<" + trim(typeParamsContent) + ">";
				pos = closeAnglePos + 1;
				// Skip whitespace after type params
				while (pos < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[pos]))) {
					pos++;
				}
			}
		}

		// Now we should be at the parameter list '('
		if (pos >= workingLine.length() || workingLine[pos] != '(') {
			return line; // Malformed
		}

		size_t parenPos = pos;

		// Find matching closing paren for parameters
		size_t closeParenPos = findMatchingParen(workingLine, parenPos);
		if (closeParenPos == std::string::npos) {
			return line; // No closing paren found
		}

		// Extract signature part (everything between parens)
		std::string signature = workingLine.substr(parenPos + 1, closeParenPos - parenPos - 1);
		std::string formattedSig = trim(signature);

		// Find the "--" separator to determine if inputs/outputs are present
		size_t dashPos = formattedSig.find("--");
		bool hasInputs = false;
		bool hasOutputs = false;

		if (dashPos != std::string::npos) {
			// Check if there are non-whitespace characters before "--"
			std::string beforeDash = formattedSig.substr(0, dashPos);
			std::string trimmedInputs = trim(beforeDash);
			hasInputs = (trimmedInputs.length() > 0);

			// Check if there are non-whitespace characters after "--"
			std::string afterDash = formattedSig.substr(dashPos + 2);
			std::string trimmedOutputs = trim(afterDash);
			hasOutputs = (trimmedOutputs.length() > 0);

			// If both inputs and outputs are empty, use empty signature
			if (!hasInputs && !hasOutputs) {
				formattedSig = "";
			} else {
				// Build formatted signature with proper spacing rules:
				// - Space after '(' if no inputs
				// - Always space before and after '--'
				// - Space before ')' if no outputs (provided by space after '--')
				std::string result;

				// Build: [space] + inputs + space + "--"
				if (!hasInputs) {
					result = " --";
				} else {
					result = trimmedInputs + " --";
				}

				// Add space after '--'
				result += " ";

				// Add outputs if present (space after '--' becomes space before ')')
				if (hasOutputs) {
					result += trimmedOutputs;
				}

				formattedSig = result;
			}
		}

		// Check for '!' after the closing paren (error-returning function)
		std::string suffix;
		pos = closeParenPos + 1;
		while (pos < workingLine.length() && std::isspace(workingLine[pos])) {
			pos++;
		}
		if (pos < workingLine.length() && workingLine[pos] == '!') {
			suffix = "!";
			pos++;
		}

		// Check for opening brace after any suffix
		size_t bracePos = workingLine.find('{', pos);
		bool hasBrace = (bracePos != std::string::npos);

		// Format: [pub] fn [receiver] name[<typeParams>]( params )! {
		std::string result;
		if (isPublic) {
			result = "pub fn " + receiver + name + typeParams + "(" + formattedSig + ")" + suffix;
		} else {
			result = "fn " + receiver + name + typeParams + "(" + formattedSig + ")" + suffix;
		}
		if (hasBrace) {
			result += " {";
		}

		return result;
	}

	// Format a test block line: test "name" {
	static std::string formatTestBlock(const std::string& line) {
		std::string trimmed = trim(line);

		// Must start with "test "
		if (!startsWithKeyword(trimmed, "test")) {
			return line;
		}

		// Find the test name in quotes
		size_t quoteStart = trimmed.find('"');
		if (quoteStart == std::string::npos) {
			return line;
		}

		size_t quoteEnd = trimmed.find('"', quoteStart + 1);
		if (quoteEnd == std::string::npos) {
			return line;
		}

		std::string testName = trimmed.substr(quoteStart, quoteEnd - quoteStart + 1);

		// Check for opening brace
		size_t bracePos = trimmed.find('{', quoteEnd);
		bool hasBrace = (bracePos != std::string::npos);

		// Format: test "name" {
		std::string result = "test " + testName;
		if (hasBrace) {
			result += " {";
		}

		return result;
	}

	// Check if a string looks like a struct name (last component starts with uppercase letter)
	// Handles both "Point" and "math::Vec3" (scoped identifiers)
	static bool isStructName(const std::string& name) {
		if (name.empty()) {
			return false;
		}
		// Find the last component after ::
		size_t lastColon = name.rfind("::");
		std::string lastComponent = (lastColon != std::string::npos) ? name.substr(lastColon + 2) : name;
		if (lastComponent.empty()) {
			return false;
		}
		return std::isupper(static_cast<unsigned char>(lastComponent[0]));
	}

	// Format struct construction with fields on separate lines
	// Input: "StructName { field1: expr1 field2: expr2 }" or "StructName { field1: expr1 field2: expr2 } -> var"
	// Output: multiline formatted version
	static std::string formatStructConstruction(const std::string& line, size_t baseIndent) {
		std::string trimmed = trim(line);

		// Find potential struct name (uppercase identifier followed by {)
		size_t bracePos = trimmed.find('{');
		if (bracePos == std::string::npos || bracePos == 0) {
			return "";
		}

		// Extract what comes before the brace
		std::string beforeBrace = trim(trimmed.substr(0, bracePos));
		if (beforeBrace.empty() || !isStructName(beforeBrace)) {
			return "";
		}

		// Check if this looks like an identifier or scoped identifier (e.g., math::Vec3)
		for (char c : beforeBrace) {
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != ':') {
				return "";
			}
		}

		std::string structName = beforeBrace;

		// Find matching closing brace
		int depth = 0;
		size_t closePos = std::string::npos;
		bool inString = false;
		for (size_t i = bracePos; i < trimmed.length(); i++) {
			char c = trimmed[i];
			if (c == '"' && (i == 0 || trimmed[i - 1] != '\\')) {
				inString = !inString;
			}
			if (!inString) {
				if (c == '{') {
					depth++;
				} else if (c == '}') {
					depth--;
					if (depth == 0) {
						closePos = i;
						break;
					}
				}
			}
		}

		if (closePos == std::string::npos) {
			return "";
		}

		// Extract content between braces
		std::string content = trimmed.substr(bracePos + 1, closePos - bracePos - 1);
		std::string afterBrace = trim(trimmed.substr(closePos + 1));

		// Parse fields: "field1: expr1 field2: expr2"
		// Fields are separated by the pattern "identifier:"
		std::vector<std::pair<std::string, std::string>> fields;
		std::string currentField;
		std::string currentValue;
		bool parsingField = true;
		bool inStr = false;
		int braceDepth = 0;

		content = trim(content);
		if (content.empty()) {
			return ""; // Empty struct construction, don't reformat
		}

		size_t i = 0;
		while (i < content.length()) {
			char c = content[i];

			// Track string state
			if (c == '"' && (i == 0 || content[i - 1] != '\\')) {
				inStr = !inStr;
			}

			// Track brace depth (for nested structs)
			if (!inStr) {
				if (c == '{') {
					braceDepth++;
				} else if (c == '}') {
					braceDepth--;
				}
			}

			if (parsingField) {
				if (c == '=' && !inStr && braceDepth == 0) {
					currentField = trim(currentField);
					parsingField = false;
					i++;
					continue;
				}
				currentField += c;
			} else {
				// Check if we've hit a new field (identifier starting with letter, followed by =)
				if (!inStr && braceDepth == 0 && std::isspace(static_cast<unsigned char>(c))) {
					// Look ahead to see if next non-space is a field name
					size_t j = i + 1;
					while (j < content.length() && std::isspace(static_cast<unsigned char>(content[j]))) {
						j++;
					}
					// Field names must start with a letter (not a digit)
					if (j < content.length() && std::isalpha(static_cast<unsigned char>(content[j]))) {
						// Check if it's an identifier followed by =
						size_t identStart = j;
						while (j < content.length() &&
								(std::isalnum(static_cast<unsigned char>(content[j])) || content[j] == '_')) {
							j++;
						}
						// Skip whitespace between identifier and =
						while (j < content.length() && std::isspace(static_cast<unsigned char>(content[j]))) {
							j++;
						}
						if (j > identStart && j < content.length() && content[j] == '=') {
							// Found next field, save current
							fields.push_back({currentField, trim(currentValue)});
							currentField.clear();
							currentValue.clear();
							parsingField = true;
							i = identStart;
							continue;
						}
					}
				}
				currentValue += c;
			}
			i++;
		}

		// Save last field
		if (!currentField.empty()) {
			fields.push_back({currentField, trim(currentValue)});
		}

		// If only one field, don't reformat
		if (fields.size() <= 1) {
			return "";
		}

		// Build formatted output
		std::ostringstream result;
		std::string indent(baseIndent, '\t');
		std::string fieldIndent(baseIndent + 1, '\t');

		result << indent << structName << " {\n";
		for (const auto& field : fields) {
			// Check if the value is a nested struct that needs formatting
			std::string nestedFormatted = formatStructConstruction(field.second, baseIndent + 1);
			if (!nestedFormatted.empty()) {
				// Nested struct was formatted - output field name and nested struct
				result << fieldIndent << field.first << " = " << trim(nestedFormatted) << "\n";
			} else {
				result << fieldIndent << field.first << " = " << field.second << "\n";
			}
		}
		result << indent << "}";

		if (!afterBrace.empty()) {
			// Normalize -> assignment: "->var" or "-> var" becomes " -> var"
			if (afterBrace.length() >= 2 && afterBrace.substr(0, 2) == "->") {
				std::string varName = trim(afterBrace.substr(2));
				result << " -> " << varName;
			} else {
				result << " " << afterBrace;
			}
		}

		return result.str();
	}

	// Normalize spacing around operators like ++ and --
	// Ensures single space before and after, unless at end of line
	static std::string normalizeOperatorSpacing(const std::string& line) {
		std::string result;
		bool inString = false;
		bool inLineComment = false;
		bool inBlockComment = false;

		for (size_t i = 0; i < line.length(); i++) {
			char c = line[i];

			// Track string literals
			if (!inLineComment && !inBlockComment && c == '"' && (i == 0 || line[i - 1] != '\\')) {
				inString = !inString;
				result += c;
				continue;
			}

			// Track comments
			if (!inString && !inLineComment && !inBlockComment && i + 1 < line.length()) {
				if (c == '/' && line[i + 1] == '/') {
					inLineComment = true;
				} else if (c == '/' && line[i + 1] == '*') {
					inBlockComment = true;
				}
			}
			if (inBlockComment && i > 0 && line[i - 1] == '*' && c == '/') {
				inBlockComment = false;
				result += c;
				continue;
			}

			// Don't modify content inside strings or comments
			if (inString || inLineComment || inBlockComment) {
				result += c;
				continue;
			}

			// Check for ++ operator
			if (c == '+' && i + 1 < line.length() && line[i + 1] == '+') {
				// Remove trailing whitespace from result, then add single space
				while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
					result.pop_back();
				}
				if (!result.empty()) {
					result += ' ';
				}
				result += "++";
				i++; // Skip second +

				// Check if there's more non-whitespace content after
				size_t next = i + 1;
				while (next < line.length() && std::isspace(static_cast<unsigned char>(line[next]))) {
					next++;
				}
				// Add space after if there's more content (that's not a closing brace/paren or end of line)
				if (next < line.length() && line[next] != ')' && line[next] != '}') {
					result += ' ';
					// Skip any existing whitespace
					while (i + 1 < line.length() && std::isspace(static_cast<unsigned char>(line[i + 1]))) {
						i++;
					}
				}
				continue;
			}

			// Check for -- operator
			if (c == '-' && i + 1 < line.length() && line[i + 1] == '-') {
				// Remove trailing whitespace from result, then add single space
				while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
					result.pop_back();
				}
				if (!result.empty()) {
					result += ' ';
				}
				result += "--";
				i++; // Skip second -

				// Check if there's more non-whitespace content after
				size_t next = i + 1;
				while (next < line.length() && std::isspace(static_cast<unsigned char>(line[next]))) {
					next++;
				}
				// Add space after if there's more content (that's not a closing brace/paren or end of line)
				if (next < line.length() && line[next] != ')' && line[next] != '}') {
					result += ' ';
					// Skip any existing whitespace
					while (i + 1 < line.length() && std::isspace(static_cast<unsigned char>(line[i + 1]))) {
						i++;
					}
				}
				continue;
			}

			// Check for << operator (shift left)
			if (c == '<' && i + 1 < line.length() && line[i + 1] == '<') {
				// Remove trailing whitespace from result, then add single space
				while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
					result.pop_back();
				}
				if (!result.empty()) {
					result += ' ';
				}
				result += "<<";
				i++; // Skip second <

				// Check if there's more non-whitespace content after
				size_t next = i + 1;
				while (next < line.length() && std::isspace(static_cast<unsigned char>(line[next]))) {
					next++;
				}
				// Add space after if there's more content (that's not a closing brace/paren or end of line)
				if (next < line.length() && line[next] != ')' && line[next] != '}') {
					result += ' ';
					// Skip any existing whitespace
					while (i + 1 < line.length() && std::isspace(static_cast<unsigned char>(line[i + 1]))) {
						i++;
					}
				}
				continue;
			}

			// Check for >> operator (shift right)
			if (c == '>' && i + 1 < line.length() && line[i + 1] == '>') {
				// Remove trailing whitespace from result, then add single space
				while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
					result.pop_back();
				}
				if (!result.empty()) {
					result += ' ';
				}
				result += ">>";
				i++; // Skip second >

				// Check if there's more non-whitespace content after
				size_t next = i + 1;
				while (next < line.length() && std::isspace(static_cast<unsigned char>(line[next]))) {
					next++;
				}
				// Add space after if there's more content (that's not a closing brace/paren or end of line)
				if (next < line.length() && line[next] != ')' && line[next] != '}') {
					result += ' ';
					// Skip any existing whitespace
					while (i + 1 < line.length() && std::isspace(static_cast<unsigned char>(line[i + 1]))) {
						i++;
					}
				}
				continue;
			}

			result += c;
		}

		return result;
	}

	// Normalize }else to } else and add spacing
	static std::string normalizeElse(const std::string& line) {
		std::string result = line;

		// First handle }else{ -> } else {
		size_t pos = 0;
		while ((pos = result.find("}else{", pos)) != std::string::npos) {
			result.replace(pos, 6, "} else {");
			pos += 8;
		}

		// Then handle }else (without brace after)
		pos = 0;
		while ((pos = result.find("}else", pos)) != std::string::npos) {
			// Check if this is really }else (not part of a larger word)
			if (pos + 5 >= result.length() || !std::isalnum(result[pos + 5])) {
				result.replace(pos, 5, "} else");
				pos += 6;
			} else {
				pos++;
			}
		}
		return result;
	}

	// Normalize keyword{ to keyword { (add space before opening brace)
	static std::string normalizeKeywordBraces(const std::string& line) {
		std::string result = line;
		const std::vector<std::string> keywords = {
				"if", "else", "for", "loop", "defer", "switch", "case", "default", "fn"};

		for (const auto& keyword : keywords) {
			std::string pattern = keyword + "{";
			size_t pos = 0;
			while ((pos = result.find(pattern, pos)) != std::string::npos) {
				// Check if this is a standalone keyword (not part of a larger word)
				bool validStart = (pos == 0 || !std::isalnum(static_cast<unsigned char>(result[pos - 1])));
				if (validStart) {
					result.replace(pos, pattern.length(), keyword + " {");
					pos += keyword.length() + 2;
				} else {
					pos++;
				}
			}
		}
		return result;
	}

	// Check if a line is a struct construction (StructName { ... })
	static bool isStructConstruction(const std::string& line) {
		std::string trimmed = trim(line);
		size_t bracePos = trimmed.find('{');
		if (bracePos == std::string::npos || bracePos == 0) {
			return false;
		}

		std::string beforeBrace = trim(trimmed.substr(0, bracePos));
		if (beforeBrace.empty() || !isStructName(beforeBrace)) {
			return false;
		}

		// Check it's a simple identifier or scoped identifier (e.g., math::Vec3)
		for (char c : beforeBrace) {
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != ':') {
				return false;
			}
		}

		// Check it has field = value pattern inside
		size_t equalsPos = trimmed.find('=', bracePos);
		return equalsPos != std::string::npos;
	}

	// Split inline braces onto separate lines
	// e.g., "if { foo } else { bar }" becomes:
	// "if {"
	// "foo"
	// "} else {"
	// "bar"
	// "}"
	static std::string splitInlineBraces(const std::string& line) {
		std::string trimmed = trim(line);

		// Don't process comments or lines containing block comment markers
		// (they may be inside multi-line comments)
		if (isComment(trimmed) || findOutsideStrings(trimmed, "*/") != std::string::npos ||
				findOutsideStrings(trimmed, "/*") != std::string::npos) {
			return line;
		}

		// Don't process lines without braces
		if (trimmed.find('{') == std::string::npos && trimmed.find('}') == std::string::npos) {
			return line;
		}

		// Don't split struct constructions - they'll be formatted later
		if (isStructConstruction(trimmed)) {
			return line;
		}

		// Don't split anonymous functions - they're formatted as single lines
		if (isAnonymousFunction(trimmed)) {
			return line;
		}

		std::ostringstream result;
		bool inString = false;
		bool inBlockComment = false;
		std::string current;
		int singleStatementBlockDepth = 0; // Track nested single-statement blocks

		for (size_t i = 0; i < trimmed.length(); i++) {
			char c = trimmed[i];

			// Track block comments
			if (!inString && i + 1 < trimmed.length()) {
				if (c == '/' && trimmed[i + 1] == '*') {
					inBlockComment = true;
				} else if (c == '*' && trimmed[i + 1] == '/') {
					current += c;
					current += trimmed[i + 1];
					i++;
					inBlockComment = false;
					continue;
				}
			}

			// Track string literals to avoid splitting inside them
			if (!inBlockComment && c == '"' && (i == 0 || trimmed[i - 1] != '\\')) {
				inString = !inString;
			}

			if (inString || inBlockComment) {
				current += c;
				continue;
			}

			if (c == '{') {
				current += c;
				// Check if there's non-whitespace content after the brace
				size_t next = i + 1;
				while (next < trimmed.length() && std::isspace(trimmed[next])) {
					next++;
				}
				if (next < trimmed.length() && trimmed[next] != '}') {
					// There's content after {, check if block is single statement
					// Find the matching closing brace
					size_t closingBrace = std::string::npos;
					int depth = 1;
					bool inStr = false;
					for (size_t j = next; j < trimmed.length(); j++) {
						if (trimmed[j] == '"' && (j == 0 || trimmed[j - 1] != '\\')) {
							inStr = !inStr;
						}
						if (!inStr) {
							if (trimmed[j] == '{') {
								depth++;
							} else if (trimmed[j] == '}') {
								depth--;
								if (depth == 0) {
									closingBrace = j;
									break;
								}
							}
						}
					}

					// If we found matching brace, check if content is single statement
					if (closingBrace != std::string::npos) {
						std::string blockContent = trimmed.substr(next, closingBrace - next);
						if (isSingleStatement(blockContent)) {
							// Single statement - don't split, continue processing
							// (the whole block stays on one line)
							singleStatementBlockDepth++;
						} else {
							// Multiple statements - split here
							result << trim(current) << '\n';
							current.clear();
							i = next - 1; // Will be incremented by loop
						}
					} else {
						// No matching brace found (multi-line block), split here
						result << trim(current) << '\n';
						current.clear();
						i = next - 1; // Will be incremented by loop
					}
				}
			} else if (c == '}') {
				// Check if we're closing a single-statement block
				if (singleStatementBlockDepth > 0) {
					// Don't split - keep everything on one line
					singleStatementBlockDepth--;
					current += c;

					// Still need to check what comes after } (else block, etc.)
					size_t next = i + 1;
					while (next < trimmed.length() && std::isspace(trimmed[next])) {
						next++;
					}

					if (next < trimmed.length()) {
						std::string remaining = trimmed.substr(next);
						if (remaining.length() >= 4 && remaining.substr(0, 4) == "else") {
							// Find the opening brace after else
							size_t elseEnd = next + 4;
							while (elseEnd < trimmed.length() && std::isspace(trimmed[elseEnd])) {
								elseEnd++;
							}
							if (elseEnd < trimmed.length() && trimmed[elseEnd] == '{') {
								// It's "} else {" - keep building
								current += " else {";
								i = elseEnd; // Skip to after the {

								// Check if else block is also single-statement
								size_t afterBrace = elseEnd + 1;
								while (afterBrace < trimmed.length() && std::isspace(trimmed[afterBrace])) {
									afterBrace++;
								}
								if (afterBrace < trimmed.length() && trimmed[afterBrace] != '}') {
									// Find closing brace for else block
									size_t elseClosingBrace = std::string::npos;
									int depth = 1;
									bool inStr = false;
									for (size_t j = afterBrace; j < trimmed.length(); j++) {
										if (trimmed[j] == '"' && (j == 0 || trimmed[j - 1] != '\\')) {
											inStr = !inStr;
										}
										if (!inStr) {
											if (trimmed[j] == '{') {
												depth++;
											} else if (trimmed[j] == '}') {
												depth--;
												if (depth == 0) {
													elseClosingBrace = j;
													break;
												}
											}
										}
									}

									if (elseClosingBrace != std::string::npos) {
										std::string elseContent =
												trimmed.substr(afterBrace, elseClosingBrace - afterBrace);
										if (isSingleStatement(elseContent)) {
											// Else is also single statement, keep on one line
											singleStatementBlockDepth++;
										} else {
											// Else has multiple statements, split
											result << trim(current) << '\n';
											current.clear();
											i = afterBrace - 1;
										}
									} else {
										// No closing brace, split
										result << trim(current) << '\n';
										current.clear();
										i = afterBrace - 1;
									}
								}
							}
						}
						// For other content after }, let normal processing handle it
					}
				} else {
					// Check if there's non-whitespace content before the brace
					std::string beforeBrace = trim(current);
					if (!beforeBrace.empty()) {
						// There's content before }, put it on its own line
						result << beforeBrace << '\n';
						current.clear();
					}
					current += c;

					// Check what comes after }
					size_t next = i + 1;
					while (next < trimmed.length() && std::isspace(trimmed[next])) {
						next++;
					}

					if (next < trimmed.length()) {
						// Check if it's "else {" pattern
						std::string remaining = trimmed.substr(next);
						if (remaining.length() >= 4 && remaining.substr(0, 4) == "else") {
							// Find the opening brace after else
							size_t elseEnd = next + 4;
							while (elseEnd < trimmed.length() && std::isspace(trimmed[elseEnd])) {
								elseEnd++;
							}
							if (elseEnd < trimmed.length() && trimmed[elseEnd] == '{') {
								// It's "} else {" - emit as one line
								current = "} else {";
								i = elseEnd; // Skip to after the {

								// Check if there's content after this {
								size_t afterBrace = elseEnd + 1;
								while (afterBrace < trimmed.length() && std::isspace(trimmed[afterBrace])) {
									afterBrace++;
								}
								if (afterBrace < trimmed.length() && trimmed[afterBrace] != '}') {
									// Check if else block is single statement
									size_t elseClosingBrace = std::string::npos;
									int depth = 1;
									bool inStr = false;
									for (size_t j = afterBrace; j < trimmed.length(); j++) {
										if (trimmed[j] == '"' && (j == 0 || trimmed[j - 1] != '\\')) {
											inStr = !inStr;
										}
										if (!inStr) {
											if (trimmed[j] == '{') {
												depth++;
											} else if (trimmed[j] == '}') {
												depth--;
												if (depth == 0) {
													elseClosingBrace = j;
													break;
												}
											}
										}
									}

									if (elseClosingBrace != std::string::npos) {
										std::string elseContent =
												trimmed.substr(afterBrace, elseClosingBrace - afterBrace);
										if (isSingleStatement(elseContent)) {
											// Else is single statement, keep on one line
											singleStatementBlockDepth++;
										} else {
											// Split
											result << current << '\n';
											current.clear();
											i = afterBrace - 1;
										}
									} else {
										// Split
										result << current << '\n';
										current.clear();
										i = afterBrace - 1;
									}
								}
							} else {
								// "else" without { - just output } and continue
								result << trim(current) << '\n';
								current.clear();
								i = next - 1;
							}
						} else {
							// There's other content after }, split here
							result << trim(current) << '\n';
							current.clear();
							i = next - 1;
						}
					}
				}
			} else {
				current += c;
			}
		}

		// Add any remaining content
		std::string remaining = trim(current);
		if (!remaining.empty()) {
			result << remaining << '\n';
		}

		std::string output = result.str();
		// Remove trailing newline if present
		if (!output.empty() && output.back() == '\n') {
			output.pop_back();
		}
		return output;
	}

	// Preprocess source to split inline braces and normalize
	static std::string preprocessBraces(const std::string& source) {
		std::istringstream input(source);
		std::ostringstream output;
		std::string line;
		bool inBlockComment = false;

		while (std::getline(input, line)) {
			// Track block comment state across lines
			std::string trimmed = trim(line);

			// Check if we enter or exit a block comment (only outside string literals)
			size_t openPos = findOutsideStrings(trimmed, "/*");
			size_t closePos = findOutsideStrings(trimmed, "*/");

			if (inBlockComment) {
				// We're inside a block comment, don't process
				output << line << '\n';
				if (closePos != std::string::npos && (openPos == std::string::npos || closePos > openPos)) {
					inBlockComment = false;
				}
				continue;
			}

			// Check if this line starts a block comment that doesn't end on same line
			if (openPos != std::string::npos) {
				if (closePos == std::string::npos || closePos < openPos) {
					// Block comment starts but doesn't end
					inBlockComment = true;
					output << line << '\n';
					continue;
				}
			}

			std::string processed = splitInlineBraces(line);
			output << processed << '\n';
		}
		return output.str();
	}

	// Count unmatched opening braces in a string (accounting for strings)
	static int countUnmatchedBraces(const std::string& s) {
		int depth = 0;
		bool inString = false;
		for (size_t i = 0; i < s.length(); i++) {
			char c = s[i];
			if (c == '"' && (i == 0 || s[i - 1] != '\\')) {
				inString = !inString;
			}
			if (!inString) {
				if (c == '{') {
					depth++;
				} else if (c == '}') {
					depth--;
				}
			}
		}
		return depth;
	}

	// Merge multi-line struct constructions back into single lines for reformatting
	// Detects: StructName { field = value ...\n } and merges them
	static std::string mergeStructConstructions(const std::string& source) {
		std::istringstream input(source);
		std::vector<std::string> lines;
		std::string line;

		while (std::getline(input, line)) {
			lines.push_back(line);
		}

		std::ostringstream output;
		size_t i = 0;
		while (i < lines.size()) {
			std::string trimmed = trim(lines[i]);

			// Check if this line starts a struct construction with unclosed brace
			// Look for "StructName { field = value" pattern with unclosed braces
			size_t bracePos = trimmed.find('{');
			if (bracePos != std::string::npos && bracePos > 0) {
				std::string beforeBrace = trim(trimmed.substr(0, bracePos));
				if (!beforeBrace.empty() && isStructName(beforeBrace)) {
					// Check if it's a simple identifier or scoped identifier (e.g., math::Vec3)
					bool isIdent = true;
					for (size_t k = 0; k < beforeBrace.length(); k++) {
						char c = beforeBrace[k];
						if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != ':') {
							isIdent = false;
							break;
						}
					}

					// Check if this line has unclosed braces (struct continues on next line)
					int unclosed = countUnmatchedBraces(trimmed);
					if (isIdent && unclosed > 0) {
						// Look ahead for field lines and closing brace
						std::string merged = trimmed;
						size_t j = i + 1;
						bool foundClose = false;
						int braceDepth = unclosed; // Track nested brace depth

						while (j < lines.size() && braceDepth > 0) {
							std::string nextTrimmed = trim(lines[j]);

							// Count braces in this line
							int lineBraces = countUnmatchedBraces(nextTrimmed);
							braceDepth += lineBraces;

							if (braceDepth == 0) {
								// Found the matching closing brace
								merged += " " + nextTrimmed;
								foundClose = true;
								j++;
								break;
							} else if (!nextTrimmed.empty() && !isComment(nextTrimmed)) {
								// Content line (field, nested struct, or closing brace of nested struct)
								merged += " " + nextTrimmed;
								j++;
							} else {
								// Empty or comment line, skip but don't stop
								j++;
							}
						}

						if (foundClose) {
							output << merged << '\n';
							i = j;
							continue;
						}
					}
				}
			}

			output << lines[i] << '\n';
			i++;
		}

		return output.str();
	}

	// Preprocess source to merge standalone opening braces with previous line
	static std::string mergeStandaloneBraces(const std::string& source) {
		std::istringstream input(source);
		std::vector<std::string> lines;
		std::string line;

		while (std::getline(input, line)) {
			// Normalize }else to } else and keyword{ to keyword {
			line = normalizeElse(line);
			line = normalizeKeywordBraces(line);
			lines.push_back(line);
		}

		// Merge lines where a line is followed by a standalone "{"
		std::vector<std::string> mergedLines;
		for (size_t i = 0; i < lines.size(); i++) {
			std::string trimmed = trim(lines[i]);
			if (i + 1 < lines.size()) {
				std::string nextTrimmed = trim(lines[i + 1]);
				if (nextTrimmed == "{" && !trimmed.empty()) {
					mergedLines.push_back(trimmed + " {");
					i++; // Skip the next line (the standalone brace)
					continue;
				}
			}
			mergedLines.push_back(lines[i]);
		}

		// Reconstruct source
		std::ostringstream output;
		for (const auto& l : mergedLines) {
			output << l << '\n';
		}
		return output.str();
	}

	// Normalize spacing between top-level declarations and optionally sort use statements
	static std::string normalizeTopLevelSpacing(const std::string& source, const FormatOptions& opts) {
		std::istringstream input(source);
		std::vector<std::string> lines;
		std::string line;

		while (std::getline(input, line)) {
			lines.push_back(line);
		}

		std::ostringstream output;
		std::string prevTopLevelType; // "use", "const", "fn_start", "comment", "shebang", ""
		int braceDepth = 0;
		bool inFunction = false;
		bool inBlockComment = false;			// Track multi-line block comment state
		std::vector<std::string> useStatements; // Buffer for collecting consecutive use statements
		std::vector<std::string> commentBuffer; // Buffer for comments that may precede a function

		auto flushUseStatements = [&]() {
			if (!useStatements.empty()) {
				// Normalize use statements
				std::vector<std::string> normalizedUses;
				for (const auto& useStmt : useStatements) {
					normalizedUses.push_back(normalizeUseStatement(useStmt));
				}
				// Sort alphabetically if enabled
				if (opts.sortImports) {
					std::sort(normalizedUses.begin(), normalizedUses.end());
				}
				for (const auto& useStmt : normalizedUses) {
					output << useStmt << '\n';
				}
				useStatements.clear();
			}
		};

		auto flushCommentBuffer = [&](bool addBlankLineBefore) {
			if (!commentBuffer.empty()) {
				if (addBlankLineBefore) {
					output << '\n';
				}
				for (const auto& comment : commentBuffer) {
					output << comment << '\n';
				}
				commentBuffer.clear();
			}
		};

		for (size_t i = 0; i < lines.size(); i++) {
			std::string trimmed = trim(lines[i]);

			if (trimmed.empty()) {
				// Preserve empty lines inside block comments
				if (inBlockComment) {
					output << '\n';
					continue;
				}
				// Flush any buffered use statements before outputting empty line
				if (!useStatements.empty()) {
					flushUseStatements();
				}
				// At top-level with buffered comments, skip empty lines (they'll be
				// normalized when we flush comments before the next declaration)
				if (!commentBuffer.empty() && braceDepth == 0) {
					continue;
				}
				// Only output empty lines when inside functions
				if (inFunction) {
					output << '\n';
				}
				continue;
			}

			// Save brace depth BEFORE processing this line's braces
			// This is needed to correctly identify top-level declarations that open a brace
			int braceDepthBeforeLine = braceDepth;

			// Track block comment state and brace depth
			// Skip brace counting when inside block comments
			if (!inBlockComment) {
				// Check if this line starts a block comment (only outside string literals)
				size_t openPos = findOutsideStrings(trimmed, "/*");
				size_t closePos = findOutsideStrings(trimmed, "*/");
				if (openPos != std::string::npos) {
					if (closePos == std::string::npos || closePos < openPos) {
						// Block comment starts but doesn't end on this line
						inBlockComment = true;
					}
					// If both exist and close comes after open, it's a single-line block comment
				}

				// Count braces only outside of comments
				if (!isComment(trimmed)) {
					for (char c : trimmed) {
						if (c == '{') {
							braceDepth++;
						}
						if (c == '}') {
							braceDepth--;
						}
					}
				}
			} else {
				// We're inside a block comment, check if it ends
				if (findOutsideStrings(trimmed, "*/") != std::string::npos) {
					inBlockComment = false;
				}
			}

			// Determine if this is a top-level declaration
			// Use braceDepthBeforeLine so that lines like "import ... {" are still recognized as top-level
			bool isTopLevel =
					(braceDepthBeforeLine == 0 ||
							((startsWithKeyword(trimmed, "fn") || startsWithKeyword(trimmed, "pub")) && !inFunction));
			std::string currentType;

			if (isTopLevel) {
				// Handle shebang - must be first non-empty line
				if (isShebang(trimmed) && i == 0) {
					currentType = "shebang";
					output << lines[i] << '\n';
					prevTopLevelType = currentType;
					continue;
				}

				// Handle top-level comments - buffer them to attach to following declaration
				// Only buffer when truly at file-level (not inside import blocks)
				if (isComment(trimmed) && braceDepthBeforeLine == 0) {
					// Buffer comments at top level (outside functions)
					commentBuffer.push_back(lines[i]);
					continue;
				}

				if (startsWithKeyword(trimmed, "use")) {
					currentType = "use";
					// Add blank line after shebang before use statements
					if (prevTopLevelType == "shebang") {
						output << '\n';
					}
					// Flush comments before use statement
					flushCommentBuffer(prevTopLevelType == "fn_start");
					// Buffer use statements for sorting
					useStatements.push_back(lines[i]);
					prevTopLevelType = currentType;
					continue; // Don't output yet, wait to sort
				} else if (startsWithKeyword(trimmed, "import")) {
					currentType = "use"; // Treat import like use for spacing
					// Flush comments before import statement
					flushCommentBuffer(prevTopLevelType == "fn_start");
				} else if (startsWithKeyword(trimmed, "pub")) {
					// Handle pub fn, pub struct, pub enum, pub const
					if (trimmed.find("pub fn") != std::string::npos ||
							trimmed.find("pub struct") != std::string::npos ||
							trimmed.find("pub enum") != std::string::npos) {
						currentType = "fn_start";
						inFunction = true;
					} else if (trimmed.find("pub const") != std::string::npos) {
						currentType = "const";
					}
				} else if (startsWithKeyword(trimmed, "const")) {
					currentType = "const";
				} else if (startsWithKeyword(trimmed, "fn")) {
					currentType = "fn_start";
					inFunction = true;
				} else if (startsWithKeyword(trimmed, "struct") || startsWithKeyword(trimmed, "enum")) {
					currentType = "fn_start"; // Treat struct/enum like fn for spacing
					inFunction = true;
				} else if (startsWithKeyword(trimmed, "test")) {
					currentType = "fn_start"; // Treat test like fn for spacing
					inFunction = true;
				}

				// Flush any buffered use statements when we encounter non-use statement
				if (!useStatements.empty() && currentType != "use") {
					flushUseStatements();
				}

				// Add appropriate spacing before top-level declarations
				// If we have buffered comments, the blank line goes before them
				// Only apply spacing rules when truly at file level (not inside import blocks)
				bool needsBlankLine = false;
				if (!prevTopLevelType.empty() && braceDepthBeforeLine == 0) {
					if ((prevTopLevelType == "use" && currentType == "const") ||
							(prevTopLevelType == "use" && currentType == "fn_start") ||
							(prevTopLevelType == "const" && currentType == "fn_start") ||
							(prevTopLevelType == "fn_start" && currentType == "fn_start") ||
							(prevTopLevelType == "fn_start" && currentType == "use") ||
							(prevTopLevelType == "fn_start" && currentType == "const") ||
							(prevTopLevelType == "shebang")) { // Always blank line after shebang
						needsBlankLine = true;
					}
				}

				// Flush comments with or without blank line
				if (!commentBuffer.empty()) {
					flushCommentBuffer(needsBlankLine);
					needsBlankLine = false; // Already handled by comment flush
				} else if (needsBlankLine) {
					output << '\n';
				}

				if (!currentType.empty()) {
					prevTopLevelType = currentType;
				}
			}

			output << lines[i] << '\n';

			// Check if we just exited a function
			if (braceDepth == 0 && inFunction) {
				inFunction = false;
			}
		}

		// Flush any remaining use statements
		flushUseStatements();
		// Flush any remaining comments
		flushCommentBuffer(false);

		return output.str();
	}

	// Main formatting function that works on source text with options
	std::string formatSource(const std::string& source, const FormatOptions& opts) {
		// First, merge multi-line struct constructions back to single lines
		std::string mergedStructs = mergeStructConstructions(source);
		// Then split inline braces onto separate lines (but not struct constructions)
		std::string split = preprocessBraces(mergedStructs);
		// Then merge any standalone opening braces with their preceding line
		std::string preprocessed = mergeStandaloneBraces(split);

		std::istringstream input(preprocessed);
		std::ostringstream output;
		std::string line;
		int indentLevel = 0;
		bool inMultilineComment = false;

		// State for struct definition field alignment
		bool inStructDef = false;
		int structDefIndent = 0;
		std::vector<std::string> structDefFields;

		bool isFirstLine = true;
		while (std::getline(input, line)) {
			std::string trimmed = trim(line);

			// Handle shebang - must be first line, output as-is (no indentation)
			if (isFirstLine && isShebang(trimmed)) {
				output << trimmed << '\n';
				isFirstLine = false;
				continue;
			}
			isFirstLine = false;

			// Handle single-line comments - just reindent them
			if (trimmed.length() >= 2 && trimmed.substr(0, 2) == "//") {
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << trimmed << '\n';
				continue;
			}

			// Handle multi-line comments (only if /* is outside string literals)
			if (!inMultilineComment && findOutsideStrings(trimmed, "/*") != std::string::npos) {
				inMultilineComment = true;
			}

			if (inMultilineComment) {
				// Indent block comment lines with current indentation
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << trimmed << '\n';
				if (findOutsideStrings(trimmed, "*/") != std::string::npos) {
					inMultilineComment = false;
				}
				continue;
			}

			// Skip empty lines
			if (trimmed.empty()) {
				output << '\n';
				continue;
			}

			// Handle closing braces - dedent before writing
			// Check if line starts with "}" (closing brace)
			if (!trimmed.empty() && trimmed[0] == '}') {
				// If we're in a struct definition, handle the closing brace specially
				if (inStructDef && trimmed == "}") {
					// Format and output all buffered fields with alignment
					auto alignedFields = formatStructFieldsAligned(structDefFields, structDefIndent);
					for (const auto& field : alignedFields) {
						output << field << '\n';
					}
					// Output closing brace
					if (indentLevel > 0) {
						indentLevel--;
					}
					for (int i = 0; i < indentLevel; i++) {
						output << '\t';
					}
					output << "}\n";
					// Reset struct state
					inStructDef = false;
					structDefFields.clear();
					continue;
				}

				if (indentLevel > 0) {
					indentLevel--;
				}

				// Special case: } else { - write it and handle indentation
				if (trimmed.find("} else {") == 0) {
					for (int i = 0; i < indentLevel; i++) {
						output << '\t';
					}
					output << trimmed << '\n';
					indentLevel++;
					continue;
				}

				// Just closing brace
				if (trimmed == "}") {
					for (int i = 0; i < indentLevel; i++) {
						output << '\t';
					}
					output << trimmed << '\n';
					continue;
				}
			}

			// Check for anonymous function first (fn (signature) { body } -> var)
			if (isAnonymousFunction(trimmed)) {
				std::string formatted = formatAnonymousFunctionSignature(trimmed);
				if (!formatted.empty()) {
					// Write with current indent
					for (int i = 0; i < indentLevel; i++) {
						output << '\t';
					}
					output << formatted << '\n';
					// Anonymous functions are self-contained on one line, don't adjust indent
					continue;
				}
			}

			// Handle struct/enum definition start
			if (startsWithKeyword(trimmed, "struct") || startsWithKeyword(trimmed, "enum") ||
					(startsWithKeyword(trimmed, "pub") && (trimmed.find("pub struct") != std::string::npos ||
																  trimmed.find("pub enum") != std::string::npos))) {
				// Output the struct header line
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << trimmed << '\n';

				if (trimmed.find('{') != std::string::npos) {
					indentLevel++;
					// Start buffering struct fields
					inStructDef = true;
					structDefIndent = indentLevel;
					structDefFields.clear();
				}
				continue;
			}

			// If we're in a struct definition, buffer fields for alignment
			if (inStructDef) {
				// Check for closing brace
				if (trimmed == "}") {
					// Format and output all buffered fields with alignment
					auto alignedFields = formatStructFieldsAligned(structDefFields, structDefIndent);
					for (const auto& field : alignedFields) {
						output << field << '\n';
					}
					// Output closing brace
					indentLevel--;
					for (int i = 0; i < indentLevel; i++) {
						output << '\t';
					}
					output << "}\n";
					// Reset struct state
					inStructDef = false;
					structDefFields.clear();
					continue;
				}
				// Buffer this field line
				structDefFields.push_back(trimmed);
				continue;
			}

			// Format function signatures (fn, pub fn)
			if (startsWithKeyword(trimmed, "fn") ||
					(startsWithKeyword(trimmed, "pub") && trimmed.find("pub fn") != std::string::npos)) {
				std::string formatted = formatFunctionSignature(line);
				// Write with current indent
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << formatted << '\n';

				if (formatted.find('{') != std::string::npos) {
					indentLevel++;
				}
				continue;
			}

			// Format test blocks
			if (startsWithKeyword(trimmed, "test")) {
				std::string formatted = formatTestBlock(line);
				// Write with current indent
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << formatted << '\n';

				if (formatted.find('{') != std::string::npos) {
					indentLevel++;
				}
				continue;
			}

			// Handle control flow keywords - keep on same line, fix indentation
			if (startsWithKeyword(trimmed, "if") || startsWithKeyword(trimmed, "for") ||
					startsWithKeyword(trimmed, "loop") ||
					startsWithKeyword(trimmed, "else") || startsWithKeyword(trimmed, "switch") ||
					startsWithKeyword(trimmed, "case") || startsWithKeyword(trimmed, "default") ||
					startsWithKeyword(trimmed, "defer")) {
				// Write with current indent
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << trimmed << '\n';

				// Track all braces to handle inline blocks like: if { "x" . } else { "y" . }
				for (char c : trimmed) {
					if (c == '{') {
						indentLevel++;
					} else if (c == '}') {
						if (indentLevel > 0) {
							indentLevel--;
						}
					}
				}
				continue;
			}

			// Handle other top-level declarations
			if (startsWithKeyword(trimmed, "use") || startsWithKeyword(trimmed, "import") ||
					startsWithKeyword(trimmed, "const") || startsWithKeyword(trimmed, "pub")) {
				// Write with current indent (should be 0)
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				// Normalize use statements to have single space
				if (startsWithKeyword(trimmed, "use")) {
					output << normalizeUseStatement(trimmed) << '\n';
				} else if (startsWithKeyword(trimmed, "const") || trimmed.find("pub const") != std::string::npos) {
					// Normalize const statements (including pub const)
					output << normalizeConstStatement(trimmed) << '\n';
				} else {
					output << trimmed << '\n';
				}

				if (trimmed.find('{') != std::string::npos) {
					indentLevel++;
				}
				continue;
			}

			// Check for struct construction (StructName { field: value ... })
			std::string structFormatted = formatStructConstruction(trimmed, static_cast<size_t>(indentLevel));
			if (!structFormatted.empty()) {
				output << structFormatted << '\n';
				// Don't track braces here - the struct is self-contained on multiple lines
				continue;
			}

			// Everything else - just fix indentation and normalize operator spacing
			for (int i = 0; i < indentLevel; i++) {
				output << '\t';
			}
			output << normalizeOperatorSpacing(trimmed) << '\n';

			// Track brace depth for other lines (but not for comments)
			// Count both opening and closing braces to handle inline blocks like:
			// if { "* " . } else { "  " . }
			if (!isComment(trimmed)) {
				for (char c : trimmed) {
					if (c == '{') {
						indentLevel++;
					} else if (c == '}') {
						if (indentLevel > 0) {
							indentLevel--;
						}
					}
				}
			}
		}

		// Apply top-level spacing normalization as final step
		return normalizeTopLevelSpacing(output.str(), opts);
	}

	// Main formatting function that works on source text (uses default options)
	std::string formatSource(const std::string& source) {
		return formatSource(source, FormatOptions{});
	}

}

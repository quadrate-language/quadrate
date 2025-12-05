#include <algorithm>
#include <cctype>
#include <qc/formatter.h>
#include <sstream>
#include <vector>

namespace Qd {

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

	// Check if a line is a comment (single-line or starts a block comment)
	static bool isComment(const std::string& line) {
		std::string trimmed = trim(line);
		return trimmed.length() >= 2 && (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*");
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

	// Format a function signature line
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

		// Find the function name, parameters, and opening brace
		size_t fnPos = workingLine.find("fn ");
		if (fnPos == std::string::npos) {
			return line;
		}

		size_t nameStart = fnPos + 3;
		while (nameStart < workingLine.length() && std::isspace(static_cast<unsigned char>(workingLine[nameStart]))) {
			nameStart++;
		}

		size_t parenPos = workingLine.find('(', nameStart);
		if (parenPos == std::string::npos) {
			return line;
		}

		std::string name = trim(workingLine.substr(nameStart, parenPos - nameStart));

		// Find matching closing paren
		int depth = 0;
		size_t closeParenPos = parenPos;
		for (size_t i = parenPos; i < workingLine.length(); i++) {
			if (workingLine[i] == '(') {
				depth++;
			}
			if (workingLine[i] == ')') {
				depth--;
				if (depth == 0) {
					closeParenPos = i;
					break;
				}
			}
		}

		if (closeParenPos == parenPos) {
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

		// Check for '!' after the closing paren (error-returning function)
		std::string suffix;
		size_t pos = closeParenPos + 1;
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

		// Format: [pub] fn name( params )! {
		std::string result;
		if (isPublic) {
			result = "pub fn " + name + "(" + formattedSig + ")" + suffix;
		} else {
			result = "fn " + name + "(" + formattedSig + ")" + suffix;
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

	// Check if a string looks like a struct name (starts with uppercase letter)
	static bool isStructName(const std::string& name) {
		if (name.empty()) {
			return false;
		}
		return std::isupper(static_cast<unsigned char>(name[0]));
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

		// Check if this looks like an identifier (alphanumeric/underscore only)
		for (char c : beforeBrace) {
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
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
				if (c == ':' && !inStr && braceDepth == 0) {
					currentField = trim(currentField);
					parsingField = false;
					i++;
					continue;
				}
				currentField += c;
			} else {
				// Check if we've hit a new field (lowercase identifier followed by colon)
				if (!inStr && braceDepth == 0 && std::isspace(static_cast<unsigned char>(c))) {
					// Look ahead to see if next non-space is a field name
					size_t j = i + 1;
					while (j < content.length() && std::isspace(static_cast<unsigned char>(content[j]))) {
						j++;
					}
					// Check if it's an identifier followed by :
					size_t identStart = j;
					while (j < content.length() &&
							(std::isalnum(static_cast<unsigned char>(content[j])) || content[j] == '_')) {
						j++;
					}
					if (j > identStart && j < content.length() && content[j] == ':') {
						// Found next field, save current
						fields.push_back({currentField, trim(currentValue)});
						currentField.clear();
						currentValue.clear();
						parsingField = true;
						i = identStart;
						continue;
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
			result << fieldIndent << field.first << ": " << field.second << "\n";
		}
		result << indent << "}";

		if (!afterBrace.empty()) {
			result << " " << afterBrace;
		}

		return result.str();
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

		// Check it's a simple identifier
		for (char c : beforeBrace) {
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
				return false;
			}
		}

		// Check it has field: pattern inside
		size_t colonPos = trimmed.find(':', bracePos);
		return colonPos != std::string::npos;
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
		if (isComment(trimmed) || trimmed.find("*/") != std::string::npos || trimmed.find("/*") != std::string::npos) {
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

		std::ostringstream result;
		bool inString = false;
		bool inBlockComment = false;
		std::string current;

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
					// There's content after {, split here
					result << trim(current) << '\n';
					current.clear();
					i = next - 1; // Will be incremented by loop
				}
			} else if (c == '}') {
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
								// There's content after {, split here
								result << current << '\n';
								current.clear();
								i = afterBrace - 1;
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

			// Check if we enter or exit a block comment
			size_t openPos = trimmed.find("/*");
			size_t closePos = trimmed.find("*/");

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

	// Merge multi-line struct constructions back into single lines for reformatting
	// Detects: StructName {\n  field: value ...\n } and merges them
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

			// Check if this line is "StructName {" (ends with { and starts with uppercase)
			if (!trimmed.empty() && trimmed.back() == '{') {
				std::string beforeBrace = trim(trimmed.substr(0, trimmed.length() - 1));
				if (!beforeBrace.empty() && isStructName(beforeBrace)) {
					// Check if it's a simple identifier
					bool isIdent = true;
					for (char c : beforeBrace) {
						if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
							isIdent = false;
							break;
						}
					}

					if (isIdent) {
						// Look ahead for field lines and closing brace
						std::string merged = beforeBrace + " {";
						size_t j = i + 1;
						bool foundClose = false;

						while (j < lines.size()) {
							std::string nextTrimmed = trim(lines[j]);
							if (nextTrimmed == "}") {
								merged += " }";
								foundClose = true;
								j++;
								break;
							} else if (nextTrimmed.find("} ->") == 0 || nextTrimmed.find("}->") == 0) {
								// Closing brace with arrow assignment
								merged += " " + nextTrimmed;
								foundClose = true;
								j++;
								break;
							} else if (nextTrimmed.find(':') != std::string::npos && !isComment(nextTrimmed)) {
								// Looks like a field line
								merged += " " + nextTrimmed;
								j++;
							} else {
								// Not a field line, stop merging
								break;
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

	// Normalize spacing between top-level declarations and sort use statements
	static std::string normalizeTopLevelSpacing(const std::string& source) {
		std::istringstream input(source);
		std::vector<std::string> lines;
		std::string line;

		while (std::getline(input, line)) {
			lines.push_back(line);
		}

		std::ostringstream output;
		std::string prevTopLevelType; // "use", "const", "fn_start", "comment", ""
		int braceDepth = 0;
		bool inFunction = false;
		bool inBlockComment = false;			// Track multi-line block comment state
		std::vector<std::string> useStatements; // Buffer for collecting consecutive use statements
		std::vector<std::string> commentBuffer; // Buffer for comments that may precede a function

		auto flushUseStatements = [&]() {
			if (!useStatements.empty()) {
				// Normalize and sort use statements alphabetically
				std::vector<std::string> normalizedUses;
				for (const auto& useStmt : useStatements) {
					normalizedUses.push_back(normalizeUseStatement(useStmt));
				}
				std::sort(normalizedUses.begin(), normalizedUses.end());
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
				// Check if this line starts a block comment
				size_t openPos = trimmed.find("/*");
				size_t closePos = trimmed.find("*/");
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
				if (trimmed.find("*/") != std::string::npos) {
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
				// Handle top-level comments - buffer them to attach to following declaration
				// Only buffer when truly at file-level (not inside import blocks)
				if (isComment(trimmed) && braceDepthBeforeLine == 0) {
					// Buffer comments at top level (outside functions)
					commentBuffer.push_back(lines[i]);
					continue;
				}

				if (startsWithKeyword(trimmed, "use")) {
					currentType = "use";
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
					// Handle pub fn and pub const
					if (trimmed.find("pub fn") != std::string::npos ||
							trimmed.find("pub struct") != std::string::npos) {
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
				} else if (startsWithKeyword(trimmed, "struct")) {
					currentType = "fn_start"; // Treat struct like fn for spacing
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
							(prevTopLevelType == "fn_start" && currentType == "const")) {
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

	// Main formatting function that works on source text
	std::string formatSource(const std::string& source) {
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

		while (std::getline(input, line)) {
			std::string trimmed = trim(line);

			// Handle single-line comments - just reindent them
			if (trimmed.length() >= 2 && trimmed.substr(0, 2) == "//") {
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << trimmed << '\n';
				continue;
			}

			// Handle multi-line comments
			if (!inMultilineComment && trimmed.find("/*") != std::string::npos) {
				inMultilineComment = true;
			}

			if (inMultilineComment) {
				// Indent block comment lines with current indentation
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << trimmed << '\n';
				if (trimmed.find("*/") != std::string::npos) {
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

			// Format function signatures
			if (startsWithKeyword(trimmed, "fn") || startsWithKeyword(trimmed, "pub")) {
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
					startsWithKeyword(trimmed, "loop") || startsWithKeyword(trimmed, "else") ||
					startsWithKeyword(trimmed, "switch") || startsWithKeyword(trimmed, "case") ||
					startsWithKeyword(trimmed, "default") || startsWithKeyword(trimmed, "defer")) {
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

			// Everything else - just fix indentation, keep content as-is
			for (int i = 0; i < indentLevel; i++) {
				output << '\t';
			}
			output << trimmed << '\n';

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
		return normalizeTopLevelSpacing(output.str());
	}

}

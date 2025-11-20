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
		std::string prevTopLevelType; // "use", "const", "fn_start", ""
		int braceDepth = 0;
		bool inFunction = false;
		std::vector<std::string> useStatements; // Buffer for collecting consecutive use statements

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

		for (size_t i = 0; i < lines.size(); i++) {
			std::string trimmed = trim(lines[i]);

			if (trimmed.empty()) {
				// Flush any buffered use statements before outputting empty line
				if (!useStatements.empty()) {
					flushUseStatements();
				}
				// Only output empty lines when inside functions
				if (inFunction) {
					output << '\n';
				}
				continue;
			}

			// Track brace depth to know when we exit a function, but skip comments
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

			// Determine if this is a top-level declaration
			bool isTopLevel = (braceDepth == 0 ||
			                   ((startsWithKeyword(trimmed, "fn") || startsWithKeyword(trimmed, "pub")) && !inFunction));
			std::string currentType;

			if (isTopLevel) {
				if (startsWithKeyword(trimmed, "use")) {
					currentType = "use";
					// Buffer use statements for sorting
					useStatements.push_back(lines[i]);
					prevTopLevelType = currentType;
					continue; // Don't output yet, wait to sort
				} else if (startsWithKeyword(trimmed, "import")) {
					currentType = "use"; // Treat import like use for spacing
				} else if (startsWithKeyword(trimmed, "pub")) {
					// Handle pub fn and pub const
					if (trimmed.find("pub fn") != std::string::npos) {
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
				}

				// Flush any buffered use statements when we encounter non-use statement
				if (!useStatements.empty() && currentType != "use") {
					flushUseStatements();
				}

				// Add appropriate spacing before top-level declarations
				if (!prevTopLevelType.empty()) {
					if ((prevTopLevelType == "use" && currentType == "const") ||
							(prevTopLevelType == "use" && currentType == "fn_start") ||
							(prevTopLevelType == "const" && currentType == "fn_start") ||
							(prevTopLevelType == "fn_start" && currentType == "fn_start") ||
							(prevTopLevelType == "fn_start" && currentType == "use") ||
							(prevTopLevelType == "fn_start" && currentType == "const")) {
						// Exactly one empty line between:
						// - use statements and constants
						// - use statements and first function
						// - constants and first function
						// - between functions
						// - functions and subsequent use statements
						// - functions and subsequent constants
						output << '\n';
					}
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

		return output.str();
	}

	// Main formatting function that works on source text
	std::string formatSource(const std::string& source) {
		// First, merge any standalone opening braces with their preceding line
		std::string preprocessed = mergeStandaloneBraces(source);

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

				if (trimmed.find('{') != std::string::npos) {
					indentLevel++;
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

			// Everything else - just fix indentation, keep content as-is
			for (int i = 0; i < indentLevel; i++) {
				output << '\t';
			}
			output << trimmed << '\n';

			// Track brace depth for other lines (but not for comments)
			if (!isComment(trimmed)) {
				for (char c : trimmed) {
					if (c == '{') {
						indentLevel++;
					}
				}
			}
		}

		// Apply top-level spacing normalization as final step
		return normalizeTopLevelSpacing(output.str());
	}

}

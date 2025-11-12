#include <qc/formatter.h>
#include <sstream>
#include <cctype>
#include <algorithm>
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

	// Check if line starts with a keyword (after trimming)
	static bool startsWithKeyword(const std::string& line, const std::string& keyword) {
		std::string trimmed = trim(line);
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

	// Format a function signature line
	static std::string formatFunctionSignature(const std::string& line) {
		std::string trimmed = trim(line);

		// Must start with "fn "
		if (!startsWithKeyword(trimmed, "fn")) {
			return line;
		}

		// Find the function name, parameters, and opening brace
		size_t fnPos = trimmed.find("fn ");
		if (fnPos == std::string::npos) {
			return line;
		}

		size_t nameStart = fnPos + 3;
		while (nameStart < trimmed.length() && std::isspace(static_cast<unsigned char>(trimmed[nameStart]))) {
			nameStart++;
		}

		size_t parenPos = trimmed.find('(', nameStart);
		if (parenPos == std::string::npos) {
			return line;
		}

		std::string name = trim(trimmed.substr(nameStart, parenPos - nameStart));

		// Find matching closing paren
		int depth = 0;
		size_t closeParenPos = parenPos;
		for (size_t i = parenPos; i < trimmed.length(); i++) {
			if (trimmed[i] == '(') depth++;
			if (trimmed[i] == ')') {
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
		std::string signature = trimmed.substr(parenPos + 1, closeParenPos - parenPos - 1);
		std::string formattedSig = trim(signature);

		// Special case: if signature contains only "--" (with possible whitespace),
		// format as " -- " to match expected style for empty parameters
		std::string sigNoSpace = formattedSig;
		sigNoSpace.erase(std::remove_if(sigNoSpace.begin(), sigNoSpace.end(),
			[](unsigned char c) { return std::isspace(c); }), sigNoSpace.end());

		if (sigNoSpace == "--") {
			formattedSig = " -- ";
		}

		// Check for '!' after the closing paren (error-returning function)
		std::string suffix;
		size_t pos = closeParenPos + 1;
		while (pos < trimmed.length() && std::isspace(trimmed[pos])) {
			pos++;
		}
		if (pos < trimmed.length() && trimmed[pos] == '!') {
			suffix = "!";
			pos++;
		}

		// Check for opening brace after any suffix
		size_t bracePos = trimmed.find('{', pos);
		bool hasBrace = (bracePos != std::string::npos);

		// Format: fn name( params )! {
		std::string result = "fn " + name + "(" + formattedSig + ")" + suffix;
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
			"if", "else", "for", "loop", "defer", "switch", "case", "default", "fn"
		};

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
					i++;  // Skip the next line (the standalone brace)
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

	// Normalize spacing between top-level declarations
	static std::string normalizeTopLevelSpacing(const std::string& source) {
		std::istringstream input(source);
		std::vector<std::string> lines;
		std::string line;

		while (std::getline(input, line)) {
			lines.push_back(line);
		}

		std::ostringstream output;
		std::string prevTopLevelType;  // "use", "fn_start", ""
		int braceDepth = 0;
		bool inFunction = false;

		for (size_t i = 0; i < lines.size(); i++) {
			std::string trimmed = trim(lines[i]);

			if (trimmed.empty()) {
				// Only output empty lines when inside functions
				if (inFunction) {
					output << '\n';
				}
				continue;
			}

			// Track brace depth to know when we exit a function
			for (char c : trimmed) {
				if (c == '{') braceDepth++;
				if (c == '}') braceDepth--;
			}

			// Determine if this is a top-level declaration
			bool isTopLevel = (braceDepth == 0 || (startsWithKeyword(trimmed, "fn") && !inFunction));
			std::string currentType;

			if (isTopLevel) {
				if (startsWithKeyword(trimmed, "use") || startsWithKeyword(trimmed, "import")) {
					currentType = "use";
				} else if (startsWithKeyword(trimmed, "fn")) {
					currentType = "fn_start";
					inFunction = true;
				}

				// Add appropriate spacing before top-level declarations
				if (!prevTopLevelType.empty()) {
					if ((prevTopLevelType == "use" && currentType == "fn_start") ||
					    (prevTopLevelType == "fn_start" && currentType == "fn_start") ||
					    (prevTopLevelType == "fn_start" && currentType == "use")) {
						// Exactly one empty line between:
						// - use statements and first function
						// - between functions
						// - functions and subsequent use statements
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

			// Handle multi-line comments
			if (!inMultilineComment && trimmed.find("/*") != std::string::npos) {
				inMultilineComment = true;
			}

			if (inMultilineComment) {
				// Keep comment lines as-is
				output << line << '\n';
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
			if (startsWithKeyword(trimmed, "fn")) {
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
			if (startsWithKeyword(trimmed, "if") ||
			    startsWithKeyword(trimmed, "for") ||
			    startsWithKeyword(trimmed, "loop") ||
			    startsWithKeyword(trimmed, "else") ||
			    startsWithKeyword(trimmed, "switch") ||
			    startsWithKeyword(trimmed, "case") ||
			    startsWithKeyword(trimmed, "default") ||
			    startsWithKeyword(trimmed, "defer")) {

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
			if (startsWithKeyword(trimmed, "use") ||
			    startsWithKeyword(trimmed, "import") ||
			    startsWithKeyword(trimmed, "const")) {

				// Write with current indent (should be 0)
				for (int i = 0; i < indentLevel; i++) {
					output << '\t';
				}
				output << trimmed << '\n';

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

			// Track brace depth for other lines
			for (char c : trimmed) {
				if (c == '{') {
					indentLevel++;
				}
			}
		}

		// Apply top-level spacing normalization as final step
		return normalizeTopLevelSpacing(output.str());
	}

}

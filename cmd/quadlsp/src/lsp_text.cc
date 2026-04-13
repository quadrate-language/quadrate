// LSP text-position helpers.
//
// Pure functions that operate on document text + line/character positions.
// Extracted into their own translation unit so the fuzz target can link
// against just this file (without jansson, qc, etc.).

#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

std::string lspGetWordAtPosition(const std::string& text, size_t line, size_t character) {
	// Split text into lines
	std::vector<std::string> lines;
	std::istringstream stream(text);
	std::string currentLine;
	while (std::getline(stream, currentLine)) {
		lines.push_back(currentLine);
	}

	if (line >= lines.size()) {
		return "";
	}

	const std::string& targetLine = lines[line];
	if (character >= targetLine.length()) {
		return "";
	}

	size_t start = character;
	size_t end = character;

	// Move start backward to beginning of word.
	// Include `::` for scoped identifiers, but not single ':' (type annotations).
	while (start > 0) {
		unsigned char c = static_cast<unsigned char>(targetLine[start - 1]);
		if (std::isalnum(c) || c == '_') {
			start--;
		} else if (c == ':' && start >= 2 && targetLine[start - 2] == ':') {
			start -= 2;
		} else {
			break;
		}
	}

	// Move end forward to end of word.
	while (end < targetLine.length()) {
		unsigned char c = static_cast<unsigned char>(targetLine[end]);
		if (std::isalnum(c) || c == '_') {
			end++;
		} else if (c == ':' && end + 1 < targetLine.length() && targetLine[end + 1] == ':') {
			end += 2;
		} else {
			break;
		}
	}

	if (end > start) {
		return targetLine.substr(start, end - start);
	}
	return "";
}

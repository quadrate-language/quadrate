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

static size_t utf8SequenceLength(const std::string& text, size_t index) {
	unsigned char lead = static_cast<unsigned char>(text[index]);
	size_t length = 1;
	if (lead >= 0xF0 && lead <= 0xF4) {
		length = 4;
	} else if (lead >= 0xE0 && lead <= 0xEF) {
		length = 3;
	} else if (lead >= 0xC2 && lead <= 0xDF) {
		length = 2;
	}
	if (length == 1 || index + length > text.size()) {
		return 1;
	}
	for (size_t i = 1; i < length; i++) {
		if ((static_cast<unsigned char>(text[index + i]) & 0xC0) != 0x80) {
			return 1;
		}
	}
	return length;
}

size_t lspByteToUtf16Column(const std::string& lineText, size_t byteColumn) {
	size_t units = 0;
	size_t i = 0;
	while (i < lineText.size() && i < byteColumn) {
		size_t length = utf8SequenceLength(lineText, i);
		units += (length == 4) ? 2 : 1;
		i += length;
	}
	if (byteColumn > lineText.size()) {
		units += byteColumn - lineText.size();
	}
	return units;
}

size_t lspUtf16ToByteColumn(const std::string& lineText, size_t utf16Column) {
	size_t units = 0;
	size_t i = 0;
	while (i < lineText.size() && units < utf16Column) {
		size_t length = utf8SequenceLength(lineText, i);
		size_t width = (length == 4) ? 2 : 1;
		if (units + width > utf16Column) {
			break;
		}
		units += width;
		i += length;
	}
	if (i >= lineText.size() && units < utf16Column) {
		i += utf16Column - units;
	}
	return i;
}

static int hexDigitValue(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

std::string lspUriToPath(const std::string& uri) {
	if (uri.compare(0, 7, "file://") != 0) {
		return "";
	}
	size_t start = 7;
	if (start < uri.size() && uri[start] != '/') {
		start = uri.find('/', start);
		if (start == std::string::npos) {
			return "";
		}
	}
	std::string path;
	path.reserve(uri.size() - start);
	for (size_t i = start; i < uri.size(); i++) {
		char c = uri[i];
		if (c == '?' || c == '#') {
			break;
		}
		if (c == '%' && i + 2 < uri.size()) {
			int high = hexDigitValue(uri[i + 1]);
			int low = hexDigitValue(uri[i + 2]);
			if (high >= 0 && low >= 0) {
				path += static_cast<char>(high * 16 + low);
				i += 2;
				continue;
			}
		}
		path += c;
	}
	return path;
}

std::string lspPathToUri(const std::string& path) {
	static const char* hexDigits = "0123456789ABCDEF";
	std::string uri = "file://";
	for (char ch : path) {
		unsigned char c = static_cast<unsigned char>(ch);
		if (std::isalnum(c) || c == '/' || c == '-' || c == '.' || c == '_' || c == '~') {
			uri += ch;
		} else {
			uri += '%';
			uri += hexDigits[c >> 4];
			uri += hexDigits[c & 0xF];
		}
	}
	return uri;
}

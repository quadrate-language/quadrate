/**
 * @file fuzz_lsp_text.cc
 * @brief Fuzz target for the language server's text-position helpers
 *
 * `lspGetWordAtPosition` is driven straight from editor-supplied line/character
 * pairs, which need not point anywhere inside the document the server holds --
 * an editor racing an edit against a hover request routinely sends a position
 * past the end of the buffer. The helper walks backwards and forwards from that
 * index over `::`, so an out-of-range or mid-word position must not read out of
 * bounds. Whatever it returns has to be a substring of the addressed line.
 *
 * The first bytes of the input pick the position, the rest is the document, so
 * the fuzzer can steer both.
 *
 * Build with: clang++ -g -O1 -fsanitize=fuzzer,address fuzz_lsp_text.cc ...
 * Run with: ./fuzz_lsp_text -max_len=10000
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

std::string lspGetWordAtPosition(const std::string& text, size_t line, size_t character);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	// Four bytes of position, then the document.
	if (size < 4 || size > 50000) {
		return 0;
	}

	size_t line = (static_cast<size_t>(data[0]) << 8) | data[1];
	size_t character = (static_cast<size_t>(data[2]) << 8) | data[3];
	std::string text(reinterpret_cast<const char*>(data + 4), size - 4);

	std::string word = lspGetWordAtPosition(text, line, character);
	if (word.empty()) {
		return 0;
	}

	// A word is always lifted out of the line it was asked for, never synthesised.
	std::vector<std::string> lines;
	std::istringstream stream(text);
	std::string current;
	while (std::getline(stream, current)) {
		lines.push_back(current);
	}
	if (line >= lines.size() || lines[line].find(word) == std::string::npos) {
		std::fprintf(stderr, "lspGetWordAtPosition returned \"%s\", which is not on line %zu\n", word.c_str(), line);
		std::abort();
	}

	return 0;
}

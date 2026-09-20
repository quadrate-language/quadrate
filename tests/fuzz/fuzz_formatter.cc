/**
 * @file fuzz_formatter.cc
 * @brief Fuzz target for the Quadrate source formatter
 *
 * The formatter has two contracts, and this target checks both on every input
 * the parser accepts:
 *
 *  1. Formatting valid source produces valid source. `formatSource` returns the
 *     input untouched when it does not parse, so anything it does change must
 *     still parse afterwards -- an editor formatting on save must never hand the
 *     author back a buffer the compiler rejects.
 *  2. Formatting is idempotent. Running the formatter twice must produce what
 *     running it once did, or `quadfmt -c` reports files as unformatted forever.
 *
 * Build with: clang++ -g -O1 -fsanitize=fuzzer,address fuzz_formatter.cc ...
 * Run with: ./fuzz_formatter corpus/ -max_len=10000
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/formatter.h>
#include <string>

namespace {

	bool parsesClean(const std::string& source, const char* what) {
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(source.c_str(), false, what);
		return root != nullptr && !ast.hasErrors();
	}

	[[noreturn]] void fail(const char* what, const std::string& input, const std::string& output) {
		std::fprintf(stderr, "formatter invariant violated: %s\n", what);
		std::fprintf(stderr, "--- input ---\n%s\n--- output ---\n%s\n", input.c_str(), output.c_str());
		std::abort();
	}

} // namespace

/**
 * @brief libFuzzer entry point
 *
 * @param data Pointer to fuzz input data
 * @param size Size of the input data in bytes
 * @return Always returns 0 (non-zero would stop fuzzing)
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	if (size == 0 || size > 50000) {
		return 0;
	}

	// The formatter takes a null-terminated C string, so an embedded NUL would
	// truncate the input and make any comparison below meaningless.
	if (std::memchr(data, '\0', size) != nullptr) {
		return 0;
	}

	std::string source(reinterpret_cast<const char*>(data), size);

	// The formatter only promises anything about source that parses.
	if (!parsesClean(source, "fuzz_input.qd")) {
		return 0;
	}

	std::string once = Qd::formatSource(source);
	if (!parsesClean(once, "fuzz_formatted.qd")) {
		fail("formatted output does not parse", source, once);
	}

	std::string twice = Qd::formatSource(once);
	if (twice != once) {
		fail("formatting is not idempotent", once, twice);
	}

	return 0;
}

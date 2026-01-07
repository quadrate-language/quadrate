/**
 * @file fuzz_parser.cc
 * @brief Fuzz target for the Quadrate parser
 *
 * This file implements a libFuzzer-compatible fuzz target that tests
 * the Quadrate parser with randomly generated inputs.
 *
 * Build with: clang++ -g -O1 -fsanitize=fuzzer,address fuzz_parser.cc ...
 * Run with: ./fuzz_parser corpus/ -max_len=10000
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <qc/ast.h>

/**
 * @brief libFuzzer entry point
 *
 * This function is called by libFuzzer with random data.
 * The parser should handle any input without crashing.
 *
 * @param data Pointer to fuzz input data
 * @param size Size of the input data in bytes
 * @return Always returns 0 (non-zero would stop fuzzing)
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	// Skip empty inputs
	if (size == 0) {
		return 0;
	}

	// Limit input size to prevent OOM
	if (size > 50000) {
		return 0;
	}

	// Create null-terminated string from fuzzer input
	char* src = new char[size + 1];
	memcpy(src, data, size);
	src[size] = '\0';

	// Parse the input - this should never crash regardless of input
	{
		Qd::Ast ast;
		ast.generate(src, false, "fuzz_input.qd");
		// Note: We only test the parser here, not semantic validation
		// Semantic validation has separate resource limits that can OOM
	}

	delete[] src;
	return 0;
}

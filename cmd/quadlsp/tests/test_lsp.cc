#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

// Simple test to verify LSP executable works correctly
// This is more of an integration test

static bool runLspTest(const std::string& input, const std::string& expectedSubstring) {
	// Create a temporary file with the input
	FILE* tmpInput = tmpfile();
	if (!tmpInput) {
		std::cerr << "Failed to create temp file" << std::endl;
		return false;
	}

	fwrite(input.c_str(), 1, input.length(), tmpInput);
	rewind(tmpInput);

	// Run the LSP server with input from temp file
	std::string command = "build/debug/cmd/quadlsp/quadlsp < /dev/stdin";
	FILE* pipe = popen(command.c_str(), "r");
	if (!pipe) {
		fclose(tmpInput);
		return false;
	}

	// Read output
	char buffer[4096];
	std::string output;
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		output += buffer;
	}

	int status = pclose(pipe);
	fclose(tmpInput);

	// Check if expected substring is in output
	bool found = output.find(expectedSubstring) != std::string::npos;
	if (!found) {
		std::cerr << "Expected substring not found: " << expectedSubstring << std::endl;
		std::cerr << "Got output: " << output.substr(0, 200) << "..." << std::endl;
	}

	return found;
}

static void testInitialize() {
	std::cout << "Testing initialize..." << std::endl;

	std::string request = R"(Content-Length: 113

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{},"rootUri":"file:///tmp"}})";

	// Should receive a response with capabilities
	assert(runLspTest(request, "capabilities"));
	assert(runLspTest(request, "textDocumentSync"));
	assert(runLspTest(request, "quadlsp"));

	std::cout << "✓ Initialize test passed" << std::endl;
}

static void testShutdown() {
	std::cout << "Testing shutdown..." << std::endl;

	std::string request = R"(Content-Length: 58

{"jsonrpc":"2.0","id":2,"method":"shutdown","params":{}})";

	// Should receive null result
	assert(runLspTest(request, "\"result\":null"));

	std::cout << "✓ Shutdown test passed" << std::endl;
}

static void testCompletion() {
	std::cout << "Testing completion..." << std::endl;

	std::string request =
			R"(Content-Length: 150

{"jsonrpc":"2.0","id":3,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/test.qd"},"position":{"line":0,"character":0}}})";

	// Should receive completion items
	assert(runLspTest(request, "\"label\":\"add\""));
	assert(runLspTest(request, "\"label\":\"sub\""));
	assert(runLspTest(request, "\"label\":\"mul\""));
	assert(runLspTest(request, "\"kind\":3"));

	std::cout << "✓ Completion test passed" << std::endl;
}

static void testJsonValidation() {
	std::cout << "Testing JSON format..." << std::endl;

	// Test that initialize returns valid JSON structure
	std::string request = R"(Content-Length: 113

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{},"rootUri":"file:///tmp"}})";

	// Check for proper JSON structure markers
	assert(runLspTest(request, "\"jsonrpc\":\"2.0\""));
	assert(runLspTest(request, "\"id\":1"));
	assert(runLspTest(request, "\"result\":{"));

	std::cout << "✓ JSON validation test passed" << std::endl;
}

static void testDocumentSymbols() {
	std::cout << "Testing document symbols..." << std::endl;

	std::string request =
			R"(Content-Length: 139

{"jsonrpc":"2.0","id":4,"method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file:///tmp/test_symbols.qd"}}})";

	// Should receive symbol list
	assert(runLspTest(request, "\"name\":"));
	assert(runLspTest(request, "\"kind\":12"));

	std::cout << "✓ Document symbols test passed" << std::endl;
}

int main() {
	std::cout << "=== Running LSP Tests ===" << std::endl;

	try {
		testJsonValidation();
		testInitialize();
		testShutdown();
		testCompletion();
		testDocumentSymbols();

		std::cout << "\n✅ All LSP tests passed!" << std::endl;
		return 0;
	} catch (const std::exception& e) {
		std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "❌ Test failed with unknown exception" << std::endl;
		return 1;
	}
}

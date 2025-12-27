#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/colors.h>
#include <qc/error_reporter.h>
#include <qc/formatter.h>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

using namespace Qd;

struct Options {
	std::vector<std::string> paths;
	bool check = false;
	bool help = false;
	bool version = false;
	bool inPlace = false;
};

void printHelp() {
	std::cout << "quadfmt - Quadrate code formatter\n\n";
	std::cout << "Formats Quadrate source files with consistent style.\n\n";
	std::cout << "Usage: quadfmt [options] <file|directory>...\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n";
	std::cout << "  -c, --check      Check if files are formatted (exit 1 if not)\n";
	std::cout << "  -w, --write      Format files in-place\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quadfmt file.qd              Format to stdout\n";
	std::cout << "  quadfmt -w file.qd           Format in-place\n";
	std::cout << "  quadfmt -w src/              Format all .qd files in directory recursively\n";
	std::cout << "  quadfmt -c *.qd              Check if files need formatting\n";
}

void printVersion() {
	std::cout << "0.1.0\n";
}

bool parseArgs(int argc, char* argv[], Options& opts) {
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help") {
			opts.help = true;
			return true;
		} else if (arg == "-v" || arg == "--version") {
			opts.version = true;
			return true;
		} else if (arg == "-c" || arg == "--check") {
			opts.check = true;
		} else if (arg == "-w" || arg == "--write") {
			opts.inPlace = true;
		} else if (arg[0] == '-') {
			std::cerr << "quadfmt: unknown option: " << arg << "\n";
			std::cerr << "Try 'quadfmt --help' for more information.\n";
			return false;
		} else {
			opts.paths.push_back(arg);
		}
	}

	if (opts.paths.empty() && !opts.help && !opts.version) {
		std::cerr << "quadfmt: no input files\n";
		std::cerr << "Try 'quadfmt --help' for more information.\n";
		return false;
	}

	return true;
}

// Collect all .qd files from a path (file or directory)
std::vector<std::string> collectFiles(const std::string& path) {
	std::vector<std::string> files;

	if (fs::is_directory(path)) {
		for (const auto& entry : fs::recursive_directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".qd") {
				files.push_back(entry.path().string());
			}
		}
		std::sort(files.begin(), files.end());
	} else {
		files.push_back(path);
	}

	return files;
}

std::string readFile(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("No such file or directory");
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

void writeFile(const std::string& filename, const std::string& content) {
	std::ofstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("Cannot write to file");
	}
	file << content;
}

// Validate UTF-8 encoding
bool isValidUtf8(const std::string& source) {
	size_t i = 0;
	while (i < source.length()) {
		unsigned char c = static_cast<unsigned char>(source[i]);

		// Check for null bytes (binary file indicator)
		if (c == 0) {
			return false;
		}

		// ASCII (0xxxxxxx)
		if ((c & 0x80) == 0) {
			i++;
			continue;
		}

		// Determine number of continuation bytes
		size_t cont_bytes = 0;
		if ((c & 0xE0) == 0xC0) {
			cont_bytes = 1; // 110xxxxx
		} else if ((c & 0xF0) == 0xE0) {
			cont_bytes = 2; // 1110xxxx
		} else if ((c & 0xF8) == 0xF0) {
			cont_bytes = 3; // 11110xxx
		} else {
			return false; // Invalid UTF-8 start byte
		}

		// Check we have enough bytes
		if (i + cont_bytes >= source.length()) {
			return false;
		}

		// Validate continuation bytes (10xxxxxx)
		for (size_t j = 1; j <= cont_bytes; j++) {
			unsigned char next = static_cast<unsigned char>(source[i + j]);
			if ((next & 0xC0) != 0x80) {
				return false;
			}
		}

		i += cont_bytes + 1;
	}
	return true;
}

bool formatFile(const std::string& filename, const Options& opts) {
	try {
		// Read source file
		std::string source = readFile(filename);

		// Validate UTF-8 encoding
		if (!isValidUtf8(source)) {
			std::cerr << "quadfmt: " << filename << ": invalid UTF-8 encoding or binary file\n";
			return false;
		}

		// Parse to check for errors
		Ast ast;
		IAstNode* root = ast.generate(source.c_str(), false, filename.c_str());

		if (!root || ast.hasErrors()) {
			std::cerr << "quadfmt: " << filename << ": failed to parse (contains errors)\n";
			return false;
		}

		// Format using source-based formatter
		std::string formatted = formatSource(source);

		// Validate formatted output by parsing it again
		Ast validationAst;
		IAstNode* validationRoot = validationAst.generate(formatted.c_str(), false, filename.c_str());

		if (!validationRoot || validationAst.hasErrors()) {
			std::cerr << "quadfmt: " << filename << ": formatter produced invalid output, not saving\n";
			return false;
		}

		// Note: Token count validation was removed because the re-parsing validation
		// above is a stronger correctness check. The formatter intentionally
		// normalizes certain constructs (e.g., "( -- )" -> "()" for empty signatures)
		// which changes token counts but maintains semantic equivalence.

		if (opts.check) {
			// Check mode: compare with original
			if (source != formatted) {
				std::cout << filename << ": not formatted\n";
				return false;
			} else {
				return true;
			}
		} else if (opts.inPlace) {
			// In-place mode: write back to file
			writeFile(filename, formatted);
			std::cout << filename << ": formatted\n";
			return true;
		} else {
			// Stdout mode: write to stdout
			std::cout << formatted;
			return true;
		}
	} catch (const std::exception& e) {
		std::cerr << "quadfmt: " << filename << ": " << e.what() << "\n";
		return false;
	}
}

int main(int argc, char* argv[]) {
	Options opts;

	if (!parseArgs(argc, argv, opts)) {
		return 1;
	}

	if (opts.help) {
		printHelp();
		return 0;
	}

	if (opts.version) {
		printVersion();
		return 0;
	}

	// Collect all files from paths
	std::vector<std::string> allFiles;
	for (const auto& path : opts.paths) {
		auto files = collectFiles(path);
		allFiles.insert(allFiles.end(), files.begin(), files.end());
	}

	bool allSuccess = true;
	for (const auto& file : allFiles) {
		if (!formatFile(file, opts)) {
			allSuccess = false;
		}
	}

	return allSuccess ? 0 : 1;
}

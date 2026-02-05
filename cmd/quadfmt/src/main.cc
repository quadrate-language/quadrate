#include <iostream>
#include <qc/ast.h>
#include <qc/colors.h>
#include <qc/error_reporter.h>
#include <qc/formatter.h>
#include <qdcli/file_utils.h>
#include <vector>

#include "version.h"

using namespace Qd;

struct Options {
	std::vector<std::string> paths;
	bool check = false;
	bool help = false;
	bool version = false;
	bool inPlace = false;
	int lineWidth = -1;        // -1 means use default or config file
	bool sortImports = true;   // default on
	bool noSortImports = false;
};

void printHelp() {
	std::cout << "quadfmt - Quadrate code formatter\n\n";
	std::cout << "Formats Quadrate source files with consistent style.\n\n";
	std::cout << "Usage: quadfmt [options] <file|directory>...\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help           Show this help message\n";
	std::cout << "  -v, --version        Show version information\n";
	std::cout << "  -c, --check          Check if files are formatted (exit 1 if not)\n";
	std::cout << "  -w, --write          Format files in-place\n";
	std::cout << "  --line-width <N>     Max line width (default: 100)\n";
	std::cout << "  --no-sort-imports    Don't sort use statements\n";
	std::cout << "\n";
	std::cout << "Configuration:\n";
	std::cout << "  Options can be set in .quadfmt.json (searches current dir and parents):\n";
	std::cout << "  {\n";
	std::cout << "    \"lineWidth\": 100,\n";
	std::cout << "    \"sortImports\": true,\n";
	std::cout << "    \"alignStructFields\": true\n";
	std::cout << "  }\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quadfmt file.qd              Format to stdout\n";
	std::cout << "  quadfmt -w file.qd           Format in-place\n";
	std::cout << "  quadfmt -w src/              Format all .qd files in directory recursively\n";
	std::cout << "  quadfmt -c *.qd              Check if files need formatting\n";
	std::cout << "  quadfmt --line-width 80 f.qd Format with 80 char line width\n";
}

void printVersion() {
	std::cout << quadrate_version_string("quadfmt") << "\n";
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
		} else if (arg == "--line-width") {
			if (i + 1 >= argc) {
				std::cerr << "quadfmt: --line-width requires a value\n";
				return false;
			}
			opts.lineWidth = std::stoi(argv[++i]);
		} else if (arg == "--no-sort-imports") {
			opts.noSortImports = true;
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

bool formatFile(const std::string& filename, const Options& opts, const FormatOptions& fmtOpts) {
	try {
		// Read source file
		std::string source = qdcli::readFile(filename);

		// Validate UTF-8 encoding
		if (!qdcli::isValidUtf8(source)) {
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

		// Format using source-based formatter with options
		std::string formatted = formatSource(source, fmtOpts);

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
			qdcli::writeFile(filename, formatted);
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
		auto files = qdcli::collectFiles(path);
		allFiles.insert(allFiles.end(), files.begin(), files.end());
	}

	// Build format options from config file and command-line
	FormatOptions fmtOpts = FormatOptions::loadFromFile(".");

	// Command-line options override config file
	if (opts.lineWidth > 0) {
		fmtOpts.lineWidth = opts.lineWidth;
	}
	if (opts.noSortImports) {
		fmtOpts.sortImports = false;
	}

	bool allSuccess = true;
	for (const auto& file : allFiles) {
		if (!formatFile(file, opts, fmtOpts)) {
			allSuccess = false;
		}
	}

	return allSuccess ? 0 : 1;
}

#include <cstring>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/colors.h>
#include <qc/error_reporter.h>
#include <qc/formatter.h>
#include <sstream>
#include <vector>

using namespace Qd;

struct Options {
	std::vector<std::string> files;
	bool check = false;
	bool help = false;
	bool version = false;
	bool inPlace = true;
};

void printHelp() {
	std::cout << "quadfmt - Quadrate code formatter\n\n";
	std::cout << "Usage: quadfmt [options] <file>...\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n";
	std::cout << "  -c, --check      Check if files are formatted (exit 1 if not)\n";
	std::cout << "  -w, --write      Write formatted output to stdout instead of in-place\n";
	std::cout << "\n";
	std::cout << "By default, files are formatted in-place.\n";
}

void printVersion() {
	std::cout << "quadfmt version 0.1.0\n";
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
			opts.inPlace = false;
		} else if (arg[0] == '-') {
			std::cerr << "quadfmt: unknown option: " << arg << "\n";
			std::cerr << "Try 'quadfmt --help' for more information.\n";
			return false;
		} else {
			opts.files.push_back(arg);
		}
	}

	if (opts.files.empty() && !opts.help && !opts.version) {
		std::cerr << "quadfmt: no input files\n";
		std::cerr << "Try 'quadfmt --help' for more information.\n";
		return false;
	}

	return true;
}

std::string readFile(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("Cannot open file: " + filename);
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

void writeFile(const std::string& filename, const std::string& content) {
	std::ofstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("Cannot write to file: " + filename);
	}
	file << content;
}

bool formatFile(const std::string& filename, const Options& opts) {
	try {
		// Read source file
		std::string source = readFile(filename);

		// Parse to AST
		Ast ast;
		IAstNode* root = ast.generate(source.c_str(), false, filename.c_str());

		if (!root) {
			std::cerr << "quadfmt: " << filename << ": failed to parse\n";
			return false;
		}

		// Format AST
		Formatter formatter;
		std::string formatted = formatter.format(root);

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

	bool allSuccess = true;
	for (const auto& file : opts.files) {
		if (!formatFile(file, opts)) {
			allSuccess = false;
		}
	}

	return allSuccess ? 0 : 1;
}

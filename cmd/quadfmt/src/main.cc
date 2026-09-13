#include <cstring>
#include <iostream>
#include <iterator>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/file_utils.h>
#include <quadrate/cli/help.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/formatter.h>
#include <vector>

using namespace Qd;

struct FmtOptions {
	bool check = false;
	bool inPlace = false;
	bool noSortImports = false;
};

void printHelp() {
	qdcli::Help help("quadfmt", "Quadrate code formatter");
	help.description("Formats Quadrate source files with consistent style.")
			.usage("[options] <file|directory>...")
			.usage("[options] -", "read from stdin, write to stdout")
			.section("Options")
			.standardOptions()
			.option('c', "--check", "Report whether files are formatted (exit 1 if not)")
			.option('w', "--write", "Format files in place")
			.option("--no-sort-imports", "Leave use statements in their original order")
			.section("Configuration")
			.text("Options can be set in .quadfmt.json (searched for in the current")
			.text("directory and its parents):")
			.text()
			.text("  { \"sortImports\": true, \"alignStructFields\": true }")
			.section("Examples")
			.item("quadfmt file.qd", "Print the formatted file to stdout")
			.item("quadfmt -w file.qd", "Format in place")
			.item("quadfmt -w src/", "Format every .qd file in a directory, recursively")
			.item("quadfmt -c src/", "Check whether any file needs formatting (for CI)")
			.item("cat file.qd | quadfmt -", "Format a buffer piped from an editor");
	help.print();
}

// Format source read from stdin and write the result to stdout.
//
// Editors format-on-save by piping the buffer through the formatter rather than
// writing to disk first (gofmt, rustfmt, black and prettier all support this),
// so without it the editor plugins have to round-trip through a temp file.
static bool formatStdin(const FormatOptions& fmtOpts) {
	std::string source((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());

	if (!qdcli::isValidUtf8(source)) {
		std::cerr << "quadfmt: <stdin>: invalid UTF-8 encoding or binary input\n";
		return false;
	}

	std::string formatted = formatSource(source, fmtOpts);

	Ast validationAst;
	IAstNode* validationRoot = validationAst.generate(formatted.c_str(), false, "<stdin>");
	if (!validationRoot || validationAst.hasErrors()) {
		Ast sourceAst;
		sourceAst.generate(source.c_str(), false, "<stdin>");
		if (sourceAst.hasErrors()) {
			std::cerr << "quadfmt: <stdin>: failed to parse (contains errors)\n";
		} else {
			std::cerr << "quadfmt: <stdin>: internal error, formatted output does not parse\n";
		}
		// Pass the input through untouched: an editor must never lose the buffer
		// because the formatter could not handle it.
		std::cout << source;
		return false;
	}

	std::cout << formatted;
	return true;
}

bool formatFile(const std::string& filename, const FmtOptions& opts, const FormatOptions& fmtOpts) {
	try {
		// Read source file
		std::string source = qdcli::readFile(filename);

		// Validate UTF-8 encoding
		if (!qdcli::isValidUtf8(source)) {
			std::cerr << "quadfmt: " << filename << ": invalid UTF-8 encoding or binary file\n";
			return false;
		}

		// Format using AST-based formatter
		// Returns original unchanged if file has parse errors
		std::string formatted = formatSource(source, fmtOpts);

		// Validate formatted output by parsing it
		Ast validationAst;
		IAstNode* validationRoot = validationAst.generate(formatted.c_str(), false, filename.c_str());

		if (!validationRoot || validationAst.hasErrors()) {
			// Distinguish parse errors in source vs formatter bugs
			Ast sourceAst;
			sourceAst.generate(source.c_str(), false, filename.c_str());
			if (sourceAst.hasErrors()) {
				std::cerr << "quadfmt: " << filename << ": failed to parse (contains errors)\n";
			} else {
				std::cerr << "quadfmt: " << filename << ": formatter produced invalid output, not saving\n";
			}
			return false;
		}

		if (opts.check) {
			// Check mode: compare with original
			if (source != formatted) {
				std::cout << filename << ": not formatted\n";
				return false;
			}
			return true;
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
	qdcli::BaseOptions base;
	FmtOptions opts;

	auto handler = [&opts](const char* arg, int& /*i*/, int /*ac*/, char* /*av*/[]) -> bool {
		if (strcmp(arg, "-c") == 0 || strcmp(arg, "--check") == 0) {
			opts.check = true;
			return true;
		}
		if (strcmp(arg, "-w") == 0 || strcmp(arg, "--write") == 0) {
			opts.inPlace = true;
			return true;
		}
		if (strcmp(arg, "--no-sort-imports") == 0) {
			opts.noSortImports = true;
			return true;
		}
		return false;
	};

	if (!qdcli::parseArgs(argc, argv, base, "quadfmt", handler)) {
		return 1;
	}

	if (base.help) {
		printHelp();
		return 0;
	}

	if (base.version) {
		qdcli::printVersion("quadfmt");
		return 0;
	}

	if (opts.check && opts.inPlace) {
		return qdcli::usageError("quadfmt", "options -w and -c are mutually exclusive");
	}

	// A lone "-" means stdin. Handled before the no-input check, which would
	// otherwise reject it as a path that does not exist.
	const bool useStdin = (base.paths.size() == 1 && base.paths[0] == "-");

	if (!useStdin && qdcli::checkNoInputFiles(base, "quadfmt")) {
		return 1;
	}

	// Collect all files from paths
	std::vector<std::string> allFiles;
	for (const auto& path : base.paths) {
		auto files = qdcli::collectFiles(path);
		allFiles.insert(allFiles.end(), files.begin(), files.end());
	}

	// Build format options from config file and command-line
	FormatOptions fmtOpts = FormatOptions::loadFromFile(".");

	// Command-line options override config file
	if (opts.noSortImports) {
		fmtOpts.sortImports = false;
	}

	if (useStdin) {
		return formatStdin(fmtOpts) ? 0 : 1;
	}

	bool allSuccess = true;
	for (const auto& file : allFiles) {
		if (!formatFile(file, opts, fmtOpts)) {
			allSuccess = false;
		}
	}

	return allSuccess ? 0 : 1;
}

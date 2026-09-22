#include "options.h"
#include "version.h"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/help.h>
#include <unistd.h>

namespace fs = std::filesystem;

static constexpr size_t MAX_STACK_SIZE = 1ULL << 26;

void printHelp() {
	qdcli::Help help("quadc", "Quadrate compiler");
	help.description("Compiles .qd source files to native executables via LLVM.")
			.usage("[options] <file>...")
			.usage("[options] <file> -- [args]", "pass args to the program, with -r")
			.usage("[options] -", "read the source from stdin")
			.section("Options")
			.standardOptions()
			.option('o', "--output", "<name>", "Output executable name (default: the source file's name)")
			.item("-O0, -O1, -O2, -O3", "Set optimization level (default: -O0)")
			.option('g', "--debug", "Generate debug information for GDB/LLDB")
			.option('s', "--stack-size", "<size>", "Set stack size (default: 1024)")
			.option('I', "--include", "<path>", "Add a module search path (repeatable)")
			.option('l', "--module", "<mod@ver>", "Pin a module to a version (e.g. -l color@1.0.0)")
			.option('r', "--run", "Compile and run immediately (uses the JIT by default)")
			.option("--no-jit", "Link and execute instead of using the JIT, with -r")
			.option("--test", "Compile and run tests")
			.option("--coverage", "Print a function coverage report (use with --test)")
			.option("--target", "<triple>", "Cross-compile for a target (e.g. aarch64-linux-gnu)")
			.option("--freestanding", "No hosted runtime - emit .o with no libc and no auto-main")
			.option("--werror", "Treat warnings as errors")
			.option("--verbose", "Show detailed compilation steps")
			.option("--save-temps", "Keep temporary files for debugging")
			.option("--dump-tokens", "Print the lexer token stream")
			.option("--dump-ast", "Print the parsed AST")
			.option("--dump-ir", "Print the generated LLVM IR")
			.option("--", "Separate program arguments from compiler options (with -r)")
			.section("Examples")
			.item("quadc main.qd", "Compile to an executable named 'main'")
			.item("quadc -o prog main.qd", "Compile to an executable named 'prog'")
			.item("quadc -r main.qd", "Compile and run immediately")
			.item("quadc -r greet.qd -- Alice", "Compile and run with the argument 'Alice'")
			.item("quadc -r -", "Compile and run source piped on stdin");
	help.print();
}

void printVersion() {
	qdcli::printVersion("quadc");
}

bool parseArgs(int argc, char* argv[], Options& opts) {
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		// "--" separates compiler args from program args (for -r/--run)
		if (arg == "--") {
			// Everything after "--" goes to runArgs
			for (int j = i + 1; j < argc; j++) {
				opts.runArgs.push_back(argv[j]);
			}
			break;
		}

		if (arg == "-h" || arg == "--help") {
			opts.help = true;
			return true;
		} else if (arg == "-v" || arg == "--version") {
			opts.version = true;
			return true;
		} else if (arg == "-o" || arg == "--output") {
			if (i + 1 >= argc) {
				qdcli::usageError("quadc", "option '-o' requires an argument");
				return false;
			}
			opts.outputName = argv[++i];
			opts.outputNameSet = true;
		} else if (arg == "--save-temps") {
			opts.saveTemps = true;
		} else if (arg == "--verbose") {
			opts.verbose = true;
		} else if (arg == "--no-color" || arg == "--no-colors") {
			opts.noColor = true;
		} else if (arg == "--dump-tokens") {
			opts.dumpTokens = true;
		} else if (arg == "--dump-ast") {
			opts.dumpAst = true;
		} else if (arg == "-r" || arg == "--run") {
			opts.run = true;
		} else if (arg == "--no-jit") {
			opts.noJIT = true;
		} else if (arg == "--dump-ir") {
			opts.dumpIR = true;
		} else if (arg == "--test") {
			opts.testMode = true;
			opts.run = true; // Tests should be run automatically
		} else if (arg == "--coverage") {
			opts.coverage = true;
		} else if (arg == "--freestanding") {
			opts.freestanding = true;
		} else if (arg == "-g" || arg == "--debug") {
			opts.debugInfo = true;
		} else if (arg == "-I" || arg == "--include") {
			if (i + 1 >= argc) {
				qdcli::usageError("quadc", "option '-I' requires an argument");
				return false;
			}
			opts.includePaths.push_back(argv[++i]);
		} else if (arg == "-l" || arg == "--module") {
			if (i + 1 >= argc) {
				qdcli::usageError("quadc", "option '-l' requires an argument (module@version)");
				return false;
			}
			std::string moduleSpec = argv[++i];

			// Parse module@version format
			size_t atPos = moduleSpec.find('@');
			if (atPos == std::string::npos || atPos == 0 || atPos == moduleSpec.size() - 1) {
				qdcli::usageError("quadc",
						"invalid argument for '-l': '" + moduleSpec + "' (expected module@version, e.g. color@1.0.0)");
				return false;
			}

			std::string moduleName = moduleSpec.substr(0, atPos);
			std::string version = moduleSpec.substr(atPos + 1);
			opts.moduleVersions[moduleName] = version;
		} else if (arg == "--werror") {
			opts.werror = true;
		} else if (arg == "--target") {
			if (i + 1 >= argc) {
				qdcli::usageError("quadc", "option '--target' requires an argument");
				return false;
			}
			opts.targetTriple = argv[++i];
		} else if (arg == "-O0") {
			opts.optLevel = 0;
		} else if (arg == "-O1") {
			opts.optLevel = 1;
		} else if (arg == "-O2") {
			opts.optLevel = 2;
		} else if (arg == "-O3") {
			opts.optLevel = 3;
		} else if (arg == "-s" || arg == "--stack-size") {
			if (i + 1 >= argc) {
				qdcli::usageError("quadc", "option '-s' requires an argument");
				return false;
			}
			std::string value = argv[++i];
			if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
				qdcli::usageError("quadc", "invalid stack size: '" + value + "' (expected a positive integer)");
				return false;
			}
			if (value.size() > 10 || std::stoull(value) > MAX_STACK_SIZE) {
				qdcli::usageError("quadc", "stack size too large: " + value + " (maximum is " +
												   std::to_string(MAX_STACK_SIZE) + ")");
				return false;
			}
			opts.stackSize = std::stoull(value);
			if (opts.stackSize == 0) {
				qdcli::usageError("quadc", "stack size must be greater than 0");
				return false;
			}
		} else if (arg == "-") {
			// Read from stdin
			opts.readStdin = true;
		} else if (arg[0] == '-') {
			qdcli::usageError("quadc", "unknown option: " + arg);
			return false;
		} else {
			// If argument is a directory, look for main.qd inside it.
			// error_code overloads throughout: the throwing is_directory() aborts
			// the process on a path longer than NAME_MAX, before any diagnostic.
			// Anything that is not a readable directory falls through and is
			// reported later as the missing file it is.
			std::error_code ec;
			if (fs::is_directory(arg, ec) && !ec) {
				fs::path mainQd = fs::path(arg) / "main.qd";
				if (fs::exists(mainQd, ec) && !ec) {
					opts.files.push_back(mainQd.string());
				} else {
					qdcli::usageError("quadc", "no main.qd found in directory '" + arg + "'");
					return false;
				}
			} else {
				opts.files.push_back(arg);
			}
		}
	}

	if (opts.coverage && !opts.testMode) {
		qdcli::usageError("quadc", "'--coverage' requires '--test'");
		return false;
	}

	// If no files and stdin is piped, read from stdin
	if (opts.files.empty() && !opts.help && !opts.version && !opts.readStdin) {
		if (!isatty(STDIN_FILENO)) {
			opts.readStdin = true;
		} else {
			qdcli::usageError("quadc", "no input files");
			return false;
		}
	}

	return true;
}

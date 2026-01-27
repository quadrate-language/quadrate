#include "options.h"
#include "version.h"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <unistd.h>

namespace fs = std::filesystem;

void printHelp() {
	std::cout << "quadc - Quadrate compiler\n\n";
	std::cout << "Compiles .qd source files to native executables via LLVM.\n\n";
	std::cout << "Usage: quadc [options] <file>...\n";
	std::cout << "       quadc [options] <file> -- [args]   # Pass args to program with -r\n";
	std::cout << "       quadc [options] -                  # Read from stdin\n";
	std::cout << "       echo 'code' | quadc -r             # Pipe code to compile and run\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help         Show this help message\n";
	std::cout << "  -v, --version      Show version information\n";
	std::cout << "  -o <name>          Output executable name (default: main)\n";
	std::cout << "  -O0, -O1, -O2, -O3 Set optimization level (default: -O0)\n";
	std::cout << "  -s <size>          Set stack size (default: 1024)\n";
	std::cout << "  -g                 Generate debug information for GDB/LLDB\n";
	std::cout << "  -I <path>          Add module search path (can be used multiple times)\n";
	std::cout << "  -l <mod@ver>       Pin module to specific version (e.g., -l color@1.0.0)\n";
	std::cout << "  --save-temps       Keep temporary files for debugging\n";
	std::cout << "  --verbose          Show detailed compilation steps\n";
	std::cout << "  --dump-tokens      Print lexer tokens\n";
	std::cout << "  --dump-ast         Print parsed AST structure\n";
	std::cout << "  -r, --run          Compile and run immediately (uses JIT by default)\n";
	std::cout << "  --no-jit           Disable JIT execution for -r mode (use traditional linking)\n";
	std::cout << "  --test             Compile and run tests\n";
	std::cout << "  --dump-ir          Print generated LLVM IR\n";
	std::cout << "  --werror           Treat warnings as errors\n";
	std::cout << "  --target <triple>  Cross-compile for target (e.g., aarch64-linux-gnu)\n";
	std::cout << "  --                 Separator for program arguments (used with -r)\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quadc main.qd                        Compile to executable 'main'\n";
	std::cout << "  quadc -o prog main.qd                Compile to executable 'prog'\n";
	std::cout << "  quadc -r main.qd                     Compile and run immediately\n";
	std::cout << "  quadc -r greet.qd -- Alice           Compile and run with argument 'Alice'\n";
	std::cout << "  echo 'fn main(--) { 42 . }' | quadc -r   Compile and run from stdin\n";
}

void printVersion() {
	std::cout << quadrate_version_string("quadc") << "\n";
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
		} else if (arg == "-o") {
			if (i + 1 >= argc) {
				std::cerr << "quadc: option '-o' requires an argument\n";
				std::cerr << "Try 'quadc --help' for more information.\n";
				return false;
			}
			opts.outputName = argv[++i];
		} else if (arg == "--save-temps") {
			opts.saveTemps = true;
		} else if (arg == "--verbose") {
			opts.verbose = true;
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
		} else if (arg == "-g") {
			opts.debugInfo = true;
		} else if (arg == "-I") {
			if (i + 1 >= argc) {
				std::cerr << "quadc: option '-I' requires an argument\n";
				std::cerr << "Try 'quadc --help' for more information.\n";
				return false;
			}
			opts.includePaths.push_back(argv[++i]);
		} else if (arg == "-l") {
			if (i + 1 >= argc) {
				std::cerr << "quadc: option '-l' requires an argument (module@version)\n";
				std::cerr << "Try 'quadc --help' for more information.\n";
				return false;
			}
			std::string moduleSpec = argv[++i];

			// Parse module@version format
			size_t atPos = moduleSpec.find('@');
			if (atPos == std::string::npos || atPos == 0 || atPos == moduleSpec.size() - 1) {
				std::cerr << "quadc: invalid format for '-l': '" << moduleSpec << "'\n";
				std::cerr << "Expected format: module@version (e.g., color@1.0.0)\n";
				return false;
			}

			std::string moduleName = moduleSpec.substr(0, atPos);
			std::string version = moduleSpec.substr(atPos + 1);
			opts.moduleVersions[moduleName] = version;
		} else if (arg == "--werror") {
			opts.werror = true;
		} else if (arg == "--target") {
			if (i + 1 >= argc) {
				std::cerr << "quadc: option '--target' requires an argument\n";
				std::cerr << "Try 'quadc --help' for more information.\n";
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
		} else if (arg == "-s") {
			if (i + 1 >= argc) {
				std::cerr << "quadc: option '-s' requires an argument\n";
				std::cerr << "Try 'quadc --help' for more information.\n";
				return false;
			}
			try {
				opts.stackSize = std::stoull(argv[++i]);
				if (opts.stackSize == 0) {
					std::cerr << "quadc: stack size must be greater than 0\n";
					return false;
				}
			} catch (const std::exception&) {
				std::cerr << "quadc: invalid stack size: " << argv[i] << "\n";
				return false;
			}
		} else if (arg == "-") {
			// Read from stdin
			opts.readStdin = true;
		} else if (arg[0] == '-') {
			std::cerr << "quadc: unknown option: " << arg << "\n";
			std::cerr << "Try 'quadc --help' for more information.\n";
			return false;
		} else {
			// If argument is a directory, look for main.qd inside it
			if (fs::is_directory(arg)) {
				fs::path mainQd = fs::path(arg) / "main.qd";
				if (fs::exists(mainQd)) {
					opts.files.push_back(mainQd.string());
				} else {
					std::cerr << "quadc: no main.qd found in directory '" << arg << "'\n";
					std::cerr << "Try 'quadc --help' for more information.\n";
					return false;
				}
			} else {
				opts.files.push_back(arg);
			}
		}
	}

	// If no files and stdin is piped, read from stdin
	if (opts.files.empty() && !opts.help && !opts.version && !opts.readStdin) {
		if (!isatty(STDIN_FILENO)) {
			opts.readStdin = true;
		} else {
			std::cerr << "quadc: no input files\n";
			std::cerr << "Try 'quadc --help' for more information.\n";
			return false;
		}
	}

	return true;
}

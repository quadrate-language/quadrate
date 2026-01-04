#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <qc/colors.h>
#include <string>
#include <vector>

#include "version.h"

// Platform abstractions
extern "C" {
#include "src/platform/exe_path_platform.h"
#include "src/platform/process_platform.h"
}

namespace fs = std::filesystem;
using Qd::Colors;

struct Command {
	const char* name;
	const char* tool;
	const char* description;
};

static const Command commands[] = {
		{"build", "quadc", "Compile Quadrate source files"},
		{"run", "quadc", "Build and run a Quadrate program"},
		{"test", "quadc", "Run tests"},
		{"fmt", "quadfmt", "Format Quadrate source files"},
		{"lint", "quadlint", "Check code for common issues"},
		{"repl", "quadrepl", "Start interactive REPL"},
		{"uses", "quaduses", "Analyze module dependencies"},
		{"lsp", "quadlsp", "Start language server"},
};

static const size_t NUM_COMMANDS = sizeof(commands) / sizeof(commands[0]);

void printHelp() {
	std::cout << Colors::bold() << "quad" << Colors::reset() << " - Quadrate language toolchain\n\n";
	std::cout << Colors::bold() << "Usage:" << Colors::reset() << " quad <command> [options] [arguments]\n";
	std::cout << "       quad <file.qd> [arguments]   (run a script directly)\n\n";
	std::cout << Colors::bold() << "Commands:" << Colors::reset() << "\n";
	std::cout << "  " << Colors::green() << "build" << Colors::reset() << "     Compile Quadrate source files\n";
	std::cout << "  " << Colors::green() << "run" << Colors::reset() << "       Build and run a Quadrate program\n";
	std::cout << "  " << Colors::green() << "test" << Colors::reset() << "      Run tests\n";
	std::cout << "  " << Colors::green() << "fmt" << Colors::reset() << "       Format Quadrate source files\n";
	std::cout << "  " << Colors::green() << "lint" << Colors::reset() << "      Check code for common issues\n";
	std::cout << "  " << Colors::green() << "repl" << Colors::reset() << "      Start interactive REPL\n";
	std::cout << "  " << Colors::green() << "uses" << Colors::reset() << "      Analyze module dependencies\n";
	std::cout << "  " << Colors::green() << "lsp" << Colors::reset() << "       Start language server\n";
	std::cout << "  " << Colors::green() << "help" << Colors::reset() << "      Show help for a command\n";
	std::cout << "  " << Colors::green() << "version" << Colors::reset() << "   Show version information\n";
	std::cout << "\n";
	std::cout << Colors::bold() << "Examples:" << Colors::reset() << "\n";
	std::cout << "  " << Colors::cyan() << "quad build main.qd" << Colors::reset() << "           Compile main.qd\n";
	std::cout << "  " << Colors::cyan() << "quad run main.qd" << Colors::reset()
			  << "             Build and run main.qd\n";
	std::cout << "  " << Colors::cyan() << "quad run greet.qd -- Alice" << Colors::reset()
			  << "   Run with argument 'Alice'\n";
	std::cout << "  " << Colors::cyan() << "quad fmt" << Colors::reset()
			  << "                     Format all .qd files in-place\n";
	std::cout << "  " << Colors::cyan() << "quad test" << Colors::reset()
			  << "                    Run tests in current directory\n";
	std::cout << "  " << Colors::cyan() << "quad script.qd" << Colors::reset()
			  << "               Run script directly (for shebang)\n";
	std::cout << "\n";
	std::cout << "Run '" << Colors::cyan() << "quad help <command>" << Colors::reset()
			  << "' for more information on a command.\n";
}

void printVersion() {
	std::cout << quadrate_version_string("quad") << "\n";
}

// Get the directory where the quad binary is located
fs::path getExecutableDir() {
	char path[4096];
	int len = exe_path_platform_get(path, sizeof(path));
	if (len > 0 && static_cast<size_t>(len) < sizeof(path)) {
		return fs::path(path).parent_path();
	}
	return fs::path();
}

// Find a tool by name, first in the same directory as quad, then in PATH
std::string findTool(const std::string& toolName) {
	// First, check in the same directory as the quad binary
	fs::path execDir = getExecutableDir();
	if (!execDir.empty()) {
		fs::path toolPath = execDir / toolName;
		if (fs::exists(toolPath)) {
			return toolPath.string();
		}
		// Also check sibling directory pattern (for build directory structure)
		// e.g., build/debug/cmd/quad/quad -> build/debug/cmd/quadc/quadc
		toolPath = execDir.parent_path() / toolName / toolName;
		if (fs::exists(toolPath)) {
			return toolPath.string();
		}
	}

	// Fall back to PATH lookup
	const char* pathEnv = std::getenv("PATH");
	if (pathEnv) {
		std::string pathStr(pathEnv);
		size_t start = 0;
		size_t end;
		while ((end = pathStr.find(':', start)) != std::string::npos) {
			std::string dir = pathStr.substr(start, end - start);
			fs::path toolPath = fs::path(dir) / toolName;
			if (fs::exists(toolPath)) {
				return toolPath.string();
			}
			start = end + 1;
		}
		// Check last segment
		std::string dir = pathStr.substr(start);
		fs::path toolPath = fs::path(dir) / toolName;
		if (fs::exists(toolPath)) {
			return toolPath.string();
		}
	}

	return "";
}

// Execute a tool with the given arguments
int execTool(const std::string& toolPath, const std::vector<std::string>& args) {
	std::vector<char*> argv;
	argv.push_back(const_cast<char*>(toolPath.c_str()));
	for (const auto& arg : args) {
		argv.push_back(const_cast<char*>(arg.c_str()));
	}
	argv.push_back(nullptr);

	int result = process_platform_exec_wait(toolPath.c_str(), argv.data());
	if (result == -1) {
		std::cerr << "quad: failed to execute " << toolPath << "\n";
		return 1;
	}
	return result;
}

const Command* findCommand(const std::string& name) {
	for (size_t i = 0; i < NUM_COMMANDS; i++) {
		if (name == commands[i].name) {
			return &commands[i];
		}
	}
	return nullptr;
}

// Find .qd files in the current directory
std::vector<std::string> findQdFiles() {
	std::vector<std::string> files;
	for (const auto& entry : fs::directory_iterator(fs::current_path())) {
		if (entry.path().extension() == ".qd") {
			files.push_back(entry.path().string());
		}
	}
	std::sort(files.begin(), files.end());
	return files;
}

int handleBuild(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadc");
	if (toolPath.empty()) {
		std::cerr << "quad: quadc not found\n";
		return 1;
	}

	std::vector<std::string> toolArgs = args;

	// If no files specified, look for .qd files in current directory
	if (toolArgs.empty()) {
		auto files = findQdFiles();
		if (files.empty()) {
			std::cerr << "quad: no .qd files found in current directory\n";
			return 1;
		}
		// Look for main.qd first
		auto it = std::find_if(
				files.begin(), files.end(), [](const std::string& f) { return fs::path(f).filename() == "main.qd"; });
		if (it != files.end()) {
			toolArgs.push_back(*it);
		} else {
			toolArgs = files;
		}
	}

	return execTool(toolPath, toolArgs);
}

int handleRun(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadc");
	if (toolPath.empty()) {
		std::cerr << "quad: quadc not found\n";
		return 1;
	}

	std::vector<std::string> toolArgs;
	toolArgs.push_back("-r");

	if (args.empty()) {
		auto files = findQdFiles();
		if (files.empty()) {
			std::cerr << "quad: no .qd files found in current directory\n";
			return 1;
		}
		auto it = std::find_if(
				files.begin(), files.end(), [](const std::string& f) { return fs::path(f).filename() == "main.qd"; });
		if (it != files.end()) {
			toolArgs.push_back(*it);
		} else {
			toolArgs.push_back(files[0]);
		}
	} else {
		for (const auto& arg : args) {
			toolArgs.push_back(arg);
		}
	}

	return execTool(toolPath, toolArgs);
}

// Check if a path should be skipped during test discovery
bool shouldSkipDir(const fs::path& path) {
	std::string name = path.filename().string();
	// Skip hidden directories, build artifacts, and dist
	if (!name.empty() && name[0] == '.') {
		return true;
	}
	if (name == "build" || name == "dist" || name == "node_modules") {
		return true;
	}
	return false;
}

// Find test files recursively in a directory
std::vector<std::string> findTestFilesRecursive(const fs::path& dir) {
	std::vector<std::string> files;
	try {
		for (auto it = fs::recursive_directory_iterator(dir); it != fs::recursive_directory_iterator(); ++it) {
			const auto& entry = *it;
			// Skip certain directories
			if (entry.is_directory() && shouldSkipDir(entry.path())) {
				it.disable_recursion_pending();
				continue;
			}
			if (entry.path().extension() == ".qd") {
				std::string filename = entry.path().stem().string();
				if (filename.find("test_") == 0 ||
						(filename.length() >= 5 && filename.rfind("_test") == filename.length() - 5)) {
					files.push_back(entry.path().string());
				}
			}
		}
	} catch (const fs::filesystem_error&) {
		// Ignore permission errors etc.
	}
	std::sort(files.begin(), files.end());
	return files;
}

int handleTest(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadc");
	if (toolPath.empty()) {
		std::cerr << "quad: quadc not found\n";
		return 1;
	}

	// Collect test files
	std::vector<std::string> testFiles;
	std::vector<std::string> options;
	bool recursive = false;

	for (const auto& arg : args) {
		if (arg == "./..." || arg == "...") {
			recursive = true;
		} else if (!arg.empty() && arg[0] == '-') {
			options.push_back(arg);
		} else {
			testFiles.push_back(arg);
		}
	}

	// If no files specified, look for *_test.qd or test_*.qd files
	if (testFiles.empty()) {
		if (recursive) {
			// Search recursively from current directory
			testFiles = findTestFilesRecursive(fs::current_path());
		} else {
			// Search only current directory
			for (const auto& entry : fs::directory_iterator(fs::current_path())) {
				if (entry.path().extension() == ".qd") {
					std::string filename = entry.path().stem().string();
					if (filename.find("test_") == 0 ||
							(filename.length() >= 5 && filename.rfind("_test") == filename.length() - 5)) {
						testFiles.push_back(entry.path().string());
					}
				}
			}
		}
		if (testFiles.empty()) {
			std::cerr << "quad: no test files found (looking for *_test.qd or test_*.qd)\n";
			if (!recursive) {
				std::cerr << "hint: use 'quad test ./...' to search recursively\n";
			}
			return 1;
		}
		std::sort(testFiles.begin(), testFiles.end());
	}

	// Run each test file separately (quadc --test only processes one file at a time)
	int result = 0;
	for (const auto& file : testFiles) {
		std::vector<std::string> toolArgs;
		toolArgs.push_back("--test");
		for (const auto& opt : options) {
			toolArgs.push_back(opt);
		}
		toolArgs.push_back(file);

		int r = execTool(toolPath, toolArgs);
		if (r != 0) {
			result = r;
		}
	}

	return result;
}

int handleFmt(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadfmt");
	if (toolPath.empty()) {
		std::cerr << "quad: quadfmt not found\n";
		return 1;
	}

	std::vector<std::string> toolArgs;

	// Add -w flag by default to format in place (unless user passes --check or -n)
	bool hasCheckFlag = false;
	for (const auto& arg : args) {
		if (arg == "--check" || arg == "-n") {
			hasCheckFlag = true;
			break;
		}
	}
	if (!hasCheckFlag) {
		toolArgs.push_back("-w");
	}

	// Add user-provided args
	for (const auto& arg : args) {
		toolArgs.push_back(arg);
	}

	// If no files specified, format all .qd files in current directory
	bool hasFiles = false;
	for (const auto& arg : args) {
		if (!arg.empty() && arg[0] != '-') {
			hasFiles = true;
			break;
		}
	}
	if (!hasFiles) {
		auto files = findQdFiles();
		if (files.empty()) {
			std::cerr << "quad: no .qd files found in current directory\n";
			return 1;
		}
		for (const auto& f : files) {
			toolArgs.push_back(f);
		}
	}

	return execTool(toolPath, toolArgs);
}

int handleLint(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadlint");
	if (toolPath.empty()) {
		std::cerr << "quad: quadlint not found\n";
		return 1;
	}

	std::vector<std::string> toolArgs = args;

	if (toolArgs.empty()) {
		auto files = findQdFiles();
		if (files.empty()) {
			std::cerr << "quad: no .qd files found in current directory\n";
			return 1;
		}
		toolArgs = files;
	}

	return execTool(toolPath, toolArgs);
}

int handleRepl(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadrepl");
	if (toolPath.empty()) {
		std::cerr << "quad: quadrepl not found\n";
		return 1;
	}
	return execTool(toolPath, args);
}

int handleUses(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quaduses");
	if (toolPath.empty()) {
		std::cerr << "quad: quaduses not found\n";
		return 1;
	}

	std::vector<std::string> toolArgs;

	// Add -w flag by default to update in place (unless user passes --check or -n)
	bool hasCheckFlag = false;
	for (const auto& arg : args) {
		if (arg == "--check" || arg == "-n") {
			hasCheckFlag = true;
			break;
		}
	}
	if (!hasCheckFlag) {
		toolArgs.push_back("-w");
	}

	// Add user-provided args
	for (const auto& arg : args) {
		toolArgs.push_back(arg);
	}

	// If no files specified, process all .qd files in current directory
	bool hasFiles = false;
	for (const auto& arg : args) {
		if (!arg.empty() && arg[0] != '-') {
			hasFiles = true;
			break;
		}
	}
	if (!hasFiles) {
		auto files = findQdFiles();
		if (files.empty()) {
			std::cerr << "quad: no .qd files found in current directory\n";
			return 1;
		}
		for (const auto& f : files) {
			toolArgs.push_back(f);
		}
	}

	// quaduses processes one file at a time, so we need to call it for each file
	int result = 0;
	std::vector<std::string> fileArgs;
	std::vector<std::string> optionArgs;
	for (const auto& arg : toolArgs) {
		if (!arg.empty() && arg[0] == '-') {
			optionArgs.push_back(arg);
		} else {
			fileArgs.push_back(arg);
		}
	}

	for (const auto& file : fileArgs) {
		std::vector<std::string> singleFileArgs = optionArgs;
		singleFileArgs.push_back(file);
		int r = execTool(toolPath, singleFileArgs);
		if (r != 0) {
			result = r;
		}
	}

	return result;
}

int handleLsp(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadlsp");
	if (toolPath.empty()) {
		std::cerr << "quad: quadlsp not found\n";
		return 1;
	}
	return execTool(toolPath, args);
}

int handleHelp(const std::vector<std::string>& args) {
	if (args.empty()) {
		printHelp();
		return 0;
	}

	const std::string& cmd = args[0];
	const Command* command = findCommand(cmd);

	if (command) {
		std::string toolPath = findTool(command->tool);
		if (!toolPath.empty()) {
			return execTool(toolPath, {"--help"});
		}
		std::cerr << "quad: " << command->tool << " not found\n";
		return 1;
	}

	std::cerr << "quad: unknown command '" << cmd << "'\n";
	std::cerr << "Run 'quad help' for usage.\n";
	return 1;
}

int main(int argc, char* argv[]) {
	// Configure colored output - honor NO_COLOR environment variable
	const bool noColors = std::getenv("NO_COLOR") != nullptr;
	Colors::setEnabled(!noColors);

	if (argc < 2) {
		printHelp();
		return 0;
	}

	std::string command = argv[1];

	// Collect remaining arguments
	std::vector<std::string> args;
	for (int i = 2; i < argc; i++) {
		args.push_back(argv[i]);
	}

	// Check if first argument is a .qd file (for shebang support: #!/usr/bin/quad)
	// If so, run it directly using quadc -r
	if (command.length() > 3 && command.substr(command.length() - 3) == ".qd") {
		std::string toolPath = findTool("quadc");
		if (toolPath.empty()) {
			std::cerr << "quad: quadc not found\n";
			return 1;
		}
		std::vector<std::string> toolArgs;
		toolArgs.push_back("-r");
		toolArgs.push_back(command);
		for (const auto& arg : args) {
			toolArgs.push_back(arg);
		}
		return execTool(toolPath, toolArgs);
	}

	// Handle built-in commands
	if (command == "help" || command == "-h" || command == "--help") {
		return handleHelp(args);
	}

	if (command == "version" || command == "-v" || command == "--version") {
		printVersion();
		return 0;
	}

	// Handle subcommands
	if (command == "build") {
		return handleBuild(args);
	}
	if (command == "run") {
		return handleRun(args);
	}
	if (command == "test") {
		return handleTest(args);
	}
	if (command == "fmt") {
		return handleFmt(args);
	}
	if (command == "lint") {
		return handleLint(args);
	}
	if (command == "repl") {
		return handleRepl(args);
	}
	if (command == "uses") {
		return handleUses(args);
	}
	if (command == "lsp") {
		return handleLsp(args);
	}

	std::cerr << "quad: unknown command '" << command << "'\n";
	std::cerr << "Run 'quad help' for usage.\n";
	return 1;
}

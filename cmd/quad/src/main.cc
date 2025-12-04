#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static const char* VERSION = "0.1.0";

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
	std::cout << "quad - Quadrate language toolchain\n\n";
	std::cout << "Usage: quad <command> [options] [arguments]\n\n";
	std::cout << "Commands:\n";
	std::cout << "  build     Compile Quadrate source files\n";
	std::cout << "  run       Build and run a Quadrate program\n";
	std::cout << "  test      Run tests\n";
	std::cout << "  fmt       Format Quadrate source files\n";
	std::cout << "  lint      Check code for common issues\n";
	std::cout << "  repl      Start interactive REPL\n";
	std::cout << "  uses      Analyze module dependencies\n";
	std::cout << "  lsp       Start language server\n";
	std::cout << "  help      Show help for a command\n";
	std::cout << "  version   Show version information\n";
	std::cout << "\n";
	std::cout << "Examples:\n";
	std::cout << "  quad build main.qd        Compile main.qd\n";
	std::cout << "  quad run main.qd          Build and run main.qd\n";
	std::cout << "  quad fmt -w *.qd          Format all .qd files in-place\n";
	std::cout << "  quad test                 Run tests in current directory\n";
	std::cout << "\n";
	std::cout << "Run 'quad help <command>' for more information on a command.\n";
}

void printVersion() {
	std::cout << "quad " << VERSION << "\n";
}

// Get the directory where the quad binary is located
fs::path getExecutableDir() {
	char path[4096];
	ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (len != -1) {
		path[len] = '\0';
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

	pid_t pid = fork();
	if (pid == -1) {
		std::cerr << "quad: failed to fork: " << strerror(errno) << "\n";
		return 1;
	}

	if (pid == 0) {
		// Child process
		execv(toolPath.c_str(), argv.data());
		std::cerr << "quad: failed to execute " << toolPath << ": " << strerror(errno) << "\n";
		_exit(127);
	}

	// Parent process
	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return 1;
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
		auto it = std::find_if(files.begin(), files.end(), [](const std::string& f) {
			return fs::path(f).filename() == "main.qd";
		});
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
		auto it = std::find_if(files.begin(), files.end(), [](const std::string& f) {
			return fs::path(f).filename() == "main.qd";
		});
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

int handleTest(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadc");
	if (toolPath.empty()) {
		std::cerr << "quad: quadc not found\n";
		return 1;
	}

	std::vector<std::string> toolArgs;
	toolArgs.push_back("--test");

	// If no files specified, look for *_test.qd or test_*.qd files
	if (args.empty()) {
		std::vector<std::string> testFiles;
		for (const auto& entry : fs::directory_iterator(fs::current_path())) {
			if (entry.path().extension() == ".qd") {
				std::string filename = entry.path().stem().string();
				if (filename.find("test_") == 0 || filename.rfind("_test") == filename.length() - 5) {
					testFiles.push_back(entry.path().string());
				}
			}
		}
		if (testFiles.empty()) {
			std::cerr << "quad: no test files found (looking for *_test.qd or test_*.qd)\n";
			return 1;
		}
		std::sort(testFiles.begin(), testFiles.end());
		for (const auto& f : testFiles) {
			toolArgs.push_back(f);
		}
	} else {
		for (const auto& arg : args) {
			toolArgs.push_back(arg);
		}
	}

	return execTool(toolPath, toolArgs);
}

int handleFmt(const std::vector<std::string>& args) {
	std::string toolPath = findTool("quadfmt");
	if (toolPath.empty()) {
		std::cerr << "quad: quadfmt not found\n";
		return 1;
	}

	std::vector<std::string> toolArgs = args;

	// If no files specified, format all .qd files in current directory
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
	return execTool(toolPath, args);
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

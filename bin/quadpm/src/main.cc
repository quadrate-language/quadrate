// quadpm - Quadrate Package Manager
// Manages 3rd party Git-based modules

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ANSI color codes for pretty output
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_CYAN "\033[36m"

// Get home directory
std::string getHomeDir() {
	const char* home = getenv("HOME");
	if (!home) {
		std::cerr << COLOR_RED << "Error: HOME environment variable not set" << COLOR_RESET << "\n";
		exit(1);
	}
	return std::string(home);
}

// Get packages directory path
std::string getPackagesDir() {
	// Check QUADRATE_CACHE environment variable first
	const char* quadrateCache = getenv("QUADRATE_CACHE");
	if (quadrateCache) {
		return std::string(quadrateCache);
	}

	// Check if XDG_DATA_HOME is set
	const char* xdgDataHome = getenv("XDG_DATA_HOME");
	if (xdgDataHome) {
		return std::string(xdgDataHome) + "/quadrate/packages";
	}

	// Default to ~/quadrate/packages
	return getHomeDir() + "/quadrate/packages";
}

// Execute a shell command and capture output
std::string execCommand(const std::string& cmd) {
	std::array<char, 128> buffer;
	std::string result;
	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe) {
		throw std::runtime_error("popen() failed!");
	}
	while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
		result += buffer.data();
	}
	int status = pclose(pipe);
	if (status != 0) {
		throw std::runtime_error("Command failed with status: " + std::to_string(status));
	}
	return result;
}

// Execute a shell command, showing output in real-time
int execCommandLive(const std::string& cmd) {
	return system(cmd.c_str());
}

// Extract module name from Git URL
// Examples:
//   https://git.sr.ht/~user/zlib -> zlib
//   https://github.com/user/http-lib -> http-lib
//   git@github.com:user/json.git -> json
std::string extractModuleName(const std::string& gitUrl) {
	// Find last '/' or ':'
	size_t lastSlash = gitUrl.find_last_of("/:");
	if (lastSlash == std::string::npos) {
		return gitUrl;
	}

	std::string name = gitUrl.substr(lastSlash + 1);

	// Remove .git suffix if present
	if (name.size() > 4 && name.substr(name.size() - 4) == ".git") {
		name = name.substr(0, name.size() - 4);
	}

	return name;
}

// Parse Git reference (tag, branch, or commit)
// Format: url[@ref]
// Examples:
//   https://git.sr.ht/~user/zlib@1.2.0
//   https://github.com/user/http@main
//   https://github.com/user/json  (defaults to main)
struct GitRef {
	std::string url;
	std::string ref;
	std::string moduleName;
};

GitRef parseGitUrl(const std::string& input) {
	GitRef result;

	// Check if there's an @ symbol for version/ref
	size_t atPos = input.find_last_of('@');

	// Make sure @ is not part of git@github.com
	if (atPos != std::string::npos && atPos > 0 && input[atPos - 1] != ':') {
		result.url = input.substr(0, atPos);
		result.ref = input.substr(atPos + 1);
	} else {
		result.url = input;
		result.ref = "main"; // Default to main branch
	}

	result.moduleName = extractModuleName(result.url);
	return result;
}

// Get the installed directory name for a module@ref
std::string getInstalledDirName(const std::string& moduleName, const std::string& ref) {
	return moduleName + "@" + ref;
}

// Clone a Git repository to the packages directory
bool gitClone(const GitRef& gitRef) {
	std::string packagesDir = getPackagesDir();

	// Create packages directory if it doesn't exist
	fs::create_directories(packagesDir);

	std::string targetDir = packagesDir + "/" + getInstalledDirName(gitRef.moduleName, gitRef.ref);

	// Check if already exists
	if (fs::exists(targetDir)) {
		std::cout << COLOR_YELLOW << "Package already exists: " << COLOR_RESET << targetDir << "\n";
		std::cout << COLOR_CYAN << "Use 'quadpm update' to update it" << COLOR_RESET << "\n";
		return true;
	}

	std::cout << COLOR_CYAN << "Fetching " << COLOR_BOLD << gitRef.moduleName << COLOR_RESET << COLOR_CYAN << " "
			  << gitRef.ref << "..." << COLOR_RESET << "\n";
	std::cout << "  → Cloning " << gitRef.url << "\n";

	// Clone with --depth 1 for faster download
	std::string cloneCmd = "git clone --depth 1 --branch " + gitRef.ref + " " + gitRef.url + " " + targetDir + " 2>&1";

	int result = execCommandLive(cloneCmd);

	if (result != 0) {
		std::cerr << COLOR_RED << "Error: Failed to clone repository" << COLOR_RESET << "\n";
		// Try to clean up partial clone
		if (fs::exists(targetDir)) {
			fs::remove_all(targetDir);
		}
		return false;
	}

	std::cout << COLOR_GREEN << "  ✓ Installed to " << COLOR_RESET << targetDir << "\n";

	// Show what module file was found
	std::string moduleFile = targetDir + "/module.qd";
	if (fs::exists(moduleFile)) {
		std::cout << COLOR_GREEN << "  ✓ Found module.qd" << COLOR_RESET << "\n";
	} else {
		std::cout << COLOR_YELLOW << "  ⚠ Warning: module.qd not found at root" << COLOR_RESET << "\n";
		std::cout << "    Package may need to be structured with module.qd at root\n";
	}

	// Check for C source files and compile if found
	std::string srcDir = targetDir + "/src";
	if (fs::exists(srcDir) && fs::is_directory(srcDir)) {
		std::cout << "  → Found src/ directory, compiling C sources...\n";

		// Collect all .c files
		std::vector<std::string> cFiles;
		for (const auto& entry : fs::directory_iterator(srcDir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".c") {
				cFiles.push_back(entry.path().string());
			}
		}

		if (!cFiles.empty()) {
			// Create lib directory
			std::string libDir = targetDir + "/lib";
			fs::create_directories(libDir);

			// Library names
			std::string libName = "lib" + gitRef.moduleName;
			std::string sharedLib = libDir + "/" + libName + ".so";
			std::string staticLib = libDir + "/" + libName + "_static.a";

			// Prefer clang, fallback to gcc
			std::string compiler = "gcc";
			if (system("which clang > /dev/null 2>&1") == 0) {
				compiler = "clang";
			}

			// Compile to object files
			std::vector<std::string> objFiles;
			bool compileFailed = false;

			for (const auto& cFile : cFiles) {
				std::string objFile = libDir + "/" + fs::path(cFile).stem().string() + ".o";
				objFiles.push_back(objFile);

				// Compile with -fPIC for shared library compatibility
				// Try to find Quadrate headers in common locations
				std::string includeFlags = "-I/usr/include";

				// Check for local development build
				if (fs::exists("dist/include/qdrt")) {
					includeFlags += " -Idist/include";
				}
				// Check for installed headers
				if (fs::exists("/usr/include/qdrt")) {
					// Already in /usr/include
				}

				std::string compileCmd = compiler + " -c -fPIC -O2 -Wall " + includeFlags + " " + cFile + " -o " + objFile + " 2>&1";

				int compileResult = execCommandLive(compileCmd);
				if (compileResult != 0) {
					std::cerr << COLOR_RED << "  ✗ Failed to compile " << COLOR_RESET << cFile << "\n";
					compileFailed = true;
					break;
				}
			}

			if (!compileFailed && !objFiles.empty()) {
				// Create shared library
				std::string objList;
				for (const auto& obj : objFiles) {
					objList += obj + " ";
				}

				std::string linkSharedCmd = compiler + " -shared " + objList + "-o " + sharedLib + " 2>&1";
				int linkResult = execCommandLive(linkSharedCmd);

				if (linkResult == 0) {
					std::cout << COLOR_GREEN << "  ✓ Built " << COLOR_RESET << libName << ".so\n";
				} else {
					std::cerr << COLOR_YELLOW << "  ⚠ Failed to build shared library" << COLOR_RESET << "\n";
				}

				// Create static library
				std::string arCmd = "ar rcs " + staticLib + " " + objList + "2>&1";
				int arResult = execCommandLive(arCmd);

				if (arResult == 0) {
					std::cout << COLOR_GREEN << "  ✓ Built " << COLOR_RESET << libName << "_static.a\n";
				} else {
					std::cerr << COLOR_YELLOW << "  ⚠ Failed to build static library" << COLOR_RESET << "\n";
				}

				// Clean up object files
				for (const auto& obj : objFiles) {
					fs::remove(obj);
				}
			}
		} else {
			std::cout << COLOR_YELLOW << "  ⚠ No .c files found in src/" << COLOR_RESET << "\n";
		}
	}

	return true;
}

// Print version information
void printVersion() {
	std::cout << "quadpm 0.1.0\n";
}

// Print usage information
void printUsage() {
	std::cout << "quadpm - Quadrate package manager\n\n";
	std::cout << "Manages 3rd party modules from Git repositories.\n\n";
	std::cout << "Usage: quadpm [options] <command> [arguments]\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n\n";
	std::cout << "Commands:\n";
	std::cout << "  get <url>[@ref]  Fetch and install a package from Git\n";
	std::cout << "  list             List installed packages\n\n";
	std::cout << "Examples:\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib@1.2.0\n";
	std::cout << "  quadpm get https://github.com/user/http@main\n";
	std::cout << "  quadpm list\n\n";
	std::cout << "Environment:\n";
	std::cout << "  QUADRATE_CACHE     Package installation directory\n";
	std::cout << "  XDG_DATA_HOME      If set, uses $XDG_DATA_HOME/quadrate/packages\n";
	std::cout << "  Default: ~/quadrate/packages\n";
}

// List installed packages
void listPackages() {
	std::string packagesDir = getPackagesDir();

	if (!fs::exists(packagesDir)) {
		std::cout << "No packages installed yet.\n";
		std::cout << "Packages will be installed to: " << packagesDir << "\n";
		return;
	}

	std::cout << COLOR_BOLD << "Installed packages:" << COLOR_RESET << "\n";
	std::cout << "Location: " << packagesDir << "\n\n";

	bool found = false;
	for (const auto& entry : fs::directory_iterator(packagesDir)) {
		if (entry.is_directory()) {
			found = true;
			std::string name = entry.path().filename().string();

			// Parse module@version format
			size_t atPos = name.find('@');
			if (atPos != std::string::npos) {
				std::string module = name.substr(0, atPos);
				std::string version = name.substr(atPos + 1);
				std::cout << "  " << COLOR_BOLD << module << COLOR_RESET << " @ " << COLOR_CYAN << version
						  << COLOR_RESET << "\n";
			} else {
				std::cout << "  " << name << "\n";
			}

			// Check for module.qd
			std::string moduleFile = entry.path().string() + "/module.qd";
			if (fs::exists(moduleFile)) {
				std::cout << "    → " << COLOR_GREEN << "module.qd found" << COLOR_RESET << "\n";
			} else {
				std::cout << "    → " << COLOR_YELLOW << "module.qd missing" << COLOR_RESET << "\n";
			}
		}
	}

	if (!found) {
		std::cout << "No packages installed.\n";
	}
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printUsage();
		return 1;
	}

	std::string command = argv[1];

	if (command == "-h" || command == "--help") {
		printUsage();
		return 0;
	}

	if (command == "-v" || command == "--version") {
		printVersion();
		return 0;
	}

	if (command == "get") {
		if (argc < 3) {
			std::cerr << COLOR_RED << "Error: 'get' requires a Git URL" << COLOR_RESET << "\n";
			std::cerr << "Usage: " << argv[0] << " get <git-url>[@ref]\n";
			std::cerr << "Example: " << argv[0] << " get https://git.sr.ht/~user/zlib@1.2.0\n";
			return 1;
		}

		std::string gitUrl = argv[2];
		GitRef gitRef = parseGitUrl(gitUrl);

		if (!gitClone(gitRef)) {
			return 1;
		}

		std::cout << "\n"
				  << COLOR_GREEN << "Success!" << COLOR_RESET
				  << " You can now use this module in your Quadrate code:\n";
		std::cout << "  " << COLOR_CYAN << "use " << gitRef.moduleName << COLOR_RESET << "\n";

		return 0;
	}

	if (command == "list" || command == "ls") {
		listPackages();
		return 0;
	}

	std::cerr << COLOR_RED << "Error: Unknown command '" << command << "'" << COLOR_RESET << "\n";
	printUsage();
	return 1;
}

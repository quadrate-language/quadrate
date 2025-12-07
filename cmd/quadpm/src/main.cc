// quadpm - Quadrate Module Manager
// Manages 3rd party Git-based modules

#include "git_ref.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

// Get modules directory path
std::string getModulesDir() {
	// Check QUADRATE_PATH environment variable first
	const char* quadratePath = getenv("QUADRATE_PATH");
	if (quadratePath) {
		return std::string(quadratePath);
	}

	// Check if XDG_DATA_HOME is set
	const char* xdgDataHome = getenv("XDG_DATA_HOME");
	if (xdgDataHome) {
		return std::string(xdgDataHome) + "/quadrate/modules";
	}

	// Default to ~/quadrate/modules
	return getHomeDir() + "/quadrate/modules";
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

// Parse module name from quadrate.toml
// Returns empty string if file doesn't exist or name not found
std::string parseModuleName(const std::string& manifestPath) {
	std::ifstream file(manifestPath);
	if (!file.is_open()) {
		return "";
	}

	std::string line;
	bool inPackageSection = false;

	while (std::getline(file, line)) {
		// Trim whitespace
		line.erase(0, line.find_first_not_of(" \t\r\n"));
		line.erase(line.find_last_not_of(" \t\r\n") + 1);

		// Check for [module] section (also accept [package] for backwards compatibility)
		if (line == "[module]" || line == "[package]") {
			inPackageSection = true;
			continue;
		}

		// Check for other sections
		if (!line.empty() && line[0] == '[') {
			inPackageSection = false;
			continue;
		}

		// Skip comments and empty lines
		if (line.empty() || line[0] == '#') {
			continue;
		}

		// Look for name = "value" in module section
		if (inPackageSection) {
			size_t eqPos = line.find('=');
			if (eqPos != std::string::npos) {
				std::string key = line.substr(0, eqPos);
				std::string value = line.substr(eqPos + 1);

				// Trim key and value
				key.erase(0, key.find_first_not_of(" \t"));
				key.erase(key.find_last_not_of(" \t") + 1);
				value.erase(0, value.find_first_not_of(" \t"));
				value.erase(value.find_last_not_of(" \t") + 1);

				if (key == "name") {
					// Remove quotes from value
					if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"') {
						return value.substr(1, value.size() - 2);
					}
					return value;
				}
			}
		}
	}

	return "";
}

// Get the installed directory name for a module@ref
std::string getInstalledDirName(const std::string& moduleName, const std::string& ref) {
	return moduleName + "@" + ref;
}

// Clone a Git repository to the modules directory
// Returns the actual module name (from manifest if present, otherwise from git ref)
// Returns empty string on failure
std::string gitClone(const GitRef& gitRef) {
	std::string modulesDir = getModulesDir();

	// Create modules directory if it doesn't exist
	fs::create_directories(modulesDir);

	std::string targetDir = modulesDir + "/" + getInstalledDirName(gitRef.moduleName, gitRef.ref);

	// Check if already exists
	if (fs::exists(targetDir)) {
		std::cout << COLOR_YELLOW << "Module already exists: " << COLOR_RESET << targetDir << "\n";
		std::cout << COLOR_CYAN << "Use 'quadpm update' to update it" << COLOR_RESET << "\n";
		return gitRef.moduleName;
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
		return "";
	}

	// Check for quadrate.toml and use module name if specified
	std::string manifestPath = targetDir + "/quadrate.toml";
	std::string manifestModuleName = parseModuleName(manifestPath);
	std::string finalDir = targetDir;
	std::string actualModuleName = gitRef.moduleName;

	if (!manifestModuleName.empty() && manifestModuleName != gitRef.moduleName) {
		// Module specifies a different name, rename the directory
		finalDir = modulesDir + "/" + getInstalledDirName(manifestModuleName, gitRef.ref);
		actualModuleName = manifestModuleName;

		// Check if target with new name already exists
		if (fs::exists(finalDir)) {
			std::cerr << COLOR_RED << "Error: Module '" << manifestModuleName << "' already exists at: " << COLOR_RESET
					  << finalDir << "\n";
			fs::remove_all(targetDir);
			return "";
		}

		fs::rename(targetDir, finalDir);
		std::cout << COLOR_GREEN << "  ✓ Installed as '" << manifestModuleName << "' to " << COLOR_RESET << finalDir
				  << "\n";
	} else {
		std::cout << COLOR_GREEN << "  ✓ Installed to " << COLOR_RESET << finalDir << "\n";
	}

	// Show what module file was found
	std::string moduleFile = finalDir + "/module.qd";
	if (fs::exists(moduleFile)) {
		std::cout << COLOR_GREEN << "  ✓ Found module.qd" << COLOR_RESET << "\n";
	} else {
		std::cout << COLOR_YELLOW << "  ⚠ Warning: module.qd not found at root" << COLOR_RESET << "\n";
		std::cout << "    Module may need to be structured with module.qd at root\n";
	}

	// Check for C source files and compile if found
	std::string srcDir = finalDir + "/src";
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
			std::string libDir = finalDir + "/lib";
			fs::create_directories(libDir);

			// Library names - always use the repository name for library files
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

				std::string compileCmd =
						compiler + " -c -fPIC -O2 -Wall " + includeFlags + " " + cFile + " -o " + objFile + " 2>&1";

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

	return actualModuleName;
}

// Compile C sources in a module directory
// Returns true on success, false on failure
bool compileCsources(const std::string& moduleDir, const std::string& moduleName) {
	std::string srcDir = moduleDir + "/src";
	if (!fs::exists(srcDir) || !fs::is_directory(srcDir)) {
		return true; // No src dir is not an error
	}

	// Collect all .c files
	std::vector<std::string> cFiles;
	for (const auto& entry : fs::directory_iterator(srcDir)) {
		if (entry.is_regular_file() && entry.path().extension() == ".c") {
			cFiles.push_back(entry.path().string());
		}
	}

	if (cFiles.empty()) {
		return true; // No C files is not an error
	}

	std::cout << "  → Compiling C sources...\n";

	// Create lib directory
	std::string libDir = moduleDir + "/lib";
	fs::create_directories(libDir);

	// Library names
	std::string libName = "lib" + moduleName;
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
		std::string includeFlags = "-I/usr/include";
		if (fs::exists("dist/include/qdrt")) {
			includeFlags += " -Idist/include";
		}

		std::string compileCmd =
				compiler + " -c -fPIC -O2 -Wall " + includeFlags + " " + cFile + " -o " + objFile + " 2>&1";

		int compileResult = execCommandLive(compileCmd);
		if (compileResult != 0) {
			std::cerr << COLOR_RED << "  ✗ Failed to compile " << COLOR_RESET << cFile << "\n";
			compileFailed = true;
			break;
		}
	}

	if (compileFailed || objFiles.empty()) {
		return false;
	}

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

	return true;
}

// Print version information
void printVersion() {
	std::cout << "quadpm 0.1.0\n";
}

// Print usage information
void printUsage() {
	std::cout << "quadpm - Quadrate module manager\n\n";
	std::cout << "Manages 3rd party modules from Git repositories.\n\n";
	std::cout << "Usage: quadpm [options] <command> [arguments]\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n\n";
	std::cout << "Commands:\n";
	std::cout << "  get <url>[@ref]  Fetch and install a module from Git\n";
	std::cout << "  update [name]    Update installed module(s) (git pull)\n";
	std::cout << "  list             List installed modules\n\n";
	std::cout << "Examples:\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib@1.2.0\n";
	std::cout << "  quadpm get https://github.com/user/http@main\n";
	std::cout << "  quadpm list\n\n";
	std::cout << "Environment:\n";
	std::cout << "  QUADRATE_PATH      Module installation directory\n";
	std::cout << "  XDG_DATA_HOME      If set, uses $XDG_DATA_HOME/quadrate/modules\n";
	std::cout << "  Default: ~/quadrate/modules\n";
}

// List installed modules
void listModules() {
	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		std::cout << "No modules installed yet.\n";
		std::cout << "Modules will be installed to: " << modulesDir << "\n";
		return;
	}

	std::cout << COLOR_BOLD << "Installed modules:" << COLOR_RESET << "\n";
	std::cout << "Location: " << modulesDir << "\n\n";

	bool found = false;
	for (const auto& entry : fs::directory_iterator(modulesDir)) {
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
		std::cout << "No modules installed.\n";
	}
}

// Update a single module by running git pull
bool updateModule(const std::string& moduleDir) {
	std::string name = fs::path(moduleDir).filename().string();

	// Parse module@version format for display
	std::string displayName = name;
	std::string moduleName = name;
	size_t atPos = name.find('@');
	if (atPos != std::string::npos) {
		moduleName = name.substr(0, atPos);
		displayName = moduleName + " @ " + name.substr(atPos + 1);
	}

	std::cout << COLOR_CYAN << "Updating " << COLOR_BOLD << displayName << COLOR_RESET << "...\n";

	// Run git pull in the module directory
	std::string pullCmd = "cd " + moduleDir + " && git pull 2>&1";
	int result = execCommandLive(pullCmd);

	if (result != 0) {
		std::cerr << COLOR_RED << "  ✗ Failed to update " << displayName << COLOR_RESET << "\n";
		return false;
	}

	std::cout << COLOR_GREEN << "  ✓ Updated " << displayName << COLOR_RESET << "\n";

	// Rebuild C sources if present
	compileCsources(moduleDir, moduleName);

	return true;
}

// Update installed modules
int updateModules(const std::string& targetModuleName) {
	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		std::cerr << COLOR_RED << "Error: No modules installed" << COLOR_RESET << "\n";
		return 1;
	}

	bool found = false;
	int failures = 0;

	for (const auto& entry : fs::directory_iterator(modulesDir)) {
		if (!entry.is_directory()) {
			continue;
		}

		std::string name = entry.path().filename().string();

		// If a specific module name was given, only update that one
		if (!targetModuleName.empty()) {
			// Match either full name (module@version) or just module name
			size_t atPos = name.find('@');
			std::string moduleName = (atPos != std::string::npos) ? name.substr(0, atPos) : name;

			if (name != targetModuleName && moduleName != targetModuleName) {
				continue;
			}
		}

		// Check if it's a git repository
		std::string gitDir = entry.path().string() + "/.git";
		if (!fs::exists(gitDir)) {
			std::cout << COLOR_YELLOW << "Skipping " << name << " (not a git repository)" << COLOR_RESET << "\n";
			continue;
		}

		found = true;
		if (!updateModule(entry.path().string())) {
			failures++;
		}
	}

	if (!found) {
		if (targetModuleName.empty()) {
			std::cerr << COLOR_RED << "Error: No modules found to update" << COLOR_RESET << "\n";
		} else {
			std::cerr << COLOR_RED << "Error: Module '" << targetModuleName << "' not found" << COLOR_RESET << "\n";
		}
		return 1;
	}

	return failures > 0 ? 1 : 0;
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

		std::string installedName = gitClone(gitRef);
		if (installedName.empty()) {
			return 1;
		}

		std::cout << "\n"
				  << COLOR_GREEN << "Success!" << COLOR_RESET
				  << " You can now use this module in your Quadrate code:\n";
		std::cout << "  " << COLOR_CYAN << "use " << installedName << COLOR_RESET << "\n";

		return 0;
	}

	if (command == "list" || command == "ls") {
		listModules();
		return 0;
	}

	if (command == "update") {
		std::string targetModuleName = (argc >= 3) ? argv[2] : "";
		return updateModules(targetModuleName);
	}

	std::cerr << COLOR_RED << "Error: Unknown command '" << command << "'" << COLOR_RESET << "\n";
	printUsage();
	return 1;
}

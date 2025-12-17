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
#include <jansson.h>
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

// Parse module name from qd.json
// Returns empty string if file doesn't exist or name not found
// Native build configuration from qd.json
struct NativeConfig {
	std::vector<std::string> link; // Libraries to link with (-l flags)
};

// Dependency from [dependencies] section
struct Dependency {
	std::string name;       // Module name (key in TOML)
	std::string url;        // Git URL or local path
	std::string sha256;     // Optional integrity hash
	bool isPath;            // true if local path, false if git URL
};

// Forward declaration
bool compileCsources(const std::string& moduleDir, const std::string& moduleName, const NativeConfig& nativeConfig);

// Parse native section from qd.json
NativeConfig parseNativeConfig(const std::string& manifestPath) {
	NativeConfig config;

	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return config;
	}

	json_t* native = json_object_get(root, "native");
	if (native && json_is_object(native)) {
		json_t* link = json_object_get(native, "link");
		if (link && json_is_array(link)) {
			size_t index;
			json_t* value;
			json_array_foreach(link, index, value) {
				if (json_is_string(value)) {
					config.link.push_back(json_string_value(value));
				}
			}
		}
	}

	json_decref(root);
	return config;
}

std::string parseModuleName(const std::string& manifestPath) {
	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return "";
	}

	std::string result;
	json_t* name = json_object_get(root, "name");
	if (name && json_is_string(name)) {
		result = json_string_value(name);
	}

	json_decref(root);
	return result;
}

// Parse dependencies from qd.json
// Supports:
//   "name": "https://git.sr.ht/~user/repo@ref"  (git URL)
//   "name": "../local/path"                      (local path)
//   "name": "*"                                  (any version, npm-style)
//   "name": { "url": "...", "integrity": "sha256-..." }  (expanded form)
std::vector<Dependency> parseDependencies(const std::string& manifestPath) {
	std::vector<Dependency> deps;

	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return deps;
	}

	json_t* dependencies = json_object_get(root, "dependencies");
	if (dependencies && json_is_object(dependencies)) {
		const char* key;
		json_t* value;
		json_object_foreach(dependencies, key, value) {
			Dependency dep;
			dep.name = key;

			if (json_is_string(value)) {
				// Simple form: "name": "url" or "name": "*"
				dep.url = json_string_value(value);
				// Check if it's a local path (starts with /, ./, ../, or ~/)
				dep.isPath = (dep.url.size() > 0 && (dep.url[0] == '/' || dep.url[0] == '.' ||
				              (dep.url.size() > 1 && dep.url[0] == '~' && dep.url[1] == '/')));
			} else if (json_is_object(value)) {
				// Expanded form: { "url": "...", "integrity": "sha256-..." }
				json_t* url = json_object_get(value, "url");
				if (url && json_is_string(url)) {
					dep.url = json_string_value(url);
					dep.isPath = (dep.url.size() > 0 && (dep.url[0] == '/' || dep.url[0] == '.' ||
					              (dep.url.size() > 1 && dep.url[0] == '~' && dep.url[1] == '/')));
				}
				json_t* integrity = json_object_get(value, "integrity");
				if (integrity && json_is_string(integrity)) {
					std::string integrityStr = json_string_value(integrity);
					// Strip "sha256-" prefix if present (npm-style)
					if (integrityStr.size() > 7 && integrityStr.substr(0, 7) == "sha256-") {
						dep.sha256 = integrityStr.substr(7);
					} else {
						dep.sha256 = integrityStr;
					}
				}
			}

			if (!dep.url.empty()) {
				deps.push_back(dep);
			}
		}
	}

	json_decref(root);
	return deps;
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

	// Check for qd.json and use module name if specified
	std::string manifestPath = targetDir + "/qd.json";
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
		std::cout << COLOR_GREEN << "  ✓ Found src/ directory" << COLOR_RESET << "\n";
		// Parse native config for link libraries
		std::string finalManifestPath = finalDir + "/qd.json";
		NativeConfig nativeConfig = parseNativeConfig(finalManifestPath);
		compileCsources(finalDir, actualModuleName, nativeConfig);
	}

	return actualModuleName;
}

// Compile C sources in a module directory
// Returns true on success, false on failure
bool compileCsources(const std::string& moduleDir, const std::string& moduleName, const NativeConfig& nativeConfig) {
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

	// Build link flags from native config
	std::string linkFlags;
	for (const auto& lib : nativeConfig.link) {
		linkFlags += "-l" + lib + " ";
	}

	std::string linkSharedCmd = compiler + " -shared " + objList + linkFlags + "-o " + sharedLib + " 2>&1";
	int linkResult = execCommandLive(linkSharedCmd);

	if (linkResult == 0) {
		std::cout << COLOR_GREEN << "  ✓ Built " << COLOR_RESET << libName << ".so\n";
	} else {
		std::cerr << COLOR_YELLOW << "  ⚠ Failed to build shared library" << COLOR_RESET << "\n";
	}

	// Create static library (note: static libs don't link with other libs directly)
	std::string arCmd = "ar rcs " + staticLib + " " + objList + "2>&1";
	int arResult = execCommandLive(arCmd);

	if (arResult == 0) {
		std::cout << COLOR_GREEN << "  ✓ Built " << COLOR_RESET << libName << "_static.a\n";
		// If there are link dependencies, write them to a .deps file for the compiler to use
		if (!nativeConfig.link.empty()) {
			std::string depsFile = libDir + "/" + libName + "_static.deps";
			std::ofstream deps(depsFile);
			if (deps.is_open()) {
				for (const auto& lib : nativeConfig.link) {
					deps << "-l" << lib << "\n";
				}
				deps.close();
				std::cout << COLOR_GREEN << "  ✓ Wrote " << COLOR_RESET << libName << "_static.deps\n";
			}
		}
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
	std::cout << "  install          Install dependencies from qd.json\n";
	std::cout << "  get <url>[@ref]  Fetch and install a module from Git\n";
	std::cout << "  update [name]    Update installed module(s) (git pull)\n";
	std::cout << "  list             List installed modules\n";
	std::cout << "  build            Build C sources in current module directory\n\n";
	std::cout << "Examples:\n";
	std::cout << "  quadpm install\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib@1.2.0\n";
	std::cout << "  quadpm get https://github.com/user/http@main\n";
	std::cout << "  quadpm list\n\n";
	std::cout << "qd.json format (npm-compatible):\n";
	std::cout << "  {\n";
	std::cout << "    \"name\": \"mymodule\",\n";
	std::cout << "    \"dependencies\": {\n";
	std::cout << "      \"glut\": \"https://git.sr.ht/~user/qd-glut@v1.0.0\",\n";
	std::cout << "      \"mylib\": \"../local/path\",\n";
	std::cout << "      \"crypto\": {\n";
	std::cout << "        \"url\": \"https://git.sr.ht/~user/qd-crypto@v2.0.0\",\n";
	std::cout << "        \"integrity\": \"sha256-abc123...\"\n";
	std::cout << "      }\n";
	std::cout << "    }\n";
	std::cout << "  }\n\n";
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
	std::string manifestPath = moduleDir + "/qd.json";
	NativeConfig nativeConfig = parseNativeConfig(manifestPath);
	compileCsources(moduleDir, moduleName, nativeConfig);

	return true;
}

// Build a module in the current directory (for local development)
int buildModule() {
	std::string cwd = fs::current_path().string();

	// Check for qd.json
	std::string manifestPath = cwd + "/qd.json";
	if (!fs::exists(manifestPath)) {
		std::cerr << COLOR_RED << "Error: No qd.json found in current directory" << COLOR_RESET << "\n";
		std::cerr << "Run this command from a module directory containing qd.json\n";
		return 1;
	}

	// Parse module name from manifest
	std::string moduleName = parseModuleName(manifestPath);
	if (moduleName.empty()) {
		std::cerr << COLOR_RED << "Error: Could not parse module name from qd.json" << COLOR_RESET << "\n";
		return 1;
	}

	std::cout << COLOR_CYAN << "Building module " << COLOR_BOLD << moduleName << COLOR_RESET << "...\n";

	// Parse native config for link libraries
	NativeConfig nativeConfig = parseNativeConfig(manifestPath);
	if (!nativeConfig.link.empty()) {
		std::cout << "  → Link libraries: ";
		for (size_t i = 0; i < nativeConfig.link.size(); i++) {
			if (i > 0) {
				std::cout << ", ";
			}
			std::cout << nativeConfig.link[i];
		}
		std::cout << "\n";
	}

	// Check for src/ directory
	std::string srcDir = cwd + "/src";
	if (!fs::exists(srcDir) || !fs::is_directory(srcDir)) {
		std::cout << COLOR_YELLOW << "No src/ directory found - nothing to build" << COLOR_RESET << "\n";
		return 0;
	}

	// Compile C sources
	if (!compileCsources(cwd, moduleName, nativeConfig)) {
		std::cerr << COLOR_RED << "Build failed" << COLOR_RESET << "\n";
		return 1;
	}

	std::cout << COLOR_GREEN << "Build complete!" << COLOR_RESET << "\n";
	return 0;
}

// Compute SHA256 hash of a file or directory
std::string computeSha256(const std::string& path) {
	std::string cmd = "sha256sum " + path + " 2>/dev/null | cut -d' ' -f1";
	try {
		std::string hash = execCommand(cmd);
		// Trim whitespace
		hash.erase(hash.find_last_not_of(" \t\r\n") + 1);
		return hash;
	} catch (...) {
		return "";
	}
}

// Install dependencies from qd.json
int installDependencies() {
	std::string cwd = fs::current_path().string();

	// Check for qd.json
	std::string manifestPath = cwd + "/qd.json";
	if (!fs::exists(manifestPath)) {
		std::cerr << COLOR_RED << "Error: No qd.json found in current directory" << COLOR_RESET << "\n";
		return 1;
	}

	// Parse dependencies
	std::vector<Dependency> deps = parseDependencies(manifestPath);
	if (deps.empty()) {
		std::cout << "No dependencies found in qd.json\n";
		return 0;
	}

	std::cout << COLOR_CYAN << "Installing " << deps.size() << " dependenc"
	          << (deps.size() == 1 ? "y" : "ies") << "..." << COLOR_RESET << "\n\n";

	int failures = 0;
	std::string modulesDir = getModulesDir();

	for (const auto& dep : deps) {
		std::cout << COLOR_BOLD << dep.name << COLOR_RESET << ": ";

		if (dep.isPath) {
			// Local path dependency
			std::string resolvedPath = dep.url;

			// Expand ~ to home directory
			if (resolvedPath.size() > 0 && resolvedPath[0] == '~') {
				resolvedPath = getHomeDir() + resolvedPath.substr(1);
			}

			// Make relative paths absolute
			if (resolvedPath.size() > 0 && resolvedPath[0] != '/') {
				resolvedPath = cwd + "/" + resolvedPath;
			}

			// Normalize the path
			resolvedPath = fs::weakly_canonical(resolvedPath).string();

			if (!fs::exists(resolvedPath)) {
				std::cout << COLOR_RED << "✗ Path not found: " << resolvedPath << COLOR_RESET << "\n";
				failures++;
				continue;
			}

			// Verify it has a module.qd
			if (!fs::exists(resolvedPath + "/module.qd")) {
				std::cout << COLOR_RED << "✗ Not a module (no module.qd): " << resolvedPath << COLOR_RESET << "\n";
				failures++;
				continue;
			}

			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << resolvedPath << " (local)\n";
		} else {
			// Git URL dependency
			GitRef gitRef = parseGitUrl(dep.url);

			// Check if already installed
			std::string installedDir = modulesDir + "/" + getInstalledDirName(gitRef.moduleName, gitRef.ref);
			if (fs::exists(installedDir)) {
				std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "already installed\n";

				// Verify sha256 if specified
				if (!dep.sha256.empty()) {
					// For git repos, we check the commit hash
					std::string gitDir = installedDir + "/.git";
					if (fs::exists(gitDir)) {
						std::string cmd = "git -C " + installedDir + " rev-parse HEAD 2>/dev/null";
						try {
							std::string commitHash = execCommand(cmd);
							commitHash.erase(commitHash.find_last_not_of(" \t\r\n") + 1);
							if (commitHash != dep.sha256) {
								std::cout << "  " << COLOR_YELLOW << "⚠ SHA256 mismatch!" << COLOR_RESET << "\n";
								std::cout << "    Expected: " << dep.sha256 << "\n";
								std::cout << "    Got:      " << commitHash << "\n";
							}
						} catch (...) {
							// Ignore errors
						}
					}
				}
				continue;
			}

			// Clone the repository
			std::string installedName = gitClone(gitRef);
			if (installedName.empty()) {
				std::cout << COLOR_RED << "✗ Failed to clone" << COLOR_RESET << "\n";
				failures++;
				continue;
			}

			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "installed\n";

			// Verify sha256 if specified
			if (!dep.sha256.empty()) {
				std::string newInstalledDir = modulesDir + "/" + getInstalledDirName(installedName, gitRef.ref);
				std::string gitDir = newInstalledDir + "/.git";
				if (fs::exists(gitDir)) {
					std::string cmd = "git -C " + newInstalledDir + " rev-parse HEAD 2>/dev/null";
					try {
						std::string commitHash = execCommand(cmd);
						commitHash.erase(commitHash.find_last_not_of(" \t\r\n") + 1);
						if (commitHash != dep.sha256) {
							std::cout << "  " << COLOR_RED << "✗ SHA256 mismatch!" << COLOR_RESET << "\n";
							std::cout << "    Expected: " << dep.sha256 << "\n";
							std::cout << "    Got:      " << commitHash << "\n";
							failures++;
						}
					} catch (...) {
						// Ignore errors
					}
				}
			}
		}
	}

	std::cout << "\n";
	if (failures > 0) {
		std::cout << COLOR_RED << failures << " dependenc" << (failures == 1 ? "y" : "ies")
		          << " failed" << COLOR_RESET << "\n";
		return 1;
	}

	std::cout << COLOR_GREEN << "All dependencies installed!" << COLOR_RESET << "\n";
	return 0;
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

	if (command == "build") {
		return buildModule();
	}

	if (command == "install" || command == "i") {
		return installDependencies();
	}

	std::cerr << COLOR_RED << "Error: Unknown command '" << command << "'" << COLOR_RESET << "\n";
	printUsage();
	return 1;
}

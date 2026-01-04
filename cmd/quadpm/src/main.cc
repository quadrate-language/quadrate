// quadpm - Quadrate Module Manager
// Manages 3rd party Git-based modules

#include "git_ref.h"
#include "semver.h"
#include "version.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jansson.h>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
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

// Execute a command safely using fork/exec (no shell interpretation)
// Returns exit status, captures stdout in 'output' if provided
int execCommandSafe(const std::vector<std::string>& args, std::string* output = nullptr, bool showOutput = false) {
	if (args.empty()) {
		return -1;
	}

	// Create pipe for capturing output
	int pipefd[2] = {-1, -1};
	if (output || showOutput) {
		if (pipe(pipefd) == -1) {
			return -1;
		}
	}

	pid_t pid = fork();
	if (pid == -1) {
		if (pipefd[0] != -1) {
			close(pipefd[0]);
			close(pipefd[1]);
		}
		return -1;
	}

	if (pid == 0) {
		// Child process
		if (pipefd[1] != -1) {
			close(pipefd[0]);
			dup2(pipefd[1], STDOUT_FILENO);
			dup2(pipefd[1], STDERR_FILENO);
			close(pipefd[1]);
		}

		// Convert args to char* array
		std::vector<char*> argv;
		for (const auto& arg : args) {
			argv.push_back(const_cast<char*>(arg.c_str()));
		}
		argv.push_back(nullptr);

		execvp(argv[0], argv.data());
		_exit(127); // exec failed
	}

	// Parent process
	if (pipefd[1] != -1) {
		close(pipefd[1]);
	}

	// Read output
	if (pipefd[0] != -1) {
		std::array<char, 256> buffer;
		ssize_t n;
		while ((n = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
			if (output) {
				output->append(buffer.data(), static_cast<size_t>(n));
			}
			if (showOutput) {
				std::cout.write(buffer.data(), n);
			}
		}
		close(pipefd[0]);
	}

	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

// Execute command and capture output (throws on failure)
std::string execCommand(const std::vector<std::string>& args) {
	std::string output;
	int status = execCommandSafe(args, &output, false);
	if (status != 0) {
		throw std::runtime_error("Command failed with status: " + std::to_string(status));
	}
	return output;
}

// Execute command showing output in real-time
int execCommandLive(const std::vector<std::string>& args) {
	return execCommandSafe(args, nullptr, true);
}

// Check if a command exists in PATH
bool commandExists(const std::string& cmd) {
	std::string output;
	int status = execCommandSafe({"which", cmd}, &output, false);
	return status == 0;
}

// Parse module name from qd.json
// Returns empty string if file doesn't exist or name not found
// Native build configuration from qd.json
struct NativeConfig {
	std::vector<std::string> link; // Libraries to link with (-l flags)
};

// Dependency from [dependencies] section
struct Dependency {
	std::string name;	 // Module name (key in TOML)
	std::string url;	 // Git URL or local path
	std::string version; // Version range (^1.0.0, ~2.0.0, >=1.0.0, etc.) or branch/tag
	std::string sha256;	 // Optional integrity hash
	bool isPath;		 // true if local path, false if git URL
	bool isSemVer;		 // true if version is a semver range
};

// Locked dependency - resolved and pinned version
struct LockedDependency {
	std::string name;		  // Module name
	std::string url;		  // Original Git URL
	std::string ref;		  // Git ref (branch/tag)
	std::string resolvedRef;  // Resolved commit hash
	std::string integrity;	  // SHA256 of commit
	bool isPath;			  // true if local path
	std::string resolvedPath; // Absolute path for local deps
};

// Forward declarations
bool compileCsources(const std::string& moduleDir, const std::string& moduleName, const NativeConfig& nativeConfig);
bool isSemVerRange(const std::string& version);
std::vector<Dependency> parseDependencies(const std::string& manifestPath);

// Get the path where a module would be installed
std::string getModuleInstallPath(const std::string& moduleName, const std::string& ref) {
	return getModulesDir() + "/" + moduleName + "@" + ref;
}

// Get the path to a local module's directory
std::string resolveLocalPath(const std::string& path, const std::string& basePath) {
	std::string resolvedPath = path;

	// Expand ~ to home directory
	if (resolvedPath.size() > 0 && resolvedPath[0] == '~') {
		resolvedPath = getHomeDir() + resolvedPath.substr(1);
	}

	// Make relative paths absolute
	if (resolvedPath.size() > 0 && resolvedPath[0] != '/') {
		resolvedPath = basePath + "/" + resolvedPath;
	}

	// Normalize the path
	return fs::weakly_canonical(resolvedPath).string();
}

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

// Parse namespace from qd.json
// Returns the namespace (for use in code), defaulting to module name if not specified
std::string parseNamespace(const std::string& manifestPath) {
	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return "";
	}

	std::string result;
	json_t* ns = json_object_get(root, "namespace");
	if (ns && json_is_string(ns)) {
		result = json_string_value(ns);
	}

	json_decref(root);
	return result;
}

// Parse dependencies from qd.json
// Supports:
//   "name": "https://git.sr.ht/~user/repo@ref"  (git URL with branch/tag)
//   "name": "https://git.sr.ht/~user/repo"      (git URL, default branch)
//   "name": "../local/path"                      (local path)
//   "name": "^1.0.0"                             (semver range, uses default registry)
//   "name": { "url": "...", "version": "^1.0.0" }  (expanded form with semver)
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
			dep.isPath = false;
			dep.isSemVer = false;

			if (json_is_string(value)) {
				std::string strValue = json_string_value(value);

				// Check if it's a local path (starts with /, ./, ../, or ~/)
				if (strValue.size() > 0 && (strValue[0] == '/' || strValue[0] == '.' ||
												   (strValue.size() > 1 && strValue[0] == '~' && strValue[1] == '/'))) {
					dep.url = strValue;
					dep.isPath = true;
				}
				// Check if it looks like a URL
				else if (strValue.find("://") != std::string::npos || strValue.substr(0, 4) == "git@") {
					// It's a URL - extract version if present
					GitRef gitRef = parseGitUrl(strValue);
					dep.url = gitRef.url;
					dep.version = gitRef.ref;
					// Check if the ref looks like semver
					dep.isSemVer = isSemVerRange(dep.version);
				}
				// Otherwise treat as version constraint (semver range)
				else {
					dep.version = strValue;
					dep.isSemVer = isSemVerRange(strValue);
					// URL will need to be looked up - leave empty for now
				}
			} else if (json_is_object(value)) {
				// Expanded form: { "url": "...", "version": "...", "integrity": "sha256-..." }
				json_t* url = json_object_get(value, "url");
				if (url && json_is_string(url)) {
					std::string urlStr = json_string_value(url);
					// Check if URL has inline version (@tag)
					GitRef gitRef = parseGitUrl(urlStr);
					dep.url = gitRef.url;
					if (gitRef.ref != "main") {
						dep.version = gitRef.ref;
					}
					dep.isPath = (dep.url.size() > 0 &&
								  (dep.url[0] == '/' || dep.url[0] == '.' ||
										  (dep.url.size() > 1 && dep.url[0] == '~' && dep.url[1] == '/')));
				}

				// Version field overrides inline @version
				json_t* version = json_object_get(value, "version");
				if (version && json_is_string(version)) {
					dep.version = json_string_value(version);
				}

				dep.isSemVer = isSemVerRange(dep.version);

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

			if (!dep.url.empty() || !dep.version.empty()) {
				deps.push_back(dep);
			}
		}
	}

	json_decref(root);
	return deps;
}

// Get the installed directory name for a module@ref
// For Go-style paths, this returns host/user/repo@ref format
std::string getInstalledDirName(const std::string& hostPath, const std::string& ref) {
	// Extract the repo name (last component)
	size_t lastSlash = hostPath.find_last_of('/');
	if (lastSlash != std::string::npos) {
		// Return host/user/repo@ref format
		std::string prefix = hostPath.substr(0, lastSlash + 1);
		std::string repoName = hostPath.substr(lastSlash + 1);
		return prefix + repoName + "@" + ref;
	}
	return hostPath + "@" + ref;
}

// Get the namespaces directory path
std::string getNamespacesDir() {
	return getModulesDir() + "/_namespaces";
}

// Find the library directory for a dependency by name
// Searches: 1) qd_modules/_namespaces/<name>, 2) sibling directories ../<name>
// currentModuleDir is used to find sibling directories
// Returns empty string if not found
std::string findDependencyLibDir(const std::string& depName, const std::string& currentModuleDir = "") {
	// First check qd_modules/_namespaces
	std::string namespacesDir = getNamespacesDir();
	std::string symlinkPath = namespacesDir + "/" + depName;

	std::string moduleDir;

	if (fs::exists(symlinkPath)) {
		// Resolve symlink to get actual module directory
		try {
			if (fs::is_symlink(symlinkPath)) {
				fs::path resolved = fs::read_symlink(symlinkPath);
				if (resolved.is_relative()) {
					moduleDir = fs::canonical(namespacesDir / resolved).string();
				} else {
					moduleDir = resolved.string();
				}
			} else {
				moduleDir = symlinkPath;
			}
		} catch (const std::exception&) {
			// Fall through to sibling check
		}
	}

	// If not found in namespaces, check sibling directories
	if (moduleDir.empty() && !currentModuleDir.empty()) {
		fs::path parentDir = fs::path(currentModuleDir).parent_path();
		std::string siblingPath = (parentDir / depName).string();
		if (fs::exists(siblingPath) && fs::is_directory(siblingPath)) {
			moduleDir = siblingPath;
		}
	}

	if (moduleDir.empty()) {
		return "";
	}

	std::string libDir = moduleDir + "/lib";
	if (fs::exists(libDir) && fs::is_directory(libDir)) {
		return libDir;
	}
	return "";
}

// Collect transitive native dependencies from a module's qd.json
// moduleDir is the directory containing qd.json (used for sibling lookup)
void collectTransitiveDeps(const std::string& manifestPath, const std::string& moduleDir,
		std::vector<std::string>& staticLibs, std::vector<std::string>& linkFlags, std::set<std::string>& visited) {
	std::vector<Dependency> deps = parseDependencies(manifestPath);

	for (const auto& dep : deps) {
		if (visited.count(dep.name)) {
			continue; // Avoid cycles
		}
		visited.insert(dep.name);

		std::string libDir = findDependencyLibDir(dep.name, moduleDir);
		if (libDir.empty()) {
			continue; // Dependency not installed or has no lib
		}

		// Find the static library
		std::string staticLib;
		std::string depsFile;
		for (const auto& entry : fs::directory_iterator(libDir)) {
			std::string filename = entry.path().filename().string();
			if (filename.find("_static.a") != std::string::npos) {
				staticLib = entry.path().string();
			} else if (filename.find("_static.deps") != std::string::npos) {
				depsFile = entry.path().string();
			}
		}

		// Add static library path
		if (!staticLib.empty()) {
			staticLibs.push_back(staticLib);
		}

		// Read and add deps file contents
		if (!depsFile.empty()) {
			std::ifstream ifs(depsFile);
			std::string line;
			while (std::getline(ifs, line)) {
				if (!line.empty() && line[0] == '-') {
					// It's a flag like -lssl
					linkFlags.push_back(line);
				} else if (!line.empty()) {
					// It's a library path - check if absolute or needs resolution
					staticLibs.push_back(line);
				}
			}
		}

		// Recursively process this dependency's dependencies
		// Find the actual module dir from libDir (go up one level)
		fs::path depModuleDir = fs::path(libDir).parent_path();
		std::string depManifestPath = (depModuleDir / "qd.json").string();
		if (fs::exists(depManifestPath)) {
			collectTransitiveDeps(depManifestPath, depModuleDir.string(), staticLibs, linkFlags, visited);
		}
	}
}

// Create or update a namespace symlink
// Returns true on success, false on conflict (different package already owns namespace)
bool createNamespaceSymlink(const std::string& namespaceName, const std::string& targetPath) {
	std::string namespacesDir = getNamespacesDir();
	fs::create_directories(namespacesDir);

	std::string symlinkPath = namespacesDir + "/" + namespaceName;

	// Check if symlink already exists
	if (fs::exists(symlinkPath) || fs::is_symlink(symlinkPath)) {
		// Check if it points to the same target
		try {
			std::string existingTarget = fs::read_symlink(symlinkPath).string();
			// Normalize for comparison (both should be relative to namespaces dir)
			if (existingTarget == targetPath || fs::weakly_canonical(namespacesDir + "/" + existingTarget) ==
														fs::weakly_canonical(namespacesDir + "/" + targetPath)) {
				return true; // Same target, no conflict
			}
			// Different target - this is a conflict
			return false;
		} catch (const std::exception&) {
			// If we can't read symlink, remove and recreate
			fs::remove(symlinkPath);
		}
	}

	// Create relative symlink
	// Target should be relative to _namespaces/ directory
	try {
		fs::create_symlink(targetPath, symlinkPath);
		return true;
	} catch (const std::exception& e) {
		std::cerr << COLOR_RED << "Error creating namespace symlink: " << COLOR_RESET << e.what() << "\n";
		return false;
	}
}

// Get all namespaces that point to a given hostPath
std::vector<std::string> getNamespacesForPackage(const std::string& hostPathWithRef) {
	std::vector<std::string> namespaces;
	std::string namespacesDir = getNamespacesDir();

	if (!fs::exists(namespacesDir)) {
		return namespaces;
	}

	for (const auto& entry : fs::directory_iterator(namespacesDir)) {
		if (fs::is_symlink(entry.path())) {
			try {
				std::string target = fs::read_symlink(entry.path()).string();
				// Check if target contains the hostPathWithRef
				if (target.find(hostPathWithRef) != std::string::npos) {
					namespaces.push_back(entry.path().filename().string());
				}
			} catch (const std::exception&) {
				// Ignore broken symlinks
			}
		}
	}

	return namespaces;
}

// Find which packages provide a given namespace
std::vector<std::string> findPackagesWithNamespace(const std::string& namespaceName) {
	std::vector<std::string> packages;
	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		return packages;
	}

	// Recursively search for qd.json files
	for (const auto& entry : fs::recursive_directory_iterator(modulesDir)) {
		if (entry.is_regular_file() && entry.path().filename() == "qd.json") {
			std::string manifestPath = entry.path().string();
			std::string ns = parseNamespace(manifestPath);
			std::string name = parseModuleName(manifestPath);

			// If namespace not specified, use module name
			if (ns.empty()) {
				ns = name;
			}

			if (ns == namespaceName) {
				// Get the package directory relative to modules dir
				std::string pkgDir = entry.path().parent_path().string();
				if (pkgDir.substr(0, modulesDir.size()) == modulesDir) {
					packages.push_back(pkgDir.substr(modulesDir.size() + 1)); // +1 for '/'
				}
			}
		}
	}

	return packages;
}

// Lockfile format version
const int LOCKFILE_VERSION = 1;

// Read lockfile (qd.lock)
std::vector<LockedDependency> readLockfile(const std::string& lockfilePath) {
	std::vector<LockedDependency> locked;

	json_error_t error;
	json_t* root = json_load_file(lockfilePath.c_str(), 0, &error);
	if (!root) {
		return locked;
	}

	// Check version
	json_t* version = json_object_get(root, "version");
	if (!version || !json_is_integer(version) || json_integer_value(version) != LOCKFILE_VERSION) {
		json_decref(root);
		return locked;
	}

	json_t* packages = json_object_get(root, "packages");
	if (!packages || !json_is_array(packages)) {
		json_decref(root);
		return locked;
	}

	size_t index;
	json_t* pkg;
	json_array_foreach(packages, index, pkg) {
		if (!json_is_object(pkg)) {
			continue;
		}

		LockedDependency dep;

		json_t* name = json_object_get(pkg, "name");
		if (name && json_is_string(name)) {
			dep.name = json_string_value(name);
		}

		json_t* url = json_object_get(pkg, "url");
		if (url && json_is_string(url)) {
			dep.url = json_string_value(url);
		}

		json_t* ref = json_object_get(pkg, "ref");
		if (ref && json_is_string(ref)) {
			dep.ref = json_string_value(ref);
		}

		json_t* resolvedRef = json_object_get(pkg, "resolved");
		if (resolvedRef && json_is_string(resolvedRef)) {
			dep.resolvedRef = json_string_value(resolvedRef);
		}

		json_t* integrity = json_object_get(pkg, "integrity");
		if (integrity && json_is_string(integrity)) {
			dep.integrity = json_string_value(integrity);
		}

		json_t* isPath = json_object_get(pkg, "isPath");
		dep.isPath = isPath && json_is_true(isPath);

		json_t* resolvedPath = json_object_get(pkg, "resolvedPath");
		if (resolvedPath && json_is_string(resolvedPath)) {
			dep.resolvedPath = json_string_value(resolvedPath);
		}

		if (!dep.name.empty()) {
			locked.push_back(dep);
		}
	}

	json_decref(root);
	return locked;
}

// Write lockfile (qd.lock)
bool writeLockfile(const std::string& lockfilePath, const std::vector<LockedDependency>& locked) {
	json_t* root = json_object();
	if (!root) {
		return false;
	}

	// Add version
	json_object_set_new(root, "version", json_integer(LOCKFILE_VERSION));

	// Add packages array
	json_t* packages = json_array();
	for (const auto& dep : locked) {
		json_t* pkg = json_object();

		json_object_set_new(pkg, "name", json_string(dep.name.c_str()));

		if (!dep.url.empty()) {
			json_object_set_new(pkg, "url", json_string(dep.url.c_str()));
		}

		if (!dep.ref.empty()) {
			json_object_set_new(pkg, "ref", json_string(dep.ref.c_str()));
		}

		if (!dep.resolvedRef.empty()) {
			json_object_set_new(pkg, "resolved", json_string(dep.resolvedRef.c_str()));
		}

		if (!dep.integrity.empty()) {
			json_object_set_new(pkg, "integrity", json_string(dep.integrity.c_str()));
		}

		json_object_set_new(pkg, "isPath", dep.isPath ? json_true() : json_false());

		if (!dep.resolvedPath.empty()) {
			json_object_set_new(pkg, "resolvedPath", json_string(dep.resolvedPath.c_str()));
		}

		json_array_append_new(packages, pkg);
	}

	json_object_set_new(root, "packages", packages);

	// Write to file with pretty formatting
	int result = json_dump_file(root, lockfilePath.c_str(), JSON_INDENT(2) | JSON_SORT_KEYS);
	json_decref(root);

	return result == 0;
}

// Get commit hash for an installed module
std::string getModuleCommitHash(const std::string& moduleDir) {
	std::string gitDir = moduleDir + "/.git";
	if (!fs::exists(gitDir)) {
		return "";
	}

	try {
		std::string hash = execCommand({"git", "-C", moduleDir, "rev-parse", "HEAD"});
		hash.erase(hash.find_last_not_of(" \t\r\n") + 1);
		return hash;
	} catch (...) {
		return "";
	}
}

// Clone a Git repository to the modules directory
// Uses Go-style paths: host/user/repo@version
// Returns the actual module name (from manifest if present, otherwise from git ref)
// Returns empty string on failure
std::string gitClone(const GitRef& gitRef) {
	std::string modulesDir = getModulesDir();

	// Create modules directory if it doesn't exist
	fs::create_directories(modulesDir);

	// Use hostPath for Go-style directory structure
	std::string installedDirName = getInstalledDirName(gitRef.hostPath, gitRef.ref);
	std::string targetDir = modulesDir + "/" + installedDirName;

	// Check if already exists
	if (fs::exists(targetDir)) {
		std::cout << COLOR_YELLOW << "Module already exists: " << COLOR_RESET << targetDir << "\n";
		std::cout << COLOR_CYAN << "Use 'quadpm update' to update it" << COLOR_RESET << "\n";
		return gitRef.moduleName;
	}

	std::cout << COLOR_CYAN << "Fetching " << COLOR_BOLD << gitRef.hostPath << COLOR_RESET << COLOR_CYAN << " @ "
			  << gitRef.ref << "..." << COLOR_RESET << "\n";
	std::cout << "  → Cloning " << gitRef.url << "\n";

	// Create parent directories for Go-style path
	fs::path targetPath(targetDir);
	fs::create_directories(targetPath.parent_path());

	// Clone with --depth 1 for faster download
	int result = execCommandLive({"git", "clone", "--depth", "1", "--branch", gitRef.ref, gitRef.url, targetDir});

	if (result != 0) {
		std::cerr << COLOR_RED << "Error: Failed to clone repository" << COLOR_RESET << "\n";
		// Try to clean up partial clone
		if (fs::exists(targetDir)) {
			fs::remove_all(targetDir);
		}
		return "";
	}

	std::string finalDir = targetDir;
	std::string actualModuleName = gitRef.moduleName;

	std::cout << COLOR_GREEN << "  ✓ Installed to " << COLOR_RESET << finalDir << "\n";

	// Show what module file was found
	std::string moduleFile = finalDir + "/module.qd";
	if (fs::exists(moduleFile)) {
		std::cout << COLOR_GREEN << "  ✓ Found module.qd" << COLOR_RESET << "\n";
	} else {
		std::cout << COLOR_YELLOW << "  ⚠ Warning: module.qd not found at root" << COLOR_RESET << "\n";
		std::cout << "    Module may need to be structured with module.qd at root\n";
	}

	// Parse namespace from qd.json and create symlink
	std::string manifestPath = finalDir + "/qd.json";
	std::string manifestModuleName = parseModuleName(manifestPath);
	std::string manifestNamespace = parseNamespace(manifestPath);

	// Determine the namespace: explicit namespace > module name > repo name
	std::string namespaceName;
	if (!manifestNamespace.empty()) {
		namespaceName = manifestNamespace;
	} else if (!manifestModuleName.empty()) {
		namespaceName = manifestModuleName;
	} else {
		namespaceName = gitRef.moduleName;
	}

	// Create namespace symlink
	// Relative path from _namespaces/ to the package directory
	std::string relativeTarget = "../" + installedDirName;

	// Check for namespace conflicts
	std::vector<std::string> existingPackages = findPackagesWithNamespace(namespaceName);
	bool hasConflict = false;
	for (const auto& pkg : existingPackages) {
		if (pkg != installedDirName) {
			hasConflict = true;
			std::cout << COLOR_YELLOW << "  ⚠ Warning: namespace '" << namespaceName
					  << "' also claimed by: " << COLOR_RESET << pkg << "\n";
		}
	}

	if (createNamespaceSymlink(namespaceName, relativeTarget)) {
		std::cout << COLOR_GREEN << "  ✓ Namespace '" << namespaceName << "' registered" << COLOR_RESET << "\n";
		if (hasConflict) {
			std::cout << COLOR_YELLOW << "    Note: Use full path in 'use' directive to disambiguate" << COLOR_RESET
					  << "\n";
			std::cout << COLOR_CYAN << "    use " << gitRef.hostPath << COLOR_RESET << "\n";
		}
	} else {
		std::cout << COLOR_YELLOW << "  ⚠ Namespace '" << namespaceName << "' already registered to different package"
				  << COLOR_RESET << "\n";
		std::cout << COLOR_CYAN << "    Use full path in 'use' directive: use " << gitRef.hostPath << COLOR_RESET
				  << "\n";
	}

	// Check for C source files and compile if found
	std::string srcDir = finalDir + "/src";
	if (fs::exists(srcDir) && fs::is_directory(srcDir)) {
		std::cout << COLOR_GREEN << "  ✓ Found src/ directory" << COLOR_RESET << "\n";
		// Parse native config for link libraries
		NativeConfig nativeConfig = parseNativeConfig(manifestPath);
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

	// Collect all .c files recursively (including platform subdirectories)
	std::vector<std::string> cFiles;
	for (const auto& entry : fs::recursive_directory_iterator(srcDir)) {
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

	// Library names - use libqd prefix to match standard library convention
	// This ensures the compiler uses usr_<namespace>_<func> name mangling
	std::string libName = "libqd" + moduleName;
	std::string sharedLib = libDir + "/" + libName + ".so";
	std::string staticLib = libDir + "/" + libName + "_static.a";

	// Prefer clang, fallback to gcc
	std::string compiler = "gcc";
	if (commandExists("clang")) {
		compiler = "clang";
	}

	// Build include paths
	std::vector<std::string> includePaths;
	includePaths.push_back("-I/usr/include");
	// Module's own include directory
	std::string moduleIncDir = moduleDir + "/include";
	if (fs::exists(moduleIncDir)) {
		includePaths.push_back("-I" + moduleIncDir);
	}
	// Module's src directory (for platform headers)
	includePaths.push_back("-I" + srcDir);
	if (fs::exists("dist/include/qdrt")) {
		includePaths.push_back("-Idist/include");
	}
	const char* libDir_env = std::getenv("QUADRATE_LIBDIR");
	if (libDir_env) {
		fs::path libPath(libDir_env);
		fs::path includePath = libPath.parent_path() / "include";
		if (fs::exists(includePath / "qdrt")) {
			includePaths.push_back("-I" + includePath.string());
		}
	}

	// Compile to object files
	std::vector<std::string> objFiles;
	bool compileFailed = false;

	for (const auto& cFile : cFiles) {
		std::string objFile = libDir + "/" + fs::path(cFile).stem().string() + ".o";
		objFiles.push_back(objFile);

		// Build compile command as vector
		std::vector<std::string> compileArgs = {compiler, "-c", "-fPIC", "-O2", "-Wall"};
		for (const auto& inc : includePaths) {
			compileArgs.push_back(inc);
		}
		compileArgs.push_back(cFile);
		compileArgs.push_back("-o");
		compileArgs.push_back(objFile);

		int compileResult = execCommandLive(compileArgs);
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
	std::vector<std::string> linkArgs = {compiler, "-shared"};
	for (const auto& obj : objFiles) {
		linkArgs.push_back(obj);
	}
	for (const auto& lib : nativeConfig.link) {
		linkArgs.push_back("-l" + lib);
	}
	linkArgs.push_back("-o");
	linkArgs.push_back(sharedLib);

	int linkResult = execCommandLive(linkArgs);

	if (linkResult == 0) {
		std::cout << COLOR_GREEN << "  ✓ Built " << COLOR_RESET << libName << ".so\n";
	} else {
		std::cerr << COLOR_YELLOW << "  ⚠ Failed to build shared library" << COLOR_RESET << "\n";
	}

	// Create static library (note: static libs don't link with other libs directly)
	std::vector<std::string> arArgs = {"ar", "rcs", staticLib};
	for (const auto& obj : objFiles) {
		arArgs.push_back(obj);
	}
	int arResult = execCommandLive(arArgs);

	if (arResult == 0) {
		std::cout << COLOR_GREEN << "  ✓ Built " << COLOR_RESET << libName << "_static.a\n";

		// Collect all dependencies for the .deps file:
		// 1. Direct link flags from native.link (e.g., -lssl, -lcrypto)
		// 2. Static libraries from qd.json dependencies
		// 3. Transitive deps from dependencies' deps files
		std::vector<std::string> staticLibs;
		std::vector<std::string> linkFlags;
		std::set<std::string> visited;

		// Add direct link flags
		for (const auto& lib : nativeConfig.link) {
			linkFlags.push_back("-l" + lib);
		}

		// Collect transitive dependencies from qd.json
		std::string manifestPath = moduleDir + "/qd.json";
		if (fs::exists(manifestPath)) {
			collectTransitiveDeps(manifestPath, moduleDir, staticLibs, linkFlags, visited);
		}

		// Write deps file if there are any dependencies
		if (!linkFlags.empty() || !staticLibs.empty()) {
			std::string depsFile = libDir + "/" + libName + "_static.deps";
			std::ofstream deps(depsFile);
			if (deps.is_open()) {
				// Write link flags first (deduped)
				std::set<std::string> seenFlags;
				for (const auto& flag : linkFlags) {
					if (seenFlags.insert(flag).second) {
						deps << flag << "\n";
					}
				}
				// Write static library paths (deduped)
				std::set<std::string> seenLibs;
				for (const auto& lib : staticLibs) {
					if (seenLibs.insert(lib).second) {
						deps << lib << "\n";
					}
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
	std::cout << quadrate_version_string("quadpm") << "\n";
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
	std::cout << "    --frozen       Only install from qd.lock (fail if outdated)\n";
	std::cout << "  lock             Generate/update qd.lock from installed modules\n";
	std::cout << "  get <url>[@ref]  Fetch and install a module from Git\n";
	std::cout << "  update [name]    Update installed module(s) (git pull)\n";
	std::cout << "  list             List installed modules\n";
	std::cout << "  build            Build C sources in current module directory\n\n";
	std::cout << "Lockfile (qd.lock):\n";
	std::cout << "  The lockfile pins exact commit hashes for reproducible builds.\n";
	std::cout << "  - 'install' creates/updates qd.lock automatically\n";
	std::cout << "  - 'install --frozen' uses qd.lock strictly (for CI)\n";
	std::cout << "  - 'lock' regenerates qd.lock from installed modules\n\n";
	std::cout << "Transitive Dependencies:\n";
	std::cout << "  quadpm automatically resolves and installs transitive dependencies.\n";
	std::cout << "  If package A depends on B, and B depends on C, all three are installed.\n\n";
	std::cout << "Examples:\n";
	std::cout << "  quadpm install\n";
	std::cout << "  quadpm install --frozen\n";
	std::cout << "  quadpm lock\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib\n";
	std::cout << "  quadpm get https://git.sr.ht/~user/zlib@1.2.0\n";
	std::cout << "  quadpm get https://github.com/user/http@main\n";
	std::cout << "  quadpm list\n\n";
	std::cout << "qd.json format (npm-compatible):\n";
	std::cout << "  {\n";
	std::cout << "    \"name\": \"mymodule\",\n";
	std::cout << "    \"dependencies\": {\n";
	std::cout << "      \"glut\": \"https://git.sr.ht/~user/qd-glut@v1.0.0\",\n";
	std::cout << "      \"http\": { \"url\": \"https://git.sr.ht/~user/qd-http\", \"version\": \"^2.0.0\" },\n";
	std::cout << "      \"mylib\": \"../local/path\",\n";
	std::cout << "      \"crypto\": {\n";
	std::cout << "        \"url\": \"https://git.sr.ht/~user/qd-crypto\",\n";
	std::cout << "        \"version\": \"~1.5.0\",\n";
	std::cout << "        \"integrity\": \"sha256-abc123...\"\n";
	std::cout << "      }\n";
	std::cout << "    }\n";
	std::cout << "  }\n\n";
	std::cout << "Semver version ranges:\n";
	std::cout << "  ^1.2.3    Compatible with version (>=1.2.3 <2.0.0)\n";
	std::cout << "  ~1.2.3    Approximately equivalent (>=1.2.3 <1.3.0)\n";
	std::cout << "  >=1.0.0   Greater than or equal\n";
	std::cout << "  <2.0.0    Less than\n";
	std::cout << "  1.2.x     Any patch version (>=1.2.0 <1.3.0)\n";
	std::cout << "  *         Any version\n";
	std::cout << "  1.0.0 - 2.0.0   Hyphen range (inclusive)\n";
	std::cout << "  >=1.0.0 <2.0.0 || >=3.0.0   Multiple ranges\n\n";
	std::cout << "Environment:\n";
	std::cout << "  QUADRATE_PATH      Module installation directory\n";
	std::cout << "  XDG_DATA_HOME      If set, uses $XDG_DATA_HOME/quadrate/modules\n";
	std::cout << "  Default: ~/quadrate/modules\n";
}

// List installed modules (handles Go-style host/user/repo@version paths)
void listModules() {
	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		std::cout << "No modules installed yet.\n";
		std::cout << "Modules will be installed to: " << modulesDir << "\n";
		return;
	}

	std::cout << COLOR_BOLD << "Installed modules:" << COLOR_RESET << "\n";
	std::cout << "Location: " << modulesDir << "\n\n";

	// Collect all packages with their info
	struct PackageInfo {
		std::string hostPath;	   // e.g., "git.sr.ht/~klahr/collections"
		std::string version;	   // e.g., "v1.0.0"
		std::string fullPath;	   // Full filesystem path
		std::string namespaceName; // Namespace for code usage
	};

	std::vector<PackageInfo> packages;

	// Recursively find packages (directories containing module.qd or qd.json)
	for (const auto& entry : fs::recursive_directory_iterator(modulesDir)) {
		if (!entry.is_directory()) {
			continue;
		}
		std::string dirname = entry.path().filename().string();
		// Skip _namespaces directory
		if (dirname == "_namespaces") {
			continue;
		}
		// Check if this looks like a package directory (has @ in name and contains module.qd or qd.json)
		size_t atPos = dirname.find('@');
		if (atPos == std::string::npos) {
			continue;
		}
		std::string moduleFile = entry.path().string() + "/module.qd";
		std::string manifestFile = entry.path().string() + "/qd.json";
		if (!fs::exists(moduleFile) && !fs::exists(manifestFile)) {
			continue;
		}

		// Extract package info
		PackageInfo info;
		info.fullPath = entry.path().string();
		info.version = dirname.substr(atPos + 1);

		// Get host/user/repo from path relative to modules dir
		std::string relativePath = info.fullPath.substr(modulesDir.size() + 1); // +1 for '/'
		size_t relAtPos = relativePath.find('@');
		if (relAtPos != std::string::npos) {
			info.hostPath = relativePath.substr(0, relAtPos);
		} else {
			info.hostPath = relativePath;
		}

		// Get namespace from qd.json
		if (fs::exists(manifestFile)) {
			std::string ns = parseNamespace(manifestFile);
			std::string name = parseModuleName(manifestFile);
			if (!ns.empty()) {
				info.namespaceName = ns;
			} else if (!name.empty()) {
				info.namespaceName = name;
			} else {
				// Extract from path
				size_t lastSlash = info.hostPath.find_last_of('/');
				info.namespaceName =
						(lastSlash != std::string::npos) ? info.hostPath.substr(lastSlash + 1) : info.hostPath;
			}
		} else {
			size_t lastSlash = info.hostPath.find_last_of('/');
			info.namespaceName = (lastSlash != std::string::npos) ? info.hostPath.substr(lastSlash + 1) : info.hostPath;
		}

		packages.push_back(info);
	}

	if (packages.empty()) {
		std::cout << "No modules installed.\n";
		return;
	}

	// Sort by hostPath
	std::sort(packages.begin(), packages.end(),
			[](const PackageInfo& a, const PackageInfo& b) { return a.hostPath < b.hostPath; });

	for (const auto& pkg : packages) {
		std::cout << "  " << COLOR_BOLD << pkg.hostPath << COLOR_RESET << " @ " << COLOR_CYAN << pkg.version
				  << COLOR_RESET << "\n";
		std::cout << "    namespace: " << COLOR_GREEN << pkg.namespaceName << COLOR_RESET << "\n";
	}

	// Show namespace symlinks
	std::string namespacesDir = getNamespacesDir();
	if (fs::exists(namespacesDir)) {
		std::cout << "\n" << COLOR_BOLD << "Registered namespaces:" << COLOR_RESET << "\n";
		for (const auto& entry : fs::directory_iterator(namespacesDir)) {
			if (fs::is_symlink(entry.path())) {
				std::string ns = entry.path().filename().string();
				try {
					std::string target = fs::read_symlink(entry.path()).string();
					std::cout << "  " << COLOR_GREEN << ns << COLOR_RESET << " → " << target << "\n";
				} catch (const std::exception&) {
					std::cout << "  " << COLOR_YELLOW << ns << COLOR_RESET << " → (broken)\n";
				}
			}
		}
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
	int result = execCommandLive({"git", "-C", moduleDir, "pull"});

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
	try {
		std::string output = execCommand({"sha256sum", path});
		// sha256sum output: "hash  filename\n" - extract just the hash
		size_t spacePos = output.find(' ');
		if (spacePos != std::string::npos) {
			output = output.substr(0, spacePos);
		}
		// Trim whitespace
		output.erase(output.find_last_not_of(" \t\r\n") + 1);
		return output;
	} catch (...) {
		return "";
	}
}

// List all tags from a remote git repository
// Returns pairs of (tag_name, commit_hash)
std::vector<std::pair<std::string, std::string>> listRemoteTags(const std::string& gitUrl) {
	std::vector<std::pair<std::string, std::string>> tags;

	try {
		// git ls-remote --tags <url>
		std::string output = execCommand({"git", "ls-remote", "--tags", gitUrl});

		// Parse output: "commit_hash\trefs/tags/tag_name" (one per line)
		std::istringstream iss(output);
		std::string line;
		while (std::getline(iss, line)) {
			if (line.empty()) {
				continue;
			}

			// Split by tab
			size_t tabPos = line.find('\t');
			if (tabPos == std::string::npos) {
				continue;
			}

			std::string commitHash = line.substr(0, tabPos);
			std::string ref = line.substr(tabPos + 1);

			// Skip ^{} dereferenced entries
			if (ref.find("^{}") != std::string::npos) {
				continue;
			}

			// Extract tag name from refs/tags/
			const std::string prefix = "refs/tags/";
			if (ref.substr(0, prefix.size()) != prefix) {
				continue;
			}

			std::string tagName = ref.substr(prefix.size());
			tags.emplace_back(tagName, commitHash);
		}
	} catch (...) {
		// Failed to list tags
	}

	return tags;
}

// Check if a dependency version is a semver range (not a branch name or commit)
bool isSemVerRange(const std::string& version) {
	if (version.empty()) {
		return false;
	}

	// Explicit semver operators
	if (version[0] == '^' || version[0] == '~' || version[0] == '>' || version[0] == '<' || version[0] == '=') {
		return true;
	}

	// Wildcard
	if (version == "*" || version.find('x') != std::string::npos || version.find('X') != std::string::npos) {
		return true;
	}

	// Hyphen range
	if (version.find(" - ") != std::string::npos) {
		return true;
	}

	// OR range
	if (version.find("||") != std::string::npos) {
		return true;
	}

	// Could be exact version (1.2.3) or branch name (main)
	// Try parsing as semver
	return isSemVer(version);
}

// Resolve a semver range to a specific tag
// Returns the resolved tag name, or empty string if not found
std::string resolveSemVerRange(const std::string& gitUrl, const std::string& range) {
	std::cout << "  → Resolving version range: " << range << "\n";

	// Parse the range
	VersionRange versionRange = parseVersionRange(range);
	if (!versionRange.isValid()) {
		std::cerr << COLOR_RED << "  ✗ Invalid version range: " << range << COLOR_RESET << "\n";
		return "";
	}

	// Get all tags
	auto remoteTags = listRemoteTags(gitUrl);
	if (remoteTags.empty()) {
		std::cerr << COLOR_YELLOW << "  ⚠ No tags found in repository" << COLOR_RESET << "\n";
		return "";
	}

	// Filter tags to those that are valid semver
	std::vector<std::pair<SemVer, std::string>> semverTags;
	for (const auto& [tagName, commitHash] : remoteTags) {
		if (isSemVer(tagName)) {
			SemVer v = parseSemVer(tagName);
			if (v.isValid()) {
				semverTags.emplace_back(v, tagName);
			}
		}
	}

	if (semverTags.empty()) {
		std::cerr << COLOR_YELLOW << "  ⚠ No semver tags found (available: ";
		for (size_t i = 0; i < remoteTags.size() && i < 5; i++) {
			if (i > 0) {
				std::cerr << ", ";
			}
			std::cerr << remoteTags[i].first;
		}
		if (remoteTags.size() > 5) {
			std::cerr << ", ...";
		}
		std::cerr << ")" << COLOR_RESET << "\n";
		return "";
	}

	// Sort by version (newest first)
	std::sort(semverTags.begin(), semverTags.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

	// Find best matching version
	for (const auto& [version, tagName] : semverTags) {
		if (versionRange.satisfies(version)) {
			std::cout << "  → Resolved to: " << COLOR_GREEN << tagName << COLOR_RESET << " (" << version.toString()
					  << ")\n";
			return tagName;
		}
	}

	std::cerr << COLOR_YELLOW << "  ⚠ No version satisfies range " << range << " (available: ";
	for (size_t i = 0; i < semverTags.size() && i < 5; i++) {
		if (i > 0) {
			std::cerr << ", ";
		}
		std::cerr << semverTags[i].second;
	}
	if (semverTags.size() > 5) {
		std::cerr << ", ...";
	}
	std::cerr << ")" << COLOR_RESET << "\n";

	return "";
}

// Install a single dependency (helper for installDependencies)
// Returns the installed module path on success, empty string on failure
// Sets installedRef to the actual ref that was installed
struct InstallResult {
	std::string modulePath; // Path to installed module
	std::string actualRef;	// Actual ref installed (may differ from requested)
	std::string commitHash; // Git commit hash
	bool alreadyInstalled;	// Was already installed
	bool success;
};

InstallResult installSingleDependency(const Dependency& dep, const std::string& basePath,
		const std::map<std::string, LockedDependency>& lockedByName, bool frozen) {
	InstallResult result;
	result.success = false;
	result.alreadyInstalled = false;

	std::string modulesDir = getModulesDir();

	if (dep.isPath) {
		// Local path dependency
		std::string resolvedPath = resolveLocalPath(dep.url, basePath);

		if (!fs::exists(resolvedPath)) {
			std::cout << COLOR_RED << "✗ Path not found: " << resolvedPath << COLOR_RESET << "\n";
			return result;
		}

		// Verify it has a module.qd
		if (!fs::exists(resolvedPath + "/module.qd")) {
			std::cout << COLOR_RED << "✗ Not a module (no module.qd): " << resolvedPath << COLOR_RESET << "\n";
			return result;
		}

		std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << resolvedPath << " (local)\n";

		result.modulePath = resolvedPath;
		result.success = true;
		return result;
	}

	// Git URL dependency
	GitRef gitRef;
	gitRef.url = dep.url;
	gitRef.moduleName = dep.name;
	gitRef.hostPath = extractHostPath(dep.url);

	// Determine the version to use
	std::string versionSpec = dep.version;
	if (versionSpec.empty()) {
		versionSpec = "main";
	}

	// Check if we have a locked version to use
	auto lockedIt = lockedByName.find(dep.name);
	std::string expectedCommit;

	if (lockedIt != lockedByName.end() && !lockedIt->second.resolvedRef.empty()) {
		// Use locked commit hash for reproducibility
		expectedCommit = lockedIt->second.resolvedRef;
		if (frozen) {
			// In frozen mode, use locked ref
			gitRef.ref = lockedIt->second.ref;
		}
	}

	// Resolve semver range if needed (and not using lockfile)
	if (!frozen && dep.isSemVer && !versionSpec.empty()) {
		std::cout << "\n";
		std::string resolvedTag = resolveSemVerRange(dep.url, versionSpec);
		if (resolvedTag.empty()) {
			std::cout << COLOR_RED << "✗ Failed to resolve version range: " << versionSpec << COLOR_RESET << "\n";
			return result;
		}
		gitRef.ref = resolvedTag;
	} else if (!frozen) {
		gitRef.ref = versionSpec;
	}

	result.actualRef = gitRef.ref;

	// Check if already installed (using Go-style hostPath)
	std::string installedDirName = getInstalledDirName(gitRef.hostPath, gitRef.ref);
	std::string installedDir = modulesDir + "/" + installedDirName;
	if (fs::exists(installedDir)) {
		std::string currentCommit = getModuleCommitHash(installedDir);

		// In frozen mode, verify commit matches lockfile
		if (frozen && !expectedCommit.empty() && currentCommit != expectedCommit) {
			std::cout << COLOR_RED << "✗ Commit mismatch (frozen)" << COLOR_RESET << "\n";
			std::cout << "  Expected: " << expectedCommit << "\n";
			std::cout << "  Got:      " << currentCommit << "\n";
			return result;
		}

		std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "already installed @ " << gitRef.ref;
		if (!currentCommit.empty()) {
			std::cout << " (" << currentCommit.substr(0, 8) << ")";
		}
		std::cout << "\n";

		result.modulePath = installedDir;
		result.commitHash = currentCommit;
		result.alreadyInstalled = true;
		result.success = true;
		return result;
	}

	// Clone the repository
	std::string installedName = gitClone(gitRef);
	if (installedName.empty()) {
		std::cout << COLOR_RED << "✗ Failed to clone" << COLOR_RESET << "\n";
		return result;
	}

	// Get the actual commit hash (use the same hostPath-based directory)
	std::string commitHash = getModuleCommitHash(installedDir);

	std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "installed @ " << gitRef.ref;
	if (!commitHash.empty()) {
		std::cout << " (" << commitHash.substr(0, 8) << ")";
	}
	std::cout << "\n";

	// Verify sha256 if specified in manifest
	if (!dep.sha256.empty() && commitHash != dep.sha256) {
		std::cout << "  " << COLOR_YELLOW << "⚠ SHA256 mismatch!" << COLOR_RESET << "\n";
		std::cout << "    Expected: " << dep.sha256 << "\n";
		std::cout << "    Got:      " << commitHash << "\n";
	}

	result.modulePath = installedDir;
	result.commitHash = commitHash;
	result.success = true;
	return result;
}

// Install dependencies from qd.json (with lockfile support)
// frozen: if true, only install from lockfile (fail if lockfile missing or outdated)
// Supports transitive dependencies - will recursively install dependencies of dependencies
int installDependencies(bool frozen = false) {
	std::string cwd = fs::current_path().string();

	// Check for qd.json
	std::string manifestPath = cwd + "/qd.json";
	if (!fs::exists(manifestPath)) {
		std::cerr << COLOR_RED << "Error: No qd.json found in current directory" << COLOR_RESET << "\n";
		return 1;
	}

	// Check for lockfile
	std::string lockfilePath = cwd + "/qd.lock";
	std::vector<LockedDependency> lockedDeps = readLockfile(lockfilePath);
	bool hasLockfile = !lockedDeps.empty();

	if (frozen && !hasLockfile) {
		std::cerr << COLOR_RED << "Error: --frozen requires qd.lock but none found" << COLOR_RESET << "\n";
		std::cerr << "Run 'quadpm install' first to generate a lockfile\n";
		return 1;
	}

	// Parse direct dependencies from manifest
	std::vector<Dependency> directDeps = parseDependencies(manifestPath);
	if (directDeps.empty()) {
		std::cout << "No dependencies found in qd.json\n";
		return 0;
	}

	// Build a map of locked deps by name for quick lookup
	std::map<std::string, LockedDependency> lockedByName;
	for (const auto& locked : lockedDeps) {
		lockedByName[locked.name] = locked;
	}

	// In frozen mode, verify all direct deps are in lockfile
	if (frozen) {
		for (const auto& dep : directDeps) {
			if (lockedByName.find(dep.name) == lockedByName.end()) {
				std::cerr << COLOR_RED << "Error: Dependency '" << dep.name << "' not in lockfile" << COLOR_RESET
						  << "\n";
				std::cerr << "Run 'quadpm install' to update the lockfile\n";
				return 1;
			}
		}
	}

	// Use a queue for BFS traversal of dependency graph
	// Each entry is (dependency, base_path for resolving relative paths)
	std::queue<std::pair<Dependency, std::string>> pendingDeps;
	std::set<std::string> processedDeps; // Track by name to avoid duplicates
	std::vector<LockedDependency> newLockedDeps;
	int failures = 0;
	int totalInstalled = 0;

	// Add direct dependencies to queue
	for (const auto& dep : directDeps) {
		pendingDeps.push({dep, cwd});
	}

	if (hasLockfile) {
		std::cout << COLOR_CYAN << "Installing from lockfile..." << COLOR_RESET << "\n\n";
	} else {
		std::cout << COLOR_CYAN << "Installing " << directDeps.size() << " direct dependenc"
				  << (directDeps.size() == 1 ? "y" : "ies") << " (+ transitive)..." << COLOR_RESET << "\n\n";
	}

	// Process dependencies using BFS
	while (!pendingDeps.empty()) {
		auto [dep, basePath] = pendingDeps.front();
		pendingDeps.pop();

		// Skip if already processed
		if (processedDeps.count(dep.name) > 0) {
			continue;
		}
		processedDeps.insert(dep.name);

		std::cout << COLOR_BOLD << dep.name << COLOR_RESET << ": ";

		// Install this dependency
		InstallResult installResult = installSingleDependency(dep, basePath, lockedByName, frozen);

		if (!installResult.success) {
			failures++;
			continue;
		}

		totalInstalled++;

		// Record in lockfile
		LockedDependency newLocked;
		newLocked.name = dep.name;
		newLocked.isPath = dep.isPath;
		if (dep.isPath) {
			newLocked.url = dep.url;
			newLocked.resolvedPath = installResult.modulePath;
		} else {
			newLocked.url = dep.url;
			newLocked.ref = installResult.actualRef;
			newLocked.resolvedRef = installResult.commitHash;
			newLocked.integrity = installResult.commitHash;
		}
		newLockedDeps.push_back(newLocked);

		// Check for transitive dependencies in the installed module
		std::string depManifest = installResult.modulePath + "/qd.json";
		if (fs::exists(depManifest)) {
			std::vector<Dependency> transitiveDeps = parseDependencies(depManifest);
			if (!transitiveDeps.empty()) {
				std::cout << "  → " << COLOR_CYAN << transitiveDeps.size() << " transitive dependenc"
						  << (transitiveDeps.size() == 1 ? "y" : "ies") << COLOR_RESET << "\n";

				// Add transitive deps to queue (use installed module's dir as base for relative paths)
				for (const auto& transDep : transitiveDeps) {
					if (processedDeps.count(transDep.name) == 0) {
						pendingDeps.push({transDep, installResult.modulePath});
					}
				}
			}
		}
	}

	std::cout << "\n";
	if (failures > 0) {
		std::cout << COLOR_RED << failures << " dependenc" << (failures == 1 ? "y" : "ies") << " failed" << COLOR_RESET
				  << "\n";
		return 1;
	}

	// Write lockfile (unless in frozen mode)
	if (!frozen && !newLockedDeps.empty()) {
		if (writeLockfile(lockfilePath, newLockedDeps)) {
			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "Updated qd.lock (" << newLockedDeps.size()
					  << " packages)\n";
		}
	}

	std::cout << COLOR_GREEN << "All " << totalInstalled << " dependencies installed!" << COLOR_RESET << "\n";
	return 0;
}

// Generate/update lockfile without installing
int generateLockfile() {
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

	std::cout << COLOR_CYAN << "Generating lockfile for " << deps.size() << " dependenc"
			  << (deps.size() == 1 ? "y" : "ies") << "..." << COLOR_RESET << "\n\n";

	std::string modulesDir = getModulesDir();
	std::vector<LockedDependency> lockedDeps;
	int missing = 0;

	for (const auto& dep : deps) {
		std::cout << COLOR_BOLD << dep.name << COLOR_RESET << ": ";

		LockedDependency locked;
		locked.name = dep.name;
		locked.isPath = dep.isPath;

		if (dep.isPath) {
			// Local path dependency
			std::string resolvedPath = dep.url;

			if (resolvedPath.size() > 0 && resolvedPath[0] == '~') {
				resolvedPath = getHomeDir() + resolvedPath.substr(1);
			}
			if (resolvedPath.size() > 0 && resolvedPath[0] != '/') {
				resolvedPath = cwd + "/" + resolvedPath;
			}
			resolvedPath = fs::weakly_canonical(resolvedPath).string();

			if (!fs::exists(resolvedPath)) {
				std::cout << COLOR_YELLOW << "⚠ not found" << COLOR_RESET << "\n";
				missing++;
				continue;
			}

			locked.url = dep.url;
			locked.resolvedPath = resolvedPath;
			lockedDeps.push_back(locked);
			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << resolvedPath << "\n";
		} else {
			// Git URL dependency
			GitRef gitRef = parseGitUrl(dep.url);
			std::string installedDir = modulesDir + "/" + getInstalledDirName(gitRef.hostPath, gitRef.ref);

			if (!fs::exists(installedDir)) {
				std::cout << COLOR_YELLOW << "⚠ not installed" << COLOR_RESET << "\n";
				missing++;
				continue;
			}

			std::string commitHash = getModuleCommitHash(installedDir);

			locked.url = gitRef.url;
			locked.ref = gitRef.ref;
			locked.resolvedRef = commitHash;
			locked.integrity = commitHash;
			lockedDeps.push_back(locked);

			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET;
			if (!commitHash.empty()) {
				std::cout << commitHash.substr(0, 8);
			}
			std::cout << "\n";
		}
	}

	std::cout << "\n";

	if (missing > 0) {
		std::cout << COLOR_YELLOW << missing << " dependenc" << (missing == 1 ? "y" : "ies")
				  << " not installed - run 'quadpm install' first" << COLOR_RESET << "\n";
	}

	if (!lockedDeps.empty()) {
		std::string lockfilePath = cwd + "/qd.lock";
		if (writeLockfile(lockfilePath, lockedDeps)) {
			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "Wrote qd.lock (" << lockedDeps.size() << " packages)\n";
			return 0;
		} else {
			std::cerr << COLOR_RED << "Error: Failed to write qd.lock" << COLOR_RESET << "\n";
			return 1;
		}
	}

	return missing > 0 ? 1 : 0;
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
		// Check for --frozen flag
		bool frozen = false;
		for (int i = 2; i < argc; i++) {
			if (std::string(argv[i]) == "--frozen") {
				frozen = true;
			}
		}
		return installDependencies(frozen);
	}

	if (command == "lock") {
		return generateLockfile();
	}

	std::cerr << COLOR_RED << "Error: Unknown command '" << command << "'" << COLOR_RESET << "\n";
	printUsage();
	return 1;
}

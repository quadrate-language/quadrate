#ifndef QUADC_MODULE_RESOLVER_H
#define QUADC_MODULE_RESOLVER_H

#include <string>
#include <unordered_map>
#include <vector>

// Set module version pins from command-line -l flags
void setModuleVersionPins(const std::unordered_map<std::string, std::string>& pins);

// Set additional module search paths from command-line -I flags
void setModuleIncludePaths(const std::vector<std::string>& paths);

// Check if a string ends with .qd (is a direct file import)
inline bool isQdFile(const std::string& name) {
	return name.size() >= 3 && name.substr(name.size() - 3) == ".qd";
}

// Expand tilde (~) in file paths
std::string expandTilde(const std::string& path);

// Get packages directory path (where quadpm installs third-party modules)
std::string getPackagesDir();

// Find a package in the packages directory
// Checks g_moduleVersionPins first for pinned versions from -l flags
// Returns the full path to the package directory, or empty string if not found
std::string findLatestPackageVersion(const std::string& moduleName);

// Get package name from a module identifier
// For file paths (ending in .qd), returns the filename without extension
// For module names, returns the module name as-is
std::string getPackageFromModuleName(const std::string& moduleName);

// Find a module file, searching in multiple locations
// Returns the full path to the module file, or empty string if not found
std::string findModuleFile(const std::string& moduleName, const std::string& sourceDir);

// Find all module files, searching in multiple locations
// For module.qd-based modules, returns just that file
// For multi-file modules (no module.qd), returns all *.qd files in the directory
// Returns empty vector if not found
std::vector<std::string> findModuleFiles(const std::string& moduleName, const std::string& sourceDir);

// Load dependencies from qd.json and add them as include paths
// Looks for qd.json in the specified directory
// Returns the list of include paths found
std::vector<std::string> loadDependenciesFromManifest(const std::string& manifestDir);

// Get all sibling .qd files in the same directory as the given file
// Excludes the given file itself and test files (*_test.qd)
// Used for directory-based namespace collection
std::vector<std::string> getSiblingQdFiles(const std::string& filePath);

// Find a namespace directory and return all its .qd files
// Searches relative to sourceDir, include paths, etc.
// Returns empty vector if namespace not found
std::vector<std::string> findNamespaceFiles(const std::string& namespaceName, const std::string& sourceDir);

#endif

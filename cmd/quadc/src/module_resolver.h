#ifndef QUADC_MODULE_RESOLVER_H
#define QUADC_MODULE_RESOLVER_H

#include <string>
#include <unordered_map>
#include <vector>

// Set module version pins from command-line -l flags
void setModuleVersionPins(const std::unordered_map<std::string, std::string>& pins);

// Set additional module search paths from command-line -I flags
void setModuleIncludePaths(const std::vector<std::string>& paths);

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

// Load dependencies from quadrate.toml and add them as include paths
// Looks for quadrate.toml in the specified directory
// Returns the list of include paths found
std::vector<std::string> loadDependenciesFromManifest(const std::string& manifestDir);

#endif

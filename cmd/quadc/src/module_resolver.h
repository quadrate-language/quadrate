#ifndef QUADC_MODULE_RESOLVER_H
#define QUADC_MODULE_RESOLVER_H

#include <string>
#include <unordered_map>

// Set module version pins from command-line -l flags
void setModuleVersionPins(const std::unordered_map<std::string, std::string>& pins);

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

#endif

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

// Get package name from a module identifier
// For file paths (ending in .qd), returns the filename without extension
// For module names, returns the module name as-is
std::string getPackageFromModuleName(const std::string& moduleName);

// The files of `use moduleName` written in sourceDir: the one file of a `use "file.qd"`, or
// every file of a module directory, found by the search every tool shares (qc/module_resolver.h)
// with the -I paths and -l pins set above. Empty when there is no such module.
struct ResolvedModule {
	std::vector<std::string> files;
	std::string namespaceName; // Its words' namespace: a full-path import's is the one its qd.json declares
};

ResolvedModule findModuleFiles(const std::string& moduleName, const std::string& sourceDir);

#endif

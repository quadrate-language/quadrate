#include "module_resolver.h"
#include <quadrate/qc/module_resolver.h>
#include <unordered_map>

// Global module version pins (set from command-line -l flags)
static std::unordered_map<std::string, std::string> g_moduleVersionPins;

// Global module include paths (set from command-line -I flags)
static std::vector<std::string> g_moduleIncludePaths;

void setModuleVersionPins(const std::unordered_map<std::string, std::string>& pins) {
	g_moduleVersionPins = pins;
}

void setModuleIncludePaths(const std::vector<std::string>& paths) {
	g_moduleIncludePaths = paths;
}

std::string getPackageFromModuleName(const std::string& moduleName) {
	if (!isQdFile(moduleName)) {
		return moduleName;
	}
	// A file import names its package after the file: `use "lib/geometry.qd"` is geometry
	const size_t lastSlash = moduleName.find_last_of('/');
	const std::string filename = lastSlash != std::string::npos ? moduleName.substr(lastSlash + 1) : moduleName;
	return filename.substr(0, filename.size() - 3);
}

ResolvedModule findModuleFiles(const std::string& moduleName, const std::string& sourceDir) {
	Qd::ModuleSearch search;
	search.sourceDir = sourceDir;
	search.includePaths = g_moduleIncludePaths;
	search.versionPins = g_moduleVersionPins;
	ResolvedModule resolved;
	if (isQdFile(moduleName)) {
		const std::string file = Qd::findModuleFile(moduleName, search);
		if (!file.empty()) {
			resolved.files = {file};
		}
		resolved.namespaceName = getPackageFromModuleName(moduleName);
		return resolved;
	}
	const Qd::ModuleLocation location = Qd::findModule(moduleName, search);
	if (!location.directory.empty()) {
		resolved.files = Qd::moduleFiles(location.directory);
	}
	resolved.namespaceName = location.namespaceName.empty() ? moduleName : location.namespaceName;
	return resolved;
}

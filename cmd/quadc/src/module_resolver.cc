#include "module_resolver.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jansson.h>
#include <unordered_map>

// Platform abstractions
extern "C" {
#include "src/platform/exe_path_platform.h"
}

// Platform-specific data directory name
// Haiku uses "data" instead of "share" for data files
#ifdef __HAIKU__
static constexpr const char* DATA_DIR_NAME = "data";
#else
static constexpr const char* DATA_DIR_NAME = "share";
#endif

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

std::string expandTilde(const std::string& path) {
	if (path.empty() || path[0] != '~') {
		return path;
	}

	const char* home = std::getenv("HOME");
	if (!home) {
		return path; // Can't expand, return as-is
	}

	// Replace ~ or ~/ with home directory
	if (path.length() == 1) {
		return std::string(home);
	} else if (path[1] == '/') {
		return std::string(home) + path.substr(1);
	}

	// ~username syntax not supported, return as-is
	return path;
}

std::string getPackagesDir() {
	// Check QUADRATE_PATH environment variable first
	const char* quadratePath = std::getenv("QUADRATE_PATH");
	if (quadratePath) {
		return std::string(quadratePath);
	}

	// Check if XDG_DATA_HOME is set
	const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
	if (xdgDataHome) {
		return std::string(xdgDataHome) + "/quadrate/modules";
	}

	// Default to ~/quadrate/modules
	const char* home = std::getenv("HOME");
	if (home) {
		return std::string(home) + "/quadrate/modules";
	}

	return ""; // No modules directory available
}

std::string findLatestPackageVersion(const std::string& moduleName) {
	std::string packagesDir = getPackagesDir();
	if (packagesDir.empty() || !std::filesystem::exists(packagesDir)) {
		return "";
	}

	// Check if this module has a pinned version from -l flag
	std::string pinnedVersion;
	if (g_moduleVersionPins.count(moduleName)) {
		pinnedVersion = g_moduleVersionPins.at(moduleName);
	}

	// If version is pinned, look for exact match
	if (!pinnedVersion.empty()) {
		std::string exactPath = packagesDir + "/" + moduleName + "@" + pinnedVersion;
		if (std::filesystem::exists(exactPath) && std::filesystem::is_directory(exactPath)) {
			return exactPath;
		}
		return ""; // Pinned version not found
	}

	// No version specified - find any version
	std::string latestVersion;
	std::string latestPath;

	try {
		for (const auto& entry : std::filesystem::directory_iterator(packagesDir)) {
			if (!entry.is_directory()) {
				continue;
			}

			std::string dirName = entry.path().filename().string();

			// Check if directory name starts with "moduleName@"
			std::string prefix = moduleName + "@";
			if (dirName.size() > prefix.size() && dirName.substr(0, prefix.size()) == prefix) {
				std::string foundVersion = dirName.substr(prefix.size());

				// For now, just use the first match (later could implement version comparison)
				// If multiple versions exist, we use the last one found by directory iterator
				latestVersion = foundVersion;
				latestPath = entry.path().string();
			}
		}
	} catch (...) {
		// Ignore errors iterating directory
		return "";
	}

	return latestPath;
}

// Helper: Get all .qd files in a directory, sorted alphabetically
static std::vector<std::string> globQdFiles(const std::string& directory) {
	std::vector<std::string> files;

	try {
		if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
			return files;
		}

		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			if (entry.is_regular_file()) {
				std::string filename = entry.path().filename().string();
				if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".qd") {
					// Exclude test files (*_test.qd) from module loading
					if (filename.size() > 8 && filename.substr(filename.size() - 8) == "_test.qd") {
						continue;
					}
					files.push_back(entry.path().string());
				}
			}
		}

		// Sort alphabetically for deterministic order
		std::sort(files.begin(), files.end());
	} catch (...) {
		// Ignore filesystem errors
	}

	return files;
}

// Helper: Try to get module files from a directory
// Returns module.qd if it exists, otherwise all .qd files in the directory
static std::vector<std::string> tryGetModuleFilesFromDir(const std::string& moduleDir) {
	return globQdFiles(moduleDir);
}

std::string getPackageFromModuleName(const std::string& moduleName) {
	// Check if this is a file path (ends with .qd)
	bool isFilePath = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isFilePath) {
		// Extract filename from path
		size_t lastSlash = moduleName.find_last_of('/');
		std::string filename = (lastSlash != std::string::npos) ? moduleName.substr(lastSlash + 1) : moduleName;

		// Remove .qd extension
		if (filename.size() >= 3 && filename.substr(filename.size() - 3) == ".qd") {
			filename = filename.substr(0, filename.size() - 3);
		}

		return filename;
	}

	// Not a file path, return as-is
	return moduleName;
}

std::string findModuleFile(const std::string& moduleName, const std::string& sourceDir) {
	// Check if this is a direct file import (ends with .qd)
	bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isDirectFile) {
		// Expand tilde in the path
		std::string expandedModuleName = expandTilde(moduleName);

		// If it's an absolute path (starts with / or was expanded from ~), use it directly
		if (!expandedModuleName.empty() && expandedModuleName[0] == '/') {
			if (std::filesystem::exists(expandedModuleName)) {
				return expandedModuleName;
			}
			return ""; // Absolute path doesn't exist
		}

		// For relative paths (including ./ and ../), resolve relative to source directory
		std::filesystem::path filePath;
		if (moduleName.size() >= 2 && (moduleName.substr(0, 2) == "./" || moduleName.substr(0, 3) == "../")) {
			// Explicit relative path
			filePath = std::filesystem::path(sourceDir) / moduleName;
		} else {
			// Implicit relative path (no leading ./ or ../)
			filePath = std::filesystem::path(sourceDir) / moduleName;
		}

		// Normalize the path (resolve .. and .)
		try {
			filePath = std::filesystem::weakly_canonical(filePath);
			if (std::filesystem::exists(filePath)) {
				return filePath.string();
			}
		} catch (...) {
			// If path resolution fails, try simple concatenation
			std::string localPath = sourceDir + "/" + moduleName;
			if (std::filesystem::exists(localPath)) {
				return localPath;
			}
		}

		// File not found
		return "";
	}

	// Module directory imports are handled by findModuleFiles()
	return "";
}

std::vector<std::string> findModuleFiles(const std::string& moduleName, const std::string& sourceDir) {
	// Check if this is a direct file import (ends with .qd)
	bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isDirectFile) {
		// For direct file imports, just return the single file
		std::string singleFile = findModuleFile(moduleName, sourceDir);
		if (!singleFile.empty()) {
			return {singleFile};
		}
		return {};
	}

	// Module directory import
	std::vector<std::string> result;

	// Try 1: Local path (relative to source file)
	result = tryGetModuleFilesFromDir(sourceDir + "/" + moduleName);
	if (!result.empty()) {
		return result;
	}

	// Try 2: Include paths from -I flags
	for (const auto& includePath : g_moduleIncludePaths) {
		std::string expandedPath = expandTilde(includePath);

		// Check if the include path IS the module directory
		result = tryGetModuleFilesFromDir(expandedPath);
		if (!result.empty()) {
			// Verify qd.json name matches
			std::string manifestPath = expandedPath + "/qd.json";
			if (std::filesystem::exists(manifestPath)) {
				json_error_t error;
				json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
				if (root) {
					json_t* name = json_object_get(root, "name");
					if (name && json_is_string(name) && json_string_value(name) == moduleName) {
						json_decref(root);
						return result;
					}
					json_decref(root);
				}
			}
			result.clear();
		}

		// Check for module as subdirectory of include path
		result = tryGetModuleFilesFromDir(expandedPath + "/" + moduleName);
		if (!result.empty()) {
			return result;
		}
	}

	// Try 3: Third-party packages directory
	std::string packagePath = findLatestPackageVersion(moduleName);
	if (!packagePath.empty()) {
		result = tryGetModuleFilesFromDir(packagePath);
		if (!result.empty()) {
			return result;
		}
	}

	// Try 4: QUADRATE_ROOT environment variable
	const char* quadrateRoot = getenv("QUADRATE_ROOT");
	if (quadrateRoot) {
		result = tryGetModuleFilesFromDir(std::string(quadrateRoot) + "/" + moduleName);
		if (!result.empty()) {
			return result;
		}
	}

	// Try 5: Standard library relative to executable
	// Note: DATA_DIR_NAME is "data" on Haiku, "share" on other platforms
	// Check this before the source tree path so we use the same files as semantic validator
	{
		char exePathBuf[4096];
		int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
		if (len > 0 && static_cast<size_t>(len) < sizeof(exePathBuf)) {
			try {
				std::filesystem::path exePath = std::filesystem::canonical(exePathBuf);
				std::filesystem::path exeDir = exePath.parent_path();
				std::filesystem::path shareDir = exeDir / ".." / DATA_DIR_NAME / "quadrate" / moduleName;
				result = tryGetModuleFilesFromDir(shareDir.string());
				if (!result.empty()) {
					return result;
				}
			} catch (...) {
				// Ignore errors resolving path
			}
		}
	}

	// Try 6: Standard library directories relative to current directory (development fallback)
	result = tryGetModuleFilesFromDir("lib/qd" + moduleName + "/qd/" + moduleName);
	if (!result.empty()) {
		return result;
	}

	// Try 7: $HOME/quadrate directory
	const char* home = getenv("HOME");
	if (home) {
		result = tryGetModuleFilesFromDir(std::string(home) + "/quadrate/" + moduleName);
		if (!result.empty()) {
			return result;
		}

		// Also check quadpm's modules/_namespaces directory (symlinks to installed modules)
		result = tryGetModuleFilesFromDir(std::string(home) + "/quadrate/modules/_namespaces/" + moduleName);
		if (!result.empty()) {
			return result;
		}
	}

	// Try 8: System-wide installation
#ifdef __HAIKU__
	result = tryGetModuleFilesFromDir("/boot/system/data/quadrate/" + moduleName);
#else
	result = tryGetModuleFilesFromDir("/usr/share/quadrate/" + moduleName);
#endif
	if (!result.empty()) {
		return result;
	}

	return {}; // Not found
}

// Load dependencies from qd.json and return include paths
std::vector<std::string> loadDependenciesFromManifest(const std::string& manifestDir) {
	std::vector<std::string> includePaths;

	std::string manifestPath = manifestDir + "/qd.json";
	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return includePaths;
	}

	json_t* dependencies = json_object_get(root, "dependencies");
	if (dependencies && json_is_object(dependencies)) {
		const char* depName;
		json_t* depValue;
		json_object_foreach(dependencies, depName, depValue) {
			std::string resolved;

			if (json_is_string(depValue)) {
				// Simple form: "name": "url" or "name": "../path"
				resolved = json_string_value(depValue);
			} else if (json_is_object(depValue)) {
				// Expanded form: { "url": "..." }
				json_t* url = json_object_get(depValue, "url");
				if (url && json_is_string(url)) {
					resolved = json_string_value(url);
				}
			}

			if (resolved.empty()) {
				continue;
			}

			// Check if it's a local path
			bool isPath =
					(resolved.size() > 0 && (resolved[0] == '/' || resolved[0] == '.' ||
													(resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/')));

			if (isPath) {
				resolved = expandTilde(resolved);
				if (resolved.size() > 0 && resolved[0] != '/') {
					resolved = manifestDir + "/" + resolved;
				}
				try {
					resolved = std::filesystem::weakly_canonical(resolved).string();
				} catch (...) {
				}
				if (std::filesystem::exists(resolved)) {
					includePaths.push_back(resolved);
				}
			} else {
				// Git URL - check if installed in packages dir via _namespaces symlink
				const char* home = getenv("HOME");
				if (home) {
					std::string namespacePath = std::string(home) + "/quadrate/modules/_namespaces/" + depName;
					if (std::filesystem::exists(namespacePath)) {
						try {
							std::string resolvedPath = std::filesystem::canonical(namespacePath).string();
							includePaths.push_back(resolvedPath);
						} catch (...) {
							// If canonical fails, try the path anyway
							includePaths.push_back(namespacePath);
						}
					}
				}
			}
		}
	}

	json_decref(root);
	return includePaths;
}

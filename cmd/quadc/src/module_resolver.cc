#include "module_resolver.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

// Platform abstractions
extern "C" {
#include "src/platform/exe_path_platform.h"
}

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
		return std::string(xdgDataHome) + "/quadrate/packages";
	}

	// Default to ~/quadrate/packages
	const char* home = std::getenv("HOME");
	if (home) {
		return std::string(home) + "/quadrate/packages";
	}

	return ""; // No packages directory available
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
	} else {
		// Module directory import (original behavior)
		// Try 1: Local path (relative to source file)
		std::string localPath = sourceDir + "/" + moduleName + "/module.qd";
		if (std::filesystem::exists(localPath)) {
			return localPath;
		}

		// Try 2: Include paths from -I flags
		for (const auto& includePath : g_moduleIncludePaths) {
			std::string expandedPath = expandTilde(includePath);

			// Check if the include path IS the module directory (contains module.qd directly)
			// This handles: -I /path/to/mymodule where mymodule/module.qd exists
			// and quadrate.toml says name = "mymodule"
			std::string directModulePath = expandedPath + "/module.qd";
			if (std::filesystem::exists(directModulePath)) {
				// Check if this directory's quadrate.toml matches the module name
				std::string manifestPath = expandedPath + "/quadrate.toml";
				if (std::filesystem::exists(manifestPath)) {
					std::ifstream file(manifestPath);
					if (file.is_open()) {
						std::string line;
						bool inPackageSection = false;
						while (std::getline(file, line)) {
							line.erase(0, line.find_first_not_of(" \t\r\n"));
							line.erase(line.find_last_not_of(" \t\r\n") + 1);
							if (line == "[module]" || line == "[package]") {
								inPackageSection = true;
								continue;
							}
							if (!line.empty() && line[0] == '[') {
								inPackageSection = false;
								continue;
							}
							if (inPackageSection) {
								size_t eqPos = line.find('=');
								if (eqPos != std::string::npos) {
									std::string key = line.substr(0, eqPos);
									std::string value = line.substr(eqPos + 1);
									key.erase(0, key.find_first_not_of(" \t"));
									key.erase(key.find_last_not_of(" \t") + 1);
									value.erase(0, value.find_first_not_of(" \t"));
									value.erase(value.find_last_not_of(" \t") + 1);
									if (key == "name") {
										if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"') {
											value = value.substr(1, value.size() - 2);
										}
										if (value == moduleName) {
											return directModulePath;
										}
										break;
									}
								}
							}
						}
					}
				}
			}

			// Check for module as subdirectory of include path
			std::string includedPath = expandedPath + "/" + moduleName + "/module.qd";
			if (std::filesystem::exists(includedPath)) {
				return includedPath;
			}
		}

		// Try 3: Third-party packages directory (installed via quadpm)
		std::string packagePath = findLatestPackageVersion(moduleName);
		if (!packagePath.empty()) {
			std::string moduleFile = packagePath + "/module.qd";
			if (std::filesystem::exists(moduleFile)) {
				return moduleFile;
			}
		}

		// Try 3: QUADRATE_ROOT environment variable
		const char* quadrateRoot = getenv("QUADRATE_ROOT");
		if (quadrateRoot) {
			std::string rootPath = std::string(quadrateRoot) + "/" + moduleName + "/module.qd";
			if (std::filesystem::exists(rootPath)) {
				return rootPath;
			}
		}

		// Try 4: Standard library directories relative to current directory (for development)
		std::string stdLibPath = "lib/qd" + moduleName + "/qd/" + moduleName + "/module.qd";
		if (std::filesystem::exists(stdLibPath)) {
			return stdLibPath;
		}

		// Try 5: Standard library relative to executable (for installed binaries)
		// Get executable path and look for ../share/quadrate/<module>/module.qd
		{
			char exePathBuf[4096];
			int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
			if (len > 0 && static_cast<size_t>(len) < sizeof(exePathBuf)) {
				try {
					std::filesystem::path exePath = std::filesystem::canonical(exePathBuf);
					std::filesystem::path exeDir = exePath.parent_path();
					std::filesystem::path sharePath = exeDir / ".." / "share" / "quadrate" / moduleName / "module.qd";
					if (std::filesystem::exists(sharePath)) {
						return sharePath.string();
					}
				} catch (...) {
					// Ignore errors resolving path
				}
			}
		}

		// Try 6: $HOME/quadrate directory
		const char* home = getenv("HOME");
		if (home) {
			std::string homePath = std::string(home) + "/quadrate/" + moduleName + "/module.qd";
			if (std::filesystem::exists(homePath)) {
				return homePath;
			}
		}

		// Try 7: System-wide installation
		std::string systemPath = "/usr/share/quadrate/" + moduleName + "/module.qd";
		if (std::filesystem::exists(systemPath)) {
			return systemPath;
		}
	}

	return ""; // Not found
}

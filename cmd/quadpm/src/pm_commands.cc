// pm_commands.cc - Command implementations for quadpm

#include "pm_impl.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>

namespace fs = std::filesystem;

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

	// Check if module has any .qd files
	if (hasQuadrateFiles(finalDir)) {
		std::cout << COLOR_GREEN << "  ✓ Found .qd files" << COLOR_RESET << "\n";
	} else {
		std::cout << COLOR_YELLOW << "  ⚠ Warning: no .qd files found at root" << COLOR_RESET << "\n";
		std::cout << "    Module should contain at least one .qd file\n";
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

// Check if C source files use Quadrate name mangling convention (usr_* functions)
// Returns true if any function matching pattern "usr_<moduleName>_" is found
bool usesQuadrateNaming(const std::vector<std::string>& cFiles, const std::string& moduleName) {
	std::string pattern = "usr_" + moduleName + "_";

	for (const auto& file : cFiles) {
		std::ifstream f(file);
		if (!f.is_open()) continue;

		std::string line;
		while (std::getline(f, line)) {
			if (line.find(pattern) != std::string::npos) {
				// Found the pattern
				return true;
			}
		}
	}
	return false;
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

	// Auto-detect naming convention by checking C source files
	// If C files use usr_<module>_* functions, use libqd* prefix for name mangling
	// Otherwise use lib* prefix for plain C functions
	bool useQdPrefix = usesQuadrateNaming(cFiles, moduleName);
	std::string libName = useQdPrefix ? ("libqd" + moduleName) : ("lib" + moduleName);
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

	// Recursively find packages (directories containing .qd files or qd.json)
	for (const auto& entry : fs::recursive_directory_iterator(modulesDir)) {
		if (!entry.is_directory()) {
			continue;
		}
		std::string dirname = entry.path().filename().string();
		// Skip _namespaces directory
		if (dirname == "_namespaces") {
			continue;
		}
		// Check if this looks like a package directory (has @ in name and contains .qd files or qd.json)
		size_t atPos = dirname.find('@');
		if (atPos == std::string::npos) {
			continue;
		}
		std::string dirPath = entry.path().string();
		std::string manifestFile = dirPath + "/qd.json";
		if (!hasQuadrateFiles(dirPath) && !fs::exists(manifestFile)) {
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

		// Verify it has at least one .qd file
		if (!hasQuadrateFiles(resolvedPath)) {
			std::cout << COLOR_RED << "✗ Not a module (no .qd files found): " << resolvedPath << COLOR_RESET << "\n";
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
int installDependencies(bool frozen) {
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

// Remove an installed module
int removeModule(const std::string& targetModuleName) {
	if (targetModuleName.empty()) {
		std::cerr << COLOR_RED << "Error: Module name required" << COLOR_RESET << "\n";
		std::cerr << "Usage: quadpm remove <name>\n";
		return 1;
	}

	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		std::cerr << COLOR_RED << "Error: No modules installed" << COLOR_RESET << "\n";
		return 1;
	}

	// Find the module directory
	std::string foundPath;
	std::string foundName;
	std::string foundHostPath;

	// Recursively search for matching package
	for (const auto& entry : fs::recursive_directory_iterator(modulesDir)) {
		if (!entry.is_directory()) {
			continue;
		}

		std::string dirname = entry.path().filename().string();

		// Skip _namespaces directory
		if (dirname == "_namespaces") {
			continue;
		}

		// Check if this looks like a package directory (has @ in name)
		size_t atPos = dirname.find('@');
		if (atPos == std::string::npos) {
			continue;
		}

		// Must have .qd files or qd.json to be a valid package
		std::string dirPath = entry.path().string();
		std::string manifestFile = dirPath + "/qd.json";
		if (!hasQuadrateFiles(dirPath) && !fs::exists(manifestFile)) {
			continue;
		}

		std::string moduleName = dirname.substr(0, atPos);

		// Get host/user/repo from path relative to modules dir
		std::string relativePath = dirPath.substr(modulesDir.size() + 1); // +1 for '/'
		size_t relAtPos = relativePath.find('@');
		std::string hostPath = (relAtPos != std::string::npos) ? relativePath.substr(0, relAtPos) : relativePath;

		// Match by:
		// 1. Full directory name (e.g., "json@master")
		// 2. Just the module name (e.g., "json")
		// 3. Full hostPath (e.g., "github.com/quadrate-language/json")
		// 4. Namespace from qd.json
		std::string manifestNamespace;
		std::string manifestModuleName;
		if (fs::exists(manifestFile)) {
			manifestNamespace = parseNamespace(manifestFile);
			manifestModuleName = parseModuleName(manifestFile);
		}

		bool matches = (dirname == targetModuleName || moduleName == targetModuleName || hostPath == targetModuleName ||
				(!manifestNamespace.empty() && manifestNamespace == targetModuleName) ||
				(!manifestModuleName.empty() && manifestModuleName == targetModuleName));

		if (matches) {
			foundPath = dirPath;
			foundName = dirname;
			foundHostPath = hostPath;
			break;
		}
	}

	if (foundPath.empty()) {
		std::cerr << COLOR_RED << "Error: Module '" << targetModuleName << "' not found" << COLOR_RESET << "\n";
		std::cerr << "Use 'quadpm list' to see installed modules\n";
		return 1;
	}

	// Get module namespace before removing (for symlink cleanup)
	std::string manifestPath = foundPath + "/qd.json";
	std::string namespaceName;
	if (fs::exists(manifestPath)) {
		namespaceName = parseNamespace(manifestPath);
		if (namespaceName.empty()) {
			namespaceName = parseModuleName(manifestPath);
		}
	}
	if (namespaceName.empty()) {
		size_t lastSlash = foundHostPath.find_last_of('/');
		namespaceName = (lastSlash != std::string::npos) ? foundHostPath.substr(lastSlash + 1) : foundHostPath;
		// Remove @version if present
		size_t atPos = namespaceName.find('@');
		if (atPos != std::string::npos) {
			namespaceName = namespaceName.substr(0, atPos);
		}
	}

	std::cout << COLOR_CYAN << "Removing " << COLOR_BOLD << foundHostPath << COLOR_RESET << "...\n";

	// Remove the module directory
	try {
		fs::remove_all(foundPath);
		std::cout << COLOR_GREEN << "  ✓ Removed " << COLOR_RESET << foundPath << "\n";
	} catch (const std::exception& e) {
		std::cerr << COLOR_RED << "  ✗ Failed to remove directory: " << e.what() << COLOR_RESET << "\n";
		return 1;
	}

	// Clean up parent directories if empty (for Go-style paths)
	fs::path parentPath = fs::path(foundPath).parent_path();
	while (parentPath.string().size() > modulesDir.size()) {
		try {
			if (fs::is_empty(parentPath)) {
				fs::remove(parentPath);
			} else {
				break;
			}
		} catch (const std::exception&) {
			break;
		}
		parentPath = parentPath.parent_path();
	}

	// Remove namespace symlink if it points to this module
	std::string namespacesDir = getNamespacesDir();
	std::string symlinkPath = namespacesDir + "/" + namespaceName;

	if (fs::exists(symlinkPath) || fs::is_symlink(symlinkPath)) {
		try {
			// Check if symlink points to the removed module
			std::string target = fs::read_symlink(symlinkPath).string();
			// Extract just the final directory name from the relative target
			// Target is like "../github.com/user/repo@version"
			if (target.find(foundName) != std::string::npos) {
				fs::remove(symlinkPath);
				std::cout << COLOR_GREEN << "  ✓ Removed namespace '" << namespaceName << "'" << COLOR_RESET << "\n";
			}
		} catch (const std::exception&) {
			// Symlink might be broken or inaccessible
			try {
				fs::remove(symlinkPath);
				std::cout << COLOR_GREEN << "  ✓ Cleaned up broken namespace symlink" << COLOR_RESET << "\n";
			} catch (const std::exception&) {
				// Ignore cleanup failures
			}
		}
	}

	std::cout << "\n" << COLOR_GREEN << "Module removed successfully!" << COLOR_RESET << "\n";
	return 0;
}

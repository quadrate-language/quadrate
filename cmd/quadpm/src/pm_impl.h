#ifndef QUADPM_IMPL_H
#define QUADPM_IMPL_H

#include "git_ref.h"
#include "semver.h"
#include <map>
#include <set>
#include <string>
#include <vector>

// ANSI color codes for pretty output
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_CYAN "\033[36m"

// Lockfile format version
extern const int LOCKFILE_VERSION;

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

// Result of installing a single dependency
struct InstallResult {
	std::string modulePath; // Path to installed module
	std::string actualRef;	// Actual ref installed (may differ from requested)
	std::string commitHash; // Git commit hash
	bool alreadyInstalled;	// Was already installed
	bool success;
};

// ============================================================================
// Utility functions (pm_utils.cc)
// ============================================================================

// Get home directory
std::string getHomeDir();

// Get modules directory path
std::string getModulesDir();

// Check if a directory contains any .qd files
bool hasQuadrateFiles(const std::string& dirPath);

// Execute a command safely using fork/exec (no shell interpretation)
// Returns exit status, captures stdout in 'output' if provided
int execCommandSafe(const std::vector<std::string>& args, std::string* output = nullptr, bool showOutput = false);

// Execute command and capture output (throws on failure)
std::string execCommand(const std::vector<std::string>& args);

// Execute command showing output in real-time
int execCommandLive(const std::vector<std::string>& args);

// Check if a command exists in PATH
bool commandExists(const std::string& cmd);

// Get the path where a module would be installed
std::string getModuleInstallPath(const std::string& moduleName, const std::string& ref);

// Get the path to a local module's directory
std::string resolveLocalPath(const std::string& path, const std::string& basePath);

// Get current platform name for platform-specific config
std::string getPlatformName();

// Get the installed directory name for a module@ref
std::string getInstalledDirName(const std::string& hostPath, const std::string& ref);

// Get the namespaces directory path
std::string getNamespacesDir();

// Find the library directory for a dependency by name
std::string findDependencyLibDir(const std::string& depName, const std::string& currentModuleDir = "");

// Collect transitive native dependencies from a module's qd.json
void collectTransitiveDeps(const std::string& manifestPath, const std::string& moduleDir,
		std::vector<std::string>& staticLibs, std::vector<std::string>& linkFlags, std::set<std::string>& visited);

// ============================================================================
// Manifest parsing functions (pm_manifest.cc)
// ============================================================================

// Parse native section from qd.json
NativeConfig parseNativeConfig(const std::string& manifestPath);

// Parse module name from qd.json
std::string parseModuleName(const std::string& manifestPath);

// Parse namespace from qd.json
std::string parseNamespace(const std::string& manifestPath);

// Parse dependencies from qd.json
std::vector<Dependency> parseDependencies(const std::string& manifestPath);

// Create or update a namespace symlink
bool createNamespaceSymlink(const std::string& namespaceName, const std::string& targetPath);

// Ensure namespace symlink exists for an installed module
// Parses qd.json to determine namespace, creates symlink if needed
// Returns true on success
bool ensureNamespaceSymlink(const std::string& installedDir, const std::string& installedDirName,
							const std::string& fallbackModuleName);

// Get all namespaces that point to a given hostPath
std::vector<std::string> getNamespacesForPackage(const std::string& hostPathWithRef);

// Find which packages provide a given namespace
std::vector<std::string> findPackagesWithNamespace(const std::string& namespaceName);

// Read lockfile (qd.lock)
std::vector<LockedDependency> readLockfile(const std::string& lockfilePath);

// Write lockfile (qd.lock)
bool writeLockfile(const std::string& lockfilePath, const std::vector<LockedDependency>& locked);

// Get commit hash for an installed module
std::string getModuleCommitHash(const std::string& moduleDir);

// ============================================================================
// Command implementations (pm_commands.cc)
// ============================================================================

// Clone a Git repository to the modules directory
std::string gitClone(const GitRef& gitRef);

// Check if C source files use Quadrate name mangling convention
bool usesQuadrateNaming(const std::vector<std::string>& cFiles, const std::string& moduleName);

// Compile C sources in a module directory
bool compileCsources(const std::string& moduleDir, const std::string& moduleName, const NativeConfig& nativeConfig);

// List installed modules
void listModules();

// Update a single module by running git pull
bool updateModule(const std::string& moduleDir);

// Build a module in the current directory
int buildModule();

// Compute SHA256 hash of a file or directory
std::string computeSha256(const std::string& path);

// List all tags from a remote git repository
std::vector<std::pair<std::string, std::string>> listRemoteTags(const std::string& gitUrl);

// Check if a dependency version is a semver range
bool isSemVerRange(const std::string& version);

// Resolve a semver range to a specific tag
std::string resolveSemVerRange(const std::string& gitUrl, const std::string& range);

// Install a single dependency
InstallResult installSingleDependency(const Dependency& dep, const std::string& basePath,
		const std::map<std::string, LockedDependency>& lockedByName, bool frozen);

// Install dependencies from qd.json
int installDependencies(bool frozen = false);

// Generate/update lockfile without installing
int generateLockfile();

// Update installed modules
int updateModules(const std::string& targetModuleName);

// Remove an installed module
int removeModule(const std::string& targetModuleName);

// Show outdated packages with available updates
int showOutdated();

// Version conflict information
struct VersionConflict {
	std::string packageName;
	std::vector<std::pair<std::string, std::string>> requirements; // (requirer, version_range)
};

// Check for version conflicts in dependencies
// Returns empty vector if no conflicts, otherwise list of conflicts
std::vector<VersionConflict> detectVersionConflicts(
		const std::vector<Dependency>& deps, const std::string& basePath, std::set<std::string>& visited);

#endif // QUADPM_IMPL_H

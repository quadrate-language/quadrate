// pm_commands.cc - Command implementations for quadpm

#include "pm_impl.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <tuple>

namespace fs = std::filesystem;

static std::string moduleNamespace(const std::string& moduleDir, const std::string& fallbackModuleName) {
	std::string manifestPath = moduleDir + "/qd.json";
	std::string manifestNamespace = parseNamespace(manifestPath);
	if (!manifestNamespace.empty()) {
		return manifestNamespace;
	}
	std::string manifestModuleName = parseModuleName(manifestPath);
	if (!manifestModuleName.empty()) {
		return manifestModuleName;
	}
	return fallbackModuleName;
}

static bool hasCSources(const std::string& moduleDir) {
	std::string srcDir = moduleDir + "/src";
	if (!fs::is_directory(srcDir)) {
		return false;
	}
	for (const auto& entry : fs::recursive_directory_iterator(srcDir)) {
		if (entry.is_regular_file() && entry.path().extension() == ".c") {
			return true;
		}
	}
	return false;
}

bool isInstallComplete(const std::string& moduleDir) {
	if (!hasCSources(moduleDir)) {
		return true;
	}
	std::string libDir = moduleDir + "/lib";
	if (!fs::is_directory(libDir)) {
		return false;
	}
	for (const auto& entry : fs::directory_iterator(libDir)) {
		std::string filename = entry.path().filename().string();
		if (filename.size() > 9 && filename.substr(filename.size() - 9) == "_static.a") {
			return true;
		}
	}
	return false;
}

static void removeStaging(const std::string& stagingDir) {
	std::error_code ec;
	fs::remove_all(stagingDir, ec);
}

// Move a fully built staging checkout to its final location, replacing what is there
static bool moveIntoPlace(const std::string& stagingDir, const std::string& targetDir) {
	std::error_code ec;
	fs::create_directories(fs::path(targetDir).parent_path(), ec);
	if (ec) {
		pmError("could not create " + fs::path(targetDir).parent_path().string() + ": " + ec.message());
		return false;
	}

	std::string trashDir;
	if (fs::exists(targetDir) || fs::is_symlink(targetDir)) {
		trashDir = makeStagingDir();
		if (trashDir.empty()) {
			pmError("could not create a staging directory in " + getModulesDir());
			return false;
		}
		fs::rename(targetDir, trashDir + "/old", ec);
		if (ec) {
			pmError("could not replace " + targetDir + ": " + ec.message());
			removeStaging(trashDir);
			return false;
		}
	}

	fs::rename(stagingDir, targetDir, ec);
	if (ec) {
		pmError("could not move the checkout to " + targetDir + ": " + ec.message());
		if (!trashDir.empty()) {
			std::error_code restoreEc;
			fs::rename(trashDir + "/old", targetDir, restoreEc);
			removeStaging(trashDir);
		}
		return false;
	}

	if (!trashDir.empty()) {
		removeStaging(trashDir);
	}
	return true;
}

static bool registerNamespace(const std::string& targetDir, const std::string& installedDirName, const GitRef& gitRef,
		std::string* registeredNamespace) {
	std::string namespaceName = moduleNamespace(targetDir, gitRef.moduleName);
	std::string relativeTarget = "../" + installedDirName;

	std::error_code ec;
	fs::path self = fs::weakly_canonical(targetDir, ec);
	std::string modulesDir = getModulesDir();
	std::vector<std::string> existingPackages = findPackagesWithNamespace(namespaceName);
	bool hasConflict = false;
	for (const auto& pkg : existingPackages) {
		std::error_code pkgEc;
		if (fs::weakly_canonical(modulesDir + "/" + pkg, pkgEc) != self) {
			hasConflict = true;
			pmWarn("namespace '" + namespaceName + "' is also claimed by: " + pkg);
		}
	}

	if (createNamespaceSymlink(namespaceName, relativeTarget)) {
		std::cout << COLOR_GREEN << "  ✓ Namespace '" << namespaceName << "' registered" << COLOR_RESET << "\n";
		if (hasConflict) {
			std::cout << COLOR_YELLOW << "    Note: Use full path in 'use' directive to disambiguate" << COLOR_RESET
					  << "\n";
			std::cout << COLOR_CYAN << "    use " << gitRef.hostPath << COLOR_RESET << "\n";
		}
		if (registeredNamespace) {
			*registeredNamespace = namespaceName;
		}
		return true;
	}

	std::cout << COLOR_YELLOW << "  ⚠ Namespace '" << namespaceName << "' already registered to different package"
			  << COLOR_RESET << "\n";
	std::cout << COLOR_CYAN << "    Use full path in 'use' directive: use " << gitRef.hostPath << COLOR_RESET << "\n";
	return false;
}

// Clone a Git repository to the modules directory
// Uses Go-style paths: host/user/repo@version
// Returns the actual module name (from manifest if present, otherwise from git ref)
// Returns empty string on failure
std::string gitClone(const GitRef& gitRef, const CloneOptions& options) {
	std::string modulesDir = getModulesDir();

	// Reject URLs/refs that git would interpret as options (argument injection).
	if (!isSafeGitArgument(gitRef.url) || !isSafeGitArgument(gitRef.ref)) {
		pmError("refusing unsafe git URL or ref: " + gitRef.url + (gitRef.ref.empty() ? "" : ("@" + gitRef.ref)));
		return "";
	}

	// Use hostPath for Go-style directory structure
	std::string installedDirName = getInstalledDirName(gitRef.hostPath, gitRef.ref);
	if (gitRef.hostPath.empty() || !isSafeInstalledDirName(installedDirName)) {
		pmError("refusing to install outside the modules directory: " + gitRef.url +
				(gitRef.ref.empty() ? "" : ("@" + gitRef.ref)));
		return "";
	}
	std::string targetDir = modulesDir + "/" + installedDirName;

	// Create modules directory if it doesn't exist
	fs::create_directories(modulesDir);

	bool replace = options.replaceExisting;
	if (fs::exists(targetDir) && !replace) {
		if (isInstallComplete(targetDir)) {
			std::cout << COLOR_YELLOW << "Module already exists: " << COLOR_RESET << targetDir << "\n";
			std::cout << COLOR_CYAN << "Use 'quadpm update' to update it" << COLOR_RESET << "\n";
			std::string namespaceName = moduleNamespace(targetDir, gitRef.moduleName);
			if (createNamespaceSymlink(namespaceName, "../" + installedDirName) && options.registeredNamespace) {
				*options.registeredNamespace = namespaceName;
			}
			return gitRef.moduleName;
		}
		pmWarn("the install at " + targetDir + " was never built; reinstalling");
		replace = true;
	}

	std::cout << COLOR_CYAN << "Fetching " << COLOR_BOLD << gitRef.hostPath << COLOR_RESET << COLOR_CYAN << " @ "
			  << (gitRef.ref.empty() ? "default branch" : gitRef.ref) << "..." << COLOR_RESET << "\n";
	std::cout << "  → Cloning " << gitRef.url << "\n";

	std::string stagingDir = makeStagingDir();
	if (stagingDir.empty()) {
		pmError("could not create a staging directory in " + modulesDir);
		return "";
	}

	// Clone with --depth 1 for faster download. The "--" separator ensures the
	// URL and target are always treated as positional arguments.
	std::vector<std::string> cloneCmd = {"git", "clone", "--depth", "1"};
	if (!gitRef.ref.empty()) {
		cloneCmd.push_back("--branch");
		cloneCmd.push_back(gitRef.ref);
	}
	cloneCmd.push_back("--");
	cloneCmd.push_back(gitRef.url);
	cloneCmd.push_back(stagingDir);
	int result = execCommandLive(cloneCmd);

	if (result != 0) {
		pmError("failed to clone repository");
		removeStaging(stagingDir);
		return "";
	}

	if (!options.expectedCommit.empty()) {
		std::string commitHash = getModuleCommitHash(stagingDir);
		if (commitHash != options.expectedCommit) {
			std::cerr << "  " << COLOR_RED << "✗ " << options.mismatchLabel << COLOR_RESET << "\n";
			std::cerr << "    Expected: " << options.expectedCommit << "\n";
			std::cerr << "    Got:      " << commitHash << "\n";
			removeStaging(stagingDir);
			return "";
		}
	}

	std::string actualModuleName = gitRef.moduleName;

	// Check if module has any .qd files
	if (hasQuadrateFiles(stagingDir)) {
		std::cout << COLOR_GREEN << "  ✓ Found .qd files" << COLOR_RESET << "\n";
	} else {
		pmWarn("no .qd files found at the module root; a module should contain at least one .qd file");
	}

	// Before the src/ check: a prebuild script may be what puts src/ there
	if (!runPrebuild(stagingDir, actualModuleName)) {
		removeStaging(stagingDir);
		return "";
	}

	// Check for C source files and compile if found
	std::string srcDir = stagingDir + "/src";
	if (fs::exists(srcDir) && fs::is_directory(srcDir)) {
		std::cout << COLOR_GREEN << "  ✓ Found src/ directory" << COLOR_RESET << "\n";
		// Parse native config for link libraries
		NativeConfig nativeConfig = parseNativeConfig(stagingDir + "/qd.json");
		if (!compileCsources(stagingDir, actualModuleName, nativeConfig)) {
			pmError("failed to build native sources for '" + actualModuleName + "'; nothing was installed");
			removeStaging(stagingDir);
			return "";
		}
	}

	if (!moveIntoPlace(stagingDir, targetDir)) {
		removeStaging(stagingDir);
		return "";
	}

	std::cout << COLOR_GREEN << "  ✓ Installed to " << COLOR_RESET << targetDir << "\n";

	registerNamespace(targetDir, installedDirName, gitRef, options.registeredNamespace);

	return actualModuleName;
}

// Ensure namespace symlink exists for an installed module
// Parses qd.json to determine namespace, creates symlink if needed
// Returns true on success
bool ensureNamespaceSymlink(
		const std::string& installedDir, const std::string& installedDirName, const std::string& fallbackModuleName) {
	return createNamespaceSymlink(moduleNamespace(installedDir, fallbackModuleName), "../" + installedDirName);
}

// Check if C source files use Quadrate name mangling convention (usr_* functions)
// Returns true if any function matching pattern "usr_<moduleName>_" is found
bool usesQuadrateNaming(const std::vector<std::string>& cFiles, const std::string& moduleName) {
	std::string pattern = "usr_" + moduleName + "_";

	for (const auto& file : cFiles) {
		std::ifstream f(file);
		if (!f.is_open()) {
			continue;
		}

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

namespace {
	bool gScriptsEnabled = true;
}

void setScriptsEnabled(bool enabled) {
	gScriptsEnabled = enabled;
}

bool scriptsEnabled() {
	return gScriptsEnabled;
}

// Run a module's prebuild script, if it declares one.
//
// This exists because quadpm compiles a module's C as part of installing it,
// which leaves no moment for the user to prepare anything first -- a module
// that vendors an upstream tree, say, has nowhere else to fetch it.
//
// The command is echoed rather than gated. Installing a module already means
// compiling its C and linking it into the program that will run it, so a script
// does not decide whether someone else's code runs, only when. What it does
// change is that it runs at install time, which --no-scripts is there to
// prevent for the cases that care: auditing a module, or mirroring one.
bool runPrebuild(const std::string& moduleDir, const std::string& moduleName) {
	ScriptsConfig scripts = parseScriptsConfig(moduleDir + "/qd.json");
	if (scripts.prebuild.empty()) {
		return true;
	}

	if (!scriptsEnabled()) {
		std::cout << COLOR_YELLOW << "  ⊘ Skipping prebuild script" << COLOR_RESET
				  << " (--no-scripts): " << scripts.prebuild << "\n";
		return true;
	}

	std::cout << "  → Running prebuild for " << moduleName << ": " << scripts.prebuild << "\n";

	int result = execShellIn(scripts.prebuild, moduleDir);
	if (result != 0) {
		std::cerr << COLOR_RED << "  ✗ Prebuild script failed" << COLOR_RESET << " (exit " << result << ")\n";
		return false;
	}

	std::cout << COLOR_GREEN << "  ✓ Prebuild finished" << COLOR_RESET << "\n";
	return true;
}

static bool isPlainToken(const std::string& s, const std::string& extra) {
	if (s.empty()) {
		return false;
	}
	for (char c : s) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && extra.find(c) == std::string::npos) {
			return false;
		}
	}
	return true;
}

// Check one native.cflags entry against the allow-list. On success, resolved
// holds the flag to pass to the compiler.
static bool checkCflag(
		const std::string& flag, const std::string& moduleDir, std::string& resolved, std::string& reason) {
	static const std::set<std::string> exactFlags = {"-g", "-g0", "-g1", "-g2", "-g3", "-ggdb", "-pthread", "-w",
			"-pedantic", "-pedantic-errors", "-ansi", "-O", "-O0", "-O1", "-O2", "-O3", "-Os", "-Oz", "-Og", "-Ofast",
			"-fPIC", "-fpic", "-fPIE", "-fpie", "-fno-common", "-fcommon", "-fwrapv", "-fno-strict-aliasing",
			"-fstrict-aliasing", "-fno-strict-overflow", "-fno-omit-frame-pointer", "-fomit-frame-pointer",
			"-fsigned-char", "-funsigned-char", "-fno-builtin", "-ffast-math", "-fno-fast-math", "-fno-math-errno",
			"-fms-extensions", "-fno-exceptions", "-fexceptions", "-fno-asynchronous-unwind-tables",
			"-ffunction-sections", "-fdata-sections", "-fno-stack-protector", "-fstack-protector",
			"-fstack-protector-strong", "-fstack-protector-all", "-fno-plt", "-fno-delete-null-pointer-checks",
			"-fno-inline", "-finline-functions", "-funroll-loops", "-fno-unroll-loops", "-fno-lto"};
	static const std::vector<std::string> valuePrefixes = {"-std=", "-fvisibility=", "-fdiagnostics-color=",
			"-ffp-contract=", "-march=", "-mtune=", "-mcpu=", "-mfpu=", "-mfloat-abi="};

	resolved = flag;
	if (exactFlags.count(flag) > 0) {
		return true;
	}

	if (flag.size() > 2 && (flag.compare(0, 2, "-D") == 0 || flag.compare(0, 2, "-U") == 0)) {
		return true;
	}

	if (flag.size() > 2 && flag.compare(0, 2, "-W") == 0) {
		if (flag.find(',') != std::string::npos) {
			reason = "-W options that forward arguments to other tools are not accepted";
			return false;
		}
		if (!isPlainToken(flag.substr(2), "-=_+.")) {
			reason = "not a warning option";
			return false;
		}
		return true;
	}

	for (const auto& prefix : valuePrefixes) {
		if (flag.size() > prefix.size() && flag.compare(0, prefix.size(), prefix) == 0) {
			if (!isPlainToken(flag.substr(prefix.size()), "-_+.")) {
				reason = "unexpected characters in the value";
				return false;
			}
			return true;
		}
	}

	if (flag.size() > 2 && flag.compare(0, 2, "-m") == 0 && flag.compare(0, 6, "-mllvm") != 0 &&
			isPlainToken(flag.substr(2), "-_.")) {
		return true;
	}

	if (flag.size() > 2 && flag.compare(0, 2, "-I") == 0) {
		fs::path path(flag.substr(2));
		fs::path normal = path.lexically_normal();
		if (path.is_absolute() || normal.empty() || *normal.begin() == ".." || flag[2] == '-') {
			reason = "include paths must be relative and inside the module";
			return false;
		}
		resolved = "-I" + (fs::path(moduleDir) / normal).lexically_normal().string();
		return true;
	}

	reason = "not on the allow-list";
	return false;
}

static bool isSafeLinkName(const std::string& lib) {
	return !lib.empty() && lib[0] != '-' && isPlainToken(lib, "-_.+:");
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
	std::sort(cFiles.begin(), cFiles.end());

	std::vector<std::string> extraFlags;
	for (const auto& flag : nativeConfig.cflags) {
		std::string resolved;
		std::string reason;
		if (!checkCflag(flag, moduleDir, resolved, reason)) {
			pmError("native.cflags: '" + flag + "' is not allowed: " + reason);
			std::cerr << "Allowed: -D, -U, -I<path inside the module>, -W<warning>, -O<level>, -std=, -g, "
						 "-pthread, -m<target option> and a fixed set of -f code-generation options.\n";
			return false;
		}
		extraFlags.push_back(resolved);
	}
	for (const auto& lib : nativeConfig.link) {
		if (!isSafeLinkName(lib)) {
			pmError("native.link: '" + lib + "' is not a library name");
			return false;
		}
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

	// Build include paths. (No blanket -I/usr/include: the compiler already
	// searches its default system include paths, and adding it explicitly only
	// widens the ambient header surface.)
	std::vector<std::string> includePaths;
	// Module's own include directory
	std::string moduleIncDir = moduleDir + "/include";
	if (fs::exists(moduleIncDir)) {
		includePaths.push_back("-I" + moduleIncDir);
	}
	// Module's src directory (for platform headers)
	includePaths.push_back("-I" + srcDir);
	if (fs::exists("dist/include/quadrate/rt")) {
		includePaths.push_back("-Idist/include");
	}
	const char* root_env = std::getenv("QUADRATE_ROOT");
	if (root_env) {
		fs::path rootPath(root_env);
		fs::path includePath = rootPath / "dist" / "include";
		if (fs::exists(includePath / "quadrate" / "rt")) {
			includePaths.push_back("-I" + includePath.string());
		}
	}
	const char* libDir_env = std::getenv("QUADRATE_LIBDIR");
	if (libDir_env) {
		fs::path libPath(libDir_env);
		fs::path includePath = libPath.parent_path() / "include";
		if (fs::exists(includePath / "quadrate" / "rt")) {
			includePaths.push_back("-I" + includePath.string());
		}
	}

	// Compile to object files
	std::vector<std::string> objFiles;
	std::set<std::string> objNames;
	bool compileFailed = false;

	for (const auto& cFile : cFiles) {
		fs::path relative = fs::path(cFile).lexically_relative(srcDir);
		relative.replace_extension("");
		std::string objName = relative.generic_string();
		std::replace(objName.begin(), objName.end(), '/', '_');
		std::string uniqueName = objName;
		for (int n = 2; !objNames.insert(uniqueName).second; n++) {
			uniqueName = objName + "_" + std::to_string(n);
		}
		std::string objFile = libDir + "/" + uniqueName + ".o";
		objFiles.push_back(objFile);

		// Build compile command as vector
		std::vector<std::string> compileArgs = {compiler, "-c", "-fPIC", "-O2", "-Wall"};
		for (const auto& inc : includePaths) {
			compileArgs.push_back(inc);
		}
		// After quadpm's own flags and include paths, so a module can override
		// what it needs to -- the compiler takes the last of a repeated option.
		for (const auto& flag : extraFlags) {
			compileArgs.push_back(flag);
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
	bool linkFailed = false;

	if (linkResult == 0) {
		std::cout << COLOR_GREEN << "  ✓ Built " << COLOR_RESET << libName << ".so\n";
	} else {
		std::cerr << COLOR_RED << "  ✗ Failed to build shared library" << COLOR_RESET << "\n";
		linkFailed = true;
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
		std::cerr << COLOR_RED << "  ✗ Failed to build static library" << COLOR_RESET << "\n";
		linkFailed = true;
	}

	// Clean up object files
	for (const auto& obj : objFiles) {
		fs::remove(obj);
	}

	return !linkFailed;
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
		std::string hostPath;	   // e.g., "github.com/klahr/collections"
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
// Update a module using semver resolution when a version constraint is available.
// Falls back to git pull for branch-based dependencies.
bool updateModule(const std::string& moduleDir, const Dependency* dep, std::string* newRef) {
	std::string modulesDir = getModulesDir();
	std::string name = fs::path(moduleDir).filename().string();

	// Parse module@version format
	std::string moduleName = name;
	std::string currentVersion;
	size_t atPos = name.find('@');
	if (atPos != std::string::npos) {
		moduleName = name.substr(0, atPos);
		currentVersion = name.substr(atPos + 1);
	}

	if (newRef) {
		*newRef = currentVersion;
	}

	std::string displayName = currentVersion.empty() ? moduleName : moduleName + " @ " + currentVersion;

	// If we have a semver-constrained dependency, resolve to newest compatible version
	if (dep && dep->isSemVer && !dep->version.empty() && isSemVer(currentVersion)) {
		std::cout << COLOR_CYAN << "Updating " << COLOR_BOLD << displayName << COLOR_RESET
				  << " (constraint: " << dep->version << ")...\n";

		std::string resolvedTag = resolveSemVerRange(dep->url, dep->version);
		if (resolvedTag.empty()) {
			std::cerr << COLOR_RED << "  ✗ Failed to resolve version range" << COLOR_RESET << "\n";
			return false;
		}

		// Check if already at the best version
		SemVer currentSV = parseSemVer(currentVersion);
		SemVer resolvedSV = parseSemVer(resolvedTag);
		if (currentSV.isValid() && resolvedSV.isValid() && !(resolvedSV > currentSV)) {
			std::cout << COLOR_GREEN << "  ✓ Already at newest compatible version (" << currentVersion << ")"
					  << COLOR_RESET << "\n";
			return true;
		}

		// Install at the new version
		std::string hostPath = extractHostPath(dep->url);
		std::string newDirName = getInstalledDirName(hostPath, resolvedTag);
		std::string newDir = modulesDir + "/" + newDirName;

		if (fs::exists(newDir) && isInstallComplete(newDir)) {
			std::cout << COLOR_GREEN << "  ✓ Version " << resolvedTag << " already installed" << COLOR_RESET << "\n";
		} else {
			GitRef gitRef;
			gitRef.url = dep->url;
			gitRef.ref = resolvedTag;
			gitRef.moduleName = moduleName;
			gitRef.hostPath = hostPath;
			std::string installed = gitClone(gitRef);
			if (installed.empty()) {
				return false;
			}
		}

		// Remove old version directory
		std::error_code ec;
		fs::remove_all(moduleDir, ec);
		if (ec) {
			std::cerr << COLOR_YELLOW << "  ⚠ Could not remove old version: " << ec.message() << COLOR_RESET << "\n";
		}

		// Update namespace symlink
		ensureNamespaceSymlink(newDir, newDirName, moduleName);

		if (newRef) {
			*newRef = resolvedTag;
		}

		std::cout << COLOR_GREEN << "  ✓ Updated " << moduleName << " " << currentVersion << " → " << resolvedTag
				  << COLOR_RESET << "\n";
		return true;
	}

	// Fallback: git pull for branch-based or unconstrained deps
	std::cout << COLOR_CYAN << "Updating " << COLOR_BOLD << displayName << COLOR_RESET << "...\n";

	int result = execCommandLive({"git", "-C", moduleDir, "pull"});

	if (result != 0) {
		std::cerr << COLOR_RED << "  ✗ Failed to update " << displayName << COLOR_RESET << "\n";
		return false;
	}

	std::cout << COLOR_GREEN << "  ✓ Updated " << displayName << COLOR_RESET << "\n";

	// Rebuild C sources if present
	std::string manifestPath = moduleDir + "/qd.json";
	bool built = runPrebuild(moduleDir, moduleName);
	if (built) {
		NativeConfig nativeConfig = parseNativeConfig(manifestPath);
		built = compileCsources(moduleDir, moduleName, nativeConfig);
	}
	if (!built) {
		pmError("failed to rebuild native sources for '" + displayName + "'; removed " + moduleDir);
		std::cerr << "Run 'quadpm install' or 'quadpm get' to reinstall it.\n";
		std::error_code ec;
		fs::remove_all(moduleDir, ec);
		return false;
	}

	return true;
}

// Build a module in the current directory (for local development)
int buildModule() {
	std::string cwd = fs::current_path().string();

	// Check for qd.json
	std::string manifestPath = cwd + "/qd.json";
	if (!fs::exists(manifestPath)) {
		pmError("no qd.json in the current directory");
		std::cerr << "Run this command from a module directory containing qd.json.\n";
		return 1;
	}

	// Parse module name from manifest
	std::string moduleName = parseModuleName(manifestPath);
	if (moduleName.empty()) {
		return pmError("could not parse the module name from qd.json");
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

	if (!nativeConfig.cflags.empty()) {
		std::cout << "  → Compile flags: ";
		for (size_t i = 0; i < nativeConfig.cflags.size(); i++) {
			if (i > 0) {
				std::cout << " ";
			}
			std::cout << nativeConfig.cflags[i];
		}
		std::cout << "\n";
	}

	// Before the src/ check, because it may be what puts src/ there
	if (!runPrebuild(cwd, moduleName)) {
		std::cerr << COLOR_RED << "Build failed" << COLOR_RESET << "\n";
		return 1;
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

	if (!isSafeGitArgument(gitUrl)) {
		pmError("refusing unsafe git URL: " + gitUrl);
		return tags;
	}

	try {
		// git ls-remote --tags -- <url> ("--" forces <url> to be positional)
		std::string output = execCommand({"git", "ls-remote", "--tags", "--", gitUrl});

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
	return parseVersionRange(version).isValid();
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

	std::string versionSpec = dep.version;
	std::string expectedCommit;

	if (frozen) {
		auto lockedIt = lockedByName.find(dep.name);
		if (lockedIt == lockedByName.end() || lockedIt->second.isPath || lockedIt->second.resolvedRef.empty()) {
			std::cout << COLOR_RED << "✗ No pinned commit in qd.lock (frozen)" << COLOR_RESET << "\n";
			std::cout << "  Run 'quadpm install' to update the lockfile.\n";
			return result;
		}
		if (lockedIt->second.url != dep.url) {
			std::cout << COLOR_RED << "✗ qd.lock is outdated (frozen)" << COLOR_RESET << "\n";
			std::cout << "  qd.lock:  " << lockedIt->second.url << "\n";
			std::cout << "  Manifest: " << dep.url << "\n";
			return result;
		}
		expectedCommit = lockedIt->second.resolvedRef;
		gitRef.ref = lockedIt->second.ref;
		if (!dep.sha256.empty() && dep.sha256 != expectedCommit) {
			std::cout << COLOR_RED << "✗ qd.lock and qd.json pin different commits" << COLOR_RESET << "\n";
			std::cout << "  qd.lock:  " << expectedCommit << "\n";
			std::cout << "  qd.json:  " << dep.sha256 << "\n";
			return result;
		}
	} else if (dep.isSemVer) {
		std::cout << "\n";
		std::string resolvedTag = resolveSemVerRange(dep.url, versionSpec);
		if (resolvedTag.empty()) {
			std::cout << COLOR_RED << "✗ Failed to resolve version range: " << versionSpec << COLOR_RESET << "\n";
			return result;
		}
		gitRef.ref = resolvedTag;
	} else {
		gitRef.ref = versionSpec;
	}

	CloneOptions cloneOptions;
	if (frozen) {
		cloneOptions.expectedCommit = expectedCommit;
		cloneOptions.mismatchLabel = "Commit mismatch (frozen)";
	} else if (!dep.sha256.empty()) {
		cloneOptions.expectedCommit = dep.sha256;
		cloneOptions.mismatchLabel = "Pinned commit mismatch";
	}

	result.actualRef = gitRef.ref;

	// Check if already installed (using Go-style hostPath)
	std::string installedDirName = getInstalledDirName(gitRef.hostPath, gitRef.ref);
	if (gitRef.hostPath.empty() || !isSafeInstalledDirName(installedDirName)) {
		std::cout << COLOR_RED << "✗ Refusing to install outside the modules directory: " << dep.url << COLOR_RESET
				  << "\n";
		return result;
	}
	std::string installedDir = modulesDir + "/" + installedDirName;
	if (fs::exists(installedDir) && isInstallComplete(installedDir)) {
		std::string currentCommit = getModuleCommitHash(installedDir);

		// In frozen mode, verify commit matches lockfile
		if (frozen && currentCommit != expectedCommit) {
			std::cout << COLOR_RED << "✗ Commit mismatch (frozen)" << COLOR_RESET << "\n";
			std::cout << "  Expected: " << expectedCommit << "\n";
			std::cout << "  Got:      " << currentCommit << "\n";
			return result;
		}

		if (!cloneOptions.expectedCommit.empty() && currentCommit != cloneOptions.expectedCommit) {
			std::cout << COLOR_YELLOW << "installed @ "
					  << (currentCommit.empty() ? "unknown" : currentCommit.substr(0, 8)) << " but qd.json pins "
					  << cloneOptions.expectedCommit.substr(0, 8) << "; reinstalling" << COLOR_RESET << "\n";
			cloneOptions.replaceExisting = true;
		} else {
			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "already installed @ "
					  << (gitRef.ref.empty() ? "HEAD" : gitRef.ref);
			if (!currentCommit.empty()) {
				std::cout << " (" << currentCommit.substr(0, 8) << ")";
			}
			std::cout << "\n";

			// Ensure namespace symlink exists (may have been missing)
			ensureNamespaceSymlink(installedDir, installedDirName, gitRef.moduleName);

			result.modulePath = installedDir;
			result.commitHash = currentCommit;
			result.alreadyInstalled = true;
			result.success = true;
			return result;
		}
	}

	// Clone the repository
	std::string installedName = gitClone(gitRef, cloneOptions);
	if (installedName.empty()) {
		std::cout << COLOR_RED << "✗ Failed to install" << COLOR_RESET << "\n";
		return result;
	}

	// Get the actual commit hash (use the same hostPath-based directory)
	std::string commitHash = getModuleCommitHash(installedDir);

	std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "installed @ " << (gitRef.ref.empty() ? "HEAD" : gitRef.ref);
	if (!commitHash.empty()) {
		std::cout << " (" << commitHash.substr(0, 8) << ")";
	}
	std::cout << "\n";

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
		return pmError("no qd.json in the current directory");
	}

	// Check for lockfile
	std::string lockfilePath = cwd + "/qd.lock";
	std::vector<LockedDependency> lockedDeps = readLockfile(lockfilePath);
	bool hasLockfile = !lockedDeps.empty();

	if (frozen && !hasLockfile) {
		pmError("--frozen requires qd.lock, but none was found");
		std::cerr << "Run 'quadpm install' first to generate a lockfile.\n";
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
				pmError("dependency '" + dep.name + "' is not in the lockfile");
				std::cerr << "Run 'quadpm install' to update the lockfile.\n";
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

	// Check for version conflicts before installing
	std::set<std::string> conflictVisited;
	std::vector<VersionConflict> conflicts = detectVersionConflicts(directDeps, cwd, conflictVisited);
	if (!conflicts.empty()) {
		pmWarn("detected version conflicts:");
		std::cout << "\n";
		for (const auto& conflict : conflicts) {
			std::cout << "  " << COLOR_BOLD << conflict.packageName << COLOR_RESET << ":\n";
			for (const auto& [requirer, version] : conflict.requirements) {
				std::cout << "    " << requirer << " requires " << COLOR_CYAN << version << COLOR_RESET << "\n";
			}
			std::cout << "\n";
		}
		std::cout << "  Consider updating your dependencies to use compatible versions.\n\n";
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

		if (frozen && lockedByName.find(dep.name) == lockedByName.end()) {
			std::cout << COLOR_RED << "✗ Not in the lockfile (frozen)" << COLOR_RESET << "\n";
			std::cout << "  Run 'quadpm install' to update the lockfile.\n";
			failures++;
			continue;
		}

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
		return pmError("no qd.json in the current directory");
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
			return pmError("failed to write qd.lock");
		}
	}

	return missing > 0 ? 1 : 0;
}

// Update installed modules
int updateModules(const std::string& targetModuleName) {
	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		return pmError("no modules installed");
	}

	// Load dependency constraints from qd.json (if present) for semver resolution
	std::map<std::string, Dependency> depsByName;
	std::string manifestPath = "qd.json";
	if (fs::exists(manifestPath)) {
		auto deps = parseDependencies(manifestPath);
		for (auto& dep : deps) {
			depsByName[dep.name] = dep;
		}
	}

	bool found = false;
	int failures = 0;
	std::vector<std::string> moduleDirs;

	// Recursively traverse to find all git repositories
	for (auto it = fs::recursive_directory_iterator(modulesDir); it != fs::recursive_directory_iterator(); ++it) {
		const auto& entry = *it;
		if (!entry.is_directory()) {
			continue;
		}

		// _namespaces holds a symlink to every module, so walking it would find
		// each one a second time under its bare name. Recursion has to be
		// disabled rather than the entry skipped: is_directory() follows the
		// symlinks, so a plain continue would still walk through them.
		if (entry.path().filename() == "_namespaces" || entry.path().filename() == STAGING_DIR_NAME) {
			it.disable_recursion_pending();
			continue;
		}

		// Check if this directory is a git repository
		std::string gitDir = entry.path().string() + "/.git";
		if (!fs::exists(gitDir)) {
			continue;
		}

		// A module is not a place to look for more modules. Its own checkouts
		// are its business -- a prebuild script that vendors an upstream tree
		// leaves one behind, and pulling that would mean updating a module to
		// something nobody asked for.
		it.disable_recursion_pending();

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

		found = true;
		moduleDirs.push_back(entry.path().lexically_normal().string());
	}

	std::map<std::string, std::string> updatedRefs;
	for (const auto& moduleDir : moduleDirs) {
		// Find matching dependency constraint
		std::string name = fs::path(moduleDir).filename().string();
		size_t atPos = name.find('@');
		std::string moduleName = (atPos != std::string::npos) ? name.substr(0, atPos) : name;
		auto depIt = depsByName.find(moduleName);
		const Dependency* dep = (depIt != depsByName.end()) ? &depIt->second : nullptr;

		std::string newRef;
		if (updateModule(moduleDir, dep, &newRef)) {
			updatedRefs[moduleDir] = newRef;
		} else {
			failures++;
		}
	}

	std::string lockfilePath = "qd.lock";
	std::vector<LockedDependency> locked =
			fs::exists(lockfilePath) ? readLockfile(lockfilePath) : std::vector<LockedDependency>();
	bool lockChanged = false;
	for (auto& entry : locked) {
		if (entry.isPath || entry.url.empty()) {
			continue;
		}
		std::string hostPath = extractHostPath(entry.url);
		std::string oldDir =
				fs::path(modulesDir + "/" + getInstalledDirName(hostPath, entry.ref)).lexically_normal().string();
		auto updated = updatedRefs.find(oldDir);
		if (updated == updatedRefs.end()) {
			continue;
		}
		std::string ref = (updated->second == "HEAD" && entry.ref.empty()) ? "" : updated->second;
		std::string commit = getModuleCommitHash(modulesDir + "/" + getInstalledDirName(hostPath, ref));
		if (commit.empty() || (ref == entry.ref && commit == entry.resolvedRef)) {
			continue;
		}
		entry.ref = ref;
		entry.resolvedRef = commit;
		entry.integrity = commit;
		lockChanged = true;
	}
	if (lockChanged) {
		if (writeLockfile(lockfilePath, locked)) {
			std::cout << COLOR_GREEN << "✓ " << COLOR_RESET << "Updated qd.lock\n";
		} else {
			pmError("failed to write qd.lock");
			failures++;
		}
	}

	if (!found) {
		if (targetModuleName.empty()) {
			pmError("no modules found to update");
		} else {
			pmError("module '" + targetModuleName + "' not found");
		}
		return 1;
	}

	return failures > 0 ? 1 : 0;
}

// Detect version conflicts in dependency tree
// Collects all version requirements for each package and checks for incompatibilities
std::vector<VersionConflict> detectVersionConflicts(
		const std::vector<Dependency>& deps, const std::string& basePath, std::set<std::string>& visited) {
	std::vector<VersionConflict> conflicts;

	// Map from package name to list of (requirer, version_range)
	std::map<std::string, std::vector<std::pair<std::string, std::string>>> requirements;

	// Queue for BFS traversal: (dependency, requirer_name, base_path)
	std::queue<std::tuple<Dependency, std::string, std::string>> pendingDeps;

	// Add direct dependencies
	for (const auto& dep : deps) {
		pendingDeps.push({dep, "(root)", basePath});
	}

	std::string modulesDir = getModulesDir();

	while (!pendingDeps.empty()) {
		auto [dep, requirer, depBasePath] = pendingDeps.front();
		pendingDeps.pop();

		// Record this requirement
		if (!dep.version.empty()) {
			requirements[dep.name].push_back({requirer, dep.version});
		}

		// Skip if already visited this package
		std::string visitKey = dep.name + "@" + dep.version;
		if (visited.count(visitKey) > 0) {
			continue;
		}
		visited.insert(visitKey);

		// Find transitive dependencies
		std::string modulePath;
		if (dep.isPath) {
			modulePath = resolveLocalPath(dep.url, depBasePath);
		} else {
			GitRef gitRef = parseGitUrl(dep.url);
			std::string installedDirName = getInstalledDirName(gitRef.hostPath, dep.version);
			modulePath = modulesDir + "/" + installedDirName;
		}

		std::string manifestPath = modulePath + "/qd.json";
		if (fs::exists(manifestPath)) {
			std::vector<Dependency> transitiveDeps = parseDependencies(manifestPath);
			for (const auto& transDep : transitiveDeps) {
				pendingDeps.push({transDep, dep.name, modulePath});
			}
		}
	}

	// Check for conflicts in collected requirements
	for (const auto& [pkgName, reqs] : requirements) {
		if (reqs.size() < 2) {
			continue; // No potential conflict with single requirement
		}

		// Check if all version ranges have a common satisfying version.
		// rangesHaveCommonVersion (in semver.cc) probes a set of boundary
		// candidate versions and reports false only when no satisfying
		// version exists across all ranges.
		std::vector<VersionRange> parsedRanges;
		std::vector<std::pair<std::string, std::string>> semverReqs;
		for (const auto& req : reqs) {
			if (!isSemVerRange(req.second)) {
				continue; // ignore branches/commits
			}
			parsedRanges.push_back(parseVersionRange(req.second));
			semverReqs.push_back(req);
		}

		if (parsedRanges.size() < 2) {
			continue;
		}

		if (!rangesHaveCommonVersion(parsedRanges)) {
			VersionConflict conflict;
			conflict.packageName = pkgName;
			conflict.requirements = semverReqs;
			conflicts.push_back(conflict);
		}
	}

	return conflicts;
}

// Show outdated packages with available updates
int showOutdated() {
	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		std::cout << "No modules installed.\n";
		return 0;
	}

	std::cout << COLOR_CYAN << "Checking for updates..." << COLOR_RESET << "\n\n";

	struct OutdatedPackage {
		std::string name;
		std::string hostPath;
		std::string currentVersion;
		std::string latestVersion;
		std::string gitUrl;
	};

	std::vector<OutdatedPackage> outdated;
	int checked = 0;

	// Find all installed packages
	for (const auto& entry : fs::recursive_directory_iterator(modulesDir)) {
		if (!entry.is_directory()) {
			continue;
		}

		std::string dirname = entry.path().filename().string();
		if (dirname == "_namespaces") {
			continue;
		}

		// Check if this looks like a package directory (has @ in name)
		size_t atPos = dirname.find('@');
		if (atPos == std::string::npos) {
			continue;
		}

		std::string dirPath = entry.path().string();
		if (!hasQuadrateFiles(dirPath) && !fs::exists(dirPath + "/qd.json")) {
			continue;
		}

		// Extract version from directory name
		std::string currentVersion = dirname.substr(atPos + 1);

		// Get host path
		std::string relativePath = dirPath.substr(modulesDir.size() + 1);
		size_t relAtPos = relativePath.find('@');
		std::string hostPath = (relAtPos != std::string::npos) ? relativePath.substr(0, relAtPos) : relativePath;

		// Only check semver-versioned packages
		if (!isSemVer(currentVersion)) {
			continue;
		}

		checked++;

		// Reconstruct git URL from hostPath
		std::string gitUrl;
		if (hostPath.find("github.com") == 0) {
			gitUrl = "https://" + hostPath;
		} else if (hostPath.find("git.sr.ht") == 0) {
			gitUrl = "https://" + hostPath;
		} else if (hostPath.find("gitlab.com") == 0) {
			gitUrl = "https://" + hostPath;
		} else {
			gitUrl = "https://" + hostPath;
		}

		// Get remote tags
		auto remoteTags = listRemoteTags(gitUrl);
		if (remoteTags.empty()) {
			continue;
		}

		// Find highest semver tag
		SemVer currentSemVer = parseSemVer(currentVersion);
		SemVer latestSemVer = currentSemVer;
		std::string latestTag;

		for (const auto& [tagName, commitHash] : remoteTags) {
			if (isSemVer(tagName)) {
				SemVer tagVer = parseSemVer(tagName);
				if (tagVer.isValid() && tagVer > latestSemVer) {
					latestSemVer = tagVer;
					latestTag = tagName;
				}
			}
		}

		if (!latestTag.empty() && latestSemVer > currentSemVer) {
			OutdatedPackage pkg;
			pkg.name = dirname.substr(0, atPos);
			pkg.hostPath = hostPath;
			pkg.currentVersion = currentVersion;
			pkg.latestVersion = latestTag;
			pkg.gitUrl = gitUrl;
			outdated.push_back(pkg);
		}
	}

	if (outdated.empty()) {
		std::cout << COLOR_GREEN << "All " << checked << " packages are up to date!" << COLOR_RESET << "\n";
		return 0;
	}

	std::cout << COLOR_BOLD << "Outdated packages:" << COLOR_RESET << "\n\n";

	// Sort by name
	std::sort(outdated.begin(), outdated.end(),
			[](const OutdatedPackage& a, const OutdatedPackage& b) { return a.hostPath < b.hostPath; });

	for (const auto& pkg : outdated) {
		std::cout << "  " << COLOR_BOLD << pkg.hostPath << COLOR_RESET << "\n";
		std::cout << "    Current: " << COLOR_YELLOW << pkg.currentVersion << COLOR_RESET << "\n";
		std::cout << "    Latest:  " << COLOR_GREEN << pkg.latestVersion << COLOR_RESET << "\n";
		std::cout << "\n";
	}

	std::cout << outdated.size() << " package" << (outdated.size() == 1 ? "" : "s") << " can be updated.\n";
	std::cout << "Run " << COLOR_CYAN << "quadpm get <url>@<version>" << COLOR_RESET << " to update.\n";

	return 0;
}

// Remove an installed module
int removeModule(const std::string& targetModuleName) {
	if (targetModuleName.empty()) {
		pmError("'remove' requires a module name");
		std::cerr << "Usage: quadpm remove <name>\n";
		return 1;
	}

	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		return pmError("no modules installed");
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
		// 3. Full hostPath (e.g., "github.com/quadrate-language/compress")
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
		pmError("module '" + targetModuleName + "' not found");
		std::cerr << "Use 'quadpm list' to see installed modules.\n";
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
			fs::path target = fs::read_symlink(symlinkPath);
			fs::path resolved = target.is_absolute() ? target : fs::path(namespacesDir) / target;
			if (resolved.lexically_normal() == fs::path(foundPath).lexically_normal()) {
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

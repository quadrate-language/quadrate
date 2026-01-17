// pm_utils.cc - Utility functions for quadpm

#include "pm_impl.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

// Get home directory
std::string getHomeDir() {
	const char* home = getenv("HOME");
	if (!home) {
		std::cerr << COLOR_RED << "Error: HOME environment variable not set" << COLOR_RESET << "\n";
		exit(1);
	}
	return std::string(home);
}

// Get modules directory path
std::string getModulesDir() {
	// Check QUADRATE_PATH environment variable first
	const char* quadratePath = getenv("QUADRATE_PATH");
	if (quadratePath) {
		return std::string(quadratePath);
	}

	// Check if XDG_DATA_HOME is set
	const char* xdgDataHome = getenv("XDG_DATA_HOME");
	if (xdgDataHome) {
		return std::string(xdgDataHome) + "/quadrate/modules";
	}

	// Default to ~/quadrate/modules
	return getHomeDir() + "/quadrate/modules";
}

// Check if a directory contains any .qd files
bool hasQuadrateFiles(const std::string& dirPath) {
	if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
		return false;
	}

	for (const auto& entry : fs::directory_iterator(dirPath)) {
		if (entry.is_regular_file() && entry.path().extension() == ".qd") {
			return true;
		}
	}
	return false;
}

// Execute a command safely using fork/exec (no shell interpretation)
// Returns exit status, captures stdout in 'output' if provided
int execCommandSafe(const std::vector<std::string>& args, std::string* output, bool showOutput) {
	if (args.empty()) {
		return -1;
	}

	// Create pipe for capturing output
	int pipefd[2] = {-1, -1};
	if (output || showOutput) {
		if (pipe(pipefd) == -1) {
			return -1;
		}
	}

	pid_t pid = fork();
	if (pid == -1) {
		if (pipefd[0] != -1) {
			close(pipefd[0]);
			close(pipefd[1]);
		}
		return -1;
	}

	if (pid == 0) {
		// Child process
		if (pipefd[1] != -1) {
			close(pipefd[0]);
			dup2(pipefd[1], STDOUT_FILENO);
			dup2(pipefd[1], STDERR_FILENO);
			close(pipefd[1]);
		}

		// Convert args to char* array
		std::vector<char*> argv;
		for (const auto& arg : args) {
			argv.push_back(const_cast<char*>(arg.c_str()));
		}
		argv.push_back(nullptr);

		execvp(argv[0], argv.data());
		_exit(127); // exec failed
	}

	// Parent process
	if (pipefd[1] != -1) {
		close(pipefd[1]);
	}

	// Read output
	if (pipefd[0] != -1) {
		std::array<char, 256> buffer;
		ssize_t n;
		while ((n = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
			if (output) {
				output->append(buffer.data(), static_cast<size_t>(n));
			}
			if (showOutput) {
				std::cout.write(buffer.data(), n);
			}
		}
		close(pipefd[0]);
	}

	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

// Execute command and capture output (throws on failure)
std::string execCommand(const std::vector<std::string>& args) {
	std::string output;
	int status = execCommandSafe(args, &output, false);
	if (status != 0) {
		throw std::runtime_error("Command failed with status: " + std::to_string(status));
	}
	return output;
}

// Execute command showing output in real-time
int execCommandLive(const std::vector<std::string>& args) {
	return execCommandSafe(args, nullptr, true);
}

// Check if a command exists in PATH
bool commandExists(const std::string& cmd) {
	std::string output;
	int status = execCommandSafe({"which", cmd}, &output, false);
	return status == 0;
}

// Get the path where a module would be installed
std::string getModuleInstallPath(const std::string& moduleName, const std::string& ref) {
	return getModulesDir() + "/" + moduleName + "@" + ref;
}

// Get the path to a local module's directory
std::string resolveLocalPath(const std::string& path, const std::string& basePath) {
	std::string resolvedPath = path;

	// Expand ~ to home directory
	if (resolvedPath.size() > 0 && resolvedPath[0] == '~') {
		resolvedPath = getHomeDir() + resolvedPath.substr(1);
	}

	// Make relative paths absolute
	if (resolvedPath.size() > 0 && resolvedPath[0] != '/') {
		resolvedPath = basePath + "/" + resolvedPath;
	}

	// Normalize the path
	return fs::weakly_canonical(resolvedPath).string();
}

// Get current platform name for platform-specific config
std::string getPlatformName() {
#if defined(__HAIKU__)
	return "haiku";
#elif defined(__linux__)
	return "linux";
#elif defined(__APPLE__)
	return "darwin";
#elif defined(_WIN32)
	return "windows";
#elif defined(__FreeBSD__)
	return "freebsd";
#elif defined(__OpenBSD__)
	return "openbsd";
#elif defined(__NetBSD__)
	return "netbsd";
#else
	return "unknown";
#endif
}

// Get the installed directory name for a module@ref
// For Go-style paths, this returns host/user/repo@ref format
std::string getInstalledDirName(const std::string& hostPath, const std::string& ref) {
	// Use "HEAD" for empty ref (repo's default branch)
	std::string actualRef = ref.empty() ? "HEAD" : ref;

	// Extract the repo name (last component)
	size_t lastSlash = hostPath.find_last_of('/');
	if (lastSlash != std::string::npos) {
		// Return host/user/repo@ref format
		std::string prefix = hostPath.substr(0, lastSlash + 1);
		std::string repoName = hostPath.substr(lastSlash + 1);
		return prefix + repoName + "@" + actualRef;
	}
	return hostPath + "@" + actualRef;
}

// Get the namespaces directory path
std::string getNamespacesDir() {
	return getModulesDir() + "/_namespaces";
}

// Find the library directory for a dependency by name
// Searches: 1) qd_modules/_namespaces/<name>, 2) sibling directories ../<name>
// currentModuleDir is used to find sibling directories
// Returns empty string if not found
std::string findDependencyLibDir(const std::string& depName, const std::string& currentModuleDir) {
	// First check qd_modules/_namespaces
	std::string namespacesDir = getNamespacesDir();
	std::string symlinkPath = namespacesDir + "/" + depName;

	std::string moduleDir;

	if (fs::exists(symlinkPath)) {
		// Resolve symlink to get actual module directory
		try {
			if (fs::is_symlink(symlinkPath)) {
				fs::path resolved = fs::read_symlink(symlinkPath);
				if (resolved.is_relative()) {
					moduleDir = fs::canonical(namespacesDir / resolved).string();
				} else {
					moduleDir = resolved.string();
				}
			} else {
				moduleDir = symlinkPath;
			}
		} catch (const std::exception&) {
			// Fall through to sibling check
		}
	}

	// If not found in namespaces, check sibling directories
	if (moduleDir.empty() && !currentModuleDir.empty()) {
		fs::path parentDir = fs::path(currentModuleDir).parent_path();
		std::string siblingPath = (parentDir / depName).string();
		if (fs::exists(siblingPath) && fs::is_directory(siblingPath)) {
			moduleDir = siblingPath;
		}
	}

	if (moduleDir.empty()) {
		return "";
	}

	std::string libDir = moduleDir + "/lib";
	if (fs::exists(libDir) && fs::is_directory(libDir)) {
		return libDir;
	}
	return "";
}

// Collect transitive native dependencies from a module's qd.json
// moduleDir is the directory containing qd.json (used for sibling lookup)
void collectTransitiveDeps(const std::string& manifestPath, const std::string& moduleDir,
		std::vector<std::string>& staticLibs, std::vector<std::string>& linkFlags, std::set<std::string>& visited) {
	std::vector<Dependency> deps = parseDependencies(manifestPath);

	for (const auto& dep : deps) {
		if (visited.count(dep.name)) {
			continue; // Avoid cycles
		}
		visited.insert(dep.name);

		std::string libDir = findDependencyLibDir(dep.name, moduleDir);
		if (libDir.empty()) {
			continue; // Dependency not installed or has no lib
		}

		// Find the static library
		std::string staticLib;
		std::string depsFile;
		for (const auto& entry : fs::directory_iterator(libDir)) {
			std::string filename = entry.path().filename().string();
			if (filename.find("_static.a") != std::string::npos) {
				staticLib = entry.path().string();
			} else if (filename.find("_static.deps") != std::string::npos) {
				depsFile = entry.path().string();
			}
		}

		// Add static library path
		if (!staticLib.empty()) {
			staticLibs.push_back(staticLib);
		}

		// Read and add deps file contents
		if (!depsFile.empty()) {
			std::ifstream ifs(depsFile);
			std::string line;
			while (std::getline(ifs, line)) {
				if (!line.empty() && line[0] == '-') {
					// It's a flag like -lssl
					linkFlags.push_back(line);
				} else if (!line.empty()) {
					// It's a library path - check if absolute or needs resolution
					staticLibs.push_back(line);
				}
			}
		}

		// Recursively process this dependency's dependencies
		// Find the actual module dir from libDir (go up one level)
		fs::path depModuleDir = fs::path(libDir).parent_path();
		std::string depManifestPath = (depModuleDir / "qd.json").string();
		if (fs::exists(depManifestPath)) {
			collectTransitiveDeps(depManifestPath, depModuleDir.string(), staticLibs, linkFlags, visited);
		}
	}
}

// pm_manifest.cc - Manifest parsing and lockfile operations for quadpm

#include "pm_impl.h"
#include <filesystem>
#include <iostream>
#include <jansson.h>

namespace fs = std::filesystem;

// Lockfile format version
const int LOCKFILE_VERSION = 1;

// Parse native section from qd.json
NativeConfig parseNativeConfig(const std::string& manifestPath) {
	NativeConfig config;

	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return config;
	}

	json_t* native = json_object_get(root, "native");
	if (native && json_is_object(native)) {
		// Parse common link libraries
		json_t* link = json_object_get(native, "link");
		if (link && json_is_array(link)) {
			size_t index;
			json_t* value;
			json_array_foreach(link, index, value) {
				if (json_is_string(value)) {
					config.link.push_back(json_string_value(value));
				}
			}
		}

		// Parse platform-specific link libraries (e.g., link_haiku, link_linux)
		std::string platformKey = "link_" + getPlatformName();
		json_t* platformLink = json_object_get(native, platformKey.c_str());
		if (platformLink && json_is_array(platformLink)) {
			size_t index;
			json_t* value;
			json_array_foreach(platformLink, index, value) {
				if (json_is_string(value)) {
					config.link.push_back(json_string_value(value));
				}
			}
		}
	}

	json_decref(root);
	return config;
}

// Parse module name from qd.json
std::string parseModuleName(const std::string& manifestPath) {
	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return "";
	}

	std::string result;
	json_t* name = json_object_get(root, "name");
	if (name && json_is_string(name)) {
		result = json_string_value(name);
	}

	json_decref(root);
	return result;
}

// Parse namespace from qd.json
// Returns the namespace (for use in code), defaulting to module name if not specified
std::string parseNamespace(const std::string& manifestPath) {
	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return "";
	}

	std::string result;
	json_t* ns = json_object_get(root, "namespace");
	if (ns && json_is_string(ns)) {
		result = json_string_value(ns);
	}

	json_decref(root);
	return result;
}

// Parse dependencies from qd.json
// Supports:
//   "name": "https://git.sr.ht/~user/repo@ref"  (git URL with branch/tag)
//   "name": "https://git.sr.ht/~user/repo"      (git URL, default branch)
//   "name": "../local/path"                      (local path)
//   "name": "^1.0.0"                             (semver range, uses default registry)
//   "name": { "url": "...", "version": "^1.0.0" }  (expanded form with semver)
//   "name": { "url": "...", "integrity": "sha256-..." }  (expanded form)
std::vector<Dependency> parseDependencies(const std::string& manifestPath) {
	std::vector<Dependency> deps;

	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return deps;
	}

	json_t* dependencies = json_object_get(root, "dependencies");
	if (dependencies && json_is_object(dependencies)) {
		const char* key;
		json_t* value;
		json_object_foreach(dependencies, key, value) {
			Dependency dep;
			dep.name = key;
			dep.isPath = false;
			dep.isSemVer = false;

			if (json_is_string(value)) {
				std::string strValue = json_string_value(value);

				// Check if it's a local path (starts with /, ./, ../, or ~/)
				if (strValue.size() > 0 && (strValue[0] == '/' || strValue[0] == '.' ||
												   (strValue.size() > 1 && strValue[0] == '~' && strValue[1] == '/'))) {
					dep.url = strValue;
					dep.isPath = true;
				}
				// Check if it looks like a URL
				else if (strValue.find("://") != std::string::npos || strValue.substr(0, 4) == "git@") {
					// It's a URL - extract version if present
					GitRef gitRef = parseGitUrl(strValue);
					dep.url = gitRef.url;
					dep.version = gitRef.ref;
					// Check if the ref looks like semver
					dep.isSemVer = isSemVerRange(dep.version);
				}
				// Otherwise treat as version constraint (semver range)
				else {
					dep.version = strValue;
					dep.isSemVer = isSemVerRange(strValue);
					// URL will need to be looked up - leave empty for now
				}
			} else if (json_is_object(value)) {
				// Expanded form: { "url": "...", "version": "...", "integrity": "sha256-..." }
				json_t* url = json_object_get(value, "url");
				if (url && json_is_string(url)) {
					std::string urlStr = json_string_value(url);
					// Check if URL has inline version (@tag)
					GitRef gitRef = parseGitUrl(urlStr);
					dep.url = gitRef.url;
					if (gitRef.ref != "master") {
						dep.version = gitRef.ref;
					}
					dep.isPath = (dep.url.size() > 0 &&
								  (dep.url[0] == '/' || dep.url[0] == '.' ||
										  (dep.url.size() > 1 && dep.url[0] == '~' && dep.url[1] == '/')));
				}

				// Version field overrides inline @version
				json_t* version = json_object_get(value, "version");
				if (version && json_is_string(version)) {
					dep.version = json_string_value(version);
				}

				dep.isSemVer = isSemVerRange(dep.version);

				json_t* integrity = json_object_get(value, "integrity");
				if (integrity && json_is_string(integrity)) {
					std::string integrityStr = json_string_value(integrity);
					// Strip "sha256-" prefix if present (npm-style)
					if (integrityStr.size() > 7 && integrityStr.substr(0, 7) == "sha256-") {
						dep.sha256 = integrityStr.substr(7);
					} else {
						dep.sha256 = integrityStr;
					}
				}
			}

			if (!dep.url.empty() || !dep.version.empty()) {
				deps.push_back(dep);
			}
		}
	}

	json_decref(root);
	return deps;
}

// Create or update a namespace symlink
// Returns true on success, false on conflict (different package already owns namespace)
bool createNamespaceSymlink(const std::string& namespaceName, const std::string& targetPath) {
	// namespaceName comes from a downloaded package's manifest. Reject anything
	// that is not a plain single path component so a malicious manifest cannot
	// place the symlink outside the _namespaces directory (path traversal).
	if (namespaceName.empty() || namespaceName == "." || namespaceName == ".." ||
			namespaceName.find('/') != std::string::npos || namespaceName.find('\\') != std::string::npos ||
			namespaceName.find("..") != std::string::npos) {
		std::cerr << COLOR_RED << "Error: invalid namespace name: " << COLOR_RESET << namespaceName << "\n";
		return false;
	}

	std::string namespacesDir = getNamespacesDir();
	fs::create_directories(namespacesDir);

	std::string symlinkPath = namespacesDir + "/" + namespaceName;

	// Check if symlink already exists
	if (fs::exists(symlinkPath) || fs::is_symlink(symlinkPath)) {
		// Check if it points to the same target
		try {
			std::string existingTarget = fs::read_symlink(symlinkPath).string();
			// Normalize for comparison (both should be relative to namespaces dir)
			if (existingTarget == targetPath || fs::weakly_canonical(namespacesDir + "/" + existingTarget) ==
														fs::weakly_canonical(namespacesDir + "/" + targetPath)) {
				return true; // Same target, no conflict
			}
			// Different target - this is a conflict
			return false;
		} catch (const std::exception&) {
			// If we can't read symlink, remove and recreate
			fs::remove(symlinkPath);
		}
	}

	// Create relative symlink
	// Target should be relative to _namespaces/ directory
	try {
		fs::create_symlink(targetPath, symlinkPath);
		return true;
	} catch (const std::exception& e) {
		std::cerr << COLOR_RED << "Error creating namespace symlink: " << COLOR_RESET << e.what() << "\n";
		return false;
	}
}

// Get all namespaces that point to a given hostPath
std::vector<std::string> getNamespacesForPackage(const std::string& hostPathWithRef) {
	std::vector<std::string> namespaces;
	std::string namespacesDir = getNamespacesDir();

	if (!fs::exists(namespacesDir)) {
		return namespaces;
	}

	for (const auto& entry : fs::directory_iterator(namespacesDir)) {
		if (fs::is_symlink(entry.path())) {
			try {
				std::string target = fs::read_symlink(entry.path()).string();
				// Check if target contains the hostPathWithRef
				if (target.find(hostPathWithRef) != std::string::npos) {
					namespaces.push_back(entry.path().filename().string());
				}
			} catch (const std::exception&) {
				// Ignore broken symlinks
			}
		}
	}

	return namespaces;
}

// Find which packages provide a given namespace
std::vector<std::string> findPackagesWithNamespace(const std::string& namespaceName) {
	std::vector<std::string> packages;
	std::string modulesDir = getModulesDir();

	if (!fs::exists(modulesDir)) {
		return packages;
	}

	// Recursively search for qd.json files
	for (const auto& entry : fs::recursive_directory_iterator(modulesDir)) {
		if (entry.is_regular_file() && entry.path().filename() == "qd.json") {
			std::string manifestPath = entry.path().string();
			std::string ns = parseNamespace(manifestPath);
			std::string name = parseModuleName(manifestPath);

			// If namespace not specified, use module name
			if (ns.empty()) {
				ns = name;
			}

			if (ns == namespaceName) {
				// Get the package directory relative to modules dir
				std::string pkgDir = entry.path().parent_path().string();
				if (pkgDir.substr(0, modulesDir.size()) == modulesDir) {
					packages.push_back(pkgDir.substr(modulesDir.size() + 1)); // +1 for '/'
				}
			}
		}
	}

	return packages;
}

// Read lockfile (qd.lock)
std::vector<LockedDependency> readLockfile(const std::string& lockfilePath) {
	std::vector<LockedDependency> locked;

	json_error_t error;
	json_t* root = json_load_file(lockfilePath.c_str(), 0, &error);
	if (!root) {
		return locked;
	}

	// Check version
	json_t* version = json_object_get(root, "version");
	if (!version || !json_is_integer(version) || json_integer_value(version) != LOCKFILE_VERSION) {
		json_decref(root);
		return locked;
	}

	json_t* packages = json_object_get(root, "packages");
	if (!packages || !json_is_array(packages)) {
		json_decref(root);
		return locked;
	}

	size_t index;
	json_t* pkg;
	json_array_foreach(packages, index, pkg) {
		if (!json_is_object(pkg)) {
			continue;
		}

		LockedDependency dep;

		json_t* name = json_object_get(pkg, "name");
		if (name && json_is_string(name)) {
			dep.name = json_string_value(name);
		}

		json_t* url = json_object_get(pkg, "url");
		if (url && json_is_string(url)) {
			dep.url = json_string_value(url);
		}

		json_t* ref = json_object_get(pkg, "ref");
		if (ref && json_is_string(ref)) {
			dep.ref = json_string_value(ref);
		}

		json_t* resolvedRef = json_object_get(pkg, "resolved");
		if (resolvedRef && json_is_string(resolvedRef)) {
			dep.resolvedRef = json_string_value(resolvedRef);
		}

		json_t* integrity = json_object_get(pkg, "integrity");
		if (integrity && json_is_string(integrity)) {
			dep.integrity = json_string_value(integrity);
		}

		json_t* isPath = json_object_get(pkg, "isPath");
		dep.isPath = isPath && json_is_true(isPath);

		json_t* resolvedPath = json_object_get(pkg, "resolvedPath");
		if (resolvedPath && json_is_string(resolvedPath)) {
			dep.resolvedPath = json_string_value(resolvedPath);
		}

		if (!dep.name.empty()) {
			locked.push_back(dep);
		}
	}

	json_decref(root);
	return locked;
}

// Write lockfile (qd.lock)
bool writeLockfile(const std::string& lockfilePath, const std::vector<LockedDependency>& locked) {
	json_t* root = json_object();
	if (!root) {
		return false;
	}

	// Add version
	json_object_set_new(root, "version", json_integer(LOCKFILE_VERSION));

	// Add packages array
	json_t* packages = json_array();
	for (const auto& dep : locked) {
		json_t* pkg = json_object();

		json_object_set_new(pkg, "name", json_string(dep.name.c_str()));

		if (!dep.url.empty()) {
			json_object_set_new(pkg, "url", json_string(dep.url.c_str()));
		}

		if (!dep.ref.empty()) {
			json_object_set_new(pkg, "ref", json_string(dep.ref.c_str()));
		}

		if (!dep.resolvedRef.empty()) {
			json_object_set_new(pkg, "resolved", json_string(dep.resolvedRef.c_str()));
		}

		if (!dep.integrity.empty()) {
			json_object_set_new(pkg, "integrity", json_string(dep.integrity.c_str()));
		}

		json_object_set_new(pkg, "isPath", dep.isPath ? json_true() : json_false());

		if (!dep.resolvedPath.empty()) {
			json_object_set_new(pkg, "resolvedPath", json_string(dep.resolvedPath.c_str()));
		}

		json_array_append_new(packages, pkg);
	}

	json_object_set_new(root, "packages", packages);

	// Write to file with pretty formatting
	int result = json_dump_file(root, lockfilePath.c_str(), JSON_INDENT(2) | JSON_SORT_KEYS);
	json_decref(root);

	return result == 0;
}

// Get commit hash for an installed module
std::string getModuleCommitHash(const std::string& moduleDir) {
	std::string gitDir = moduleDir + "/.git";
	if (!fs::exists(gitDir)) {
		return "";
	}

	try {
		std::string hash = execCommand({"git", "-C", moduleDir, "rev-parse", "HEAD"});
		hash.erase(hash.find_last_not_of(" \t\r\n") + 1);
		return hash;
	} catch (...) {
		return "";
	}
}

#include <quadrate/qc/module_resolver.h>

extern "C" {
#include "src/platform/exe_path_platform.h"
}
#include "src/platform/platform_paths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Qd {

	namespace {

		bool isDirectory(const std::string& path) {
			std::error_code ec;
			return !path.empty() && fs::is_directory(path, ec);
		}

		// One string field of a qd.json. The manifest is small and flat where these fields live,
		// and this library does not link a JSON parser; the tools that read the rest of it do.
		std::string manifestField(const std::string& directory, const std::string& key) {
			std::ifstream file(directory + "/qd.json");
			if (!file) {
				return "";
			}
			std::stringstream buffer;
			buffer << file.rdbuf();
			const std::string content = buffer.str();
			const size_t keyPos = content.find("\"" + key + "\"");
			const size_t colon = keyPos == std::string::npos ? keyPos : content.find(':', keyPos + key.size() + 2);
			const size_t open = colon == std::string::npos ? colon : content.find('"', colon + 1);
			const size_t close = open == std::string::npos ? open : content.find('"', open + 1);
			return close == std::string::npos ? "" : content.substr(open + 1, close - open - 1);
		}

		// Version order for `name@version` directories: digit runs compare as numbers, so 1.10.0
		// is above 1.9.0, and anything else compares as text
		bool versionLess(const std::string& a, const std::string& b) {
			size_t i = 0;
			size_t j = 0;
			while (i < a.size() && j < b.size()) {
				if (std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j]))) {
					size_t ie = i;
					size_t je = j;
					while (ie < a.size() && std::isdigit(static_cast<unsigned char>(a[ie]))) {
						ie++;
					}
					while (je < b.size() && std::isdigit(static_cast<unsigned char>(b[je]))) {
						je++;
					}
					const std::string na = a.substr(i, ie - i);
					const std::string nb = b.substr(j, je - j);
					if (na.size() != nb.size()) {
						return na.size() < nb.size();
					}
					if (na != nb) {
						return na < nb;
					}
					i = ie;
					j = je;
				} else {
					if (a[i] != b[j]) {
						return a[i] < b[j];
					}
					i++;
					j++;
				}
			}
			return a.size() - i < b.size() - j;
		}

		// The newest of the `<base>@<version>` directories in `parent`, or the pinned one
		std::string installedVersion(const std::string& parent, const std::string& base, const std::string& pin) {
			if (!pin.empty()) {
				const std::string pinned = parent + "/" + base + "@" + pin;
				return isDirectory(pinned) ? pinned : "";
			}
			std::string best;
			std::string bestVersion;
			std::error_code ec;
			for (const auto& entry : fs::directory_iterator(parent, ec)) {
				const std::string dirName = entry.path().filename().string();
				const std::string prefix = base + "@";
				if (!entry.is_directory(ec) || dirName.compare(0, prefix.size(), prefix) != 0) {
					continue;
				}
				const std::string version = dirName.substr(prefix.size());
				if (best.empty() || versionLess(bestVersion, version)) {
					best = entry.path().string();
					bestVersion = version;
				}
			}
			return best;
		}

		// Every installed package whose qd.json claims `namespaceName`
		std::vector<std::string> packagesClaiming(const std::string& packages, const std::string& namespaceName) {
			std::vector<std::string> claims;
			std::error_code ec;
			for (auto it = fs::recursive_directory_iterator(packages, ec);
					!ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
				const std::string name = it->path().filename().string();
				if (it.depth() == 0 && (name == "_namespaces" || name.rfind('.', 0) == 0)) {
					it.disable_recursion_pending();
					continue;
				}
				if (name == "qd.json" &&
						manifestField(it->path().parent_path().string(), "namespace") == namespaceName) {
					claims.push_back(it->path().parent_path().string());
				}
			}
			std::sort(claims.begin(), claims.end());
			return claims;
		}

		// A `use github.com/user/repo`: a path to an installed package rather than a short name
		bool isFullPathImport(const std::string& name) {
			return !name.empty() && name[0] != '.' && name[0] != '~' && name[0] != '/' &&
				   name.find('/') != std::string::npos;
		}

		ModuleLocation findPackage(const std::string& name, const ModuleSearch& search) {
			ModuleLocation location;
			const std::string packages = packagesDirectory();
			if (!isDirectory(packages)) {
				return location;
			}
			auto pinIt = search.versionPins.find(name);
			const std::string pin = pinIt != search.versionPins.end() ? pinIt->second : "";

			if (isFullPathImport(name)) {
				const fs::path path(name);
				location.directory = installedVersion(
						(fs::path(packages) / path.parent_path()).string(), path.filename().string(), pin);
				if (!location.directory.empty()) {
					const std::string declared = manifestField(location.directory, "namespace");
					location.namespaceName = declared.empty() ? path.filename().string() : declared;
				}
				return location;
			}

			location.namespaceName = name;
			if (pin.empty()) {
				const std::string link = packages + "/_namespaces/" + name;
				if (isDirectory(link)) {
					std::vector<std::string> claims = packagesClaiming(packages, name);
					if (claims.size() > 1) {
						location.ambiguous = std::move(claims);
						return location;
					}
					std::error_code ec;
					const fs::path target = fs::canonical(link, ec);
					location.directory = ec ? link : target.string();
					return location;
				}
			}
			location.directory = installedVersion(packages, name, pin);
			return location;
		}

	} // namespace

	std::string expandTilde(const std::string& path) {
		const char* home = std::getenv("HOME");
		if (home == nullptr || path.empty() || path[0] != '~' || (path.size() > 1 && path[1] != '/')) {
			return path;
		}
		return std::string(home) + path.substr(1);
	}

	std::string packagesDirectory() {
		if (const char* quadratePath = std::getenv("QUADRATE_PATH")) {
			return quadratePath;
		}
		if (const char* xdgDataHome = std::getenv("XDG_DATA_HOME")) {
			return std::string(xdgDataHome) + "/quadrate/modules";
		}
		if (const char* home = std::getenv("HOME")) {
			return std::string(home) + "/quadrate/modules";
		}
		return "";
	}

	std::vector<std::string> moduleRoots() {
		std::vector<std::string> roots;
		auto add = [&roots](const std::string& path) {
			if (isDirectory(path)) {
				roots.push_back(path);
			}
		};
		if (const char* root = std::getenv("QUADRATE_ROOT")) {
			add(root);
		}
		char exePath[4096];
		const int len = exe_path_platform_get(exePath, sizeof exePath);
		if (len > 0 && static_cast<size_t>(len) < sizeof exePath) {
			std::error_code ec;
			const fs::path exe = fs::canonical(exePath, ec);
			if (!ec) {
				add((exe.parent_path() / ".." / DATA_DIR_NAME / "quadrate").lexically_normal().string());
			}
		}
		if (const char* home = std::getenv("HOME")) {
			add(std::string(home) + "/quadrate");
		}
#ifdef __HAIKU__
		add("/boot/system/data/quadrate");
#else
		add("/usr/share/quadrate");
#endif
		return roots;
	}

	std::vector<std::string> moduleFiles(const std::string& directory) {
		std::vector<std::string> files;
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(directory, ec)) {
			const std::string name = entry.path().filename().string();
			const bool isQd = name.size() > 3 && name.compare(name.size() - 3, 3, ".qd") == 0;
			const bool isTest = name.size() > 8 && name.compare(name.size() - 8, 8, "_test.qd") == 0;
			if (isQd && !isTest && entry.is_regular_file(ec)) {
				files.push_back(entry.path().string());
			}
		}
		std::sort(files.begin(), files.end());
		return files;
	}

	std::vector<std::string> moduleSiblings(const std::string& file, const std::vector<std::string>& moduleFiles,
			const std::function<std::unordered_set<std::string>(const std::string&)>& qdImportsOf) {
		const std::string fileName = fs::path(file).filename().string();
		const std::unordered_set<std::string> importedByFile = qdImportsOf(file);
		std::vector<std::string> siblings;
		for (const std::string& other : moduleFiles) {
			if (other == file || importedByFile.count(fs::path(other).filename().string()) > 0 ||
					qdImportsOf(other).count(fileName) > 0) {
				continue;
			}
			siblings.push_back(other);
		}
		return siblings;
	}

	ModuleLocation findModule(const std::string& name, const ModuleSearch& search) {
		auto found = [&name](const std::string& directory) {
			ModuleLocation location;
			location.directory = directory;
			location.namespaceName = name;
			return location;
		};
		auto holdsModule = [](const std::string& directory) {
			return isDirectory(directory) && !moduleFiles(directory).empty();
		};

		if (!search.sourceDir.empty() && holdsModule(search.sourceDir + "/" + name)) {
			return found(search.sourceDir + "/" + name);
		}
		for (const std::string& written : search.includePaths) {
			const std::string includePath = expandTilde(written);
			if (manifestField(includePath, "name") == name && holdsModule(includePath)) {
				return found(includePath);
			}
			if (holdsModule(includePath + "/" + name)) {
				return found(includePath + "/" + name);
			}
		}
		ModuleLocation package = findPackage(name, search);
		if (!package.ambiguous.empty() || (!package.directory.empty() && holdsModule(package.directory))) {
			return package;
		}
		for (const std::string& root : moduleRoots()) {
			if (holdsModule(root + "/" + name)) {
				return found(root + "/" + name);
			}
		}
		return {};
	}

	std::string findModuleFile(const std::string& path, const ModuleSearch& search) {
		auto existing = [](const fs::path& candidate) {
			std::error_code ec;
			return fs::is_regular_file(candidate, ec) ? fs::weakly_canonical(candidate, ec).string() : std::string();
		};
		const std::string expanded = expandTilde(path);
		if (!expanded.empty() && expanded[0] == '/') {
			return existing(expanded);
		}
		std::vector<std::string> dirs;
		dirs.push_back(search.sourceDir.empty() ? "." : search.sourceDir);
		dirs.insert(dirs.end(), search.fileSearchDirs.begin(), search.fileSearchDirs.end());
		for (const std::string& dir : dirs) {
			const std::string file = existing(fs::path(dir) / expanded);
			if (!file.empty()) {
				return file;
			}
		}
		return "";
	}

} // namespace Qd

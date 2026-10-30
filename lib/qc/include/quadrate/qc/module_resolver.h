/**
 * @file module_resolver.h
 * @brief Where a `use` finds its module: the one search every tool runs.
 *
 * quadc, the semantic validator, quadlsp, quadrepl, quadpm and the embedding library each
 * kept their own copy of this search, and the copies had drifted: the validator looked for installed packages in a
 * `cache/` directory quadpm no longer writes, quadc picked whichever installed version the
 * directory listing happened to return last, and only quadc honoured `-l` version pins. A
 * program could be validated against one module and compiled against another.
 */

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Qd {

	/// Where to look, beyond the fixed places every search tries
	struct ModuleSearch {
		/// The directory of the file that wrote the `use`
		std::string sourceDir;
		/// Further directories for a `use "file.qd"`, after sourceDir: the importing module's own
		std::vector<std::string> fileSearchDirs;
		/// `-I` paths, and the local dependencies a qd.json names
		std::vector<std::string> includePaths;
		/// `-l name@version`: the installed version of `name` to use
		std::unordered_map<std::string, std::string> versionPins;
	};

	/// What a `use` of a module directory resolved to
	struct ModuleLocation {
		/// The module's directory; empty when there is none
		std::string directory;
		/// The namespace its words go under: the name written, or for a full-path import
		/// (`use github.com/user/collections`) the namespace its qd.json declares
		std::string namespaceName;
		/// When a short name is claimed by more than one installed package, their directories,
		/// and `directory` is empty: the program has to name one by its full path
		std::vector<std::string> ambiguous;
	};

	/// `~` and `~/...` expanded against $HOME; anything else unchanged
	std::string expandTilde(const std::string& path);

	/// Where quadpm installs: $QUADRATE_PATH, else $XDG_DATA_HOME/quadrate/modules, else
	/// ~/quadrate/modules. Empty when none can be named.
	std::string packagesDirectory();

	/// The fixed directories modules live under, in the order they are searched:
	/// $QUADRATE_ROOT, the installation's share/quadrate beside the running executable,
	/// ~/quadrate, and the system's share/quadrate. Only those that exist.
	std::vector<std::string> moduleRoots();

	/// A module directory's source files: every *.qd but the *_test.qd, sorted
	std::vector<std::string> moduleFiles(const std::string& directory);

	/// The files of a module that `file` is validated alongside, so it sees their definitions:
	/// every one of `moduleFiles` but `file` itself, one it `use`s, and one that `use`s it, which
	/// the `use` brings in already. `qdImportsOf` gives the file names a file `use`s.
	std::vector<std::string> moduleSiblings(const std::string& file, const std::vector<std::string>& moduleFiles,
			const std::function<std::unordered_set<std::string>(const std::string&)>& qdImportsOf);

	/// Resolve `use name`. The search, first match wins: `sourceDir/name`; each include path,
	/// which is the module itself when its qd.json says so, else holds it as `name/`; the
	/// installed packages; then each of moduleRoots().
	ModuleLocation findModule(const std::string& name, const ModuleSearch& search);

	/// Resolve `use "path.qd"`: an absolute path as written (after `~`), else relative to
	/// sourceDir, then to each of fileSearchDirs. Empty when the file is in none of them.
	std::string findModuleFile(const std::string& path, const ModuleSearch& search);

} // namespace Qd

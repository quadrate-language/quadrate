// quadpm - Quadrate Module Manager
// Manages 3rd party Git-based modules

#include "pm_impl.h"
#include "version.h"
#include <cstring>
#include <iostream>
#include <quadrate/cli/cli.h>
#include <string>

// Print usage information
static void printUsage() {
	std::cout << "quadpm - Quadrate module manager\n\n";
	std::cout << "Manages 3rd party modules from Git repositories.\n\n";
	std::cout << "Usage: quadpm [options] <command> [arguments]\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n\n";
	std::cout << "Commands:\n";
	std::cout << "  install          Install dependencies from qd.json\n";
	std::cout << "    --frozen       Only install from qd.lock (fail if outdated)\n";
	std::cout << "  lock             Generate/update qd.lock from installed modules\n";
	std::cout << "  get <url>[@ref]  Fetch and install a module from Git\n";
	std::cout << "  update [name]    Update installed module(s) (git pull)\n";
	std::cout << "  remove <name>    Remove an installed module\n";
	std::cout << "  list             List installed modules\n";
	std::cout << "  outdated         Show packages with available updates\n";
	std::cout << "  build            Build C sources in current module directory\n\n";
	std::cout << "Lockfile (qd.lock):\n";
	std::cout << "  The lockfile pins exact commit hashes for reproducible builds.\n";
	std::cout << "  - 'install' creates/updates qd.lock automatically\n";
	std::cout << "  - 'install --frozen' uses qd.lock strictly (for CI)\n";
	std::cout << "  - 'lock' regenerates qd.lock from installed modules\n\n";
	std::cout << "Transitive Dependencies:\n";
	std::cout << "  quadpm automatically resolves and installs transitive dependencies.\n";
	std::cout << "  If package A depends on B, and B depends on C, all three are installed.\n\n";
	std::cout << "Examples:\n";
	std::cout << "  quadpm install\n";
	std::cout << "  quadpm install --frozen\n";
	std::cout << "  quadpm lock\n";
	std::cout << "  quadpm get https://github.com/user/zlib\n";
	std::cout << "  quadpm get https://github.com/user/zlib@1.2.0\n";
	std::cout << "  quadpm get https://github.com/user/qdhttp@master\n";
	std::cout << "  quadpm list\n\n";
	std::cout << "qd.json format (npm-compatible):\n";
	std::cout << "  {\n";
	std::cout << "    \"name\": \"mymodule\",\n";
	std::cout << "    \"dependencies\": {\n";
	std::cout << "      \"glut\": \"https://github.com/user/qd-glut@v1.0.0\",\n";
	std::cout << "      \"http\": { \"url\": \"https://github.com/user/qd-http\", \"version\": \"^2.0.0\" },\n";
	std::cout << "      \"mylib\": \"../local/path\",\n";
	std::cout << "      \"crypto\": {\n";
	std::cout << "        \"url\": \"https://github.com/user/qd-crypto\",\n";
	std::cout << "        \"version\": \"~1.5.0\",\n";
	std::cout << "        \"integrity\": \"sha256-abc123...\"\n";
	std::cout << "      }\n";
	std::cout << "    }\n";
	std::cout << "  }\n\n";
	std::cout << "Semver version ranges:\n";
	std::cout << "  ^1.2.3    Compatible with version (>=1.2.3 <2.0.0)\n";
	std::cout << "  ~1.2.3    Approximately equivalent (>=1.2.3 <1.3.0)\n";
	std::cout << "  >=1.0.0   Greater than or equal\n";
	std::cout << "  <2.0.0    Less than\n";
	std::cout << "  1.2.x     Any patch version (>=1.2.0 <1.3.0)\n";
	std::cout << "  *         Any version\n";
	std::cout << "  1.0.0 - 2.0.0   Hyphen range (inclusive)\n";
	std::cout << "  >=1.0.0 <2.0.0 || >=3.0.0   Multiple ranges\n\n";
	std::cout << "Environment:\n";
	std::cout << "  QUADRATE_PATH      Module installation directory\n";
	std::cout << "  XDG_DATA_HOME      If set, uses $XDG_DATA_HOME/quadrate/modules\n";
	std::cout << "  Default: ~/quadrate/modules\n";
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printUsage();
		return 1;
	}

	std::string command = argv[1];

	if (command == "-h" || command == "--help") {
		printUsage();
		return 0;
	}

	if (command == "-v" || command == "--version") {
		qdcli::printVersion("quadpm");
		return 0;
	}

	if (command == "get") {
		if (argc < 3) {
			std::cerr << COLOR_RED << "Error: 'get' requires a Git URL" << COLOR_RESET << "\n";
			std::cerr << "Usage: " << argv[0] << " get <git-url>[@ref]\n";
			std::cerr << "Example: " << argv[0] << " get https://github.com/user/zlib@1.2.0\n";
			return 1;
		}

		std::string gitUrl = argv[2];
		GitRef gitRef = parseGitUrl(gitUrl);

		std::string installedName = gitClone(gitRef);
		if (installedName.empty()) {
			return 1;
		}

		std::cout << "\n"
				  << COLOR_GREEN << "Success!" << COLOR_RESET
				  << " You can now use this module in your Quadrate code:\n";
		std::cout << "  " << COLOR_CYAN << "use " << installedName << COLOR_RESET << "\n";

		return 0;
	}

	if (command == "list" || command == "ls") {
		listModules();
		return 0;
	}

	if (command == "update") {
		std::string targetModuleName = (argc >= 3) ? argv[2] : "";
		return updateModules(targetModuleName);
	}

	if (command == "remove" || command == "rm" || command == "uninstall") {
		std::string targetModuleName = (argc >= 3) ? argv[2] : "";
		return removeModule(targetModuleName);
	}

	if (command == "build") {
		return buildModule();
	}

	if (command == "install" || command == "i") {
		// Check for --frozen flag
		bool frozen = false;
		for (int i = 2; i < argc; i++) {
			if (std::string(argv[i]) == "--frozen") {
				frozen = true;
			}
		}
		return installDependencies(frozen);
	}

	if (command == "lock") {
		return generateLockfile();
	}

	if (command == "outdated") {
		return showOutdated();
	}

	std::cerr << COLOR_RED << "Error: Unknown command '" << command << "'" << COLOR_RESET << "\n";
	printUsage();
	return 1;
}

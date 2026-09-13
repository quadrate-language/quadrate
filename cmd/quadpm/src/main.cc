// quadpm - Quadrate Module Manager
// Manages 3rd party Git-based modules

#include "pm_impl.h"
#include "version.h"
#include <cstring>
#include <iostream>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/help.h>
#include <string>
#include <unistd.h>
#include <vector>

// Print usage information
static void printUsage() {
	qdcli::Help help("quadpm", "Quadrate module manager");
	help.description("Installs and updates third-party modules from Git repositories.")
			.usage("[options] <command> [arguments]")
			.section("Commands")
			.item("install", "Install the dependencies listed in qd.json")
			.item("lock", "Generate or update qd.lock from the installed modules")
			.item("get <url>[@ref]", "Fetch and install one module from Git")
			.item("update [name]", "Update installed modules (git pull)")
			.item("remove <name>", "Remove an installed module")
			.item("list", "List installed modules")
			.item("outdated", "Show modules with newer versions available")
			.item("build", "Build the C sources of the module in this directory")
			.section("Options")
			.standardOptions()
			.option("--frozen", "With 'install', install only from qd.lock (fail if outdated)")
			.section("Lockfile")
			.text("qd.lock pins exact commit hashes for reproducible builds. 'install'")
			.text("creates and updates it, 'install --frozen' uses it strictly (for CI),")
			.text("and 'lock' regenerates it from what is installed. Transitive")
			.text("dependencies are resolved and installed automatically.")
			.section("Manifest (qd.json)")
			.text("{")
			.text("  \"name\": \"mymodule\",")
			.text("  \"dependencies\": {")
			.text("    \"glut\":   \"https://github.com/user/qd-glut@v1.0.0\",")
			.text("    \"http\":   { \"url\": \"https://github.com/user/qd-http\", \"version\": \"^2.0.0\" },")
			.text("    \"mylib\":  \"../local/path\",")
			.text("    \"crypto\": { \"url\": \"https://github.com/user/qd-crypto\",")
			.text("                \"version\": \"~1.5.0\", \"commit\": \"a1b2c3d4...\" }")
			.text("  }")
			.text("}")
			.section("Version ranges")
			.item("^1.2.3", "Compatible with the version (>=1.2.3 <2.0.0)")
			.item("~1.2.3", "Approximately equivalent (>=1.2.3 <1.3.0)")
			.item("1.2.x", "Any patch version (>=1.2.0 <1.3.0)")
			.item(">=1.0.0, <2.0.0", "Comparison ranges")
			.item("1.0.0 - 2.0.0", "Hyphen range (inclusive)")
			.item(">=1.0.0 <2.0.0 || >=3.0.0", "Several ranges at once")
			.item("*", "Any version")
			.section("Environment")
			.item("QUADRATE_PATH", "Module installation directory")
			.item("XDG_DATA_HOME", "If set, uses $XDG_DATA_HOME/quadrate/modules")
			.text()
			.text("Default: ~/quadrate/modules")
			.section("Examples")
			.item("quadpm install", "Install everything qd.json asks for")
			.item("quadpm install --frozen", "Install strictly from qd.lock (for CI)")
			.item("quadpm get https://github.com/user/zlib", "Install a module from Git")
			.item("quadpm get https://github.com/user/zlib@1.2.0", "Install one version of it")
			.item("quadpm list", "List what is installed");
	help.print();
}

static bool g_pmColor = false;

bool pmColorEnabled() {
	return g_pmColor;
}

void pmSetColorEnabled(bool enabled) {
	g_pmColor = enabled;
}

int main(int argc, char** argv) {
	bool noColorFlag = false;
	std::vector<char*> args;
	for (int i = 0; i < argc; i++) {
		if (std::string(argv[i]) == "--no-color" || std::string(argv[i]) == "--no-colors") {
			noColorFlag = true;
			continue;
		}
		args.push_back(argv[i]);
	}
	argc = static_cast<int>(args.size());
	argv = args.data();
	pmSetColorEnabled(!noColorFlag && !qdcli::noColor() && isatty(STDOUT_FILENO) && isatty(STDERR_FILENO));

	// A bare invocation of a command dispatcher is a request to see the commands,
	// so print help and succeed -- the same thing `quad` with no arguments does.
	// The file-taking tools differ deliberately: a missing path there is a
	// mistake, and they report it tersely on stderr.
	if (argc < 2) {
		printUsage();
		return 0;
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
			pmError("'get' requires a Git URL");
			std::cerr << "Usage: quadpm get <git-url>[@ref]\n";
			std::cerr << "Example: quadpm get https://github.com/user/zlib@1.2.0\n";
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

	// Unknown input gets the same two lines as every other tool rather than the
	// whole help page on stdout, which buried the diagnostic.
	if (!command.empty() && command[0] == '-') {
		return pmUsageError("unknown option: " + command);
	}
	return pmUsageError("unknown command '" + command + "'");
}
